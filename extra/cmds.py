import enum

@enum.unique
class GFCommand(enum.IntEnum):
    LED_SET_SEQ = 0x41
    LED_OFF = 0x42
    LED_SET_BRIGHTNESS = 0x62

    DIAG_PING = 0x50
    DIAG_MEM_USAGE = 0x4D
    DIAG_DEBUG = 0x44
    DIAG_RELAY = 0x52

    IR_TX = 0x54
    IR_RX = 0x49
    SHOT = 0x53

    BOARD_HELLO = 0x48
    BOARD_CONFIG = 0x63

    @property
    def c_name(self):
        return 'MPC_CMD_' + self.name


def _generate_defines(out_file):
    with open(out_file, 'w') as fh:
        fh.write('#ifndef _MPC_CMD_H\n')
        fh.write('#define _MPC_CMD_H\n')

        for cmd in GFCommand:
            fh.write('#define %s 0x%02X\n' % (cmd.c_name, cmd.value))

        fh.write('#define MPC_CMD_MAX %d\n' % max(GFCommand))

        fh.write('#endif\n')


if __name__ == '__main__':
    import sys
    _generate_defines(sys.argv[1])
