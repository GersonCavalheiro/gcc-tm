"""Check stub contracts and symbol visibility using the selected libgomp build."""
from collections import Counter
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
summary = re.fullmatch(r"\[teste\] lifecycle stubs OK; size_max=(\d+)", lines.pop())
assert summary, result.stdout
size_max = summary.group(1)
counts = Counter()
fields = {
    "begin": ["table", "slot", "outermost"],
    "register": ["ctx", "object", "size"],
    "end": ["slot"],
    "fail": ["ctx", "cause"],
    "discard": ["slot"],
    "backoff": ["attempt"],
}
values = {name: [] for name in fields}
for line in lines:
    match = re.fullmatch(r"\[newtm\] newtm_(\w+): (.+)", line)
    assert match, line
    name, arguments = match.groups()
    assert name in fields, line  # No implicit hash/access/bridge operations.
    pairs = [item.split("=", 1) for item in arguments.split(", ")]
    assert [pair[0] for pair in pairs] == fields[name], line
    assert all(len(pair) == 2 and pair[1] for pair in pairs), line
    values[name].append(dict(pairs))
    counts[name] += 1
assert counts == {"begin": 5, "register": 3, "end": 4,
                  "fail": 5, "discard": 4, "backoff": 3}, counts
assert [v["size"] for v in values["register"]] == ["32", "0", size_max]
assert [v["cause"] for v in values["fail"]] == ["1", "2", "0", "99", "1"]
assert [v["attempt"] for v in values["backoff"]] == ["0", "1", size_max]

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
print("OK: 24 chamadas, INVALID, slots/dados preservados, outermost=false e logs")
print("OK: seis simbolos internos presentes no archive e ausentes da API dinamica")
