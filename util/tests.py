import sys
import asyncio
import functools
import operator
import time

from tamp import Structure, uint16_t, uint8_t
from gforce.mpc import MPC_CMD, MPC_ADDR, mpc_pkt
from gforce.comm import GForceServer


class MemUsage(Structure):
    _fields_ = [
        ('data', uint16_t.le),
        ('heap', uint16_t.le),
        ('total', uint16_t.le),
    ]


class MemberLogin(Structure):
    _fields_ = [
        ('uuid', uint8_t[8]),
    ]


async def start(gf):
    start_data = [MPC_ADDR.CHEST, 56, 127, 138, 103, 83, 0, 15, 15, 68, 72, 0, 44, 1, 88, 113]
    gf.send_cmd(MPC_CMD.IR_RX, start_data)

    print('Start sequence sent')


async def stop(gf):
    stop_data = [MPC_ADDR.CHEST, 8, 127, 153, 250]
    gf.send_cmd(MPC_CMD.IR_RX, stop_data)

    print('Stop sequence sent')


async def ping(gf):
    ping_addrs = functools.reduce(operator.or_, MPC_ADDR)
    replies = 0

    with gf.collect(MPC_CMD.DIAG_RELAY) as collect:
        gf.send_cmd(MPC_CMD.DIAG_PING, [ping_addrs])

        try:
            while ping_addrs != 0:
                reply = await asyncio.wait_for(collect(), 1.0)

                print('ping reply from %s' % (reply.saddr))

                replies += 1
                ping_addrs &= ~reply.saddr

        except asyncio.TimeoutError:
            pass

    if replies == 0:
        print('no replies received')

    else:
        print('%d replies' % replies)


async def mem_usage(gf):
    addrs = functools.reduce(operator.or_, MPC_ADDR)

    with gf.collect(MPC_CMD.DIAG_MEM_USAGE) as collect:
        gf.send_cmd(MPC_CMD.DIAG_MEM_USAGE, [addrs])

        try:
            while addrs != 0:
                reply = await asyncio.wait_for(collect(), 1.0)

                usage = MemUsage()
                usage.unpack(bytes(reply.data))

                mem_used = usage.data + usage.heap
                mem_percent = float(mem_used) / usage.total * 100.0

                print('MEM USAGE: %s data: %d; heap %d; %d/%d %0.2f%%' %
                      (reply.saddr, usage.data, usage.heap, mem_used, usage.total, mem_percent))

                addrs &= ~reply.saddr

        except asyncio.TimeoutError:
            pass


def unpack_inner(outer):
    pkt = mpc_pkt()

    try:
        pkt.unpack(bytes(outer.data))

    except ChecksumMismatchError:
        pass

    return pkt


async def test_shot(gf):

    start_timeout = 15.0
    inter_shot = 10.0

    await start(gf)

    # TODO
    await asyncio.sleep(start_timeout)

    sensors = [MPC_ADDR.LS, MPC_ADDR.RS, MPC_ADDR.CHEST, MPC_ADDR.BACK, MPC_ADDR.PHASOR]

    for sensor in sensors:
        try:
            with gf.collect(MPC_CMD.IR_RX) as collect:
                gf.send_cmd(MPC_CMD.IR_RX, [sensor, 0x0c, 0x63, 0x88, 0xA6])

                shot = await asyncio.wait_for(collect(), timeout=1.0)
                print('SHOT', shot.saddr)

                if shot.saddr != sensor:
                    print('Shot %s but hit on %s' % (sennsor, shot.saddr))

        except asyncio.TimeoutError:
            print('Timeout waiting for shot on', sensor)

        await asyncio.sleep(inter_shot)


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
            gf.send_cmd('255.255.255.255', MPC_CMD.DIAG_PING, [MPC_ADDR.MASTER])

            while True:
                pkt, addr = await asyncio.wait_for(collect(), 5.0)
                print(addr)

    except asyncio.TimeoutError:
        pass


async def listen(gf):
    while True:
        with gf.collect() as collect:
            event = await collect()

            print('RX', event.cmd, event.saddr, event.data)


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

        elif sys.argv[1] == 'test-shot':
            fn = test_shot

        elif sys.argv[1] == 'mem':
            fn = mem_usage

        elif sys.argv[1] == 'listen':
            fn = listen

        loop.run_until_complete(fn(vest))


if __name__ == '__main__':
    main()
