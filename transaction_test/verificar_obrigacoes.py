"""Step 6.2 inventory, derivation and lifetime; no coverage certification."""
from pathlib import Path
import json
import re
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/obligations'
out.mkdir(parents=True, exist_ok=True)
report, summaries = [], []
executions, passed = 0, False

def run(label, args):
    r = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=120)
    message = r.stdout + r.stderr
    (out / (label + '.log')).write_text(message)
    report.append({'case': label, 'passed': r.returncode == 0})
    if r.returncode:
        raise RuntimeError(label + '\n' + message)
    return message

def summary(label, message, functional, expect_split):
    found = re.search(r'OBLIGATION (.*)', message)
    assert found, message
    data = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', found[1])}
    assert data['kinds'] == 127 and data['source'] and data['unbound'], data
    if expect_split:
        assert data['split'], data
    assert data['reconciled'], data
    assert bool(data['delegated']) == functional, data
    if not functional:
        assert data['rejected'], data
    summaries.append({'case': label, **data})

try:
    plugin = out / 'obligation_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
        root / 'obligation_probe_plugin.cc', '-o', plugin])
    for language in ('c', 'c++'):
        for opt, lto, checking in (('-O0', False, '2'), ('-O2', False, '2'),
                                    ('-O2', True, '2'), ('-O2', False, '0')):
            for linkage in ('dynamic', 'archive'):
                label = f'{language.replace("+", "p")}-{opt[1:]}-lto{int(lto)}-{linkage}'
                if checking == '0':
                    label += '-checking0'
                flags = [gcc / 'xgcc', f'-B{gcc}/', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', opt,
                         *(['-flto'] if lto else [])]
                obj = out / (label + '.o')
                message = run(label + '-compile', flags + ['-x', language, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=' + checking, '-g', '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0',
                    f'-fplugin={plugin}', '-c', root / 'obligations.c', '-o', obj])
                summary(label, message, True, language == 'c')
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if linkage == 'dynamic'
                        else [gomp / 'libgomp.a'])
                if language == 'c++':
                    libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
                binary = out / label
                run(label + '-link', flags + [obj, *libs, '-pthread', '-o', binary])
                run(label + '-run', [binary]); executions += 1
                symbols = run(label + '-symbols', ['nm', '-u', binary])
                assert 'newtm_obligation_' not in symbols
        for opt in ('-O0', '-O2'):
            for checking in ('0', '2'):
                label = 'refused-' + language.replace('+', 'p') + '-' + opt[1:] + '-checking' + checking
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, opt, '-fopenmp',
                    '-fno-openmp-transaction-stm', '-DREFUSED_CASE', '-fchecking=' + checking,
                    '-g', f'-fplugin={plugin}', '-c', root / 'obligations.c', '-o', out / (label + '.o')])
                summary(label, message, False, language == 'c')
    passed = True
    print(f'Obligations: {len(report)} checks passed, {executions} executions (O01-O07, derivation, copy, invalidation, refusal, GC, C/C++, O0/O2, LTO, DSO/archive).')
finally:
    (out / 'report.json').write_text(json.dumps({'passed': passed, 'checks': report,
        'executions': executions, 'summaries': summaries}, indent=2) + '\n')
