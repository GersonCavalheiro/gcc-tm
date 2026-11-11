"""Prepared-call integration; descriptors are consumed before native lowering."""
from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/calls'
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
    plugin = out / 'call_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}',
        f'-I{src / "libcpp/include"}', root / 'call_probe_plugin.cc', '-o', plugin])
    executions = 0
    for language in ('c', 'c++'):
        for opt, lto in (('-O0', False), ('-O2', False), ('-O2', True)):
            for linkage in ('dynamic', 'archive'):
                label = f'{language.replace("+", "p")}-{opt[1:]}-lto{int(lto)}-{linkage}'
                flags = [gcc / 'xgcc', f'-B{gcc}/', '-Wall', '-Wextra', '-Werror',
                         '-UNDEBUG', opt, f'-I{src / "include"}', *(['-flto'] if lto else [])]
                obj = out / (label + '.o')
                run(label + '-compile', flags + ['-x', language, '-fopenmp', '-fchecking=2',
                    '-g', f'-fplugin={plugin}', '-fplugin-arg-call_probe-require',
                    '-fdump-tree-gimple', '-c', root / 'calls.c', '-o', obj])
                text = ''.join(p.read_text() for p in out.glob(label + '.*.gimple'))
                assert 'newtm_call_bindings' in text and 'newtm_return_binding' in text
                assert '__newtm_publish_result' not in text
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if linkage == 'dynamic'
                        else [gomp / 'libgomp.a'])
                if language == 'c++':
                    libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
                binary = out / label
                run(label + '-link', flags + [obj, '-Wl,--wrap=GOMP_transaction_stm_v1_write',
                    *libs, '-pthread', '-o', binary])
                run(label + '-run', [binary]); executions += 1
                symbols = run(label + '-symbols', ['nm', binary])
                assert '.newtm.v1' in symbols and 'invoke_' not in symbols and 'probe_call_body' not in symbols
                if linkage == 'dynamic':
                    assert 'GOMP_transaction_stm_v1_write@GOMP_NEWTM_2.0' in symbols
                    assert 'GOMP_transaction_stm_v1_status@GOMP_NEWTM_2.0' in symbols
    for name, languages, reason in (
        ('calls_refused.c', ('c', 'c++'), 'source-only'),
        ('calls_frame_refused.c', ('c',), 'prepared nested call requires a different lexical frame')):
        for language in languages:
            for opt in ('-O0', '-O2'):
                label = Path(name).stem + '-' + language.replace('+', 'p') + '-' + opt[1:]
                flags = [gcc / 'xgcc', f'-B{gcc}/', '-Wall', '-Wextra', '-Werror',
                         '-UNDEBUG', opt, f'-I{src / "include"}']
                obj = out / (label + '.o')
                compile_args = ['-x', language, '-fopenmp', '-fchecking=2', '-g',
                                f'-fplugin={plugin}', '-fplugin-arg-call_probe-refused',
                                '-c', root / name, '-o', obj]
                message = run(label + '-require', flags + compile_args
                              + ['-fplugin-arg-call_probe-require'], expected=1)
                assert reason in message and 'internal compiler error' not in message, message
                # Without activation certification, a refused body still fails
                # closed at runtime and preserves the caller's common objects.
                run(label + '-compile', flags + compile_args)
                libs = [gomp / 'libgomp.a']
                if language == 'c++':
                    libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
                binary = out / label
                run(label + '-link', flags + [obj, *libs, '-pthread', '-o', binary])
                run(label + '-run', [binary]); executions += 1
    for language in ('c', 'c++'):
        label = 'tree-call-' + language.replace('+', 'p')
        run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-fopenmp',
            '-fchecking=2', '-g', '-fdump-tree-gimple', '-c',
            src / 'gcc/testsuite/c-c++-common/gomp/transaction-call-1.c',
            '-o', out / (label + '.o')])
        text = ''.join(p.read_text() for p in out.glob(label + '.*.gimple'))
        assert all(name in text for name in ('newtm_call_bindings', 'newtm_return_binding',
                   '__builtin_GOMP_transaction_stm_v1_read', '__builtin_GOMP_transaction_stm_v1_write'))
        assert '__newtm_publish_result' not in text
    print(f'Calls: {len(report)} checks passed, {executions} executions (C/C++, O0/O2, DSO/archive, LTO, refusals).')
finally:
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
