# DJGPP cross-compile

This builds `uc2.exe` for DOS / FreeDOS using the DJGPP toolchain.
The output is a 32-bit protected-mode DOS executable that runs under
the bundled `cwsdpmi.exe` extender (or any DPMI host).

## One-time setup

1. Get a DJGPP cross-toolchain.  The simplest source is the prebuilt
   release from `andrewwutw/build-djgpp`:

   ```sh
   curl -fsLO https://github.com/andrewwutw/build-djgpp/releases/download/v3.4/djgpp-linux64-gcc1220.tar.bz2
   sudo mkdir -p /opt && sudo tar xjf djgpp-linux64-gcc1220.tar.bz2 -C /opt
   ```

   This puts the toolchain at `/opt/djgpp/`.  Use any prefix; pass
   it as `-DDJGPP_ROOT=<prefix>` when configuring.

2. (Linux hosts) Make sure your shell has not exported `CPATH` or
   `CPLUS_INCLUDE_PATH`.  Some distros and dev environments
   (Intel oneAPI, certain conda envs) export them.  GCC honours these
   regardless of `-nostdinc`, so any host include directory listed there
   ends up *first* in the cross-compiler's search path -- typically
   pulling in glibc headers that fail to compile against DJGPP libc.
   Either `unset CPATH CPLUS_INCLUDE_PATH` for the build shell, or
   wrap the cmake invocation in `env -u CPATH -u CPLUS_INCLUDE_PATH`.

## Build

```sh
unset CPATH CPLUS_INCLUDE_PATH
cmake -B build-djgpp \
      -DCMAKE_TOOLCHAIN_FILE=cmake/djgpp-toolchain.cmake \
      -DDJGPP_ROOT=/opt/djgpp
cmake --build build-djgpp
```

Output: `build-djgpp/cli/uc2` (also linked as `uc2.exe`).  Copy it
plus `cwsdpmi.exe` (shipped with DJGPP at
`<DJGPP_ROOT>/i586-pc-msdosdjgpp/bin/cwsdpmi.exe`) to a DOS volume.

## Status

- Compiles clean against DJGPP gcc 7.2.0 and 12.2.0.
- Library (`libuc2.a`) builds without changes.
- CLI uses the DOS compat layer in `cli/src/compat/compat_dos.c` for
  the BSD `err.h` and POSIX `fnmatch` shims.
- Not yet run under DOSBox-X end-to-end; smoke-test follow-up tracked
  separately.

## Notes

- The toolchain file forces `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY`
  because the compiler check would otherwise try to execute a DOS .exe
  on the host kernel and fail.
- DJGPP's `unistd.h` provides POSIX-shaped APIs; most of the existing
  source compiles unchanged.  The library has no DOS-specific code
  paths.
