cmake_minimum_required(VERSION 3.6)
project("GForce Master" C ASM)

set(AVR_MCU atxmega256a3u)
set(MCU_SPEED "32000000UL")

set(PROJECT_SOURCES

	main.c
	sounds.c
	xbee.c
	display.c
	game.c
	lights.c
	ds2483.c
	ibutton.c

	../common/spi.c
	../common/malloc.c
	../common/mempool.c
	../common/mpc.c
	../common/mpctwi.c
	../common/queue.c
	../common/serialcomm.c
	../common/uart.c
	../common/timer.c
	../common/twi_master.c
	../common/twi_slave.c
	../common/util.c
)


include(../cmakedefs)

add_definitions(-DW_SOUNDS=0)

add_avr_executable(main ${SOURCES})
