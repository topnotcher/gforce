import enum

from tamp import Structure, uint8_t, LengthField, Computed, Const, EnumWrap,\
                 PackedLength, Byte


__all__ = ['MPC_CRC_POLY', 'MPC_CRC_SHIFT', 'MPC_CMD', 'MPC_ADDR', 'Enum',
           'crc_ibutton_update', 'framed_pkt', 'mpc_pkt', 'ChecksumMismatchError',
           'FrameSyncError', 'FrameStartError', 'FrameFormatError',
           'IR_CRC_POLY', 'IR_CRC_SHIFT']


MPC_CRC_POLY = 0x0C
MPC_CRC_SHIFT = 0x67

IR_CRC_SHIFT = 0x55
IR_CRC_POLY = 0x24


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

    MEMBER_LOGIN = 0x6D

    IR_TX = 0x54
    IR_RX = 0x49

    HELLO = 0x48
    CONFIG = 0x63


class MPC_ADDR(Enum):
    CHEST = 0x01
    LS = 0x02
    BACK = 0x04
    RS = 0x08
    PHASOR = 0x10
    MASTER = 0x40


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
        ('saddr', EnumWrap(MPC_ADDR, uint8_t)),
        ('chksum', Computed(uint8_t, '_compute_chksum', mismatch_exc=ChecksumMismatchError)),
        ('data', Byte[LengthField('len')]),
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


def crc_ibutton_update(crc, data):
    crc = (crc ^ data) & 0xFF

    for i in range(8):
        if crc & 0x01 == 0x01:
            crc = ((crc >> 1) ^ 0x8c)
        else:
            crc >>= 1

        crc &= 0xFF

    return crc


def ir_crc(crc, data, poly=IR_CRC_POLY):
    for i in range(8):
        fb = (crc ^ data) & 0x01

        if fb == 1:
            crc = 0x80 | ((crc ^ poly) >> 1)
        else:
            crc >>= 1
        data >>= 1

    return crc
