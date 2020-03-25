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
    def __init__(self, server):
        self.server = server
        self.loop = server.loop
        self.last_rx = 0
        self.transport = None
        self.stream = StreamUnpacker(framed_pkt)
        self._broker = _EventBroker()

    def send_cmd(self, cmd, data):
        frame = framed_pkt()
        pkt = frame.pkt

        pkt.cmd = cmd
        pkt.saddr = 4 # TODO? (yah, wtf is this crap?)
        pkt.data = data

        frame.daddr = MPC_ADDR.BACK
        self.transport.write(bytes(frame))

    def pkt_received(self, pkt):
        self.last_rx = time.monotonic()
        self._broker.dispatch(pkt, self)
        self.server.pkt_received(pkt, self.ip)

    def collect(self, event=None):
        return _EventCollector(self, event=event)

    def subscribe(self, callback, cmd=None):
        self._broker.subscribe(callback, cmd)

    def unsubscribe(self, callback, cmd=None):
        self._broker.unsubscribe(callback, cmd)

    @property
    def ip(self):
        return self.transport.get_extra_info('peername')[0]

    @property
    def port(self):
        return self.transport.get_extra_info('peername')[1]

    def connection_made(self, transport):
        self.transport = transport
        self.server.connection_made(self)

    def connection_lost(self, exc):
        print('Disconnected', self.ip)

    def data_received(self, data):
        more = True
        while more:
            try:
                for frame in self.stream.unpack(data):
                    self.pkt_received(frame.pkt)

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


class GForceServer:
    # ClientConn = collections.namedtuple('ClientConn', ('stream', 'client'))

    def __init__(self, port, loop, bind_host='0.0.0.0'):
        self.bind_host = bind_host
        self.port = port
        self.loop = loop

        self._clients = {}
        self._broker = _EventBroker()

        listen = self.loop.create_server(lambda: GForceClient(self), host=self.bind_host, port=self.port)
        self.task = self.loop.create_task(listen)

    def connection_made(self, conn):
        print('Connection from', conn.ip)
        self._clients[conn.ip] = conn

    def pkt_received(self, pkt, addr):
        self._broker.dispatch(pkt, self, addr)

    def send_cmd(self, ip, cmd, data):
        client = self._clients.get(ip)
        if not client:
            raise RuntimeError('ARRRRRGGGGGG!!!!')

        client.send_cmd(cmd, data)

    def collect(self, event=None):
        return _EventCollector(self, event=event)

    def subscribe(self, callback, cmd=None):
        self._broker.subscribe(callback, cmd)

    def unsubscribe(self, callback, cmd=None):
        self._broker.unsubscribe(callback, cmd)
