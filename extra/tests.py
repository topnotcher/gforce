import time
import asyncio
import sys
import collections
import functools
import operator
import socket
import binascii
import select

from tamp import *

from cmds import MPC_CMD, MPC_ADDR, MPC_CRC_POLY, MPC_CRC_SHIFT


class ChecksumMismatchError(ValueError):
    pass


class FrameFormatError(ValueError):
    pass


class FrameStartError(FrameFormatError):
    def __init__(self, *args):
        super(FrameStartError, self).__init__('Invalid start byte')


class FrameSyncError(FrameFormatError):
    def __init__(self, msg):
        super(FrameSyncError, self).__init__('chk byte mismatch: ' + msg)


class mpc_pkt(Structure):
    _fields_ = [
        ('len', uint8_t),
        ('cmd', EnumWrap(MPC_CMD, uint8_t)),
        ('saddr', uint8_t),
        ('chksum', Computed(uint8_t, '_compute_chksum', mismatch_exc=ChecksumMismatchError)),
        ('data', uint8_t[LengthField('len')]),
    ]

    def _compute_chksum(self):
        chksum = MPC_CRC_SHIFT

        hdr_fields = ['len', 'cmd', 'saddr']
        for field in hdr_fields:
            chksum = crc_ibutton_update(chksum, getattr(self, field))

        for byte in self.data:
            chksum = crc_ibutton_update(chksum, byte)

        return chksum


class framed_pkt(Structure):
    _fields_ = [
        ('start', Const(uint8_t, 0xFF, mismatch_exc=FrameStartError)),
        ('daddr', uint8_t),
        ('len', uint8_t),
        ('chk', Computed(uint8_t, '_compute_chk', mismatch_exc=FrameSyncError)),
        ('pkt', PackedLength(mpc_pkt, 'len')),
    ]

    def _compute_chk(self):
        return (self.daddr ^ self.len) & 0xFE


class GForceClient:
    def __init__(self, server, ip):
        self.server = server
        self.ip = ip
        self.queue = asyncio.Queue(loop=self.server.loop)
        self.last_rx = 0

    async def get_pkt(self, timeout=1.0):
        return (await asyncio.wait_for(self.queue.get(), timeout, loop=self.server.loop))

    def send_cmd(self, cmd, data):
        self.server.send_cmd(self.ip, cmd, data)

    def pkt_received(self, pkt):
        self.queue.put_nowait(pkt)
        self.last_rx = time.monotonic()


class GForceServer:
    ClientConn = collections.namedtuple('ClientConn', ('stream', 'client'))

    def __init__(self, port, loop, bind_host='0.0.0.0'):
        self.bind_host = bind_host
        self.port = port
        self.loop = loop

        self._clients = {}

        listen = self.loop.create_datagram_endpoint(lambda: self, local_addr=(self.bind_host, self.port))
        self._transport, _ = self.loop.run_until_complete(listen)

    def connect(self, ip):
        return self._connect(ip).client

    def _connect(self, ip):
        if ip not in self._clients:
            self._clients[ip] = self.ClientConn(StreamUnpacker(framed_pkt), GForceClient(self, ip))

        return self._clients[ip]

    def send_cmd(self, ip, cmd, data):
        frame = framed_pkt()
        pkt = frame.pkt

        pkt.cmd = cmd
        pkt.saddr = 4 # TODO?
        pkt.data = data

        frame.daddr = MPC_ADDR.MASTER

        self._transport.sendto(bytes(frame), (ip, self.port))

    def connection_made(self, transport):
        self._transport = transport

        sock = self._transport.get_extra_info('socket')
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, True)

    def datagram_received(self, data, addr):
        stream, client = self._connect(addr[0])

        more = True
        while more:
            try:
                for frame in stream.unpack(data):
                    client.pkt_received(frame.pkt)

                # TODO:
                # This is some shitty bullshit. If there's an exception in
                # unpack, there may be unprocessed bytes in the internal
                # buffer. We want to process all the bytes we can now.
                more = False
            except FrameFormatError as e:
                print('Error receiving frame:', str(e))
                print(data)
                data = None

            except ChecksumMismatchError as e:
                print('Checksum mismatch:', str(e))
                print(data)
                data = None

    def active_clients(self, timeout=25.0):
        return (conn.client for conn in self._clients.values() if conn.client.last_rx >= time.monotonic() - timeout)


def crc_ibutton_update(crc, data):
    crc = (crc ^ data) & 0xFF

    for i in range(8):
        if crc & 0x01 == 0x01:
            crc = (crc >> 1) ^ 0x8C
        else:
            crc >>= 1

        crc &= 0xFF

    return crc


async def start(gf):
    start_data = [MPC_ADDR.CHEST, 56, 127, 138, 103, 83, 0, 15, 15, 68, 72, 0, 44, 1, 88, 113]
    gf.send_cmd(MPC_CMD.IR_RX, start_data)

    print('Start sequence sent')


async def stop(gf):
    stop_data = [MPC_ADDR.CHEST, 8, 127, 153, 250]
    gf.send_cmd(MPC_CMD.IR_RX, stop_data)

    print('Stop sequence sent')


async def ping(gf):
    # TODO: what horrible things have I done that I can't include master in
    # here?  (not that it matters anyway, since master relays the other
    # replies.)
    gf.send_cmd(MPC_CMD.DIAG_PING, [functools.reduce(operator.or_, set(MPC_ADDR) - set([MPC_ADDR.MASTER]))])

    def handle_reply(outer_reply):
        reply = mpc_pkt()

        try:
            reply.unpack(bytes(outer_reply.data))

        except ChecksumMismatchError:
            checksum = False
        else:
            checksum = True

        print('ping reply from %s (fwd: %s), checksum=%s' %
                (MPC_ADDR(reply.saddr), MPC_ADDR(outer_reply.saddr), 'good' if checksum else 'bad'))

    replies = 0
    try:
        while True:
            handle_reply(await gf.get_pkt())
            replies += 1

    except asyncio.TimeoutError:
        pass

    if replies == 0:
        print('no replies!')


async def discover(gf):
    gf.send_cmd('255.255.255.255', MPC_CMD.DIAG_PING, [MPC_ADDR.CHEST])
    await asyncio.sleep(5.0)

    for client in gf.active_clients(timeout=10.0):
        print(client.ip)


def main():
    loop = asyncio.get_event_loop()
    gf = GForceServer(9750, loop)

    if sys.argv[1] == 'discover':
        loop.run_until_complete(discover(gf))
    else:
        vest = gf.connect('192.168.2.25')

        if sys.argv[1] == 'ping':
            fn = ping

        elif sys.argv[1] == 'start':
            fn = start

        elif sys.argv[1] == 'stop':
            fn = stop

        loop.run_until_complete(fn(vest))


if __name__ == '__main__':
    main()
