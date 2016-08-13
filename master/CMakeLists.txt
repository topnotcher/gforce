cmake_minimum_required(VERSION 3.6)
project("GForce Master" C ASM)

set(AVR_MCU atxmega256a3u)
set(MCU_SPEED "32000000UL")

include(../cmakedefs)

add_definitions(-DW_SOUNDS=0)

add_avr_executable(main ${SOURCES})
