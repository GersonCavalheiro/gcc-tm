"""Per-call status, private alternatives and failure epilogues after outlining."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/current-status'
out.mkdir(parents=True, exist_ok=True)


def run(label, args, expected=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=240)
    message = result.stdout + result.stderr
    (out / (label + '.log')).write_text(message)
    if result.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    return message


plugin = out / 'current_status_probe.so'
run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'current_status_probe_plugin.cc', '-o', plugin])
checks = 1
executions = 0
negative = ('invert', 'old_status', 'overwrite', 'early', 'binding_early',
            'clobber', 'constructor', 'raw_alias', 'private_dummy', 'cause',
            'drop_fail', 'failure_return', 'cycle', 'publish_bypass', 'drop_final',
            'entry_begin', 'entry_helper', 'entry_end', 'entry_outer', 'entry_raw_alias', 'unknown_return', 'subobject_output')
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for optimization in ('0', '2'):
            for mode in (*negative, 'copy', 'reverse', 'entry_copy', 'entry_reverse', 'observe'):
                label = f'{mode}-{language.replace("+", "p")}-{checking}-O{optimization}'
                obj = out / (label + '.o')
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                    '-O' + optimization, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=' + checking, '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0',
                    f'-fplugin={plugin}', '-fplugin-arg-current_status_probe-mode=' + mode,
                    '-c', root / 'current_status.c', '-o', obj], 1 if mode in negative else 0)
                if mode in negative:
                    assert 'native status probe did not mutate' not in message, label + '\n' + message
                    assert 'native status mutation was accepted' not in message, label + '\n' + message
                    assert 'transaction transformed status proof failed:' in message, label + '\n' + message
                    assert 'CURRENT_STATUS_MUTATION refused' in message, label + '\n' + message
                    assert 'internal compiler error' not in message, label + '\n' + message
                    if mode in ('raw_alias', 'entry_raw_alias'):
                        assert 'CURRENT_STATUS_RAW_ALIAS unflagged' in message, message
                    if mode == 'unknown_return':
                        assert 'failure returns without passing through FAIL' in message, message
                    if mode == 'subobject_output':
                        assert 'output is not a resolved local buffer object' in message, message
                elif mode == 'observe':
                    assert 'CURRENT_STATUS dependent functions=' in message and 'dependencies=4' in message, message
                checks += 1
                if mode in ('copy', 'reverse', 'entry_copy', 'entry_reverse', 'observe'):
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
        for mode in ('entry_begin', 'entry_helper', 'observe'):
            label = f'parallel-{mode}-{language.replace("+", "p")}-{checking}'
            obj = out / (label + '.o')
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O2',
                '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={plugin}', '-fplugin-arg-current_status_probe-mode=' + mode,
                '-c', root / 'controller_parallel.c', '-o', obj], 0 if mode == 'observe' else 1)
            if mode != 'observe':
                assert 'transaction transformed status proof failed:' in message, message
                assert 'native status mutation was accepted' not in message, message
                assert 'CURRENT_STATUS_MUTATION refused' in message, message
                assert 'internal compiler error' not in message, message
            else:
                exe = out / label
                run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                    f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp', '-pthread', '-o', exe])
                run(label + '-run', [exe]); executions += 1; checks += 2
            checks += 1
for checking in ('0', '2'):
    for optimization in ('0', '2'):
        # This GCC removes the empty GNU cleanup before ompexp in O2.
        # Mutate the actual router only where it exists; execute both modes.
        cleanup_modes = ('cleanup_invert', 'cleanup_alias', 'observe') if optimization == '0' else ('observe',)
        for mode in cleanup_modes:
            label = f'cleanup-{mode}-{checking}-O{optimization}'
            obj = out / (label + '.o')
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-O' + optimization,
                '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={plugin}', '-fplugin-arg-current_status_probe-mode=' + mode,
                '-c', root / 'controller_basic.c', '-o', obj], 0 if mode == 'observe' else 1)
            if mode != 'observe':
                assert 'transaction transformed status proof failed:' in message, message
                assert 'native status mutation was accepted' not in message, message
                assert 'native status probe did not mutate' not in message, message
                assert 'CURRENT_STATUS_MUTATION refused' in message, message
                assert 'internal compiler error' not in message, message
            else:
                for linkage in ('dynamic', 'archive'):
                    exe = out / (label + '-' + linkage)
                    libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
                            if linkage == 'dynamic' else [gomp / 'libgomp.a'])
                    run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                        *libs, '-pthread', '-o', exe])
                    run(label + '-' + linkage + '-run', [exe]); executions += 1; checks += 2
            checks += 1
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for mode in ('entry_helper', 'entry_end', 'observe'):
            label = f'global-selector-{mode}-{language.replace("+", "p")}-{checking}'
            obj = out / (label + '.o')
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O2',
                '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={plugin}', '-fplugin-arg-current_status_probe-mode=' + mode,
                '-c', root / 'current_status_global.c', '-o', obj], 0 if mode == 'observe' else 1)
            if mode != 'observe':
                assert 'transaction transformed status proof failed:' in message, message
                assert 'native status mutation was accepted' not in message, message
                assert 'CURRENT_STATUS_MUTATION refused' in message, message
                assert 'internal compiler error' not in message, message
            else:
                for linkage in ('dynamic', 'archive'):
                    exe = out / (label + '-' + linkage)
                    libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
                            if linkage == 'dynamic' else [gomp / 'libgomp.a'])
                    run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                        *libs, '-pthread', '-o', exe])
                    run(label + '-' + linkage + '-run', [exe], 17); executions += 1; checks += 2
            checks += 1
print(f'Status atual: {checks} verificações, {executions} execuções; C/C++, O0/O2, checking 0/2, GC, DSO/archive.')
