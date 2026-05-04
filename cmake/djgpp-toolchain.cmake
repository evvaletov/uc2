# CMake toolchain file for DJGPP cross-compile (DOS / FreeDOS).
#
# Usage:
#   cmake -B build-djgpp -DCMAKE_TOOLCHAIN_FILE=cmake/djgpp-toolchain.cmake
#   cmake --build build-djgpp
#
# Requires the DJGPP cross-toolchain on PATH or at DJGPP_ROOT.  The standard
# layout from andrewwutw/build-djgpp and the djfdyuruiry/djgpp docker image
# is /usr/local/bin/djgpp/.  Override with -DDJGPP_ROOT=<path> if installed
# elsewhere.

set(CMAKE_SYSTEM_NAME Generic)        # bare DJGPP DOS, no OS abstractions
set(CMAKE_SYSTEM_PROCESSOR i386)

# Project source uses `if(DJGPP)` to gate the DOS compat layer (cli/src/
# compat/compat_dos.c, sys-include/dos shim).  Set the variable up front
# so those guards activate.
set(DJGPP TRUE)

# Locate the toolchain prefix.
if(NOT DEFINED DJGPP_ROOT)
    if(EXISTS /usr/local/bin/djgpp)
        set(DJGPP_ROOT /usr/local/bin/djgpp)
    elseif(EXISTS /opt/djgpp)
        set(DJGPP_ROOT /opt/djgpp)
    endif()
endif()

if(DEFINED DJGPP_ROOT AND EXISTS ${DJGPP_ROOT})
    set(_DJGPP_BIN ${DJGPP_ROOT}/bin)
else()
    set(_DJGPP_BIN "")
endif()

set(CMAKE_C_COMPILER   ${_DJGPP_BIN}/i586-pc-msdosdjgpp-gcc)
set(CMAKE_CXX_COMPILER ${_DJGPP_BIN}/i586-pc-msdosdjgpp-g++)
set(CMAKE_AR           ${_DJGPP_BIN}/i586-pc-msdosdjgpp-ar    CACHE FILEPATH "")
set(CMAKE_RANLIB       ${_DJGPP_BIN}/i586-pc-msdosdjgpp-ranlib CACHE FILEPATH "")
set(CMAKE_STRIP        ${_DJGPP_BIN}/i586-pc-msdosdjgpp-strip  CACHE FILEPATH "")

if(DEFINED DJGPP_ROOT)
    set(CMAKE_FIND_ROOT_PATH ${DJGPP_ROOT}/i586-pc-msdosdjgpp)
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# DJGPP can produce static binaries; tests run inside DOSBox-X.
set(CMAKE_EXE_LINKER_FLAGS_INIT "")

# CMake's compiler check tries to build a test binary.  DJGPP-produced
# .exe binaries are valid COFF executables that the host kernel will
# refuse to run, so use STATIC_LIBRARY mode.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
