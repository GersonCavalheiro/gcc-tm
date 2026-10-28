"""Run each lock-inversion regression in a separate process with a deadline."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent
executable = root / ".build" / "hash_diagnostics"
for mode in ("init-stdout", "entry-oom", "table-oom", "buckets-oom"):
    try:
        result = subprocess.run(
            [str(executable), mode], capture_output=True, text=True, timeout=10
        )
    except subprocess.TimeoutExpired as error:
        (root / ".build" / f"hash_diagnostics-{mode}.log").write_bytes(
            (error.stdout or b"") + (error.stderr or b"")
        )
        raise SystemExit(f"FAIL: {mode}: timeout, possível inversão de locks")
    (root / ".build" / f"hash_diagnostics-{mode}.log").write_text(
        result.stdout + result.stderr
    )
    expected = 0 if mode == "init-stdout" else 1
    marker = (
        "[teste] init stdout without lock inversion OK"
        if mode == "init-stdout"
        else "[teste] fatal stderr without lock inversion OK"
    )
    if result.returncode != expected or marker not in result.stdout:
        raise SystemExit(f"FAIL: {mode}: {result.returncode}\n{result.stdout}{result.stderr}")
    if mode == "init-stdout":
        if result.stderr or result.stdout.count("[newtm] newtm_hash_create:") != 1:
            raise SystemExit(f"FAIL: {mode}: diagnóstico inesperado")
    elif "libgomp: Out of memory allocating" not in result.stderr:
        raise SystemExit(f"FAIL: {mode}: diagnóstico fatal ausente")
    print(f"OK: {mode}, diagnóstico fora dos locks, saída={expected}")
