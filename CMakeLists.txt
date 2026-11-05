cmake_minimum_required(VERSION 3.16)
project(UC2
    VERSION 3.0.0
    DESCRIPTION "UltraCompressor II — cross-platform revival"
    LANGUAGES C ASM
)

include(cmake/UC2Platform.cmake)

option(UC2_BUILD_PYTHON "Build the Python reader extension and bundled CLI" OFF)
set(UC2_VERSION_LABEL "${PROJECT_VERSION}" CACHE STRING "Version displayed by the CLI")
if(UC2_BUILD_PYTHON AND MSVC)
    # Standalone CLI downloads must not require a separately installed VC runtime.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

add_subdirectory(lib)
add_subdirectory(cli)
add_subdirectory(contrib/libarchive)
add_subdirectory(contrib/totalcommander)
if(UC2_BUILD_PYTHON)
    add_subdirectory(python)
endif()

option(UC2_BUILD_TESTS "Build tests" ON)
if(UC2_BUILD_TESTS AND NOT DJGPP)
    enable_testing()
    add_subdirectory(tests)
endif()
