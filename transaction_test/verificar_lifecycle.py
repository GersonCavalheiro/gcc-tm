"""Check context lifecycle and symbol visibility using the selected libgomp build."""
from pathlib import Path
import re
import subprocess
import sys

if not __debug__:
    raise SystemExit(
        "verificar_lifecycle.py requer assercoes: remova -O/-OO e PYTHONOPTIMIZE"
    )

ROOT = Path(__file__).resolve().parent
library = Path(sys.argv[1]).resolve()
result = subprocess.run(
    [str(ROOT / ".build" / "lifecycle_stubs")],
    text=True, capture_output=True, check=True, timeout=10,
)
(ROOT / ".build" / "lifecycle_stubs.log").write_text(result.stdout + result.stderr)
assert not result.stderr, result.stderr
lines = result.stdout.splitlines()
assert lines.pop() == "[teste] context lifecycle OK", result.stdout
registers = []
for line in lines:
    assert line.startswith("[newtm] "), line
    if line.startswith("[newtm] newtm_register:"):
        registers.append(line)
assert [re.search(r"kind=(-?\d+)$", line).group(1)
        for line in registers] == ["0", "1", "99"]
assert sum("newtm_hash_create:" in line for line in lines) == 1
assert sum("newtm_hash_destroy:" in line for line in lines) == 1
assert sum("newtm_hash_release:" in line for line in lines) == 5
fields = {name: None for name in
          ("begin", "register", "end", "fail", "discard", "backoff",
           "policy_destroy", "reserve", "read", "write", "after_failure",
           "execute")}

# Permanent errors must not hang or manufacture a recoverable result.
for mode, message in [
    ("policy-live", "transaction policy destroyed with active reservations"),
    ("bridge-invalid", "transaction registration requires a nonnull object and size"),
]:
    error = subprocess.run([str(ROOT / ".build" / "lifecycle_stubs"), mode],
                           text=True, capture_output=True, timeout=10)
    (ROOT / ".build" / ("context-" + mode + ".log")).write_text(
        error.stdout + error.stderr)
    assert error.returncode == 1 and message in error.stderr, error
    if mode == "bridge-invalid":
        assert error.stdout.count("newtm_hash_release:") == 1
        assert error.stdout.count("newtm_discard:") == 1
        assert error.stdout.count("newtm_hash_destroy:") == 1

# Hidden functions must be present in the archive but absent from the DSO API.
symbols = {"newtm_" + name for name in fields}
archive = subprocess.check_output(
    ["nm", "--defined-only", str(library / "libgomp.a")], text=True,
)
dynamic = subprocess.check_output(
    ["nm", "-D", "--defined-only", str(library / "libgomp.so")], text=True,
)
defined = lambda output: {line.split()[-1].split("@")[0]
                          for line in output.splitlines() if line.split()}
assert symbols <= defined(archive), symbols - defined(archive)
assert not symbols & defined(dynamic), symbols & defined(dynamic)
print("OK: criacao, transferencia de lease, aninhamento, falhas e limpeza real")
print("OK: escalares/agregados descartados sem publicar; simbolos internos ocultos")
