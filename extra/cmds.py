import enum
import sys
import inspect


MPC_CRC_POLY = 0x32
MPC_CRC_SHIFT = 0x67


@enum.unique
class _c_enum(enum.IntEnum):

    @property
    def c_name(self):
        return self.__class__.__name__ + '_' + self.name


class MPC_CMD(_c_enum):
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


class MPC_ADDR(_c_enum):
    CHEST = 0x01
    LS = 0x02
    BACK = 0x04
    RS = 0x08
    PHASOR = 0x10
    MASTER = 0x40


def _generate_enum(enum_type):
    cls_name = enum_type.__name__

    buf = 'enum %s {\n' % cls_name

    for member in enum_type:
        buf += '\t %s = 0x%02X,\n' % (member.c_name, member.value)

    buf += '}; \n'
    buf += '#define %s_MAX %d\n' % (cls_name, max(enum_type))

    return buf


def _generate_defines(out_file):
    define_symbols = [
        'MPC_CMD',
        'MPC_CRC_POLY',
        'MPC_CRC_SHIFT',
        'MPC_ADDR',
    ]

    with open(out_file, 'w') as fh:
        fh.write('#ifndef _MPC_CMD_H\n')
        fh.write('#define _MPC_CMD_H\n')

        for symbol in define_symbols:
            value = getattr(sys.modules[__name__], symbol)

            if isinstance(value, int):
                fh.write('#define %s 0x%02X\n' % (symbol, value))

            elif inspect.isclass(value) and issubclass(value, _c_enum):
                fh.write(_generate_enum(value))

            else:
                raise ValueError

        fh.write('#endif\n')


if __name__ == '__main__':
    _generate_defines(sys.argv[1])
