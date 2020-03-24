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
	xbee.c
	game.c
	lights.c
	config.c
	debug.c
	mpcipc.c

	../drivers/tlc59711.c

	../drivers/sam/twi/slave.c
	../drivers/sam/twi/master.c
	../drivers/sam/sercom.c
	../drivers/sam/led_spi.c
	../drivers/sam/dma.c
	../drivers/sam/isr.c
	../drivers/sam/port.c

	../common/malloc.c
	../common/leds.c
	../common/mempool.c
	../common/mpc.c
	../common/mpctwi.c
	# ../common/mpcphasor.c
	../common/serialcomm.c
	../common/irrx.c
	../common/settings.c

	sdhc/hal_mci_sync.c
	sdhc/hpl_sdhc.c
	sdhc/init.c
	sdhc/sd_mmc.c
	sdhc/sdmmc_diskio.c

	../fatfs/ff.c
	../fatfs/ffsystem.c
	../fatfs/ffunicode.c

	mp3/bitstream.c
	mp3/buffers.c
	mp3/dct32.c
	mp3/dequant.c
	mp3/dqchan.c
	mp3/huffman.c
	mp3/hufftabs.c
	mp3/imdct.c
	mp3/mp3dec.c
	mp3/mp3tabs.c
	mp3/polyphase.c
	mp3/scalfact.c
	mp3/stproc.c
	mp3/subband.c
	mp3/trigtabs.c
)

include(../cmakedefs)

option(ENABLE_SOUND "Build with sound effects." OFF)
if (ENABLE_SOUND)
	add_definitions("-DW_SOUNDS=1")
else()
	add_definitions("-DW_SOUNDS=0")
endif()

add_arm_executable(main ${SOURCES})
