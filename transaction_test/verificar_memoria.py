"""Post-outlining memory identities, storage, layouts and proof dependencies."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/memory'
out.mkdir(parents=True, exist_ok=True)


def run(label, args, expected=0):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=120)
    message = result.stdout + result.stderr
    (out / (label + '.log')).write_text(message)
    if result.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    return message


plugin = out / 'memory_model_probe.so'
run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'memory_model_probe_plugin.cc', '-o', plugin])
checks = 1
executions = 0
negative = ('choice', 'switch_choice', 'field_object', 'field_object_size', 'field_kind', 'field_storage',
            'index', 'private_read', 'private_write', 'offset', 'layout', 'alias',
            'registration_extent', 'registration_kind', 'source_layout',
            'private_instance', 'private_clobber', 'real_constant', 'cast', 'early_address', 'scalar_alias', 'scalar_alias_raw', 'foreign_binding', 'forged_global')
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for optimization in ('0', '2'):
            modes = tuple(mode for mode in negative if mode != 'source_layout' or language == 'c')
            for mode in (*modes, 'copy', 'observe'):
                label = f'{mode}-{language.replace("+", "p")}-{checking}-O{optimization}'
                obj = out / (label + '.o')
                message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                    '-O' + optimization, '-fopenmp', '-fopenmp-transaction-stm',
                    '-fchecking=' + checking, '--param=ggc-min-expand=0', '--param=ggc-min-heapsize=0', f'-fplugin={plugin}',
                    '-fplugin-arg-memory_model_probe-mode=' + mode,
                    '-c', root / 'memory_model.c', '-o', obj], 1 if mode in negative else 0)
                if mode in negative:
                    assert 'memory probe did not mutate' not in message, message
                    assert 'transaction transformed memory proof failed:' in message, label + '\n' + message
                    if mode == 'scalar_alias_raw':
                        assert 'MEMORY_RAW_ALIAS observed_unflagged' in message, message
                        assert 'unflagged scalar alias was accepted' not in message, message
                    if mode == 'foreign_binding':
                        assert 'MEMORY_FOREIGN_ACTIVATION refused' in message, message
                        assert 'foreign activation was accepted' not in message, message
                elif mode == 'observe':
                    assert 'MEMORY_MODEL dependent functions=' in message and 'dependencies=7' in message, message
                checks += 1
                if mode in ('observe', 'copy'):
                    for linkage in ('dynamic', 'archive'):
                        exe = out / (label + '-' + linkage)
                        libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp']
                                if linkage == 'dynamic' else [gomp / 'libgomp.a'])
                        run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', obj,
                            *libs, '-pthread', '-o', exe])
                        run(label + '-' + linkage + '-run', [exe])
                        checks += 2
                        executions += 1
for checking in ('0', '2'):
    for optimization in ('0', '2'):
        for mode in ('frame_root', 'frame_overlap', 'frame_alignment'):
            label = f'{mode}-{checking}-O{optimization}'
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', f'-I{src / "include"}',
                '-O' + optimization, '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={plugin}', '-fplugin-arg-memory_model_probe-mode=' + mode,
                '-c', root / 'calls.c', '-o', out / (label + '.o')], 1)
            assert 'memory probe did not mutate' not in message, message
            assert 'transaction transformed memory proof failed:' in message, label + '\n' + message
            checks += 1
print(f'Memória remapeada: {checks} verificações, {executions} execuções; C/C++, O0/O2, checking 0/2, DSO/archive.')
