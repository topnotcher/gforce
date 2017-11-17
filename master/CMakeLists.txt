cmake_minimum_required(VERSION 3.6)
project("GForce Master" C ASM)

set(AVR_MCU atxmega256a3u)
set(MCU_SPEED "32000000UL")
set(FREERTOS_PORT "xmega")

set(PROJECT_SOURCES

	main.c
	sounds.c
	xbee.c
	display.c
	game.c
	lights.c
	ds2483.c
	ibutton.c

	../drivers/xmega/clock.c
	../drivers/xmega/spi.c
	../drivers/xmega/twi/master.c
	../drivers/xmega/twi/slave.c
	../drivers/xmega/twi/twi.c
	../drivers/xmega/uart.c
	../drivers/xmega/dma.c

	../common/malloc.c
	../common/mempool.c
	../common/mpc.c
	../common/mpctwi.c
	../common/mpcphasor.c
	../common/serialcomm.c
	../common/irrx.c
)

include_directories(../bsp)

include(../cmakedefs)

add_definitions(-DW_SOUNDS=0)

add_avr_executable(main ${SOURCES})
