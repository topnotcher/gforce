cmake_minimum_required(VERSION 3.6)
project("GForce Shoulder" C ASM)

if(PLATFORM STREQUAL "xmega")
	set(AVR_MCU atxmega128a4u)
	set(MCU_SPEED "32000000UL")

	set(PLATFORM_SOURCES
		xmega.c
		../drivers/xmega/clock.c
		../drivers/xmega/led_usart_spi.c
		../drivers/xmega/twi/master.c
		../drivers/xmega/twi/slave.c
	)
else()
	message(FATAL_ERROR "Unknown platform ${PLATFORM}.")
endif()


set(PROJECT_SOURCES
	main.c
	default.c

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

	${PLATFORM_SOURCES}
)

include_directories(../include)

include(../cmakedefs)

add_avr_executable(main ${SOURCES})
