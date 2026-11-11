"""Closed effect coverage on the actual remapped frontend CFG."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/current-coverage'
out.mkdir(parents=True, exist_ok=True)


def run(label, args, expected=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=240)
    message = result.stdout + result.stderr
    (out / (label + '.log')).write_text(message)
    if result.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    return message


plugin = out / 'coverage_probe.so'
run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'coverage_probe_plugin.cc', '-o', plugin])
stdcpp = subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip()
checks = 1
executions = 0
negative = ('read', 'write', 'alias_read', 'alias_write', 'index', 'condition',
            'internal_argument', 'builtin', 'cfa', 'extra_status', 'clobber',
            'volatile', 'qualified', 'indirect', 'asm', 'duplicate_private_read',
            'duplicate_private_write', 'prepared_duplicate', 'prepared_original',
            'prepared_constructor', 'prepared_hidden_call', 'return')
entry_negative = tuple('entry_' + m for m in ('read', 'write', 'alias_read', 'alias_write',
                                             'index', 'internal_argument', 'builtin', 'cfa',
                                             'extra_status', 'clobber', 'volatile', 'qualified', 'asm', 'prefix'))
positive = ('scalar', 'address', 'arithmetic', 'entry_scalar', 'entry_address', 'entry_arithmetic', 'observe')


def refused(message):
    assert 'coverage baseline refused' not in message, message
    assert 'coverage probe did not mutate' not in message, message
    assert 'coverage mutation has unexpected result' not in message, message
    assert 'transaction transformed coverage proof failed:' in message, message
    assert 'CURRENT_COVERAGE_MUTATION refused' in message, message
    assert 'internal compiler error' not in message, message


def execute(label, obj, language):
    global checks, executions
    for linkage in ('dynamic', 'archive'):
        exe = out / (label + '-' + linkage)
        libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
                if linkage == 'dynamic' else [gomp / 'libgomp.a'])
        run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
            *libs, *([stdcpp] if language == 'c++' else []), '-pthread', '-o', exe])
        run(label + '-' + linkage + '-run', [exe]); checks += 2; executions += 1


for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for optimization in ('0', '2'):
            for mode in (*negative, *entry_negative, *positive):
                label = f'{mode}-{language.replace("+", "p")}-{checking}-O{optimization}'
                obj = out / (label + '.o')
                failed = mode in negative or mode in entry_negative
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                    '-O' + optimization, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=' + checking, '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0',
                    f'-fplugin={plugin}', '-fplugin-arg-coverage_probe-mode=' + mode,
                    '-c', root / 'current_coverage.c', '-o', obj], 1 if failed else 0)
                if failed:
                    refused(message)
                    if mode == 'prepared_constructor':
                        assert 'uncovered direct read' in message, message
                    if mode == 'prepared_hidden_call':
                        assert 'unclassified side effect in an operand' in message, message
                elif mode == 'observe':
                    assert 'CURRENT_COVERAGE dependent functions=' in message and 'dependencies=15' in message, message
                checks += 1
                if not failed:
                    execute(label, obj, language)
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for mode in ('entry_read', 'entry_write', 'entry_index', 'entry_extra_status', 'observe'):
            label = f'parallel-{mode}-{language.replace("+", "p")}-{checking}'
            obj = out / (label + '.o')
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O2',
                '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={plugin}', '-fplugin-arg-coverage_probe-mode=' + mode,
                '-c', root / 'current_coverage_parallel.c', '-o', obj], 0 if mode == 'observe' else 1)
            if mode != 'observe': refused(message)
            else: execute(label, obj, language)
            checks += 1
for source in ('controller_basic.c', 'memory_model.c', 'channels.c', 'current_status.c'):
    for language in ('c', 'c++'):
        for optimization in ('0', '2'):
            label = f'{Path(source).stem}-{language.replace("+", "p")}-O{optimization}'
            obj = out / (label + '.o')
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O' + optimization,
                '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=2',
                f'-fplugin={plugin}', '-fplugin-arg-coverage_probe-mode=observe',
                '-c', root / source, '-o', obj])
            assert 'CURRENT_COVERAGE dependent functions=' in message, message
            checks += 1; execute(label, obj, language)
for optimization in ('0', '2'):
    label = 'external-cpp-O' + optimization; obj = out / (label + '.o')
    message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-O' + optimization,
        '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=2', f'-fplugin={plugin}',
        '-c', root / 'channels_external.C', '-o', obj])
    assert 'CURRENT_COVERAGE dependent functions=' in message, message
    checks += 1; execute(label, obj, 'c++')
print(f'Cobertura atual: {checks} verificações, {executions} execuções; C/C++, O0/O2, checking 0/2, GC, DSO/archive, parallel e GNU.')
