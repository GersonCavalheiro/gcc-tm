"""Reconstruct loaded transaction graphs; never transport approval."""
from pathlib import Path
import re
import subprocess
import sys

if not __debug__:
    raise SystemExit("Execute Python sem -O/-OO ou PYTHONOPTIMIZE")

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
checking_filter = sys.argv[3] if len(sys.argv) > 3 else None
if checking_filter not in (None, '0', '2'):
    raise SystemExit('checking deve ser 0 ou 2')
checking_levels = (checking_filter,) if checking_filter else ('0', '2')
out = root / ('.build/lto-graph-checking' + checking_filter if checking_filter else '.build/lto-graph')
out.mkdir(parents=True, exist_ok=True)
checks = executions = 0
section = sys.argv[2] if len(sys.argv) > 2 else "all"
if section not in ("all", "positive", "negative", "fixtures", "units", "format", "templates"):
    raise SystemExit("seção inválida")


def run(label, args, failure=False):
    global checks
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=300)
    message = result.stdout + result.stderr
    (out / (label + '.log')).write_text(message)
    assert 'internal compiler error' not in message, label + '\n' + message
    assert (result.returncode != 0) == failure, label + '\n' + message
    checks += 1
    return message


plugin = out / ('plugin-' + section) / 'lto_graph_probe.so'
plugin.parent.mkdir(parents=True, exist_ok=True)
run('plugin-' + section, ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'lto_graph_probe_plugin.cc', '-o', plugin])
stdcpp = subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip()


def probe(mode, level):
    return [f'-fplugin={plugin}', '-fplugin-arg-lto_graph_probe-mode=' + mode,
            '-fplugin-arg-lto_graph_probe-level=' + level]


def compile_source(label, source, language, level, checking='2', mode='observe', extra=()):
    obj = out / (label + '.o')
    msg = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O' + level,
        '-flto', '-fno-fat-lto-objects', '-fopenmp', '-fopenmp-transaction-stm',
        '-fchecking=' + checking, *probe(mode, level), *extra,
        '-c', root / source, '-o', obj])
    if mode not in ('observe', 'rebuild', 'revoke'):
        assert 'LTO_MUTATION ' + mode + ' serialized' in msg, msg
    return obj


def link(label, objects, language, level, linkage='dynamic', mode='observe',
         extra=(), failure=False, roots=None, checking='2'):
    global executions
    exe = out / label
    libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
            if linkage == 'dynamic' else [gomp / 'libgomp.a'])
    msg = run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', '-O' + level,
        '-flto', '-fchecking=' + checking, *probe(mode, level), *extra, *objects,
        *libs, *([stdcpp] if language == 'c++' else []), '-pthread', '-o', exe], failure)
    if failure:
        assert re.search(r'error: transaction (?:LTO|transformed|current)', msg), msg
        return msg
    approved = re.findall(r'LTO_GRAPH (WPA|LTRANS|MONOLITHIC) approved=(\d+)', msg)
    if roots == 0:
        assert approved and all(int(count) == 0 for _, count in approved), msg
    elif '-flto-partition=none' in extra:
        assert any(phase == 'MONOLITHIC' and int(count) for phase, count in approved), msg
    else:
        for phase in ('WPA', 'LTRANS'):
            counts = [int(count) for p, count in approved if p == phase]
            assert counts and sum(counts) == roots if roots is not None else counts and sum(counts), msg
    assert 'LTO_FRONTIER revoked' in msg, msg
    if mode == 'rebuild': assert 'LTO_REBUILD new generation' in msg, msg
    if mode == 'revoke': assert 'LTO_REVOKE irreversible generation' in msg, msg
    run(label + '-run', [exe]); executions += 1
    return msg


if section in ("all", "positive"):
    for language in ('c', 'c++'):
        for level in ('0', '2'):
            for checking in checking_levels:
                label = f'graph-{language.replace("+", "p")}-{checking}-O{level}'
                obj = compile_source(label, 'lto_graph.c', language, level, checking,
                    extra=('--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0'))
                for mode in ('observe', 'rebuild', 'revoke'):
                    for linkage in ('dynamic', 'archive'):
                        link(label + '-' + mode + '-' + linkage, [obj], language, level, linkage, mode, roots=3, checking=checking)

if section in ("all", "negative"):
    negative = ('version', 'mode', 'envelope', 'map', 'roots', 'component', 'missing', 'layout', 'raw', 'guard',
                'ssa_use', 'phi', 'context', 'status', 'owner', 'channel', 'entry')
    for language in ('c', 'c++'):
        for level in ('0', '2'):
            for checking in checking_levels:
                for mode in negative:
                    label = f'{mode}-{language.replace("+", "p")}-{checking}-O{level}'
                    obj = compile_source(label, 'lto_graph.c', language, level, checking=checking, mode=mode)
                    link(label, [obj], language, level, failure=True, checking=checking)

if section in ("all", "fixtures"):
    for source in ('capture_remap.c', 'controller_basic.c', 'current_coverage.c',
                   'current_status.c', 'memory_model.c', 'channels_external.C'):
        for language in ('c', 'c++'):
            if source.endswith('.C') and language == 'c': continue
            for level in ('0', '2'):
                label = f'{Path(source).stem}-{language.replace("+", "p")}-O{level}'
                obj = compile_source(label, source, language, level)
                for linkage in ('dynamic', 'archive'):
                    link(label + '-' + linkage, [obj], language, level, linkage)

if section in ("all", "units"):
    for language in ('c', 'c++'):
        label = 'units-' + language.replace('+', 'p')
        main = compile_source(label + '-main', 'lto_graph.c', language, '2', extra=('-DNEWTM_OTHER_UNIT',))
        other = compile_source(label + '-other', 'lto_graph_other.c', language, '2')
        for partition in ('max', '1to1', 'balanced', 'none'):
            for linkage in ('dynamic', 'archive'):
                link(label + '-' + partition + '-' + linkage, [main, other], language, '2', linkage,
                     extra=('-flto-partition=' + partition,), roots=4)
        merged = out / (label + '-merged.o')
        msg = run(label + '-incremental', [gcc / 'xgcc', f'-B{gcc}/', '-flto', '-r', '-flinker-output=rel',
            *probe('observe', '2'), main, other, '-o', merged])
        assert 'LTO_GRAPH MONOLITHIC approved=4' in msg, msg
        for linkage in ('dynamic', 'archive'):
            link(label + '-incremental-' + linkage, [merged], language, '2', linkage, roots=4)

if section in ("all", "format"):
    obj = compile_source('format-native', 'lto_graph.c', 'c', '2', extra=('-ffat-lto-objects',))
    # Native fallback must also retain the requested function options.
    native = out / 'format-native'
    run('fat-native-link', [gcc / 'xgcc', f'-B{gcc}/', '-fno-lto', obj,
        f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp', '-o', native])
    run('fat-native-run', [native]); executions += 1
    link('fat-lto', [obj], 'c', '2', roots=3)
    for language in ('c', 'c++'):
        label = 'unused-' + language.replace('+', 'p')
        unused = compile_source(label, 'lto_graph_unused.c', language, '2')
        for linkage in ('dynamic', 'archive'):
            link(label + '-' + linkage, [unused], language, '2', linkage, roots=0)
    sections = run('format-sections', ['readelf', '-SW', obj])
    name = re.search(r'\.gnu\.lto_\.lto\.[0-9a-f]+', sections).group(0)
    header = out / 'format-header.bin'
    run('format-dump', ['objcopy', '--dump-section', name + '=' + str(header), obj])
    raw = bytearray(header.read_bytes())
    assert int.from_bytes(raw[2:4], sys.byteorder) == 1, raw
    raw[2:4] = (0).to_bytes(2, sys.byteorder)
    header.write_bytes(raw)
    legacy = out / 'format-legacy.o'
    run('format-patch', ['objcopy', '--update-section', name + '=' + str(header), obj, legacy])
    msg = run('format-refused', [gcc / 'xgcc', f'-B{gcc}/', '-flto', '-r', '-flinker-output=rel',
        legacy, '-o', out / 'format-refused.o'], failure=True)
    assert 'version' in msg and '16.0' in msg and '16.1' in msg, msg

if section in ("all", "templates"):
    for level in ('0', '2'):
        label = 'template-owner-O' + level
        obj = compile_source(label, 'lto_graph_template.C', 'c++', level)
        for linkage in ('dynamic', 'archive'):
            link(label + '-' + linkage, [obj], 'c++', level, linkage,
                 extra=('-flto-partition=max',), roots=1)

print(f'Grafo LTO [{section}, checking={checking_filter or "0/2"} nas matrizes positive/negative]: {checks} verificações, {executions} execuções; somente os casos da seção selecionada foram executados.')
