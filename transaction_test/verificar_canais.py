"""Current incoming/result proofs, after outlining, without descriptor access."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/channels'
out.mkdir(parents=True, exist_ok=True)


def run(label, args, expected=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=180)
    message = result.stdout + result.stderr
    (out / (label + '.log')).write_text(message)
    if result.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    return message


plugin = out / 'channel_probe.so'
run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'channel_probe_plugin.cc', '-o', plugin])
checks = 1
executions = 0
negative = ('incoming_short', 'overlap', 'result_dead', 'incoming_dead', 'escape',
            'slot_swap', 'missing_field', 'alias', 'incoming_write', 'publish_value', 'publish_binding',
            'entry_incoming_short', 'entry_slot_swap', 'entry_missing_field', 'entry_incoming_dead')
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for optimization in ('0', '2'):
            for mode in (*negative, 'copy', 'observe'):
                label = f'{mode}-{language.replace("+", "p")}-{checking}-O{optimization}'
                obj = out / (label + '.o')
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                    '-O' + optimization, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=' + checking, '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0',
                    f'-fplugin={plugin}', '-fplugin-arg-channel_probe-mode=' + mode,
                    '-c', root / 'channels.c', '-o', obj], 1 if mode in negative else 0)
                if mode in negative:
                    assert 'channel probe did not mutate' not in message, message
                    assert 'channel mutation was accepted' not in message, message
                    assert 'transaction transformed channel proof failed:' in message, message
                    assert 'CHANNEL_MUTATION refused' in message, message
                    assert 'internal compiler error' not in message, message
                elif mode == 'observe':
                    assert 'CHANNEL_MODEL dependent functions=' in message and 'dependencies=6' in message, message
                checks += 1
                if mode in ('copy', 'observe'):
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
        for mode in ('observe', 'entry_missing_field', 'entry_incoming_dead'):
            label = f'parallel-{mode}-{language.replace("+", "p")}-{checking}'
            obj = out / (label + '.o')
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O2',
                '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={plugin}', '-fplugin-arg-channel_probe-mode=' + mode,
                '-c', root / 'controller_parallel.c', '-o', obj], 0 if mode == 'observe' else 1)
            if mode != 'observe':
                assert 'transaction transformed channel proof failed:' in message, message
                assert 'CHANNEL_MUTATION refused' in message, message
                assert 'channel probe did not mutate' not in message, message
                assert 'channel mutation was accepted' not in message, message
                assert 'internal compiler error' not in message, message
            else:
                exe = out / label
                run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                    f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp', '-pthread', '-o', exe])
                run(label + '-run', [exe])
                checks += 2
                executions += 1
            checks += 1
for checking in ('0', '2'):
    for optimization in ('0', '2'):
        label = f'external-exceptions-{checking}-O{optimization}'
        obj = out / (label + '.o')
        run(label, [gcc / 'xgcc', f'-B{gcc}/', '-O' + optimization,
            '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
            '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0',
            '-c', root / 'channels_external.C', '-o', obj])
        checks += 1
        for linkage in ('dynamic', 'archive'):
            exe = out / (label + '-' + linkage)
            libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
                    if linkage == 'dynamic' else [gomp / 'libgomp.a'])
            libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
            run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                *libs, '-pthread', '-o', exe])
            run(label + '-' + linkage + '-run', [exe])
            checks += 2
            executions += 1
print(f'Canais remapeados: {checks} verificações, {executions} execuções; C/C++, O0/O2, checking 0/2, GC, DSO/archive.')
