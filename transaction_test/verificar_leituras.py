"""Generated STM reads in dormant prepared functions, before production activation."""
from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
src = root.parent / 'gcc-src-tm'
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/reads'
out.mkdir(parents=True, exist_ok=True)
report = []

def run(label, args):
    r = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=90)
    message = r.stdout + r.stderr
    (out / (label + '.log')).write_text(message)
    report.append({'case': label, 'passed': r.returncode == 0})
    if r.returncode:
        raise RuntimeError(label + '\n' + message)
    return message

try:
    plugin = out / 'read_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}',
        f'-I{src / "libcpp/include"}', root / 'read_probe_plugin.cc', '-o', plugin])
    executions = 0
    for language in ('c', 'c++'):
        for opt, lto in (('-O0', False), ('-O2', False), ('-O2', True)):
            for linkage in ('dynamic', 'archive'):
                label = f'{language.replace("+", "p")}-{opt[1:]}-lto{int(lto)}-{linkage}'
                flags = [gcc / 'xgcc', f'-B{gcc}/', '-Wall', '-Wextra', '-Werror',
                         '-UNDEBUG', opt, f'-I{src / "include"}', *(['-flto'] if lto else [])]
                obj = out / (label + '.o')
                run(label + '-compile', flags + ['-x', language, '-fopenmp', '-fchecking=2',
                    '-g', f'-fplugin={plugin}', '-fdump-tree-gimple', '-c', root / 'reads.c', '-o', obj])
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if linkage == 'dynamic'
                        else [gomp / 'libgomp.a'])
                if language == 'c++':
                    libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
                binary = out / label
                run(label + '-link', flags + [obj, '-Wl,--wrap=GOMP_transaction_stm_v1_read',
                    *libs, '-pthread', '-o', binary])
                run(label + '-run', [binary]); executions += 1
                symbols = run(label + '-symbols', ['nm', binary])
                assert '.newtm.v1' in symbols and '__newtm_publish_result' not in symbols
                if linkage == 'dynamic':
                    assert 'GOMP_transaction_stm_v1_read@GOMP_NEWTM_2.0' in symbols
    for name, languages in [
        ('c-c++-common/gomp/transaction-read-1.c', ('c', 'c++')),
        ('g++.dg/gomp/transaction-read-private-1.C', ('c++',))]:
        for language in languages:
            label = 'tree-' + Path(name).stem + '-' + language.replace('+', 'p')
            run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-fopenmp',
                '-fchecking=2', '-g', '-fdump-tree-gimple', '-c', src / 'gcc/testsuite' / name,
                '-o', out / (label + '.o')])
            text = ''.join(p.read_text() for p in out.glob(label + '.*.gimple'))
            count = text.count('__builtin_GOMP_transaction_stm_v1_read (')
            assert count == (0 if 'private' in name else 2)
            assert '__newtm_publish_result' not in text
    print(f'Reads: {len(report)} checks passed, {executions} executions (C/C++, O0/O2, DSO/archive, LTO).')
finally:
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
