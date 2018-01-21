set(SAM_PART "saml21e17b")
set(SAM_CPU "cortex-m0plus")

add_compile_options(-msoft-float)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -msoft-float")
