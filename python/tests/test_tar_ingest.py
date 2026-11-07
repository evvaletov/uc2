"""Exercise native stream parsing through the CLI shipped in the wheel."""
from pathlib import Path
import subprocess
import sys
from uc2._cli import executable


def test_named_tar_entries():
    script = Path(__file__).resolve().parents[2] / 'tests/scripts/check_tar_ingest.py'
    result = subprocess.run([sys.executable, str(script), str(executable())], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
