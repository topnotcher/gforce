import time
import sys
import functools
import operator
import socket
import binascii
import select

from tamp import *

from cmds import MPC_CMD, MPC_ADDR, MPC_CRC_POLY, MPC_CRC_SHIFT

GF_ADDR = ('192.168.2.25', 9750)


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

class GForce:
    def __init__(self, host, port):
        self.host = host
        self.port = port

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
        self.sock.bind(('0.0.0.0', port))


    def _send(self, data):
        if isinstance(data, Structure):
            data = bytes(data)

        # print('send', binascii.hexlify(data))
        self.sock.sendto(data, (self.host, self.port))

    def send_cmd(self, cmd, data):
        frame = framed_pkt()
        pkt = frame.pkt

        pkt.cmd = cmd
        pkt.saddr = 4 # TODO?
        pkt.data = data

        frame.daddr = MPC_ADDR.MASTER

        self._send(frame)

    def collect_timeout(self, timeout=1.0):
        pkts = []

        last_act = time.monotonic()

        while time.monotonic() - last_act < timeout:
            r, _, __ = select.select([self.sock], [], [], timeout - (time.monotonic() - last_act))

            if self.sock in r:

                data, addr = self.sock.recvfrom(1024)
                if addr != (self.host, self.port):
                    continue

                while len(data):
                    f = framed_pkt()

                    try:
                        consumed = f.unpack(data)

                        # TODO: this should probably treat the datagram like a
                        # byte stream. The xbee may not be so friendly in how
                        # it packetizes data...

                        # print('received %d, consumed %d' % (len(data), consumed))
                        data = data[consumed:]

                    except FrameFormatError as e:
                        print('Error receiving frame:', str(e))
                        print(data)

                    except ChecksumMismatchError as e:
                        print('Checksum mismatch:', str(e))
                        print(data)

                    else:
                        pkts.append(f.pkt)

        return pkts


def crc_ibutton_update(crc, data):
    crc = (crc ^ data) & 0xFF

    for i in range(8):
        if crc & 0x01 == 0x01:
            crc = (crc >> 1) ^ 0x8C
        else:
            crc >>= 1

        crc &= 0xFF

    return crc


def start(gf):
    start_data = [MPC_ADDR.CHEST, 56, 127, 138, 103, 83, 0, 15, 15, 68, 72, 0, 44, 1, 88, 113]
    gf.send_cmd(MPC_CMD.IR_RX, start_data)

    print('Start sequence sent')


def stop(gf):
    stop_data = [MPC_ADDR.CHEST, 8, 127, 153, 250]
    gf.send_cmd(MPC_CMD.IR_RX, stop_data)

    print('Stop sequence sent')


def ping(gf):

    # TODO: what horrible things have I done that I can't include master in
    # here?  (not that it matters anyway, since master relays the other
    # replies.)
    gf.send_cmd(MPC_CMD.DIAG_PING, [functools.reduce(operator.or_, set(MPC_ADDR) - set([MPC_ADDR.MASTER]))])
    replies = gf.collect_timeout()

    if replies:
        for outer_reply in replies:
            reply = mpc_pkt()

            try:
                reply.unpack(bytes(outer_reply.data))

            except ChecksumMismatchError:
                checksum = False
            else:
                checksum = True

            print('ping reply from %s (fwd: %s), checksum=%s' %
                  (MPC_ADDR(reply.saddr), MPC_ADDR(outer_reply.saddr), 'good' if checksum else 'bad'))
    else:
        print('no replies!')


if __name__ == '__main__':
    gf = GForce(*GF_ADDR)

    if sys.argv[1] == 'ping':
        ping(gf)

    elif sys.argv[1] == 'start':
        start(gf)

    elif sys.argv[1] == 'stop':
        stop(gf)
