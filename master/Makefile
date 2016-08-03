# MCU name
MCU = atxmega256a3u
F_CPU = 32000000

C_DEFS=W_SOUNDS=1 DEBUG=0

# Output format. (can be srec, ihex, binary)
FORMAT = ihex 

# dir to put output files.
OUTPUT = output

# Target file name (without extension).
TARGET = $(OUTPUT)/main

# Optimization level, can be [0, 1, 2, 3, s]. 0 turns off optimization.
# (Note: 3 is not always the best optimization level. See avr-libc FAQ.)
OPT = 2

EXTRAINCDIRS = ../include ../freertos/include

include ../makedefs
