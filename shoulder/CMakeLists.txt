cmake_minimum_required(VERSION 3.6)
project("GForce Shoulder" C ASM)

if(PLATFORM STREQUAL "xmega")
	if(SHOULDER_V1_HW)
		set(AVR_MCU atxmega32a4u)
		add_definitions(-DSHOULDER_V1_HW)
	else()
		set(AVR_MCU atxmega128a4u)
	endif()

	set(MCU_SPEED "16000000UL")

	set(PLATFORM_SOURCES
		xmega.c
		../drivers/xmega/clock.c
		../drivers/xmega/led_spi.c
		../drivers/xmega/led_usart_spi.c
		../drivers/xmega/spi.c
		../drivers/xmega/twi/twi.c
		../drivers/xmega/twi/master.c
		../drivers/xmega/twi/slave.c
		../drivers/xmega/uart.c
		../drivers/xmega/dma.c

		../common/buzz.c
		../common/irrx_xmega.c
	)
elseif(PLATFORM STREQUAL "saml21")
	include(${CMAKE_SOURCE_DIR}/saml21.cmake)
	include(${CMAKE_SOURCE_DIR}/../build/sam.cmake)

	message("-- Configuring for ${SAM_PART}")

	set(PLATFORM_SOURCES
		saml21.c
		../drivers/sam/twi/slave.c
		../drivers/sam/twi/master.c
		../drivers/sam/sercom.c
		../drivers/sam/led_spi.c
		../drivers/sam/dma.c
		../drivers/sam/isr.c
	)

elseif(PLATFORM STREQUAL "samd51")
	include(${CMAKE_SOURCE_DIR}/samd51.cmake)
	include(${CMAKE_SOURCE_DIR}/../build/sam.cmake)

	message("-- Configuring for ${SAM_PART}")

	set(PLATFORM_SOURCES
		samd51.c
		../drivers/sam/twi/slave.c
		../drivers/sam/twi/master.c
		../drivers/sam/sercom.c
		../drivers/sam/led_spi.c
		../drivers/sam/dma.c
		../drivers/sam/isr.c
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
	../common/irrx.c
	../common/settings.c

	${PLATFORM_SOURCES}
)

include(../cmakedefs)

# TODO
if(PLATFORM STREQUAL "xmega")
	add_avr_executable(main ${SOURCES})
elseif(PLATFORM STREQUAL "saml21")
	add_arm_executable(main ${SOURCES})
elseif(PLATFORM STREQUAL "samd51")
	add_arm_executable(main ${SOURCES})
else()
	message(FATAL_ERROR "Unknown platform ${PLATFORM}.")
endif()
