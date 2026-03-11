cmake_minimum_required(VERSION 3.16)
project(UC2
    VERSION 3.0.0
    DESCRIPTION "UltraCompressor II — cross-platform revival"
    LANGUAGES C ASM
)

include(cmake/UC2Platform.cmake)

add_subdirectory(lib)
add_subdirectory(cli)

option(UC2_BUILD_TESTS "Build tests" ON)
if(UC2_BUILD_TESTS AND NOT DJGPP)
    enable_testing()
    add_subdirectory(tests)
endif()
