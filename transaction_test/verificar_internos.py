"""Check examples using actual internal STM, independent of pragma integration."""
import os
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parent
try:
    threads, repetitions = map(int, sys.argv[1:])
    if threads <= 0 or repetitions <= 0:
        raise ValueError
except ValueError:
    raise SystemExit('Uso: verificar_internos.py THREADS REPETICOES (positivos)')


def execute(name, arguments, label, team):
    path = root / '.build' / f'{label}.log'
    with path.open('w') as output:
        try:
            result = subprocess.run(
                [str(root / '.build' / name), *arguments], stdout=output,
                stderr=subprocess.PIPE, text=True, timeout=60,
                env=dict(os.environ, OMP_NUM_THREADS=str(team), OMP_DYNAMIC='FALSE'),
            )
        except subprocess.TimeoutExpired as error:
            raise SystemExit(f'FAIL: {label}: timeout; consulte {path}') from error
    if result.stderr:
        with path.open('a') as output:
            output.write(result.stderr)
    if result.returncode or result.stderr:
        raise SystemExit(f'FAIL: {label}: saída={result.returncode}; consulte {path}')
    log = path.read_text()
    # One global owner; explicit shutdown after the parallel join. With retry,
    # attempts/acquisitions may exceed committed operations but must all drain.
    assert log.count('[newtm] newtm_hash_create:') == 1, path
    assert log.count('[newtm] newtm_hash_destroy:') == 1, path
    assert log.count('[newtm] newtm_hash_acquire:') == log.count('[newtm] newtm_hash_release:'), path
    assert '[newtm] newtm_read:' in log, path
    return log, path


for team in sorted({1, 2, threads}):
    label = f'interno-simples-{team}'
    log, path = execute('transaction_interno_simples', [], label, team)
    matches = re.findall(r'^\[teste\] interno simples OK: threads=(\d+) repeticoes=(\d+) valor=(\d+)$',
                         log, re.MULTILINE)
    assert len(matches) == 1 and tuple(map(int, matches[0])) == (team, 100, team * 100), path
    assert '[newtm] newtm_write:' in log, path
    print(f'OK: interno simples, threads={team}, valor={team * 100}')

for team in sorted({1, threads}):
    for w, r in ((1, 9), (1, 1), (9, 1), (0, 1), (1, 0)):
        label = f'interno-carga-{team}-{w}-{r}'
        log, path = execute('transaction_interno_carga',
                            [str(team), f'{w}:{r}', str(repetitions)], label, team)
        matches = re.findall(
            r'^\[teste\] interno carga OK: threads=(\d+) relacao=(\d+):(\d+) repeticoes=(\d+) '
            r'transacoes=(\d+) escritas=(\d+) leituras=(\d+) valor=(\d+)$', log, re.MULTILINE)
        total = team * repetitions
        writes = team * (repetitions * w // (w + r))
        assert len(matches) == 1 and tuple(map(int, matches[0])) == (
            team, w, r, repetitions, total, writes, total - writes, writes), path
        assert ('[newtm] newtm_write:' in log) == bool(writes), path
        print(f'OK: interno carga {w}:{r}, threads={team}, escritas={writes}, leituras={total-writes}')

# Keep the same command-line rejection contract as the bridge workload.
for arguments in ([], ['0', '1:1', '1'], ['1', '0:0', '1'],
                  ['1', '1:1', '0'], ['1', '4294967296:1', '1'],
                  ['2', '1:1', '18446744073709551615']):
    result = subprocess.run([str(root / '.build' / 'transaction_interno_carga'), *arguments],
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 1 and 'Uso:' in result.stderr, arguments
print('OK: argumentos inválidos rejeitados antes de iniciar transações')
