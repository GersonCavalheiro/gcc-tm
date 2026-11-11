"""Whole-graph counterproofs for PRIVATE, early use, derived store and retirement."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

if not __debug__:
    raise SystemExit('Asserções Python precisam estar habilitadas')
root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
src = root.parent / 'gcc-src-tm'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/integrated-counterproof'
out.mkdir(parents=True, exist_ok=True)
commands, cases = [], []
passed = False
detectors = {
    'private': 'transaction transformed memory proof failed: memory operation lost its current instance',
    'early': 'transaction transformed status proof failed: V05: result or binding consumed before its own call succeeded',
    'drop_store': 'transaction transformed memory proof failed: memory operation changed its role',
    'retired': 'transaction current graph composition refused',
}


def run(label, args, failure=False):
    args = list(map(str, args))
    result = subprocess.run(args, capture_output=True, text=True, timeout=180)
    text = result.stdout + result.stderr
    log = out / (label + '.log')
    log.write_text(text)
    commands.append({'case': label, 'command': args, 'returncode': result.returncode,
                     'log': str(log), 'sha256': hashlib.sha256(log.read_bytes()).hexdigest()})
    assert 'internal compiler error' not in text, text
    assert (result.returncode != 0) == failure, text
    return text


try:
    plugin = out / 'integrated_counterproof.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
        root / 'integrated_counterproof_plugin.cc', '-o', plugin])
    stdcpp = subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip()
    for language in ('c', 'c++'):
        for level in ('0', '2'):
            for checking in ('0', '2'):
                for mode in ('observe', 'private', 'early', 'drop_store', 'retired'):
                    if mode == 'drop_store' and language == 'c++':
                        continue
                    label = f'{mode}-{language.replace("+", "p")}-O{level}-checking{checking}'
                    obj = out / (label + '.o')
                    obj.unlink(missing_ok=True)
                    text = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                        '-O' + level, '-fchecking=' + checking, '-fopenmp',
                        '-fopenmp-transaction-stm', f'-fplugin={plugin}',
                        '-fplugin-arg-integrated_counterproof-mode=' + mode,
                        '-c', root / 'integrated_counterproof.c', '-o', obj], mode != 'observe')
                    if mode == 'observe':
                        assert 'INTEGRATED_BASELINE approved' in text, text
                        exe = out / label
                        run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                            f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp',
                            *([stdcpp] if language == 'c++' else []), '-pthread', '-o', exe])
                        run(label + '-run', [exe])
                    else:
                        assert 'INTEGRATED_MUTATION ' + mode + ' applied' in text, text
                        assert 'error: transaction ' in text, text
                        assert detectors[mode] in text, text
                        assert re.search(r'transaction rule V0[1-8], operation .+, function ', text), text
                        rule = {'private': 'V04', 'early': 'V05',
                                'drop_store': 'V01', 'retired': 'V08'}[mode]
                        assert 'transaction rule ' + rule + ', operation ' in text, text
                        assert re.search(r'\.c:\d+:\d+:', text), text
                        assert 'did not mutate' not in text, text
                        assert 'accepted the mutated graph' not in text, text
                        assert 'retained aggregate approval' not in text, text
                        assert not obj.exists(), label
                        if mode == 'retired':
                            assert 'INTEGRATED_RETIRED refused old=INVALID new=REFUSED proved=0' in text, text
                    cases.append({'case': label, 'mode': mode, 'passed': True,
                                  'diagnostics': [line for line in text.splitlines() if 'error:' in line],
                                  'context_notes': [line for line in text.splitlines() if 'note:' in line]})
    passed = True
finally:
    (out / 'report.json').write_text(json.dumps({'passed': passed,
        'stage_6_6_complete': False, 'cases': cases, 'commands': commands}, indent=2) + '\n')
print(f'Contraprovas integradas: {len(cases)} casos aprovados.')
