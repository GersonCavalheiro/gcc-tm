"""Actual owning controller CFG after remapping, independently of old approval."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/current-owner'
out.mkdir(parents=True, exist_ok=True)


def run(label, args, expected=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=240)
    message = result.stdout + result.stderr
    (out / (label + '.log')).write_text(message)
    if result.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    return message


plugin = out / 'current_owner_probe.so'
run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'current_owner_probe_plugin.cc', '-o', plugin])
checks = 1
executions = 0
negative = ('drop_begin', 'drop_body', 'drop_status', 'drop_end', 'drop_fail', 'drop_cleanup',
            'outer', 'foreign_context', 'foreign_slot', 'slot_escape', 'slot_overwrite',
            'cause_conflict', 'cause_invalid', 'counter_reset', 'counter_double',
            'counter_wrap', 'counter_bound', 'retry_invalid', 'retry_unknown', 'retry_spoof',
            'retry_skip', 'failure_cycle', 'live_clobber', 'missing_boundary', 'early_boundary',
            'outside', 'duplicate_body', 'alias_status', 'fatal_conflict', 'begin_success',
            'body_success', 'status_success', 'end_success', 'unknown_return', 'stale_fields',
            'entry_bypass', 'consumed_context', 'new_alias_outside', 'context_subscript')
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for optimization in ('0', '2'):
            for mode in (*negative, 'copy', 'reverse', 'dead_status', 'observe'):
                label = f'{mode}-{language.replace("+", "p")}-{checking}-O{optimization}'
                obj = out / (label + '.o')
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                    '-O' + optimization, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=' + checking, '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0',
                    f'-fplugin={plugin}', '-fplugin-arg-current_owner_probe-mode=' + mode,
                    '-c', root / 'current_owner.c', '-o', obj], 1 if mode in negative else 0)
                if mode in negative:
                    assert 'native owner probe did not mutate' not in message, message
                    assert 'native owner baseline was refused' not in message, message
                    assert 'native owner mutation has unexpected result' not in message, message
                    assert 'transaction transformed owner proof failed:' in message, message
                    assert 'CURRENT_OWNER_MUTATION refused' in message, message
                    assert 'internal compiler error' not in message, message
                    if mode == 'alias_status':
                        assert 'CURRENT_OWNER_RAW_ALIAS unflagged' in message, message
                elif mode == 'observe':
                    assert 'CURRENT_OWNER dependent controllers=' in message and 'dependencies=3' in message, message
                checks += 1
                if mode in ('copy', 'reverse', 'dead_status', 'observe'):
                    for linkage in ('dynamic', 'archive'):
                        exe = out / (label + '-' + linkage)
                        libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
                                if linkage == 'dynamic' else [gomp / 'libgomp.a'])
                        run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                            *libs, '-pthread', '-o', exe])
                        run(label + '-' + linkage + '-run', [exe])
                        checks += 2
                        executions += 1
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for mode in ('foreign_slot', 'counter_bound', 'retry_skip', 'missing_boundary', 'alias_status'):
            label = f'parallel-{mode}-{language.replace("+", "p")}-{checking}'
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O2',
                '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={plugin}', '-fplugin-arg-current_owner_probe-mode=' + mode,
                '-c', root / 'controller_parallel.c', '-o', out / (label + '.o')], 1)
            assert 'transaction transformed owner proof failed:' in message, message
            assert 'CURRENT_OWNER_MUTATION refused' in message, message
            assert 'native owner mutation has unexpected result' not in message, message
            assert 'native owner probe did not mutate' not in message, message
            assert 'native owner baseline was refused' not in message, message
            assert 'internal compiler error' not in message, message
            checks += 1
for source in ('controller_parallel.c', 'controller_basic.c', 'current_status.c'):
    for language in ('c', 'c++'):
        for optimization in ('0', '2'):
            label = f'{Path(source).stem}-{language.replace("+", "p")}-O{optimization}'
            obj = out / (label + '.o')
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                '-O' + optimization, '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=2',
                f'-fplugin={plugin}', '-fplugin-arg-current_owner_probe-mode=observe',
                '-c', root / source, '-o', obj])
            assert 'CURRENT_OWNER dependent controllers=' in message, message
            checks += 1
            for linkage in ('dynamic', 'archive'):
                exe = out / (label + '-' + linkage)
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
                        if linkage == 'dynamic' else [gomp / 'libgomp.a'])
                run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                    *libs, '-pthread', '-o', exe])
                run(label + '-' + linkage + '-run', [exe]); checks += 2; executions += 1
print(f'Propriedade atual: {checks} verificações, {executions} execuções; C/C++, O0/O2, checking 0/2, GC, DSO/archive.')
