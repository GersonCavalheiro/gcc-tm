"""Recheck four counterproofs in actual WPA/LTRANS/monolithic/incremental windows."""
from pathlib import Path
import datetime
import hashlib
import json
import re
import subprocess
import sys

if not __debug__:
    raise SystemExit('Execute sem otimização das asserções Python')
root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out = root / '.build/lto-counterproof' / stamp
out.mkdir(parents=True)
commands, cases = [], []
passed = False
configs = {
    'wpa': ('wpa', ['-flto-partition=balanced']),
    'balanced': ('ltrans', ['-flto-partition=balanced']),
    'max': ('ltrans', ['-flto-partition=max']),
    '1to1': ('ltrans', ['-flto-partition=1to1']),
    'monolithic': ('monolithic', ['-flto-partition=none']),
    'incremental': ('incremental', ['-r', '-flinker-output=rel']),
    'fat': ('ltrans', ['-flto-partition=balanced']),
}
selection = sys.argv[2:] or list(configs)
if any(name not in configs for name in selection):
    raise SystemExit('Configuração inválida')
detectors = {
    'private': ('V04', 'memory operation lost its current instance'),
    'early': ('V05', 'result or binding consumed before its own call succeeded'),
    'drop_store': ('V01', 'memory operation changed its role'),
    'retired': ('V08', 'transaction current graph composition refused'),
}


def run(label, args, failure=False):
    args = list(map(str, args))
    r = subprocess.run(args, capture_output=True, text=True, timeout=180)
    text = r.stdout + r.stderr
    log = out / (label + '.log')
    log.write_text(text)
    commands.append({'case': label, 'command': args, 'returncode': r.returncode,
                     'log': str(log), 'sha256': hashlib.sha256(log.read_bytes()).hexdigest()})
    assert 'internal compiler error' not in text, text
    assert (r.returncode != 0) == failure, text
    return text


try:
    plugin = out / 'lto_counterproof.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
        root / 'lto_counterproof_plugin.cc', '-o', plugin])
    stdcpp = subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip()
    for language in ('c', 'c++'):
        # Directed pairing, not the full Cartesian product O0/O2 x checking0/2.
        for level, checking in (('0', '0'), ('2', '2')):
            common = [gcc / 'xgcc', f'-B{gcc}/', '-O' + level, '-flto', '-fchecking=' + checking]
            base = f'{language.replace("+", "p")}-O{level}-checking{checking}'
            for fat in (False, True):
                selected = [c for c in selection if (c == 'fat') == fat]
                if not selected:
                    continue
                obj = out / (base + f'-fat{int(fat)}.o')
                run(base + f'-fat{int(fat)}-compile', [*common, '-x', language,
                    '-ffat-lto-objects' if fat else '-fno-fat-lto-objects',
                    '-fopenmp', '-fopenmp-transaction-stm', '-c',
                    root / 'integrated_counterproof.c', '-o', obj])
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if checking == '0'
                        else [gomp / 'libgomp.a'])
                if language == 'c++':
                    libs.append(stdcpp)
                for config in selected:
                    phase, extra = configs[config]
                    for mode in ('observe', *detectors):
                        if language == 'c++' and mode == 'drop_store':
                            continue
                        label = base + '-' + config + '-' + mode
                        target = out / (label + ('.o' if config == 'incremental' else ''))
                        probe = [f'-fplugin={plugin}', '-fplugin-arg-lto_counterproof-mode=' + mode,
                                 '-fplugin-arg-lto_counterproof-phase=' + phase]
                        text = run(label, [*common, *probe, *extra, obj,
                            *([] if config == 'incremental' else libs), '-pthread', '-o', target], mode != 'observe')
                        assert f'LTO_COUNTERPROOF {phase} baseline approved=1' in text, text
                        if mode != 'observe':
                            rule, cause = detectors[mode]
                            assert f'LTO_COUNTERPROOF {phase} mutation={mode} applied' in text, text
                            assert f'LTO_COUNTERPROOF {phase} mutation={mode} refused proved=0' in text, text
                            assert cause in text and 'transaction rule ' + rule + ', operation ' in text, text
                            assert re.search(r'\.c:\d+:\d+:', text), text
                            assert not target.exists(), target
                        else:
                            assert 'LTO_COUNTERPROOF frontier revoked' in text, text
                            if config == 'incremental':
                                exe = out / (label + '-final')
                                final = run(label + '-final', [*common, *probe, target, *libs, '-pthread', '-o', exe])
                                assert 'LTO_COUNTERPROOF wpa baseline approved=1' in final, final
                                assert 'LTO_COUNTERPROOF ltrans baseline approved=1' in final, final
                                run(label + '-run', [exe])
                            else:
                                run(label + '-run', [target])
                        cases.append({'case': label, 'configuration': config, 'phase': phase,
                                      'mode': mode, 'passed': True})
                    if fat:
                        # The native fallback of the same fat object must remain executable.
                        exe = out / (base + '-fat-native')
                        run(base + '-fat-native-link', [gcc / 'xgcc', f'-B{gcc}/', '-fno-lto', obj,
                            *libs, '-pthread', '-o', exe])
                        run(base + '-fat-native-run', [exe])
    passed = True
finally:
    report = {'passed': passed, 'stage_6_6_complete': False,
              'scope': 'C/C++; paired O0/checking0 and O2/checking2; aggregate C only; dynamic/checking0 and archive/checking2',
              'configurations': selection, 'cases': cases, 'commands': commands}
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(out / 'report.json', flush=True)
print(f'Contraprovas LTO: {len(cases)} casos aprovados.')
