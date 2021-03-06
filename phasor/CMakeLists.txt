cmake_minimum_required(VERSION 3.6)
project("GForce Phasor" C ASM)

set(AVR_MCU atxmega32a4u)
set(MCU_SPEED "32000000UL")
set(FREERTOS_PORT "xmega")

set(PROJECT_SOURCES
	main.c
	irtx.c
	trigger.c

	../common/buzz.c
	../common/irrx.c
	../common/leds.c
	../common/diag.c
	../common/mempool.c
	../common/malloc.c
	../common/mpctwi.c
	../common/mpc.c
	../common/settings.c
	../common/serialcomm.c
	../common/mpcphasor.c

	../drivers/xmega/led_spi.c
	../drivers/xmega/twi/master.c
	../drivers/xmega/twi/slave.c
	../drivers/xmega/twi/twi.c
	../drivers/xmega/clock.c
	../drivers/xmega/spi.c
	../drivers/xmega/uart.c
	../drivers/xmega/dma.c

	../drivers/tlc59711.c
)

include(../cmakedefs)

add_avr_executable(main ${SOURCES})
