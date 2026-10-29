"""Exercise read-heavy, balanced and write-heavy transaction bridge workloads."""
import os
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parent
if len(sys.argv) != 3:
    raise SystemExit("Uso: verificar_carga.py THREADS REPETICOES")
try:
    threads, repetitions = map(int, sys.argv[1:])
except ValueError:
    raise SystemExit("THREADS e REPETICOES devem ser inteiros positivos")
if threads <= 0 or repetitions <= 0:
    raise SystemExit("THREADS e REPETICOES devem ser positivos")

pattern = re.compile(
    r"^\[teste\] carga OK: threads=(\d+) relacao=(\d+):(\d+) repeticoes=(\d+) "
    r"transacoes=(\d+) escritas=(\d+) leituras=(\d+) valor=(\d+)$", re.MULTILINE
)
for weights in ((1, 9), (1, 1), (9, 1)):
    w, r = weights
    ratio = f"{w}:{r}"
    path = root / ".build" / f"transaction_carga-{w}-{r}.log"
    with path.open("w") as output:
        try:
            result = subprocess.run(
                [str(root / ".build" / "transaction_carga"), str(threads), ratio,
                 str(repetitions)],
                stdout=output, stderr=subprocess.PIPE, text=True, timeout=60,
                env=dict(os.environ, OMP_DYNAMIC="FALSE"),
            )
        except subprocess.TimeoutExpired as error:
            raise SystemExit(f"FAIL: carga {ratio}: timeout; consulte {path}") from error
    if result.stderr:
        with path.open("a") as output:
            output.write(result.stderr)
    if result.returncode != 0 or result.stderr:
        raise SystemExit(f"FAIL: carga {ratio}: saída={result.returncode}; consulte {path}")
    log = path.read_text()
    matches = pattern.findall(log)
    writes = threads * (repetitions * w // (w + r))
    total = threads * repetitions
    expected = (threads, w, r, repetitions, total, writes, total - writes, writes)
    if len(matches) != 1 or tuple(map(int, matches[0])) != expected:
        raise SystemExit(f"FAIL: carga {ratio}: resultado divergente; consulte {path}")
    for operation, count in (("create", 1), ("acquire", 4 * total),
                             ("release", 4 * total), ("destroy", 1)):
        if log.count(f"[newtm] newtm_hash_{operation}:") != count:
            raise SystemExit(f"FAIL: carga {ratio}: contagem de {operation}; consulte {path}")
    print(f"OK: carga {ratio}, threads={threads}, repetições={repetitions}, "
          f"escritas={writes}, leituras={total - writes}")
