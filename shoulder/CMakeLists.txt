cmake_minimum_required(VERSION 3.6)
project("GForce Shoulder" C ASM)

set(AVR_MCU atxmega128a4u)
set(MCU_SPEED "32000000UL")

set(PROJECT_SOURCES
	main.c
	test.c
	default.c

	../drivers/xmega/clock.c
	../drivers/xmega/led_usart_spi.c
	../drivers/xmega/twi/master.c
	../drivers/xmega/twi/slave.c

	../drivers/tlc59711.c

	../common/buzz.c
	../common/irrx.c
	../common/leds.c
	../common/uart.c
	../common/malloc.c
	../common/mempool.c
	../common/mpctwi.c
	../common/mpc.c
	../common/diag.c
)

include_directories(../include)

include(../cmakedefs)

add_avr_executable(main ${SOURCES})
