# libuc2 — UC2 decompression library

set(LIBUC2_SOURCES src/decompress.c src/compress.c src/uc2_tables.c src/uc2_cdc.c src/uc2_merkle.c)

# Embed super.bin: use .S with .incbin on GCC/Clang, generated C array on MSVC
if(MSVC)
    set(SUPER_C "${CMAKE_CURRENT_BINARY_DIR}/super_data.c")
    add_custom_command(
        OUTPUT "${SUPER_C}"
        COMMAND "${CMAKE_COMMAND}"
            -DINPUT="${CMAKE_CURRENT_SOURCE_DIR}/src/super.bin"
            -DOUTPUT="${SUPER_C}"
            -P "${PROJECT_SOURCE_DIR}/cmake/bin2c.cmake"
        DEPENDS src/super.bin "${PROJECT_SOURCE_DIR}/cmake/bin2c.cmake"
        COMMENT "Generating super_data.c from super.bin"
    )
    list(APPEND LIBUC2_SOURCES "${SUPER_C}")
else()
    set(SUPER_BIN_ABS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/super.bin")
    configure_file(src/super_data.S.in "${CMAKE_CURRENT_BINARY_DIR}/super_data.S" @ONLY)
    list(APPEND LIBUC2_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/super_data.S")
endif()

add_library(uc2 STATIC ${LIBUC2_SOURCES})

target_include_directories(uc2
    PUBLIC  include
    PRIVATE src
            "${PROJECT_BINARY_DIR}/lib"  # for configured uc2_version.h
)

target_compile_features(uc2 PUBLIC c_std_99)
target_compile_definitions(uc2 PRIVATE NDEBUG)

configure_file(
    include/uc2/uc2_version.h.in
    "${PROJECT_BINARY_DIR}/lib/uc2/uc2_version.h"
    @ONLY
)
