"""Step 6.4.1: read-only High GIMPLE flow and finally continuations."""
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
out = root / '.build/flow'
out.mkdir(parents=True, exist_ok=True)
report, summaries = [], []
executions, passed = 0, False

def run(label, args):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=120)
    message = result.stdout + result.stderr
    (out / (label + '.log')).write_text(message)
    report.append({'case': label, 'passed': result.returncode == 0})
    if result.returncode:
        raise RuntimeError(label + '\n' + message)
    return message

def summary(label, message):
    found = re.search(r'FLOW scenarios=(\d+) assertions=(\d+) production_nodes=(\d+)', message)
    assert found and int(found[1]) >= 28 and int(found[2]) and int(found[3]), message
    summaries.append({'case': label, 'scenarios': int(found[1]), 'assertions': int(found[2]),
                      'production_nodes': int(found[3])})

try:
    plugin = out / 'flow_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
        root / 'flow_probe_plugin.cc', '-o', plugin])
    for language in ('c', 'c++'):
        for opt, lto, checking in (('-O0', False, '2'), ('-O2', False, '2'),
                                  ('-O2', True, '2'), ('-O2', False, '0')):
            for linkage in ('dynamic', 'archive'):
                label = f'{language.replace("+", "p")}-{opt[1:]}-lto{int(lto)}-{linkage}-checking{checking}'
                flags = [gcc / 'xgcc', f'-B{gcc}/', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', opt,
                         *(['-flto'] if lto else [])]
                obj = out / (label + '.o')
                message = run(label + '-compile', flags + ['-x', language, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=' + checking, '-g', '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0',
                    f'-fplugin={plugin}', '-c', root / 'flow.c', '-o', obj])
                summary(label, message)
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if linkage == 'dynamic'
                        else [gomp / 'libgomp.a'])
                if language == 'c++':
                    libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
                binary = out / label
                run(label + '-link', flags + [obj, *libs, '-pthread', '-o', binary])
                run(label + '-run', [binary]); executions += 1
                symbols = run(label + '-symbols', ['nm', '-u', binary])
                assert 'newtm_flow_' not in symbols and 'newtm_obligation_' not in symbols
        for opt in ('-O0', '-O2'):
            for checking in ('0', '2'):
                label = 'tracing-' + language.replace('+', 'p') + '-' + opt[1:] + '-checking' + checking
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, opt, '-fopenmp',
                    '-fno-openmp-transaction-stm', '-DREFUSED_CASE', '-fchecking=' + checking,
                    '-g', f'-fplugin={plugin}', '-c', root / 'obligations.c', '-o', out / (label + '.o')])
                summary(label, message)
    passed = True
    print(f'Flow: {len(report)} checks passed, {executions} executions; '
          f'{sum(s["scenarios"] for s in summaries)} synthetic scenarios '
          '(finally continuations, loops, switches, returns, noreturn, refusal, immutability, GC, checking 0/2).')
finally:
    (out / 'report.json').write_text(json.dumps({'passed': passed, 'checks': report,
        'executions': executions, 'summaries': summaries}, indent=2) + '\n')
