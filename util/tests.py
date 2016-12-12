import time
import asyncio
import sys
import collections
import functools
import operator
import socket
import binascii
import select

from tamp import StreamUnpacker
from gforce.mpc import MPC_CMD, MPC_ADDR, MPC_CRC_POLY, MPC_CRC_SHIFT, mpc_pkt,\
                       FrameFormatError, ChecksumMismatchError, framed_pkt,\
                       mpc_pkt


class _EventCollector:
    def __init__(self, gf, event=None):
        self.gf = gf
        self.event = event
        self._queue = asyncio.Queue(loop=self.gf.loop)

    def __enter__(self):
        self.gf.subscribe(self._handle_event, cmd=self.event)

        return self

    def __exit__(self, _, __, ___):
        self.gf.unsubscribe(self._handle_event, self.event)

    def __call__(self):
        return asyncio.ensure_future(self._queue.get(), loop=self.gf.loop)

    def _handle_event(self, pkt, _, *args):
        if len(args) == 0:
            self._queue.put_nowait(pkt)
        else:
            self._queue.put_nowait((pkt, *args))


class _EventBroker:
    def __init__(self):
        self._subs = {}

    def dispatch(self, pkt, *args):
        for cmd in (pkt.cmd, None):
            for callback in self._subs.get(cmd, []):
                callback(pkt, *args)

    def subscribe(self, callback, cmd=None):
        subs = self._subs.setdefault(cmd, set())
        subs.add(callback)

    def unsubscribe(self, callback, cmd=None):
        subs = self._subs.setdefault(cmd, set())
        subs.remove(callback)


class GForceClient:
    def __init__(self, server, ip):
        self.server = server
        self.ip = ip
        self.loop = server.loop
        self.last_rx = 0

        self._broker = _EventBroker()

    async def get_pkt(self, timeout=1.0):
        return (await asyncio.wait_for(self.queue.get(), timeout, loop=self.server.loop))

    def send_cmd(self, cmd, data):
        self.server.send_cmd(self.ip, cmd, data)

    def pkt_received(self, pkt):
        self.last_rx = time.monotonic()
        self._broker.dispatch(pkt, self)

    def collect(self, event=None):
        return _EventCollector(self, event=event)

    def subscribe(self, callback, cmd=None):
        self._broker.subscribe(callback, cmd)

    def unsubscribe(self, callback, cmd=None):
        self._broker.unsubscribe(callback, cmd)


class GForceServer:
    ClientConn = collections.namedtuple('ClientConn', ('stream', 'client'))

    def __init__(self, port, loop, bind_host='0.0.0.0'):
        self.bind_host = bind_host
        self.port = port
        self.loop = loop

        self._clients = {}
        self._broker = _EventBroker()

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

                    self._broker.dispatch(frame.pkt, self, addr[0])

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

    def collect(self, event=None):
        return _EventCollector(self, event=event)

    def subscribe(self, callback, cmd=None):
        self._broker.subscribe(callback, cmd)

    def unsubscribe(self, callback, cmd=None):
        self._broker.unsubscribe(callback, cmd)


async def start(gf):
    start_data = [MPC_ADDR.CHEST, 56, 127, 138, 103, 83, 0, 15, 15, 68, 72, 0, 44, 1, 88, 113]
    gf.send_cmd(MPC_CMD.IR_RX, start_data)

    print('Start sequence sent')


async def stop(gf):
    stop_data = [MPC_ADDR.CHEST, 8, 127, 153, 250]
    gf.send_cmd(MPC_CMD.IR_RX, stop_data)

    print('Stop sequence sent')


async def ping(gf):
    ping_addrs = functools.reduce(operator.or_, set(MPC_ADDR) - set([MPC_ADDR.MASTER]))
    replies = 0

    with gf.collect(MPC_CMD.DIAG_RELAY) as collect:
        gf.send_cmd(MPC_CMD.DIAG_PING, [ping_addrs])

        try:
            while ping_addrs != 0:
                outer = await asyncio.wait_for(collect(), 1.0)
                reply = unpack_inner(outer)

                print('ping reply from %s' % (reply.saddr))

                replies += 1
                ping_addrs &= ~reply.saddr

        except asyncio.TimeoutError:
            pass

    if replies == 0:
        print('no replies received')

    else:
        print('%d replies' % replies)


def unpack_inner(outer):
    pkt = mpc_pkt()

    try:
        pkt.unpack(bytes(outer.data))

    except ChecksumMismatchError:
        pass

    return pkt


async def shot(gf):
    try:
        with gf.collect(MPC_CMD.IR_RX) as collect:
            gf.send_cmd(MPC_CMD.IR_RX, [MPC_ADDR.LS, 0x0c, 0x63, 0x88, 0xA6])

            shot = await asyncio.wait_for(collect(), timeout=1.0)

            print('SHOT', shot.saddr)

    except asyncio.TimeoutError:
        print('No shot received')


async def discover(gf):
    try:
        with gf.collect(MPC_CMD.DIAG_RELAY) as collect:
            gf.send_cmd('255.255.255.255', MPC_CMD.DIAG_PING, [MPC_ADDR.CHEST])

            while True:
                pkt, addr = await asyncio.wait_for(collect(), 5.0)
                print(addr)

    except asyncio.TimeoutError:
        pass



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

        elif sys.argv[1] == 'shot':
            fn = shot

        loop.run_until_complete(fn(vest))


if __name__ == '__main__':
    main()
