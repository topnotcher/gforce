cmake_minimum_required(VERSION 3.6)
project("GForce Phasor" C ASM)

set(AVR_MCU atxmega32a4u)
set(MCU_SPEED "32000000UL")

set(PROJECT_SOURCES
	main.c
	irtx.c
	trigger.c

	../common/buzz.c
	../common/irrx.c
	../common/leds.c
	../common/led_spi.c
	../common/tlc59711.c
	../common/malloc.c
	../common/mempool.c
	../common/serialcomm.c
	../common/uart.c
	../common/mpc.c
	../common/tasks.c
	../common/timer.c
	../common/twi_master.c
	../common/twi_slave.c
	../common/util.c
	../common/diag.c
)

include(../cmakedefs)

add_avr_executable(main ${SOURCES})
