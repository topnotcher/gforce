from gforce.mpc import *


import sys
import inspect


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
        'IR_CRC_POLY',
        'IR_CRC_SHIFT',
        'IR_CMD',
    ]

    with open(out_file, 'w') as fh:
        fh.write('#ifndef _MPC_CMD_H\n')
        fh.write('#define _MPC_CMD_H\n')

        for symbol in define_symbols:
            value = getattr(sys.modules[__name__], symbol)

            if isinstance(value, int):
                fh.write('#define %s 0x%02X\n' % (symbol, value))

            elif inspect.isclass(value) and issubclass(value, Enum):
                fh.write(_generate_enum(value))

            else:
                raise ValueError

        fh.write('#endif\n')


if __name__ == '__main__':
    _generate_defines(sys.argv[1])
