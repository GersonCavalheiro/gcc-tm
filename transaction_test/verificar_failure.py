"""Verify cleanup precedes fatal diagnostics and conflicts alone reach backoff."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
binary = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else root / ".build" / "failure"
for mode in (None, "invalid", "zero", "invalid-dominates", "twice",
             "execute-invalid", "execute-lies", "execute-unbalanced",
             "execute-unknown", "execute-conflict-invalid"):
    command = [str(binary)]
    if mode:
        command.append(mode)
    result = subprocess.run(command, capture_output=True, text=True, timeout=10)
    (root / ".build" / f"{binary.name}-{mode or 'conflict'}.log").write_text(
        result.stdout + result.stderr
    )
    if result.returncode != (1 if mode else 0):
        raise SystemExit(f"{mode}: {result.returncode}\n{result.stdout}{result.stderr}")
    for primitive in ("newtm_discard:", "newtm_hash_release:"):
        if result.stdout.count(primitive) != 1:
            raise SystemExit(f"{mode}: expected one {primitive}\n{result.stdout}")
    if mode:
        if "[teste] fatal after complete cleanup; no retry" not in result.stdout:
            raise SystemExit(f"{mode}: cleanup was not checked")
        backoffs = result.stdout.count("newtm_backoff:")
        if backoffs != (1 if mode == "twice" else 0):
            raise SystemExit(f"{mode}: unexpected backoff")
        if "transaction invalid attempt" not in result.stderr and mode != "twice":
            raise SystemExit(f"{mode}: missing permanent diagnostic")
print("OK: conflito, INVALID dominante, descarte único e fatal após limpeza")
