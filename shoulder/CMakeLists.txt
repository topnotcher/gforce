cmake_minimum_required(VERSION 3.6)
project("GForce Shoulder" C ASM)

if(PLATFORM STREQUAL "xmega")
	if(SHOULDER_V1_HW)
		set(AVR_MCU atxmega32a4u)
		add_definitions(-DSHOULDER_V1_HW)
	else()
		set(AVR_MCU atxmega128a4u)
	endif()

	set(MCU_SPEED "32000000UL")

	set(PLATFORM_SOURCES
		xmega.c
		../drivers/xmega/clock.c
		../drivers/xmega/led_spi.c
		../drivers/xmega/led_usart_spi.c
		../drivers/xmega/spi.c
		../drivers/xmega/twi/twi.c
		../drivers/xmega/twi/master.c
		../drivers/xmega/twi/slave.c

		../common/buzz.c
		../common/irrx.c
		../common/uart.c
	)
elseif(PLATFORM STREQUAL "saml21")
	set(SAM_PART "saml21e17b")
	message("Building for ${SAM_PART}")

	set(PLATFORM_SOURCES
		saml21.c
		../drivers/saml21/twi/slave.c
		../drivers/saml21/twi/master.c
		../drivers/saml21/sercom.c
		../drivers/saml21/led_spi.c
		../drivers/saml21/dma.c
	)

else()
	message(FATAL_ERROR "Unknown platform ${PLATFORM}.")
endif()

set(FREERTOS_PORT ${PLATFORM})

set(PROJECT_SOURCES
	main.c
	default.c

	../drivers/tlc59711.c

	../common/leds.c
	../common/mpc.c
	../common/malloc.c
	../common/mempool.c
	../common/mpctwi.c
	../common/diag.c

	${PLATFORM_SOURCES}
)

include(../cmakedefs)

# TODO
if(PLATFORM STREQUAL "xmega")
	add_avr_executable(main ${SOURCES})
elseif(PLATFORM STREQUAL "saml21")
	add_arm_executable(main ${SOURCES})
else()
	message(FATAL_ERROR "Unknown platform ${PLATFORM}.")
endif()
