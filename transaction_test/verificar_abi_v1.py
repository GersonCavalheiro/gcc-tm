"""Validate STM v1 DSO/archive contracts and compiler-built calls, not pragma STM."""
from pathlib import Path
import json
import re
import subprocess
import sys

root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
src = root.parent / 'gcc-src-tm'
out = root / '.build' / 'abi-v1'
out.mkdir(parents=True, exist_ok=True)
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
base = [str(gcc / 'xgcc'), f'-B{gcc}/', '-Wall', '-Wextra', '-Werror',
        f'-I{src / "include"}', '-UNDEBUG', '-pthread']
report = []

def run(label, command):
    result = subprocess.run(list(map(str, command)), text=True, capture_output=True, timeout=90)
    (out / (label + '.log')).write_text(result.stdout + result.stderr)
    report.append({'case': label, 'passed': result.returncode == 0})
    if result.returncode:
        print(result.stdout + result.stderr)
        raise RuntimeError(label)
    return result.stdout

try:
    run('plugin-build', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
                        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}',
                        f'-I{src / "libcpp/include"}', root / 'abi_v1_plugin.cc',
                        '-o', out / 'probe.so'])
    symbols = run('symbols', ['objdump', '-T', gomp / 'libgomp.so'])
    operations = ['begin', 'register', 'read', 'write', 'end', 'fail', 'discard', 'after_failure', 'nest_begin', 'nest_end', 'status']
    for op in operations:
        assert any('GOMP_NEWTM_2.0' in line and line.split()[-1] == 'GOMP_transaction_stm_v1_' + op
                   for line in symbols.splitlines()), op
    for op in ['begin', 'register', 'end']:
        assert any('GOMP_NEWTM_1.0' in line and line.split()[-1] == 'GOMP_transaction_' + op
                   for line in symbols.splitlines()), op
    assert not any(line.split() and line.split()[-1].startswith('newtm_') for line in symbols.splitlines())
    versions = run('versions', ['readelf', '--version-info', gomp / 'libgomp.so'])
    assert 'Parent 1: GOMP_NEWTM_1.0' in versions
    for language in ['c', 'c++']:
        for opt in ['-O0', '-O2']:
            for emitter in ['header', 'builder']:
                for linkage in ['dynamic', 'archive']:
                    label = f'{language.replace("+", "p")}-{opt[1:]}-{linkage}-{emitter}'
                    obj = out / (label + '.o')
                    plugin_flags = [f'-fplugin={out / "probe.so"}'] if emitter == 'builder' else []
                    run(label + '-compile', base + plugin_flags + ['-x', language, '-fopenmp', '-fchecking=2',
                        opt, '-fdump-tree-omplower',
                        '-c', root / 'abi_v1.c', '-o', obj])
                    executable = out / label
                    libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if linkage == 'dynamic'
                            else [gomp / 'libgomp.a', '-pthread'])
                    run(label + '-link', base + [obj] + libs + ['-o', executable])
                    run(label + '-run', [executable])
                    deps = run(label + '-dependencies', ['readelf', '-d', executable])
                    assert ('Shared library: [libgomp.so.1]' in deps) == (linkage == 'dynamic')
                    undefined = run(label + '-undefined', ['nm', '-u', executable])
                    if linkage == 'dynamic':
                        for op in operations:
                            assert 'GOMP_transaction_stm_v1_' + op + '@GOMP_NEWTM_2.0' in undefined
    fault = out / 'faults'
    wraps = ['newtm_begin', 'newtm_read', 'newtm_hash_owner_release', 'gomp_fatal']
    run('faults-build', base + ['-O2', f'-I{src / "libgomp"}', root / 'abi_v1_faults.c',
        '-Wl,' + ','.join('--wrap=' + w for w in wraps), gomp / 'libgomp.a',
        '-pthread', '-o', fault])
    run('faults-entry-translation', [fault])
    run('faults-fatal-cleanup', [fault, 'fatal'])
    # Confirm no activation: existing pragma still calls only the old bridge.
    trace = out / 'trace.c'
    trace.write_text('int f(void){int a=0;\n#pragma omp transaction(a)\n{a++;}\nreturn a;}\n')
    run('trace-unchanged', base + ['-fopenmp', '-O0', '-fdump-tree-gimple', '-c', trace, '-o', out / 'trace.o'])
    trace_symbols = run('trace-symbols', ['nm', '-u', out / 'trace.o'])
    assert 'GOMP_transaction_begin' in trace_symbols
    trace_dump = ''.join(p.read_text() for p in out.glob('trace.c.*.gimple'))
    ordinary = re.sub(r'(?ms)^[^\n]*\.newtm\.v1 \([^\n]*\)\n\{.*?^\}\n', '', trace_dump)
    assert ordinary and 'GOMP_transaction_begin' in ordinary
    assert 'GOMP_transaction_stm_v1_' not in ordinary and '.newtm.v1 (' not in ordinary
    print(f'ABI v1: {len(report)} checks passed (C/C++, O0/O2, DSO/archive, fault injection).')
finally:
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
