set(SAM_PART "samd51n20a")
set(SAM_CPU "cortex-m4")

add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mfloat-abi=hard -mfpu=fpv4-sp-d16")
