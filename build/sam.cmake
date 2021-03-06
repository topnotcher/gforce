# CMake Toolchain file for the gcc-arm-embedded toolchain.
# https://launchpad.net/gcc-arm-embedded
#
# Copyright (c) 2013 Swift Navigation Inc.
# Contact: Fergus Noble <fergus@swift-nav.com>
#
# This source is subject to the license found in the file 'LICENSE' which must
# be be distributed together with this source. All other rights reserved.
#
# THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
# EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.

# Targeting an embedded system, no OS.

if(NOT ((CMAKE_BUILD_TYPE MATCHES Release) OR
	(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo) OR
	(CMAKE_BUILD_TYPE MATCHES Debug) OR
	(CMAKE_BUILD_TYPE MATCHES MinSizeRel)))
	set(
		CMAKE_BUILD_TYPE Release
		CACHE STRING "Choose cmake build type: Debug Release RelWithDebInfo MinSizeRel"
		FORCE
		)
endif()

add_compile_options(
	-fno-common -ffunction-sections -fdata-sections
    -mcpu=${SAM_CPU} -mthumb -specs=nosys.specs
)

function(add_arm_executable EXECUTABLE_NAME)
	if(NOT SAM_PART)
		message(FATAL_ERROR "SAM_PART not defined!")
	endif()

	if(NOT ARGN)
		message(FATAL_ERROR "No source files given for ${EXECUTABLE_NAME}.")
	endif(NOT ARGN)

	set(elf_file ${EXECUTABLE_NAME}-${SAM_PART}.elf)
	set(bin_file ${EXECUTABLE_NAME}-${SAM_PART}.bin)
	set(map_file ${EXECUTABLE_NAME}-${SAM_PART}.map)

	get_filename_component(TOOLCHAIN_DIRECTORY "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)

	set(STARTUP_CODE "${TOOLCHAIN_DIRECTORY}/startup_${PLATFORM}.c")
	list(APPEND ARGN "${STARTUP_CODE}")

	add_executable(${elf_file} ${ARGN})
	string(TOUPPER "__${SAM_PART}__" SAM_PART_DEFINE)

	target_compile_definitions(${elf_file} PRIVATE ${SAM_PART_DEFINE}=1)

	set_target_properties(
		${elf_file}
		PROPERTIES
		LINK_FLAGS
			"-Wl,-T${TOOLCHAIN_DIRECTORY}/${SAM_PART}.ld \
			-mcpu=${SAM_CPU} \
			-Wl,--entry=Reset_Handler \
			-Wl,--gc-sections -Wl,-Map,${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${map_file}"
	)

	add_custom_command(
		TARGET ${elf_file}
		COMMAND
			${ARM_OBJCOPY} -O binary ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${elf_file} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${bin_file}
		COMMAND
			${ARM_SIZE} -t -A ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${elf_file}
		BYPRODUCTS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${bin_file}
		DEPENDS ${elf_file}
	)

	# program flash with edbg
	add_custom_target(
		program
		${CMSIS_DAP_PROGRAMMER} -bpv -t ${PLATFORM} -f ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${bin_file}
		DEPENDS ${elf_file}
		COMMENT "Uploading ${bin_file} to ${SAM_PART}..."
		)

endfunction()

set(BUILD_SHARED_LIBS OFF)

