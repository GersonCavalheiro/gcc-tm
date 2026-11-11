"""Exercise the real resolver's storage-rule propagation in isolated maps."""
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
stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out = root / '.build/storage-rule' / stamp
out.mkdir(parents=True)
commands, cases = [], []
passed = False


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
    plugin = out / 'storage_rule_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        '-fvisibility=hidden', '-Wl,-Bsymbolic', f'-I{gcc}', f'-I{src / "gcc"}',
        f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
        root / 'storage_rule_probe_plugin.cc', '-o', plugin])
    for language in ('c', 'c++'):
        for level in ('0', '2'):
            for checking in ('0', '2'):
                for mode in ('global_private', 'invalid_tag', 'wrong_branch',
                             'range', 'first_failure', 'valid_private', 'registered'):
                    positive = mode in ('valid_private', 'registered')
                    rule = 'V03' if mode in ('range', 'first_failure') else 'V04'
                    label = f'{mode}-{language.replace("+", "p")}-O{level}-checking{checking}'
                    obj = out / (label + '.o')
                    text = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                        '-O' + level, '-fchecking=' + checking, f'-fplugin={plugin}',
                        '-fplugin-arg-storage_rule_probe-mode=' + mode, '-c',
                        root / 'storage_rule.c', '-o', obj], not positive)
                    assert f'STORAGE_RULE mode={mode} accepted={int(positive)}' in text, text
                    if positive:
                        exe = out / label
                        run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj, '-o', exe])
                        run(label + '-run', [exe])
                    else:
                        assert 'transaction rule ' + rule + ', operation ' in text, text
                        assert re.search(r'\.c:\d+:\d+:', text), text
                        assert not obj.exists(), obj
                        if mode == 'global_private':
                            assert 'constant PRIVATE classification lacks an activation-owned allocation' in text, text
                        if mode in ('invalid_tag', 'wrong_branch'):
                            assert 'underlying guard analysis:' in text, text
                            assert 'storage classification guard is not established' in text, text
                        if mode == 'first_failure':
                            assert 'earlier identity failure' in text, text
                    cases.append({'case': label, 'positive': positive,
                                  'expected_rule': None if positive else rule})
    passed = True
finally:
    (out / 'report.json').write_text(json.dumps({'passed': passed, 'step': 3,
        'stage_6_6_complete': False, 'scope': 'Component resolver, real source included with hidden isolated maps; not integrated graph proof',
        'cases': cases, 'commands': commands}, indent=2) + '\n')
    print(out / 'report.json', flush=True)
print(f'Regra de armazenamento: {len(cases)} casos aprovados.')
