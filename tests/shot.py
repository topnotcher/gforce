import asyncio

from gforce.mpc import MPC_ADDR, MPC_CMD, IR_CRC_SHIFT, ir_crc
from gforce.testing import TestCase


class TestShot(TestCase):
    async def setup(self):
        self.start_timeout = 2
        self.inter_shot = 6
        self.lock = asyncio.Lock()

        await self.start() 

    async def start(self):
        # TODO
        start_data = [MPC_ADDR.CHEST, 56, 127, 138, 103]
        crc = start_data[-1]
        for byte in [83, 0, 15, 15, 33, 0, 0, 44, 1, 88]:
            crc = ir_crc(crc, byte)
            start_data.append(byte)

        start_data.append(crc)
        self.dut.send_cmd(MPC_CMD.IR_RX, start_data)

        await asyncio.sleep(self.start_timeout)

    async def _test_shot(self, sensor):
        with (await self.lock):
            try:
                with self.dut.collect(MPC_CMD.IR_RX) as collect:
                    self.dut.send_cmd(MPC_CMD.IR_RX, [sensor, 0x0c, 0x63, 0x88, 0xA6])

                    shot = await asyncio.wait_for(collect(), timeout=1.0)

                    if shot.saddr != sensor:
                        raise AssertionError('Shot %s but hit on %s' % (sensor, shot.saddr))

            except asyncio.TimeoutError:
                raise AssertionError('Timeout waiting for shot on %s' % sensor)

            await asyncio.sleep(self.inter_shot)

    async def test_shot_ls(self):
        await self._test_shot(MPC_ADDR.LS)

    async def test_shot_rs(self):
        await self._test_shot(MPC_ADDR.RS)

    async def test_shot_chest(self):
        await self._test_shot(MPC_ADDR.CHEST)

    async def test_shot_back(self):
        await self._test_shot(MPC_ADDR.BACK)

    async def test_shot_phasor(self):
        await self._test_shot(MPC_ADDR.PHASOR)

    async def teardown(self):
        stop_data = [MPC_ADDR.CHEST, 8, 127, 153, 250]
        self.dut.send_cmd(MPC_CMD.IR_RX, stop_data)
