"""Consolida evidências existentes; não executa testes nem aprova a 6.6."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone

if not __debug__:
    raise SystemExit('Execute Python sem -O/-OO ou PYTHONOPTIMIZE')

root = Path(__file__).resolve().parent.parent
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
out = root / 'transaction_test/.build/acceptance' / stamp
out.mkdir(parents=True, exist_ok=False)
artifacts = {}


def preserve(path):
    source = Path(path)
    if not source.is_absolute():
        source = root / source
    source = source.resolve()
    key = str(source)
    if key in artifacts:
        return artifacts[key]
    relative = (source.relative_to(root) if source.is_relative_to(root)
                else Path('external') / source.relative_to('/'))
    target = out / 'evidence' / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    entry = {'source': key, 'snapshot': str(target.relative_to(out)),
             'sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    artifacts[key] = entry
    return entry


def read(path):
    entry = preserve(path)
    return (out / entry['snapshot']).read_text()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


try:
    base = 'transaction_test/.build/lto-graph-checking0/'
    recent = json.loads(read(base + 'acceptance-checking0.json'))
    negative = []
    modes = ('version', 'mode', 'envelope', 'map', 'roots', 'component',
             'missing', 'layout', 'raw', 'guard', 'ssa_use', 'phi', 'context',
             'status', 'owner', 'channel', 'entry')
    for language in ('c', 'cpp'):
        for level in ('0', '2'):
            for mode in modes:
                label = f'{mode}-{language}-0-O{level}'
                compile_log = read(base + label + '.log')
                link_log = read(base + label + '-link.log')
                require('LTO_MUTATION ' + mode + ' serialized' in compile_log, label)
                require(re.search(r'error: transaction (?:LTO|transformed|current)', link_log), label)
                require('internal compiler error' not in compile_log + link_log, label)
                negative.append(label)
    positive = []
    for language in ('c', 'cpp'):
        for level in ('0', '2'):
            prefix = f'graph-{language}-0-O{level}'
            read(base + prefix + '.log')
            for mode in ('observe', 'rebuild', 'revoke'):
                for linkage in ('dynamic', 'archive'):
                    label = prefix + '-' + mode + '-' + linkage
                    log = read(base + label + '-link.log')
                    for phase in ('WPA', 'LTRANS'):
                        counts = re.findall(r'LTO_GRAPH ' + phase + r' approved=(\d+)', log)
                        require(counts and sum(map(int, counts)) == 3, label)
                    require('LTO_FRONTIER revoked' in log, label)
                    if mode == 'rebuild':
                        require('LTO_REBUILD new generation' in log, label)
                    if mode == 'revoke':
                        require('LTO_REVOKE irreversible generation' in log, label)
                    read(base + label + '-run.log')
                    positive.append(label)
    native = []
    current = 'transaction_test/.build/current-graph-checking0/'
    for mode in ('controller', 'producer'):
        for language in ('c', 'cpp'):
            for level in ('0', '2'):
                label = f'{mode}-{language}-0-O{level}'
                log = read(current + label + '.log')
                require('CURRENT_GRAPH_' + mode + ' checked' in log, label)
                require('CURRENT_GRAPH_FRONTIER revoked' in log, label)
                require('error:' not in log, label)
                for linkage in ('dynamic', 'archive'):
                    read(current + label + '-' + linkage + '-link.log')
                    read(current + label + '-' + linkage + '-run.log')
                native.append(label)
    summaries = [read('/tmp/stm-acceptance-checking0-' + name + '.log')
                 for name in ('lto-negative', 'lto-positive', 'current')]
    for text, checks, runs in zip(summaries, (137, 53, 41), (0, 24, 16)):
        require(f'{checks} verificações, {runs} execuções' in text, 'resumo incompatível')
    historical = []
    for path in sorted((root / 'transaction_test/.build').glob('*/report.json')):
        data = json.loads(read(path))
        rows = data.get('checks', []) if isinstance(data, dict) else data
        passed = (data.get('passed') if isinstance(data, dict) else
                  bool(rows) and all(row.get('passed') is True for row in rows))
        historical.append({'suite': path.parent.name, 'checks': len(rows),
                           'reported_passed': passed, 'reexecuted': False,
                           'executions': data.get('executions') if isinstance(data, dict) else None})
    for path in ('gcc-src-tm/OPENMP_TRANSACTION_RETOMADA.md',
                 'gcc-src-tm/OPENMP_TRANSACTION_ACCEPTANCE_SURVEY.md',
                 'transaction_test/verificar_grafo_lto.py',
                 'transaction_test/verificar_grafo_atual.py',
                 'transaction_test/consolidar_aceitacao.py'):
        preserve(path)
    report = {'schema': 1, 'consolidated_at_utc': stamp,
              'acceptance': 'PARTIAL', 'stage_6_6_complete': False,
              'recent': {'negative_lto_cases': negative, 'positive_lto_runs': positive,
                         'controller_producer_cases': native, 'controller_producer_runs': 16,
                         'checks_reported': 231, 'executions_reported': 40,
                         'checking': 0, 'execution_record': recent,
                         'exit_status_provenance': 'Successful script completion recorded in session; logs alone do not encode every process exit status.'},
              'historical': historical,
              'state_at_consolidation': {
                  'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
                  'status': subprocess.check_output(['git', 'status', '--porcelain'], cwd=root, text=True),
                  'platform': subprocess.check_output(['uname', '-sm'], text=True).strip(),
                  'python': sys.version,
                  'build_source_correspondence_verified': False},
              'open_items': ['Full transaction/critical DejaGNU regressions',
                             'Diagnostics and normative example audit',
                             'Integrated acceptance across V01-V08',
                             'Build/source correspondence and final documentation',
                             'Remaining LTO phases/configurations and checking=2 regression'],
              'artifacts': list(artifacts.values())}
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(out / 'report.json')
except Exception:
    (out / 'INCOMPLETE').write_text('Consolidação interrompida; não usar como aprovação.\n')
    raise
