# Targeting an embedded system, no OS.
set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

find_program(CMAKE_C_COMPILER arm-none-eabi-gcc)
find_program(CMAKE_CXX_COMPILER arm-none-eabi-g++)
find_program(CMAKE_CXX_COMPILER arm-none-eabi-g++)

find_program(ARM_OBJCOPY arm-none-eabi-objcopy)
find_program(ARM_SIZE arm-none-eabi-size)
find_program(CMSIS_DAP_PROGRAMMER edbg)

set(CMAKE_C_FLAGS_RELEASE "-O3" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_DEBUG "-O0 -gdwarf-3 -gstrict-dwarf" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -gdwarf-3 -gstrict-dwarf" CACHE STRING "" FORCE)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
