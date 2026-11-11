"""Stage 5.1: preparation and refusal; no STM memory instrumentation claim."""
from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
src = root.parent / 'gcc-src-tm'
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/preparation'
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
    plugin = out / 'prepare_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}',
        f'-I{src / "libcpp/include"}', root / 'prepare_probe_plugin.cc', '-o', plugin])
    for language in ('c', 'c++'):
        for opt in ('-O0', '-O2'):
            for linkage in ('dynamic', 'archive'):
                label = f'{language.replace("+", "p")}-{opt[1:]}-{linkage}'
                obj = out / (label + '.o')
                flags = [gcc / 'xgcc', f'-B{gcc}/', '-Wall', '-Wextra', '-Werror',
                         '-UNDEBUG', opt, f'-I{src / "include"}']
                run(label + '-compile', flags + ['-x', language, '-fopenmp', '-fchecking=2',
                    '-g', f'-fplugin={plugin}', '-fdump-tree-gimple', '-c', root / 'prepare.c', '-o', obj])
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if linkage == 'dynamic'
                        else [gomp / 'libgomp.a', '-pthread'])
                if language == 'c++':
                    libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
                binary = out / label
                run(label + '-link', flags + [obj, *libs, '-o', binary])
                run(label + '-run', [binary])
                symbols = run(label + '-symbols', ['nm', binary])
                assert '.newtm.body.' in symbols and '__newtm_publish_result' not in symbols
            diagnostic = run(f'{language.replace("+", "p")}-{opt[1:]}-refusal',
                flags + ['-x', language, '-fopenmp', '-fchecking=2', f'-fplugin={plugin}',
                    '-fplugin-arg-prepare_probe-require', '-c', root / 'prepare.c', '-o', out / 'refused.o'], 1)
            assert 'transaction body cannot be prepared' in diagnostic
            assert 'internal compiler error' not in diagnostic
    print(f'Preparation: {len(report)} checks passed, 8 executions (C/C++, O0/O2, DSO/archive).')
finally:
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
