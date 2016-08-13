cmake_minimum_required(VERSION 3.6)
project("GForce Phasor" C ASM)

set(AVR_MCU atxmega32a4u)
set(MCU_SPEED "32000000UL")

include(../cmakedefs)

add_avr_executable(main ${SOURCES})
