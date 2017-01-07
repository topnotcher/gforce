cmake_minimum_required(VERSION 3.6)
project("GForce Shoulder" C ASM)

set(AVR_MCU atxmega128a4u)
set(MCU_SPEED "32000000UL")

set(PROJECT_SOURCES
	main.c
	test.c
	default.c

	../bsp/xmega/clock.c
	../common/buzz.c
	../common/irrx.c
	../common/leds.c
	../common/uart.c
	../bsp/xmega/led_usart_spi.c
	../common/tlc59711.c
	../common/malloc.c
	../common/mempool.c
	../common/mpctwi.c
	../common/mpc.c
	../common/twi_master.c
	../common/twi_slave.c
	../common/diag.c
)

include_directories(../bsp)

include(../cmakedefs)

add_avr_executable(main ${SOURCES})
