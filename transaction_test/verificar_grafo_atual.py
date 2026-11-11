"""Atomic current frontend graphs, freshness, shared SCCs and revocation."""
from pathlib import Path
import subprocess
import sys

if not __debug__:
    raise SystemExit("Execute Python sem -O/-OO ou PYTHONOPTIMIZE")

section = sys.argv[2] if len(sys.argv) > 2 else "all"
if section not in ("all", "controller-checking0"):
    raise SystemExit("seção inválida")

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / ('.build/current-graph-checking0' if section != 'all' else '.build/current-graph')
out.mkdir(parents=True, exist_ok=True)


def run(label, args, expected=0):
    r = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=300)
    message = r.stdout + r.stderr
    (out / (label + '.log')).write_text(message)
    if r.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    assert 'internal compiler error' not in message, message
    return message


plugin = out / 'current_graph_probe.so'
run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'current_graph_probe_plugin.cc', '-o', plugin])
stdcpp = subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip()
checks = 1
executions = 0


def execute(label, obj, language):
    global checks, executions
    for linkage in ('dynamic', 'archive'):
        exe = out / (label + '-' + linkage)
        libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
                if linkage == 'dynamic' else [gomp / 'libgomp.a'])
        run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
            *libs, *([stdcpp] if language == 'c++' else []), '-pthread', '-o', exe])
        run(label + '-' + linkage + '-run', [exe]); checks += 2; executions += 1


if section == 'all':
    modes = ('observe', 'partial', 'operand', 'signature', 'layout', 'edge', 'notify', 'recheck', 'metadata', 'rebuild', 'failed')
    for language in ('c', 'c++'):
        for checking in ('0', '2'):
            for optimization in ('0', '2'):
                for mode in modes:
                    label = f'{mode}-{language.replace("+", "p")}-{checking}-O{optimization}'
                    obj = out / (label + '.o')
                    message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O' + optimization,
                        '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                        '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0',
                        f'-fplugin={plugin}', '-fplugin-arg-current_graph_probe-mode=' + mode,
                        '-c', root / 'current_graph.c', '-o', obj], 1 if mode == 'failed' else 0)
                    if mode in ('observe', 'partial'): assert 'CURRENT_GRAPH approved=3 dependencies=0' in message, message
                    else: assert 'CURRENT_GRAPH_' + mode + ' checked' in message, message
                    if mode == 'partial': assert 'CURRENT_GRAPH_PARTIAL no approval' in message, message
                    if mode != 'failed':
                        assert 'CURRENT_GRAPH_FRONTIER revoked' in message, message
                    checks += 1
                    if mode != 'failed': execute(label, obj, language)
    for source in ('capture_remap.c', 'controller_basic.c', 'current_coverage.c', 'current_status.c'):
        for language in ('c', 'c++'):
            for optimization in ('0', '2'):
                label = f'{Path(source).stem}-{language.replace("+", "p")}-O{optimization}'
                obj = out / (label + '.o')
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O' + optimization,
                    '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=2', f'-fplugin={plugin}',
                    '-c', root / source, '-o', obj])
                assert 'CURRENT_GRAPH approved=' in message, message
                assert 'CURRENT_GRAPH_FRONTIER revoked' in message, message
                checks += 1; execute(label, obj, language)
for mode, source in (('controller', 'current_graph.c'), ('producer', 'capture_remap.c')):
    for language in ('c', 'c++'):
        for checking in (('0',) if section != 'all' else ('0', '2')):
            for optimization in ('0', '2'):
                label = f'{mode}-{language.replace("+", "p")}-{checking}-O{optimization}'
                obj = out / (label + '.o')
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O' + optimization,
                    '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking, f'-fplugin={plugin}',
                    '-fplugin-arg-current_graph_probe-mode=' + mode,
                    '-c', root / source, '-o', obj])
                assert 'CURRENT_GRAPH_' + mode + ' checked' in message, message
                assert 'CURRENT_GRAPH_FRONTIER revoked' in message, message
                checks += 1; execute(label, obj, language)
if section == 'all':
    for optimization in ('0', '2'):
        label = 'external-cpp-O' + optimization; obj = out / (label + '.o')
        message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-O' + optimization,
            '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=2', f'-fplugin={plugin}',
            '-c', root / 'channels_external.C', '-o', obj])
        assert 'CURRENT_GRAPH approved=' in message, message
        checks += 1; execute(label, obj, 'c++')
    for language in ('c', 'c++'):
        label = 'lto-' + language.replace('+', 'p'); obj = out / (label + '.o')
        message = run(label + '-frontend', [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O2', '-flto',
            '-fopenmp', '-fopenmp-transaction-stm', f'-fplugin={plugin}', '-c', root / 'current_graph.c', '-o', obj])
        assert 'CURRENT_GRAPH approved=3' in message and 'CURRENT_GRAPH_FRONTIER revoked' in message, message
        linked = out / (label + '-linked.o')
        message = run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', '-flto', '-fuse-linker-plugin',
            f'-fplugin={plugin}', '-r', obj, '-o', linked])
        assert 'CURRENT_GRAPH_LTO reconstructed' in message, message
        checks += 2
if section != 'all':
    print(f'Controlador/produtor checking=0: {checks} verificações, {executions} execuções; C/C++, O0/O2, DSO/archive; invalidação e não ressurreição da aprovação.')
else:
    print(f'Grafo atual: {checks} verificações, {executions} execuções; composição, SCC, revogação, reconstrução, GC, C/C++, O0/O2, DSO/archive e fronteiras IPA/LTO.')
