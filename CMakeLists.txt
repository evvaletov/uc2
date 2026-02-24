cmake_minimum_required(VERSION 3.16)
project(UC2
    VERSION 3.0.0
    DESCRIPTION "UltraCompressor II — cross-platform revival"
    LANGUAGES C ASM
)

include(cmake/UC2Platform.cmake)

add_subdirectory(lib)
add_subdirectory(cli)
