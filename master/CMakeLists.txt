cmake_minimum_required(VERSION 3.6)
project("GForce Master" C ASM)

set(SAM_PART "samd51n20a")
set(SAM_CPU "cortex-m4")

add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mfloat-abi=hard -mfpu=fpv4-sp-d16")

set(AVR_MCU atxmega256a3u)
set(FREERTOS_PORT ${PLATFORM})

include(${CMAKE_SOURCE_DIR}/../build/sam.cmake)

set(PROJECT_SOURCES

	main.c
	# sounds.c
	# xbee.c
	game.c
	lights.c

	../drivers/sam/twi/slave.c
	../drivers/sam/twi/master.c
	../drivers/sam/sercom.c
	../drivers/sam/led_spi.c
	../drivers/sam/dma.c
	../drivers/sam/isr.c

	../common/malloc.c
	../common/mempool.c
	../common/mpc.c
	../common/mpctwi.c
	# ../common/mpcphasor.c
	#../common/serialcomm.c
	../common/irrx.c
)

include(../cmakedefs)

option(ENABLE_SOUND "Build with sound effects." OFF)
if (ENABLE_SOUND)
	add_definitions("-DW_SOUNDS=1")
else()
	add_definitions("-DW_SOUNDS=0")
endif()

add_arm_executable(main ${SOURCES})
