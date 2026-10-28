"""Validate irreversible owner closure in isolated, bounded processes."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent
for mode in (
    "unused", "nested", "concurrent-close", "pending-cleanup",
    "creation-inflight", "residual-entries", "admission-race", "lease-errors",
    "late-begin", "exit-active", "finalize-active",
):
    try:
        result = subprocess.run(
            [str(root / ".build" / "hash_owner"), mode],
            capture_output=True, text=True, timeout=15,
        )
    except subprocess.TimeoutExpired as error:
        (root / ".build" / f"hash_owner-{mode}.log").write_bytes(
            (error.stdout or b"") + (error.stderr or b"")
        )
        raise SystemExit(f"FAIL: {mode}: timeout")
    (root / ".build" / f"hash_owner-{mode}.log").write_text(
        result.stdout + result.stderr
    )
    expected = 1 if mode == "late-begin" else 0
    if result.returncode != expected:
        raise SystemExit(f"FAIL: {mode}: {result.returncode}\n{result.stdout}{result.stderr}")
    if mode == "late-begin":
        if "transaction admission failed" not in result.stderr:
            raise SystemExit("FAIL: late-begin: diagnóstico ausente")
    elif result.stderr:
        raise SystemExit(f"FAIL: {mode}: {result.stderr}")
    if mode == "exit-active":
        if "[teste] exit with active participation" not in result.stdout:
            raise SystemExit("FAIL: exit-active: marcador ausente")
        if "newtm_hash_destroy:" in result.stdout:
            raise SystemExit("FAIL: exit-active: destruição sem quiescência")
    elif mode != "late-begin" and "[teste] hash owner OK" not in result.stdout:
        raise SystemExit(f"FAIL: {mode}: marcador ausente")
    print(f"OK: owner {mode}, saída={expected}")
