cmake_minimum_required(VERSION 3.6)
project("GForce Chest" C ASM)

set(AVR_MCU atxmega32a4u)
set(MCU_SPEED "32000000UL")
set(FREERTOS_PORT "xmega")

set(PROJECT_SOURCES
	main.c

	../common/buzz.c
	../common/irrx.c
	../common/leds.c
	../common/malloc.c
	../common/mempool.c
	../common/mpctwi.c
	../common/mpc.c
	../common/diag.c

	../drivers/xmega/led_spi.c
	../drivers/xmega/twi/master.c
	../drivers/xmega/twi/slave.c
	../drivers/xmega/twi/twi.c
	../drivers/xmega/clock.c
	../drivers/xmega/spi.c
	../drivers/xmega/uart.c

	../drivers/tlc59711.c
)

include(../cmakedefs)

add_avr_executable(main ${SOURCES})
