# CMake toolchain file for DJGPP cross-compilation (DOS target)
#
# Usage: cmake -B build-dos -DCMAKE_TOOLCHAIN_FILE=cmake/djgpp.cmake
#        cmake --build build-dos

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i586)

set(DJGPP_ROOT "/usr/local/djgpp" CACHE PATH "DJGPP installation root")
set(DJGPP_PREFIX "i586-pc-msdosdjgpp" CACHE STRING "DJGPP toolchain prefix")

set(CMAKE_C_COMPILER   "${DJGPP_ROOT}/bin/${DJGPP_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${DJGPP_ROOT}/bin/${DJGPP_PREFIX}-g++")
set(CMAKE_ASM_COMPILER "${DJGPP_ROOT}/bin/${DJGPP_PREFIX}-gcc")
set(CMAKE_AR           "${DJGPP_ROOT}/bin/${DJGPP_PREFIX}-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB       "${DJGPP_ROOT}/bin/${DJGPP_PREFIX}-ranlib" CACHE FILEPATH "")
set(CMAKE_STRIP        "${DJGPP_ROOT}/bin/${DJGPP_PREFIX}-strip" CACHE FILEPATH "")

# This DJGPP cross-compiler has /usr/include baked in and -nostdinc doesn't
# remove it.  Using -I (not -isystem) puts the DJGPP paths before /usr/include
# so the correct headers are always found first.
set(_DJGPP_NOSTDINC "-nostdinc -I${DJGPP_ROOT}/lib/gcc/${DJGPP_PREFIX}/12.2.0/include -I${DJGPP_ROOT}/lib/gcc/${DJGPP_PREFIX}/12.2.0/include-fixed -I${DJGPP_ROOT}/${DJGPP_PREFIX}/sys-include")
set(CMAKE_C_FLAGS_INIT "${_DJGPP_NOSTDINC}")
set(CMAKE_ASM_FLAGS_INIT "${_DJGPP_NOSTDINC}")

set(CMAKE_FIND_ROOT_PATH "${DJGPP_ROOT}/${DJGPP_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_EXECUTABLE_SUFFIX ".exe")

set(DJGPP TRUE)
set(DOS TRUE)
