#!/usr/bin/env python3
"""Compare actual processes and retain logs; independent of assert/Python -O."""
from pathlib import Path
import json
import subprocess
import sys


def run_program(binary, args, log):
    try:
        with log.open('w') as output:
            result = subprocess.run([str(binary.resolve()), *args], stdout=output,
                                    stderr=subprocess.STDOUT, timeout=60)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f'Timeout: {binary} {args}; see {log}') from error
    return result.returncode, log.read_text(errors='replace')


def main():
    directory = Path(sys.argv[1]).resolve()
    variants = ('sequential', 'internal', 'directive', 'gnu_tm')
    cases = [[]]
    invalid = []
    if directory.name == '03-concurrent-counter':
        cases += [['1', '17'], ['4', '101']]
        invalid = [['0', '10'], ['2', '0'], ['2'], ['abc', '10']]
    if directory.name == '04-workload':
        cases += [['1', '37', '1', '9'], ['4', '100', '1', '1'],
                  ['4', '100', '9', '1'], ['4', '20', '0', '1'],
                  ['4', '20', '1', '0']]
        invalid = [['4', '10', '0', '0'], ['0', '10', '1', '9'],
                   ['4', '10', '-1', '9'], ['4', '1000001', '1', '9'], ['x']]
    if directory.name == '09-compute-transactions':
        cases += [['1', '7', '11'], ['2', '16', '256'], ['4', '32', '1024'], ['8', '16', '64']]
        invalid = [['0', '10', '10'], ['2', '0', '10'], ['2', '10', '0'],
                   ['2', '10000', '1000000'], ['2']]
    report = []
    for number, args in enumerate(cases):
        expected = None
        for variant in variants:
            log = directory / '.build' / f'check-{number}-{variant}.log'
            status, text = run_program(directory / '.build' / variant, args, log)
            lines = [line for line in text.splitlines() if line.startswith('RESULT ')]
            if status or len(lines) != 1:
                raise RuntimeError(f'{variant}: exit_status={status}, results={lines}; see {log}')
            if expected is None:
                expected = lines[0]
            if lines[0] != expected:
                raise RuntimeError(f'{variant}: {lines[0]!r} != {expected!r}; see {log}')
            report.append({'version': variant, 'args': args, 'result': lines[0]})
    for number, args in enumerate(invalid):
        for variant in variants:
            log = directory / '.build' / f'invalid-{number}-{variant}.log'
            status, text = run_program(directory / '.build' / variant, args, log)
            if status != 1 or 'RESULT ' in text or not ('Usage:' in text or 'Invalid argument:' in text or 'weights' in text or 'must not exceed' in text):
                raise RuntimeError(f'{variant}: incorrect rejection of invalid input; see {log}')
            report.append({'version': variant, 'args': args, 'result': 'INVALID_INPUT_REJECTED'})
    (directory / '.build' / 'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'{directory.name}: {len(cases)*4} valid executions, {len(invalid)*4} input rejections; PASS')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        sys.exit(1)
