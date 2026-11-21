#!/usr/bin/env python3
"""Preserve destinations after late corruption and failed output publication."""
import os
from pathlib import Path
import random
import shutil
import stat
import struct
import subprocess
import sys
import tempfile

from check_cli_io import proof_for, trailer

CLI = str(Path(sys.argv[1]).resolve())


def run(*args, cwd, ok=True, env=None):
    result = subprocess.run([CLI, *map(str, args)], cwd=cwd, env=env,
                            capture_output=True, timeout=30)
    assert (result.returncode == 0) == ok, (args, result)
    assert not list(Path(cwd).rglob('U???????.TMP' if os.name == 'nt' else '.uc2-tmp-*')), list(Path(cwd).rglob('U???????.TMP' if os.name == 'nt' else '.uc2-tmp-*'))
    return result


with tempfile.TemporaryDirectory(prefix='uc2-transactions-') as tmp:
    root = Path(tmp)
    source = root / 'source'
    source.mkdir()
    data = random.Random(99).randbytes(160001)
    (source / 'data').write_bytes(data)
    archive = root / 'caf\u00e9.uc2'
    run('-qw', '--chunked', archive, 'data', cwd=source)
    pristine = archive.read_bytes()
    damaged = bytearray(pristine)
    start = damaged.index(b'UC2CHN1\0')
    count = struct.unpack_from('<I', damaged, start + 8)[0]
    assert count > 2
    last = struct.unpack_from('<I', damaged, start + 88 + (count - 1) * 48)[0]
    damaged[last] ^= 1
    archive.write_bytes(damaged)
    dest = root / 'caf\u00e9-dest'
    dest.mkdir()
    target = dest / 'data'
    # The decoder has delivered earlier chunks before the last chunk fails.
    result = run('-xf', archive, cwd=dest, ok=False)
    assert b'Decompression complete' not in result.stderr
    assert not target.exists()
    target.write_bytes(b'keep existing bytes')
    target.chmod(0o444)
    old_mode = stat.S_IMODE(target.stat().st_mode)
    run('-qxf', archive, cwd=dest, ok=False)
    assert target.read_bytes() == b'keep existing bytes'
    assert stat.S_IMODE(target.stat().st_mode) == old_mode
    archive.write_bytes(pristine)
    run('-qxf', archive, cwd=dest)
    assert target.read_bytes() == data

    # Register cleanup once, and use short sibling names even at NAME_MAX.
    many = root / 'many'
    many.mkdir()
    for i in range(150):
        (many / str(i)).write_bytes(str(i).encode())
    if os.name != 'nt':
        (many / ('x' * 255)).write_bytes(b'long name')
    many_archive = root / 'many.uc2'
    run('-qw', many_archive, '.', cwd=many)
    many_out = root / 'many-out'
    many_out.mkdir()
    run('-qx', many_archive, cwd=many_out)
    assert {p.name: p.read_bytes() for p in (many_out / 'many').iterdir()} == {
        p.name: p.read_bytes() for p in many.iterdir()}

    proof = root / 'proof.ots'
    if os.name != 'nt':
        archive.chmod(0o640)
    original_mode = stat.S_IMODE(archive.stat().st_mode)
    proof.write_bytes(proof_for(b'wrong bytes'))
    run('--ots-attach', proof, archive, cwd=root, ok=False)
    assert archive.read_bytes() == pristine
    proof.write_bytes(proof_for(pristine))
    run('--ots-attach', proof, archive, cwd=root)
    stamped = pristine + trailer(len(pristine), proof.read_bytes())
    assert archive.read_bytes() == stamped
    assert stat.S_IMODE(archive.stat().st_mode) == original_mode
    extracted = root / 'output.ots'
    extracted.write_bytes(b'old proof')
    run('--ots-extract', archive, extracted, cwd=root)
    assert extracted.read_bytes() == proof.read_bytes()

    if os.name == 'nt':
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        create = kernel.CreateFileW
        create.argtypes = (wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                           ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD,
                           wintypes.HANDLE)
        create.restype = wintypes.HANDLE
        close = kernel.CloseHandle
        close.argtypes = (wintypes.HANDLE,)
        close.restype = wintypes.BOOL
        for readonly in (False, True):
            target.chmod(0o666)
            target.write_bytes(b'Windows locked destination')
            target.chmod(0o444 if readonly else 0o666)
            before_mode = stat.S_IMODE(target.stat().st_mode)
            # FILE_SHARE_READ | FILE_SHARE_WRITE, deliberately no SHARE_DELETE.
            handle = create(str(target), 0x80000000, 3, None, 3, 0x80, None)
            assert handle != ctypes.c_void_p(-1).value, ctypes.get_last_error()
            try:
                run('-qxf', archive, cwd=dest, ok=False)
            finally:
                assert close(handle)
            assert target.read_bytes() == b'Windows locked destination'
            assert stat.S_IMODE(target.stat().st_mode) == before_mode
        target.chmod(0o666)

    if sys.platform == 'linux' and shutil.which('cc') and not os.environ.get('ASAN_OPTIONS'):
        # Interpose only on owned staged paths, never on production code or on
        # the library's anonymous compression spools. Exercise buffered close
        # errors, write errors, final rename/link errors and a no-clobber race.
        shim = root / 'fault.c'
        shim.write_text(r'''
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
static int staged(FILE *f) {
    char proc[64], path[4096];
    snprintf(proc,sizeof proc,"/proc/self/fd/%d",fileno(f));
    ssize_t n=readlink(proc,path,sizeof path-1);
    if(n<0) return 0;
    path[n]=0; return strstr(path,"/.uc2-tmp-")!=NULL;
}
static int mode(const char *name) {
    const char *value=getenv("UC2_TEST_FAULT");
    return value && !strcmp(value,name);
}
pid_t getpid(void) {
    if(mode("collision")) return 4242;
    pid_t (*real)(void)=dlsym(RTLD_NEXT,"getpid"); return real();
}
time_t time(time_t *out) {
    if(mode("collision")) { if(out) *out=1700000000; return 1700000000; }
    time_t (*real)(time_t *)=dlsym(RTLD_NEXT,"time"); return real(out);
}
static void race_target(const char *path) {
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd>=0) { write(fd,"racing output",13); close(fd); }
}
#ifdef SYS_renameat2
long syscall(long number,...) {
    /* The CLI uses syscall only for renameat2; libc's own syscalls are internal. */
    if(number!=SYS_renameat2) { errno=ENOSYS; return -1; }
    va_list ap; va_start(ap,number);
    int oldfd=va_arg(ap,int); const char *a=va_arg(ap,const char *);
    int newfd=va_arg(ap,int); const char *b=va_arg(ap,const char *);
    unsigned flags=va_arg(ap,unsigned); va_end(ap);
    if(mode("fallback") || mode("race-link")) { errno=ENOSYS; return -1; }
    if(mode("publish")) { errno=EACCES; return -1; }
    if(mode("race")) race_target(b);
    long (*real)(long,...)=dlsym(RTLD_NEXT,"syscall");
    return real(number,oldfd,a,newfd,b,flags);
}
#endif
int native_rename_available(const char *a,const char *b) {
#ifdef SYS_renameat2
    long (*real)(long,...)=dlsym(RTLD_NEXT,"syscall");
    if(real(SYS_renameat2,AT_FDCWD,a,AT_FDCWD,b,1u)==0) return 1;
    if(errno!=ENOSYS && errno!=EINVAL && errno!=EOPNOTSUPP) return -1;
#else
    (void)a; (void)b;
#endif
    return 0;
}
size_t fwrite(const void *p,size_t size,size_t n,FILE *f) {
    size_t (*real)(const void *,size_t,size_t,FILE *)=dlsym(RTLD_NEXT,"fwrite");
    if(mode("write") && staged(f)) { errno=ENOSPC; return 0; }
    return real(p,size,n,f);
}
int fclose(FILE *f) {
    int (*real)(FILE *)=dlsym(RTLD_NEXT,"fclose");
    int fail=mode("close") && staged(f);
    int result=real(f);
    if(fail) { errno=ENOSPC; return EOF; }
    return result;
}
int ferror(FILE *f) {
    int (*real)(FILE *)=dlsym(RTLD_NEXT,"ferror");
    if(mode("sticky") && staged(f)) { errno=ENOSPC; return 1; }
    return real(f);
}
int rename(const char *a,const char *b) {
    int (*real)(const char *,const char *)=dlsym(RTLD_NEXT,"rename");
    if(mode("publish") && strstr(a,".uc2-tmp-")) { errno=EACCES; return -1; }
    return real(a,b);
}
int link(const char *a,const char *b) {
    int (*real)(const char *,const char *)=dlsym(RTLD_NEXT,"link");
    if(strstr(a,".uc2-tmp-")) {
        if(mode("publish")) { errno=EACCES; return -1; }
        if(mode("no-links")) { errno=EOPNOTSUPP; return -1; }
        if(mode("race") || mode("race-link")) race_target(b);
    }
    return real(a,b);
}
''')
        library = root / 'fault.so'
        subprocess.run(['cc', '-shared', '-fPIC', '-Wall', '-Wextra', shim,
                        '-ldl', '-o', library], check=True, capture_output=True)
        import ctypes
        probe = ctypes.CDLL(str(library)).native_rename_available
        probe.argtypes = (ctypes.c_char_p, ctypes.c_char_p)
        probe.restype = ctypes.c_int
        probe_from, probe_to = root / 'rename-from', root / 'rename-to'
        probe_from.write_bytes(b'probe')
        native_rename = probe(os.fsencode(probe_from), os.fsencode(probe_to))
        assert native_rename in (0, 1), 'unexpected native rename probe failure'
        for path in (probe_from, probe_to):
            if path.exists():
                path.unlink()
        collision_name = '.uc2-tmp-4242-1700000000-0'
        collision_source = source / collision_name
        collision_source.write_bytes(data)
        collision_archive = root / 'collision.uc2'
        subprocess.run([CLI, '-qw', str(collision_archive), collision_name], cwd=source,
                       check=True, capture_output=True, timeout=30)
        collision_source.unlink()
        env = dict(os.environ, LD_PRELOAD=str(library), UC2_TEST_FAULT='collision')
        # The requested output looks exactly like our first candidate. It must
        # never be used as the stage; no-replace publication must still succeed.
        result = subprocess.run([CLI, '-qx', str(collision_archive)], cwd=dest,
                                env=env, capture_output=True, timeout=30)
        assert result.returncode == 0, result
        collision_output = dest / collision_name
        assert collision_output.read_bytes() == data
        collision_output.unlink()
        assert not list(dest.glob('.uc2-tmp-*'))
        for fault in ('write', 'close', 'sticky', 'publish'):
            env = dict(os.environ, LD_PRELOAD=str(library), UC2_TEST_FAULT=fault)
            for existing in (False, True):
                if target.exists():
                    target.unlink()
                if existing:
                    target.write_bytes(b'preserved')
                run('-qxf' if existing else '-qx', archive, cwd=dest, ok=False, env=env)
                assert target.read_bytes() == b'preserved' if existing else not target.exists()
            for previous in (pristine, stamped):
                archive.write_bytes(previous)
                archive.chmod(0o640)
                run('--ots-attach', proof, '-f', archive, cwd=root, ok=False, env=env)
                assert archive.read_bytes() == previous
                assert stat.S_IMODE(archive.stat().st_mode) == 0o640
            extracted.write_bytes(b'previous proof')
            run('--ots-extract', archive, extracted, cwd=root, ok=False, env=env)
            assert extracted.read_bytes() == b'previous proof'
        for fault in ('race', 'race-link', 'fallback', 'no-links'):
            if target.exists():
                target.unlink()
            env = dict(os.environ, LD_PRELOAD=str(library), UC2_TEST_FAULT=fault)
            race = fault.startswith('race')
            unavailable = fault == 'no-links' and not native_rename
            run('-qx', archive, cwd=dest, ok=not (race or unavailable), env=env)
            if unavailable:
                assert not target.exists()
            else:
                assert target.read_bytes() == (b'racing output' if race else data)
    else:
        print('POSIX interposition checks skipped; portable corruption/OTS checks ran')

print('CLI transactional output checks passed')
