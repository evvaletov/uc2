#!/usr/bin/env python3
"""Check option-operand ownership before any command can modify its archive."""
import io
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile

from check_cli_io import proof_for


CLI = str(Path(sys.argv[1]).resolve())
PAYLOAD = b'option ownership regression\n'
LONG_NAMES = ('--ingest', '--ingest-tar', '--ingest-restore', '--ots-info',
              '--ots-extract', '--ots-attach=proof', '--chunked', '--')


def run(*args, cwd, success=True, data=PAYLOAD):
    result = subprocess.run([CLI, *map(str, args)], cwd=cwd, input=data,
                            capture_output=True, timeout=20)
    assert (result.returncode == 0) == success, (args, result)
    return result


with tempfile.TemporaryDirectory(prefix='uc2-cli-options-') as temporary:
    root = Path(temporary)
    source = root / 'data.txt'
    source.write_bytes(PAYLOAD)
    archive = root / 'source.uc2'
    run('-qw', archive, source, cwd=root)
    original = archive.read_bytes()

    # Each short option owns its complete value, including another option's
    # exact spelling. Test separate, clustered and attached forms.
    for name in LONG_NAMES:
        directory = root / name
        directory.mkdir()
        for option in ('d', 'C'):
            forms = [('-qx', '-' + option, name),
                     ('-qx' + option, name), ('-qx' + option + name,)]
            for flags in forms:
                target = directory / 'data.txt'
                target.unlink(missing_ok=True)
                run(*flags, archive, cwd=root)
                assert archive.read_bytes() == original, (name, flags)
                assert target.read_bytes() == PAYLOAD, (name, flags)
        for flags in (('-qw', '-L', name), ('-qwL', name), ('-qwL' + name,)):
            result = run(*flags, archive, source, cwd=root, success=False)
            assert b'Compression level' in result.stderr, result
            assert archive.read_bytes() == original, (name, flags)

    for index, flags in enumerate((('-qw', '-L', '2'), ('-qwL', '2'), ('-qwL2',))):
        created = root / f'level-{index}.uc2'
        run(*flags, created, source, cwd=root)
        assert run('--ingest-restore', created, cwd=root).stdout == PAYLOAD

    literal = root / 'literal'
    literal.mkdir()
    for index, name in enumerate(LONG_NAMES[:-1]):
        (literal / name).write_bytes(PAYLOAD)
        created = literal / f'file-{index}.uc2'
        run('-qw', '--', created, name, cwd=literal)
        assert run('--ingest-restore', created, cwd=root).stdout == PAYLOAD

    # Ingestion accepts only its long operation and optional creation mode.
    # Unsupported flags and mixed operations must preserve an existing output.
    extras = [('-q',), ('-L2',), ('-L', '2'), ('-C', str(root)),
              ('-d', '--ots-info'), ('-w',), ('extra-input',),
              ('--unknown',), ('--ots-info',), ('--ingest', str(archive))]
    for operation in ('--ingest', '--ingest-tar', '--ingest-restore'):
        for extra in extras:
            for args in ((operation, archive, *extra), (*extra, operation, archive)):
                result = run(*args, cwd=root, success=False)
                assert (b'only one ingestion or OTS operation' in result.stderr
                        or b'ingestion modes do not accept' in result.stderr), result
                assert result.stdout == b'', result
                assert archive.read_bytes() == original, args
    for args in (('--ingest',), ('--ingest=',), ('--ots-attach', '--ingest', archive),
                 ('--ots-attach=', archive), ('--chunked', '--ingest-restore', archive)):
        run(*args, cwd=root, success=False)
        assert archive.read_bytes() == original, args

    # Ordinary separate and inline long arguments, both chunk layouts, a
    # trailing delimiter, and a leading-hyphen output path remain supported.
    for chunked in (False, True):
        flags = ('--chunked',) if chunked else ()
        for inline in (False, True):
            created = root / f'ingest-{chunked}-{inline}.uc2'
            operation = ('--ingest=' + str(created),) if inline else ('--ingest', created)
            run(*flags, *operation, '--', cwd=root)
            assert run('--ingest-restore=' + str(created), cwd=root).stdout == PAYLOAD
    values = root / 'long-values'
    values.mkdir()
    run('--ingest=--ots-info', cwd=values)
    assert run('--ingest-restore=--ots-info', cwd=values).stdout == PAYLOAD

    tar_bytes = io.BytesIO()
    with tarfile.open(fileobj=tar_bytes, mode='w') as tar:
        member = tarfile.TarInfo('payload')
        member.size = len(PAYLOAD)
        tar.addfile(member, io.BytesIO(PAYLOAD))
    for chunked in (False, True):
        flags = ('--chunked',) if chunked else ()
        created = root / f'tar-{chunked}.uc2'
        run(*flags, '--ingest-tar', created, cwd=root, data=tar_bytes.getvalue())
        assert run('-p', created, cwd=root).stdout == PAYLOAD

    # The proof operand belongs to --ots-attach, and a delimiter protects an
    # option-like output filename from every long-option parser.
    proof = proof_for(original)
    (values / '--ingest').write_bytes(proof)
    with_proof = values / 'proof.uc2'
    with_proof.write_bytes(original)
    run('--ots-attach=--ingest', with_proof, cwd=values)
    assert b'leaf matches archive: yes' in run('--ots-info', with_proof, cwd=root).stdout
    run('--ots-extract', '--', with_proof, '--ingest-restore', cwd=values)
    assert (values / '--ingest-restore').read_bytes() == proof
    assert archive.read_bytes() == original
    assert source.read_bytes() == PAYLOAD

print('CLI option ownership checks passed')
