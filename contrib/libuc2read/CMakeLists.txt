# SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
cmake_minimum_required(VERSION 3.16)
project(UC2Reader VERSION 3.0.0 LANGUAGES C)
include(GNUInstallDirs)
get_filename_component(UC2_ROOT "../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SUPER_C "${CMAKE_CURRENT_BINARY_DIR}/super_data.c")
add_custom_command(OUTPUT "${SUPER_C}"
    COMMAND "${CMAKE_COMMAND}" -DINPUT=${UC2_ROOT}/lib/src/super.bin
            -DOUTPUT=${SUPER_C} -P "${UC2_ROOT}/cmake/bin2c.cmake"
    DEPENDS "${UC2_ROOT}/lib/src/super.bin" "${UC2_ROOT}/cmake/bin2c.cmake")
# Only the LGPL decoder, list header, and original SuperMaster data are linked.
# No compressor, rANS implementation, or other GPL module enters this target.
add_library(uc2read SHARED "${UC2_ROOT}/lib/src/decompress.c" "${SUPER_C}")
target_compile_features(uc2read PRIVATE c_std_99)
target_compile_definitions(uc2read PUBLIC UC2_SHARED UC2_HUFFMAN_ONLY
                                  PRIVATE UC2_BUILD UC2_SUPER_C NDEBUG)
target_include_directories(uc2read PUBLIC "${UC2_ROOT}/lib/include"
                                 PRIVATE "${UC2_ROOT}/lib/src")
set_target_properties(uc2read PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 3
                      C_VISIBILITY_PRESET hidden)
configure_file(libuc2read.pc.in libuc2read.pc @ONLY)
install(TARGETS uc2read LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                       RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                       ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES "${UC2_ROOT}/lib/include/uc2/libuc2.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/uc2)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libuc2read.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES README.md "${UC2_ROOT}/LICENSES/LGPL-3.0-only.txt"
              "${UC2_ROOT}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/libuc2read)
