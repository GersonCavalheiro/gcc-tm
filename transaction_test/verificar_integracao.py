"""Task 5.6: end-to-end fault coverage beyond the controller smoke matrix."""
from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/integration'
out.mkdir(parents=True, exist_ok=True)
report = []
coverage = []
passed = False

def run(label, args, expected=0):
    try:
        r = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=90)
    except subprocess.TimeoutExpired as error:
        report.append({'case': label, 'passed': False, 'timeout': True})
        raise RuntimeError(label + ': timeout') from error
    message = r.stdout + r.stderr
    (out / (label + '.log')).write_text(message)
    report.append({'case': label, 'passed': r.returncode == expected})
    if r.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    return message

def traces(message):
    return [(parts[1], list(map(int, parts[2:]))) for line in message.splitlines()
            if line.startswith('TRACE ') for parts in [line.split()]]

try:
    executions = 0
    for language in ('c', 'c++'):
        for opt, lto in (('-O0', False), ('-O2', False), ('-O2', True)):
            for linkage in ('dynamic', 'archive'):
                label = f'{language.replace("+", "p")}-{opt[1:]}-lto{int(lto)}-{linkage}'
                flags = [gcc / 'xgcc', f'-B{gcc}/', '-Wall', '-Wextra', '-Werror', '-UNDEBUG',
                         opt, f'-I{src / "include"}', *(['-flto'] if lto else [])]
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if linkage == 'dynamic'
                        else [gomp / 'libgomp.a'])
                if language == 'c++':
                    libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
                compile_flags = ['-x', language, '-fopenmp', '-fopenmp-transaction-stm',
                                 '-fchecking=2', '-g', '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0']
                obj = out / (label + '.o')
                run(label + '-compile', flags + compile_flags + ['-fdump-tree-gimple', '-c', root / 'integration.c', '-o', obj])
                operations = ('begin', 'register', 'read', 'write', 'status', 'nest_begin', 'nest_end', 'end', 'after_failure')
                binary = out / label
                run(label + '-link', flags + [obj, '-Wl,' + ','.join('--wrap=GOMP_transaction_stm_v1_' + s for s in operations),
                    *libs, '-pthread', '-o', binary])
                message = run(label + '-baseline', [binary]); executions += 1
                baseline = traces(message)
                assert len(baseline) == 1 and baseline[0][0] == 'commit', message
                sequence = baseline[0][1]
                assert set(sequence) == set(range(1, 9)), sequence
                # Every emitted operation position: two consecutive conflicts,
                # exact prefix replay, cleanup, then a complete third attempt.
                for target, operation in enumerate(sequence, 1):
                    message = run(label + f'-conflict-{target}', [binary, target, 1], expected=1 if operation == 1 else 0)
                    executions += 1
                    if operation == 1:
                        assert 'controller failed without context' in message and not traces(message)
                    else:
                        assert traces(message) == [('discard', sequence[:target]), ('discard', sequence[:target]), ('commit', sequence)], message
                # Permanent and unknown causes at the first/last position of
                # each service kind: no commit and no retry after the failure.
                for operation in range(1, 9):
                    positions = [i + 1 for i, op in enumerate(sequence) if op == operation]
                    for target in sorted({positions[0], positions[-1]}):
                        for cause in (2, 77):
                            message = run(label + f'-invalid-{target}-{cause}', [binary, target, cause], expected=1)
                            executions += 1
                            if operation == 1:
                                assert 'controller failed without context' in message and not traces(message)
                            else:
                                assert 'invalid attempt' in message
                                assert traces(message) == [('discard', sequence[:target])], message
                coverage.append({'configuration': label, 'events': len(sequence), 'operations': sorted(set(sequence))})
                deps = run(label + '-dependencies', ['readelf', '-d', binary])
                assert ('Shared library: [libgomp.so.1]' in deps) == (linkage == 'dynamic')
                fixtures = [('views', 'integration_views.c', ('read', 'end', 'after_failure')),
                            ('parallel', 'integration_parallel.c', ()),
                            ('control', 'integration_control.c', ('write', 'after_failure'))]
                if language == 'c':
                    fixtures.append(('aggregate', 'integration_aggregate.c', ('write', 'end', 'after_failure')))
                for suffix, name, wraps in fixtures:
                    obj = out / (label + '-' + suffix + '.o')
                    run(label + '-' + suffix + '-compile', flags + compile_flags + ['-c', root / name, '-o', obj])
                    exe = out / (label + '-' + suffix)
                    wrap_flags = ['-Wl,' + ','.join('--wrap=GOMP_transaction_stm_v1_' + s for s in wraps)] if wraps else []
                    run(label + '-' + suffix + '-link', flags + [obj, *wrap_flags,
                        *libs, '-pthread', '-o', exe])
                    run(label + '-' + suffix + '-run', [exe]); executions += 1
                obj = out / (label + '-bounds.o')
                run(label + '-bounds-compile', flags + compile_flags + ['-c', root / 'integration_bounds.c', '-o', obj])
                exe = out / (label + '-bounds')
                run(label + '-bounds-link', flags + [obj,
                    '-Wl,--wrap=GOMP_transaction_stm_v1_read,--wrap=GOMP_transaction_stm_v1_after_failure',
                    *libs, '-pthread', '-o', exe])
                for index in ('0', '1', '-1', '2', '2147483647', '-2147483648',
                              '9223372036854775807', '-9223372036854775808'):
                    invalid = index not in ('0', '1')
                    message = run(label + '-bounds-' + index, [exe, index], expected=int(invalid)); executions += 1
                    if invalid:
                        assert 'BOUNDS invalid before publication' in message and 'invalid attempt' in message
    for language in ('c', 'c++'):
        for opt in ('-O0', '-O2'):
            label = 'global-pointer-refused-' + language.replace('+', 'p') + '-' + opt[1:]
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-fopenmp',
                '-fopenmp-transaction-stm', '-fchecking=2', opt, '-c',
                root / 'integration_global_pointer_refused.c', '-o', out / (label + '.o')], expected=1)
            assert 'transaction body cannot be prepared' in message and 'internal compiler error' not in message, message
    passed = True
    print(f'Integration: {len(report)} checks passed, {executions} executions (real pragma, all operation positions, double retry, private views, bounds, GC, C/C++, O0/O2, LTO, DSO/archive).')
finally:
    (out / 'report.json').write_text(json.dumps({'passed': passed, 'executions': executions,
        'checks': report, 'coverage': coverage}, indent=2) + '\n')
