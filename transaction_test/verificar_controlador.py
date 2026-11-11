"""Production opt-in pragma: owning slot, whole-block retry and fatal cleanup."""
from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/controller'
out.mkdir(parents=True, exist_ok=True)
report = []

def run(label, args, expected=0):
    r = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=90)
    message = r.stdout + r.stderr
    (out / (label + '.log')).write_text(message)
    report.append({'case': label, 'passed': r.returncode == expected})
    if r.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    return message

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
                obj = out / (label + '.o')
                run(label + '-compile', flags + ['-x', language, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=2', '-g', '-fdump-tree-gimple', '-c', root / 'controller.c', '-o', obj])
                text = ''.join(p.read_text() for p in out.glob(label + '.*.gimple'))
                assert 'newtm_entry_bindings' in text and 'newtm_owner_slot' in text
                assert '__builtin_GOMP_transaction_stm_v1_after_failure' in text
                assert '__builtin_GOMP_transaction_begin (' not in text
                assert '__builtin_GOMP_transaction_register (' not in text
                wrap = ','.join('--wrap=GOMP_transaction_stm_v1_' + s for s in ('begin', 'write', 'end', 'after_failure'))
                binary = out / label
                run(label + '-link', flags + [obj, '-Wl,' + wrap, *libs, '-pthread', '-o', binary])
                for mode in range(6):
                    message = run(label + f'-run-{mode}', [binary, mode], expected=1 if mode >= 3 else 0)
                    executions += 1
                    if mode == 3:
                        assert 'controller failed without context' in message
                    elif mode >= 4:
                        assert 'invalid attempt' in message
                symbols = run(label + '-symbols', ['nm', binary])
                if linkage == 'dynamic':
                    assert 'GOMP_transaction_begin' not in symbols and 'GOMP_transaction_register' not in symbols
                    assert 'GOMP_transaction_stm_v1_fatal@GOMP_NEWTM_2.1' in symbols
                    assert 'GOMP_transaction_stm_v1_begin@GOMP_NEWTM_2.0' in symbols
                parallel = out / (label + '-parallel.o')
                run(label + '-parallel-compile', flags + ['-x', language, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=2', '-g', '-c', root / 'controller_parallel.c', '-o', parallel])
                binary = out / (label + '-parallel')
                run(label + '-parallel-link', flags + [parallel, *libs, '-pthread', '-o', binary])
                run(label + '-parallel-run', [binary]); executions += 1
                basic = out / (label + '-basic.o')
                run(label + '-basic-compile', flags + ['-x', language, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=2', '-g', '-c', root / 'controller_basic.c', '-o', basic])
                binary = out / (label + '-basic')
                run(label + '-basic-link', flags + [basic, *libs, '-pthread', '-o', binary])
                run(label + '-basic-run', [binary]); executions += 1
                if language == 'c++':
                    reference = out / (label + '-reference.o')
                    run(label + '-reference-compile', flags + ['-fopenmp', '-fopenmp-transaction-stm',
                        '-fchecking=2', '-g', '-c', root / 'controller_reference.C', '-o', reference])
                    binary = out / (label + '-reference')
                    run(label + '-reference-link', flags + [reference, *libs, '-pthread', '-o', binary])
                    run(label + '-reference-run', [binary]); executions += 1
    for opt in ('-O0', '-O2'):
        label = 'cleanup-' + opt[1:]
        flags = [gcc / 'xgcc', f'-B{gcc}/', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', opt,
                 f'-I{src / "include"}', f'-I{src / "libgomp"}']
        obj = out / (label + '.o')
        run(label + '-compile', flags + ['-fopenmp', '-fopenmp-transaction-stm', '-fchecking=2',
            '-c', root / 'controller_cleanup.c', '-o', obj])
        binary = out / label
        wraps = ['GOMP_transaction_stm_v1_begin', 'newtm_begin', 'newtm_hash_owner_release',
                 'GOMP_transaction_stm_v1_write', 'GOMP_transaction_stm_v1_after_failure', 'gomp_fatal']
        run(label + '-link', flags + [obj, '-Wl,' + ','.join('--wrap=' + w for w in wraps),
            gomp / 'libgomp.a', '-pthread', '-o', binary])
        for mode in (1, 2):
            run(label + f'-run-{mode}', [binary, mode]); executions += 1
    for name, languages, reason in (
        ('calls_refused.c', ('c', 'c++'), 'source-only'),
        ('calls_frame_refused.c', ('c',), 'prepared nested call requires a different lexical frame')):
        for language in languages:
            for opt in ('-O0', '-O2'):
                label = Path(name).stem + '-' + language.replace('+', 'p') + '-' + opt[1:]
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                    '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=2', opt,
                    f'-I{src / "include"}', '-c', root / name, '-o', out / (label + '.o')], expected=1)
                assert reason in message and 'internal compiler error' not in message, message
    for language in ('c', 'c++'):
        for opt in ('-O0', '-O2'):
            label = 'stale-' + language.replace('+', 'p') + '-' + opt[1:]
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-fopenmp',
                '-fopenmp-transaction-stm', '-fchecking=2', opt, '-c', root / 'controller_stale.c',
                '-o', out / (label + '.o')], expected=1)
            assert 'capture has no executable entry identity' in message and 'internal compiler error' not in message, message
        label = 'tree-controller-' + language.replace('+', 'p')
        run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-fopenmp', '-fopenmp-transaction-stm',
            '-fchecking=2', '-fdump-tree-gimple', '-c',
            src / 'gcc/testsuite/c-c++-common/gomp/transaction-controller-1.c', '-o', out / (label + '.o')])
        text = ''.join(p.read_text() for p in out.glob(label + '.*.gimple'))
        assert all('__builtin_GOMP_transaction_stm_v1_' + op in text for op in ('begin', 'end', 'after_failure', 'fatal'))
        assert '__builtin_GOMP_transaction_begin (' not in text
        label = 'default-' + language.replace('+', 'p')
        obj = out / (label + '.o')
        run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-fopenmp', '-fno-openmp-transaction-stm',
            '-c', root / 'controller_basic.c', '-o', obj])
        symbols = run(label + '-symbols', ['nm', '-u', obj])
        assert 'GOMP_transaction_begin' in symbols
    message = run('tree-frame', [gcc / 'xgcc', f'-B{gcc}/', '-fopenmp', '-fopenmp-transaction-stm',
        '-fchecking=2', '-c', src / 'gcc/testsuite/gcc.dg/gomp/transaction-controller-frame-1.c',
        '-o', out / 'tree-frame.o'], expected=1)
    assert 'prepared nested call requires a different lexical frame' in message and 'internal compiler error' not in message
    symbols = run('dso-symbols', ['objdump', '-T', gomp / 'libgomp.so'])
    assert any('GOMP_NEWTM_2.1' in line and line.split()[-1] == 'GOMP_transaction_stm_v1_fatal'
               for line in symbols.splitlines())
    versions = run('dso-versions', ['readelf', '--version-info', gomp / 'libgomp.so'])
    assert 'Parent 1: GOMP_NEWTM_2.0' in versions
    print(f'Controller: {len(report)} checks passed, {executions} executions (real pragma, C/C++, O0/O2, DSO/archive, LTO).')
finally:
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
