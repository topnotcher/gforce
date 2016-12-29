cmake_minimum_required(VERSION 3.6)
project("GForce OLED" C ASM)

set(AVR_MCU atxmega32a4u)
set(MCU_SPEED "32000000UL")

set(PROJECT_SOURCES
	display.c
	main.c
	mastercomm.c

	../common/comm.c
	../common/malloc.c
	../common/mempool.c
	../common/queue.c
	../common/serialcomm.c
	../common/timer.c
	../common/util.c
)

include(../cmakedefs)

add_avr_executable(main ${SOURCES})
