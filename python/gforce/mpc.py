import enum


MPC_CRC_POLY = 0x32
MPC_CRC_SHIFT = 0x67


@enum.unique
class Enum(enum.IntEnum):

    @property
    def c_name(self):
        return self.__class__.__name__ + '_' + self.name


class MPC_CMD(Enum):
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


class MPC_ADDR(Enum):
    CHEST = 0x01
    LS = 0x02
    BACK = 0x04
    RS = 0x08
    PHASOR = 0x10
    MASTER = 0x40
