"""Compile/run existing transaction cases when DejaGNU is unavailable.

This checks acceptance, rejection, ICE absence and exit status. It does not
interpret Tcl dg-final scans or replace a complete DejaGNU execution.
"""
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
source = root.parent / 'gcc-src-tm'
out = root / '.build' / 'regression-compiler'
out.mkdir(parents=True, exist_ok=True)
cc = build / 'gcc' / 'xgcc'
gomp = build / 'x86_64-pc-linux-gnu' / 'libgomp'
report = []
for directory, languages in (
    ('gcc/testsuite/c-c++-common/gomp', ('c', 'c++')),
    ('gcc/testsuite/gcc.dg/gomp', ('c',)),
    ('gcc/testsuite/g++.dg/gomp', ('c++',)),
    ('libgomp/testsuite/libgomp.c-c++-common', ('c', 'c++')),
):
    for path in sorted((source / directory).glob('transaction*')):
        text = path.read_text()
        negative = 'dg-error' in text
        execute = 'dg-do run' in text and not negative
        flags = re.search(r'dg-options "([^"]*)"', text)
        flags = shlex.split(flags.group(1)) if flags else ['-fopenmp']
        for language in languages:
            label = directory.replace('/', '-') + '-' + path.name + '-' + language.replace('+', 'p')
            binary = out / label
            command = [str(cc), f'-B{build / "gcc"}/', f'-B{gomp}/', '-x', language,
                       *flags, str(path), f'-L{gomp / ".libs"}', f'-Wl,-rpath,{gomp / ".libs"}',
                       *([] if execute else ['-c']), '-o', str(binary)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            log = result.stdout + result.stderr
            passed = result.returncode == (1 if negative else 0) and 'internal compiler error' not in log
            if passed and execute:
                run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
                passed = run.returncode == 0
                log += run.stdout + run.stderr
            (out / (label + '.log')).write_text(log)
            report.append({'case': label, 'passed': passed, 'command': command})
            if not passed: print(f'FAIL: {label}\n{log}')
(out / 'resultado.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'{sum(row["passed"] for row in report)}/{len(report)} casos existentes; sem interpretar dg-final')
raise SystemExit(not all(row['passed'] for row in report))
