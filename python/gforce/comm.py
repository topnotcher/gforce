import collections
import time
import asyncio
import socket

from tamp import StreamUnpacker
from .mpc import FrameFormatError, ChecksumMismatchError, framed_pkt, MPC_ADDR

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

    def collect(self, event=None):
        return _EventCollector(self, event=event)

    def subscribe(self, callback, cmd=None):
        self._broker.subscribe(callback, cmd)

    def unsubscribe(self, callback, cmd=None):
        self._broker.unsubscribe(callback, cmd)
