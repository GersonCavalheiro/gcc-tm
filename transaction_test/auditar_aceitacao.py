"""Directed section-9 and integrated-refusal audit; never grants acceptance."""
from pathlib import Path
import json
import re
import subprocess
import sys
import hashlib

if not __debug__:
    raise SystemExit('Asserções Python precisam estar habilitadas')
root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
src = root.parent / 'gcc-src-tm'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/acceptance-audit'
out.mkdir(parents=True, exist_ok=True)
commands = []
negatives = []
positives = []
baseline_passed = False
passed = False


def run(label, args, failure=False):
    args = list(map(str, args))
    result = subprocess.run(args, capture_output=True, text=True, timeout=180)
    text = result.stdout + result.stderr
    path = out / (label + '.log')
    path.write_text(text)
    commands.append({'case': label, 'command': args, 'returncode': result.returncode,
                     'log': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    assert 'internal compiler error' not in text, text
    assert (result.returncode != 0) == failure, text
    return text


# Expected detector, rather than merely any compiler error. The mode name
# is deliberately not treated as proof of its nominal verification rule.
detectors = {
    'version': r'LTO contract version or envelope is invalid',
    'mode': r'LTO manual ABI entry cannot publish graph roots',
    'envelope': r'LTO contract section is invalid',
    'map': r'LTO contract map is invalid',
    'roots': r'LTO current entry has no graph root contract',
    'component': r'transformed memory proof failed: missing live-window memory contract',
    'missing': r'LTO prepared entry has an unprotected caller',
    'layout': r'LTO signature has an incompatible ABI layout',
    'raw': r'transformed coverage proof failed: uncovered direct read',
    'guard': r'transformed channel proof failed: result value/binding lost its reaching producer',
    'ssa_use': r'LTO SSA correspondence cannot be reconstructed',
    'phi': r'LTO SSA correspondence cannot be reconstructed',
    'context': r'transformed call lost its borrowed context',
    'status': r'transformed status proof failed: V07: epilogue loses or substitutes the originating operation status',
    'owner': r'transformed owner proof failed: V07: controller service consumes a different or copied owning slot',
    'channel': r'transformed channel proof failed: channel arguments, slots or reaching producers changed',
    'entry': r'LTO controller lost its prepared invocation',
}
try:
    plugin = out / 'lto_graph_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
        root / 'lto_graph_probe_plugin.cc', '-o', plugin])
    stdcpp = subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip()
    flags = [gcc / 'xgcc', f'-B{gcc}/', '-O0', '-flto', '-fchecking=0',
             f'-fplugin={plugin}', '-fplugin-arg-lto_graph_probe-mode=observe',
             '-fplugin-arg-lto_graph_probe-level=0']
    obj = out / 'baseline.o'
    run('baseline-compile', [*flags, '-fopenmp', '-fopenmp-transaction-stm',
        '-c', root / 'lto_graph.c', '-o', obj])
    text = run('baseline-link', [*flags, obj, f'-L{gomp}', f'-Wl,-rpath,{gomp}',
        '-lgomp', '-pthread', '-o', out / 'baseline'])
    for phase in ('WPA', 'LTRANS'):
        counts = re.findall(r'LTO_GRAPH ' + phase + r' approved=(\d+)', text)
        assert counts and sum(map(int, counts)) == 3, text
    run('baseline-run', [out / 'baseline'])
    baseline_passed = True
    # One fresh representative per mutation after the confirmed build.
    for mode, detector in detectors.items():
        obj = out / (mode + '.o')
        flags = [gcc / 'xgcc', f'-B{gcc}/', '-O0', '-flto', '-fchecking=0',
                 f'-fplugin={plugin}', '-fplugin-arg-lto_graph_probe-mode=' + mode,
                 '-fplugin-arg-lto_graph_probe-level=0']
        text = run(mode + '-compile', [*flags, '-fno-fat-lto-objects', '-fopenmp',
            '-fopenmp-transaction-stm', '-c', root / 'lto_graph.c', '-o', obj])
        assert 'LTO_MUTATION ' + mode + ' serialized' in text, text
        exe = out / (mode + '-refused')
        assert not exe.exists(), 'Use diretório novo para evitar executável residual'
        text = run(mode + '-link', [*flags, obj, f'-L{gomp}', f'-Wl,-rpath,{gomp}',
            '-lgomp', '-pthread', '-o', exe], failure=True)
        assert re.search(detector, text), text
        assert re.search(r'transaction rule V0[1-8], operation .+, function ', text), text
        assert re.search(r'\.(?:c|C):\d+:\d+:', text), text
        assert not re.search(r'LTO_GRAPH \w+ approved=[1-9]', text), text
        assert not exe.exists(), mode
        diagnostics = [line for line in text.splitlines() if 'error: transaction' in line]
        negatives.append({'mode': mode, 'integrated_refusal': True,
            'detector': detector, 'diagnostics': diagnostics,
            'explicit_rule': bool(re.search(r'transaction rule V0[1-8]', text)),
            'source_location': bool(re.search(r'\.(?:c|C):\d+:\d+:', text)),
            'context_notes': [line for line in text.splitlines() if 'note:' in line],
            'scope': 'C O0 checking=0; LTO link; no positive certificate or executable'})
    for language in ('c', 'c++'):
        for level in ('0', '2'):
            label = f'contract-{language.replace("+", "p")}-O{level}'
            obj = out / (label + '.o')
            # Observe aggregate approval, rather than merely compile success.
            flags = [gcc / 'xgcc', f'-B{gcc}/', '-O' + level, '-flto', '-fchecking=0',
                     f'-fplugin={plugin}', '-fplugin-arg-lto_graph_probe-mode=observe',
                     '-fplugin-arg-lto_graph_probe-level=' + level]
            run(label + '-compile', [*flags, '-x', language, '-fopenmp',
                '-fopenmp-transaction-stm', '-c', root / 'acceptance_contract.c', '-o', obj])
            exe = out / label
            text = run(label + '-link', [*flags, obj, f'-L{gomp}', f'-Wl,-rpath,{gomp}',
                '-lgomp', *([stdcpp] if language == 'c++' else []), '-pthread', '-o', exe])
            for phase in ('WPA', 'LTRANS'):
                counts = re.findall(r'LTO_GRAPH ' + phase + r' approved=(\d+)', text)
                assert counts and sum(map(int, counts)) == (3 if language == 'c' else 2), text
            assert 'LTO_FRONTIER revoked' in text, text
            run(label + '-run', [exe])
            positives.append(label)
    passed = True
finally:
    (out / 'report.json').write_text(json.dumps({'audit_execution_passed': passed,
        'stage_6_6_complete': False, 'baseline_passed': baseline_passed,
        'negative': negatives, 'positive': positives,
        'commands': commands}, indent=2) + '\n')
print(f'Auditoria dirigida: {len(negatives)} recusas integradas; {len(positives)} positivos executados.')
