"""Run real DejaGNU transaction/critical suites and preserve every summary."""
from pathlib import Path
import collections
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
build = Path(sys.argv[1]).resolve()
local_tools = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out = root / 'transaction_test/.build/dejagnu' / stamp
out.mkdir(parents=True)
gomp = build / 'x86_64-pc-linux-gnu/libgomp'
env = os.environ.copy()
env.update(OMP_NUM_THREADS='4', OMP_DYNAMIC='FALSE', LC_ALL='C')
library_paths = [str(gomp / '.libs')]
if local_tools:
    env['PATH'] = str(local_tools / 'usr/bin') + os.pathsep + env['PATH']
    env['EXPECT'] = str(local_tools / 'usr/bin/expect')
    env['DEJAGNULIBS'] = str(local_tools / 'usr/share/dejagnu')
    env['TCLLIBPATH'] = str(local_tools / 'usr/lib/tcltk/x86_64-linux-gnu/expect5.45.4')
    library_paths.append(str(local_tools / 'usr/lib/x86_64-linux-gnu'))
env['LD_LIBRARY_PATH'] = os.pathsep.join(library_paths + [env.get('LD_LIBRARY_PATH', '')])
selection = sys.argv[3:] or [f'{tool}-{pattern}' for pattern in ('transaction', 'critical')
                            for tool in ('gcc', 'g++', 'libgomp')]
results = []
passed = False
try:
    version = subprocess.run(['runtest', '--version'], env=env, capture_output=True, text=True, check=True)
    (out / 'versions.log').write_text(version.stdout + version.stderr)
    subprocess.run(['make', '-C', str(build / 'gcc'), 'site.exp'], env=env,
                   capture_output=True, text=True, check=True)
    site = build / 'gcc/site.exp'
    # The uninstalled driver needs the local spec at link time.  GCC's
    # generated site.exp explicitly supports overrides in its final section.
    with site.open('a') as stream:
        stream.write('\nset TEST_ALWAYS_FLAGS {-B' + str(gomp) + '/ -L'
                     + str(gomp / '.libs') + ' -Wl,-rpath,' + str(gomp / '.libs') + '}\n')
    for label in selection:
        tool, pattern = label.split('-', 1)
        if tool not in ('gcc', 'g++', 'libgomp') or pattern not in ('transaction', 'critical'):
            raise ValueError(label)
        destination = out / label
        destination.mkdir()
        if tool == 'libgomp':
            args = ['make', '-C', str(gomp), '-j1', 'check',
                    f'RUNTESTFLAGS=c.exp={pattern}-*.c c++.exp={pattern}-*.c']
            suite = gomp / 'testsuite'
        else:
            dirname = f'testsuite-acceptance-{stamp}-{label.replace("+", "p")}'
            args = ['make', '-C', str(build / 'gcc'), '-j1', 'check-' + tool,
                    'TESTSUITEDIR=' + dirname, f'RUNTESTFLAGS=gomp.exp={pattern}-*']
            suite = build / 'gcc' / dirname / tool
        if env.get('EXPECT'):
            args.append('EXPECT=' + env['EXPECT'])
        process = subprocess.run(args, env=env, capture_output=True, text=True, timeout=900)
        (destination / 'make.log').write_text(process.stdout + process.stderr)
        summaries = sorted(suite.glob('*.sum'))
        logs = sorted(suite.glob('*.log'))
        for path in [*summaries, *logs]:
            shutil.copyfile(path, destination / path.name)
        counts = collections.Counter()
        errors = []
        for path in summaries:
            for line in path.read_text(errors='replace').splitlines():
                status = line.split(':', 1)[0]
                if status in ('PASS', 'FAIL', 'XPASS', 'XFAIL', 'UNSUPPORTED', 'UNRESOLVED', 'UNTESTED', 'ERROR'):
                    counts[status] += 1
                if status in ('FAIL', 'XPASS', 'UNRESOLVED', 'UNTESTED', 'ERROR'):
                    errors.append(line)
        text = process.stdout + process.stderr
        infrastructure_errors = [line for line in text.splitlines()
                                 if line.startswith('ERROR:') or 'could not find `runtest' in line]
        ok = (process.returncode == 0 and bool(summaries) and counts['PASS'] > 0
              and not errors and not infrastructure_errors)
        result = {'suite': label, 'passed': ok, 'command': args, 'returncode': process.returncode,
                  'counts': dict(counts), 'unexpected': errors,
                  'infrastructure_errors': infrastructure_errors,
                  'artifacts': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in destination.iterdir() if p.is_file()}}
        results.append(result)
        print(label, dict(counts), 'PASS' if ok else 'NOT APPROVED', flush=True)
        if not ok:
            raise RuntimeError(label + ': examine ' + str(destination))
    passed = True
finally:
    (out / 'report.json').write_text(json.dumps({'passed': passed, 'stage_6_6_complete': False,
        'scope': selection, 'suites': results,
        'environment': {k: env[k] for k in ('OMP_NUM_THREADS', 'OMP_DYNAMIC', 'LD_LIBRARY_PATH')},
        'local_tools': str(local_tools) if local_tools else None}, indent=2) + '\n')
    print(out / 'report.json', flush=True)
