#!/usr/bin/python

import socket

UDP_IP = '192.168.1.25'
UDP_PORT = 9750


def crc(crc, data):

    crc = crc ^ data;

    for i in range(0,8):
        if (crc & 0x01):
	        crc = (crc >> 1) ^ 0x8C;
        else:
	        crc >>= 1;

    return crc

# len, cmd, src, chksum, data...

data = [255, 1,ord('I'), 0, 'chksum', 0x38]

chk = crc(0x67, data[1]);
chk = crc(chk, data[2])
chk = crc(chk, data[3])
chk = crc(chk, data[5])

data[4] = chk;

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print(data);
#for byte in data:
sock.sendto(bytes(data), (UDP_IP,UDP_PORT))
