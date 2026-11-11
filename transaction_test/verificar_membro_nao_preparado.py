"""D34: a declared unprepared graph member must fail aggregate composition."""
from pathlib import Path
import datetime
import hashlib
import json
import re
import subprocess
import sys

if not __debug__:
    raise SystemExit('Execute com asserções Python habilitadas')
root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
gcc = Path(sys.argv[1]).resolve() / 'gcc'
gomp = gcc.parent / 'x86_64-pc-linux-gnu/libgomp/.libs'
smoke = '--smoke' in sys.argv[2:]
out = root / '.build/unprepared-member' / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out.mkdir(parents=True)
commands, cases = [], []
passed = False


def run(label, args, failure=False):
    args = list(map(str, args))
    r = subprocess.run(args, capture_output=True, text=True, timeout=180)
    text = r.stdout + r.stderr
    path = out / (label + '.log')
    path.write_text(text)
    commands.append({'case': label, 'command': args, 'returncode': r.returncode,
                     'log': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    assert 'internal compiler error' not in text, text
    assert (r.returncode != 0) == failure, text
    return text


try:
    plugin = out / 'unprepared_member_probe.so'
    run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
        f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
        root / 'unprepared_member_probe_plugin.cc', '-o', plugin])
    for language in (('c',) if smoke else ('c', 'c++')):
        for checking in (('0',) if smoke else ('0', '2')):
            label = f'{language.replace("+", "p")}-checking{checking}'
            flags = [gcc / 'xgcc', f'-B{gcc}/', '-O0', '-fopenmp', '-fopenmp-transaction-stm',
                     '-fchecking=' + checking, '-x', language, f'-fplugin={plugin}']
            obj = out / (label + '.o')
            text = run(label + '-baseline', [*flags, '-fplugin-arg-unprepared_member_probe-observe',
                '-c', root / 'unprepared_member.c', '-o', obj])
            assert 'UNPREPARED_MEMBER baseline approved' in text, text
            exe = out / label
            run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj, f'-L{gomp}',
                f'-Wl,-rpath,{gomp}', '-lgomp', '-pthread', '-o', exe])
            run(label + '-run', [exe])
            bad = out / (label + '-refused.o')
            text = run(label + '-refused', [*flags, '-c', root / 'unprepared_member.c', '-o', bad], True)
            for marker in ('baseline approved', 'declared dependency appended', 'refused proved=0'):
                assert 'UNPREPARED_MEMBER ' + marker in text, text
            assert 'transaction current graph contains an unprepared member' in text, text
            assert 'transaction rule V06, operation prepared graph member, function' in text, text
            assert 'unprepared_dependency' in text and re.search(r'\.c:\d+:\d+:', text), text
            assert not bad.exists(), bad
            cases.append(label)
    passed = True
finally:
    (out / 'report.json').write_text(json.dumps({'passed': passed, 'smoke': smoke,
        'stage_6_6_complete': False, 'cases': cases, 'commands': commands}, indent=2) + '\n')
    print(out / 'report.json', flush=True)
