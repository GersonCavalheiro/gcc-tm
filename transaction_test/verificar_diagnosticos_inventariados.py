"""Directed regression of contextual diagnostics in the inventoried clone gates."""
from pathlib import Path
import datetime
import hashlib
import json
import re
import subprocess
import sys

if not __debug__:
    raise SystemExit('Execute sem desativar as asserções Python')
root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
gcc = Path(sys.argv[1]).resolve() / 'gcc'
gomp = gcc.parent / 'x86_64-pc-linux-gnu/libgomp/.libs'
stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out = root / '.build/inventoried-diagnostics' / stamp
out.mkdir(parents=True)
commands, negatives, positives = [], [], []
passed = False
specs = (
    ('parameter_owner', 'graph_cycle.c', 'V06', 'prepared signature lost its parameter correspondence'),
    ('frame_field', 'graph_cycle.c', 'V03', 'operand lost its frame correspondence'),
    ('controller_channels', 'graph_cycle.c', 'V06', 'incompatible prepared signature'),
    ('controller_owner', 'controller_parallel.c', 'V07', 'owner mismatch'),
    ('controller_remove', 'graph_cycle.c', 'V07', 'lacks a unique lowered body invocation'),
    ('registration_rebind', 'memory_registration.c', 'V03', 'registration changed its object identity'),
    ('offset_flag', 'memory_static_array.c', 'V03', 'offset lacks an overflow guard'),
    ('memory_size', 'memory_static_access.c', 'V03', 'access changed its static identity'),
)


def run(label, args, failure=False):
    args = list(map(str, args))
    r = subprocess.run(args, capture_output=True, text=True, timeout=180)
    text = r.stdout + r.stderr
    p = out / (label + '.log')
    p.write_text(text)
    commands.append({'case': label, 'command': args, 'returncode': r.returncode,
                     'log': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()})
    assert 'internal compiler error' not in text, text
    assert (r.returncode != 0) == failure, text
    return text


try:
    plugin = out / 'cfg_abi_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
        root / 'cfg_abi_probe_plugin.cc', '-o', plugin])
    stdcpp = subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip()
    for language in ('c', 'c++'):
        for checking in ('0', '2'):
            flags = [gcc / 'xgcc', f'-B{gcc}/', f'-I{src / "include"}', '-O0',
                     '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking]
            # A valid control for every distinct mutated fixture/configuration.
            for source in dict.fromkeys(s[1] for s in specs):
                label = f'baseline-{Path(source).stem}-{language.replace("+", "p")}-{checking}'
                obj = out / (label + '.o')
                run(label + '-compile', [*flags, '-x', language, '-c', root / source, '-o', obj])
                exe = out / label
                run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj, f'-L{gomp}',
                    f'-Wl,-rpath,{gomp}', '-lgomp', *([stdcpp] if language == 'c++' else []),
                    '-pthread', '-o', exe])
                run(label + '-run', [exe])
                positives.append(label)
            for mode, source, rule, cause in specs:
                label = f'{mode}-{language.replace("+", "p")}-{checking}'
                obj = out / (label + '.o')
                text = run(label, [*flags, '-x', language, f'-fplugin={plugin}',
                    '-fplugin-arg-cfg_abi_probe-mode=' + mode, '-c', root / source,
                    '-o', obj], failure=True)
                assert cause in text, text
                assert 'transaction rule ' + rule + ', operation ' in text, text
                assert 'function ' in text and re.search(r'\.c:\d+:\d+:', text), text
                assert 'did not find its target' not in text, text
                assert not obj.exists(), obj
                negatives.append({'case': label, 'rule': rule, 'cause': cause})
    passed = True
finally:
    (out / 'report.json').write_text(json.dumps({'passed': passed, 'step': 2,
        'stage_6_6_complete': False, 'negatives': negatives, 'positives': positives,
        'commands': commands}, indent=2) + '\n')
    print(out / 'report.json', flush=True)
print(f'Diagnósticos inventariados: {len(negatives)} recusas e {len(positives)} controles executados.')
