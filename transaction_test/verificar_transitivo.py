"""Historical local graph: recursion and a refused member."""
from pathlib import Path
import subprocess
import re
import sys
root = Path(__file__).resolve().parent
src = root.parent / 'gcc-src-tm'
build = Path(sys.argv[1]).resolve()
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/graph'
out.mkdir(parents=True, exist_ok=True)
plugin = out / 'graph_probe.so'
def run(label, args, expected):
    result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=120)
    message = result.stdout + result.stderr
    (out / (label + '.log')).write_text(message)
    if result.returncode != expected:
        raise RuntimeError(label + '\n' + message)
    return message
run('plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'graph_probe_plugin.cc', '-o', plugin], 0)
checks = 1
cfg_plugin = out / 'cfg_abi_probe.so'
run('cfg-abi-plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'cfg_abi_probe_plugin.cc', '-o', cfg_plugin], 0)
checks += 1
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for mode, diagnostic in (
                ('abi', 'transaction ABI mismatch in transformed call'),
                ('target', 'transaction transformed body has an unverified call target'),
                ('indirect', 'transaction transformed body has an unverified call target'),
                ('channels', 'transaction transformed prepared call has incompatible channels'),
                ('context', 'transaction transformed call lost its borrowed context'),
                ('context_overwrite', 'transaction transformed call lost its borrowed context'),
                ('context_alias', 'transaction transformed call lost its borrowed context'),
                ('context_copy', None)):
            label = f'cfg-{mode}-{language.replace("+", "p")}-{checking}'
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
                '-O0', '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={cfg_plugin}', f'-fplugin-arg-cfg_abi_probe-mode={mode}',
                '-c', root / 'graph_cycle.c', '-o', out / (label + '.o')], 0 if diagnostic is None else 1)
            if diagnostic is not None:
                assert diagnostic in message, message
            checks += 1
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for mode, diagnostic, source in (
                ('offset_flag', 'transaction transformed offset lacks an overflow guard', 'memory_static_array.c'),
                ('offset_projection', 'transaction transformed offset lost its checked arithmetic', 'memory_static_array.c'),
                ('offset_operator', 'transaction transformed offset lost its checked arithmetic', 'memory_static_array.c'),
                ('offset_unchecked', 'transaction transformed offset lost its checked arithmetic', 'memory_static_array.c'),
                ('offset_bypass', 'transaction transformed offset lost its checked arithmetic', 'memory_static_array.c'),
                ('memory_rebind', 'transaction transformed access changed its static identity', 'memory_static_access.c'),
                ('memory_insert', 'transaction transformed access lost its operation identity', 'memory_empty.c'),
                ('memory_size', 'transaction transformed access changed its static identity', 'memory_static_access.c'),
                ('memory_offset', 'transaction transformed access changed its static identity', 'memory_static_access.c'),
                ('memory_offset_max', 'transaction transformed access lacks a range guard', 'memory_static_array.c'),
                ('memory_operation', 'transaction transformed access lost its operation identity', 'memory_static_access.c'),
                ('memory_duplicate', 'transaction transformed access lost its operation identity', 'memory_static_access.c'),
                ('registration_rebind', 'transaction transformed registration changed its object identity', 'memory_registration.c'),
                ('registration_self_offset', 'transaction transformed registration', 'memory_registration.c'),
                ('registration_valid_extent', 'transaction transformed registration changed its object identity', 'memory_registration.c'),
                ('registration_valid_kind', 'transaction transformed registration changed its object identity', 'memory_registration.c'),
                ('registration_duplicate', 'transaction transformed registration changed its object identity', 'memory_registration.c'),
                ('registration_object', 'transaction transformed registration lacks an identity guard', 'memory_registration.c'),
                ('registration_extent_zero', 'transaction transformed registration lacks an identity guard', 'memory_registration.c'),
                ('registration_extent_max', 'transaction transformed registration lacks an identity guard', 'memory_registration.c'),
                ('registration_kind', 'transaction transformed registration lacks an identity guard', 'memory_registration.c'),
                ('controller_capture_swap', 'transaction capture changed its remapping origin', 'graph_cycle.c'),
                ('controller_capture_truncate', 'transaction capture changed its remapping origin', 'graph_cycle.c'),
                ('capture_field_swap', 'transaction capture changed its remapping origin', 'capture_remap.c'),
                ('capture_store_swap', 'transaction capture changed its remapping origin', 'capture_remap.c'),
                ('parameter_owner', 'transaction prepared signature lost its parameter correspondence', 'graph_cycle.c'),
                ('formal_swap', 'transaction prepared signature lost its parameter correspondence', 'graph_cycle.c'),
                ('frame_field', 'transaction transformed operand lost its frame correspondence', 'graph_cycle.c'),
                ('controller_remove', 'transaction controller lacks a unique lowered body invocation', 'graph_cycle.c'),
                ('controller_duplicate', 'transaction controller lacks a unique lowered body invocation', 'graph_cycle.c'),
                ('controller_channels', 'transaction controller lost its lowered body correspondence', 'graph_cycle.c'),
                ('controller_owner', 'transaction controller lost its lowered body correspondence', 'controller_parallel.c')):
            label = f'{mode}-{language.replace("+", "p")}-{checking}'
            message = run(label, [gcc / 'xgcc', f'-B{gcc}/', f'-I{src / "include"}', '-x', language,
                '-O0', '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                f'-fplugin={cfg_plugin}', f'-fplugin-arg-cfg_abi_probe-mode={mode}',
                '-c', root / source, '-o', out / (label + '.o')], 1)
            assert diagnostic in message, message
            checks += 1
for source in ('capture_remap.c', 'memory_registration.c', 'memory_static_access.c', 'memory_static_array.c'):
    for language in ('c', 'c++'):
        for checking in ('0', '2'):
            label = f'{source[:-2]}-positive-{language.replace("+", "p")}-{checking}'
            run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O0', '-fopenmp',
                '-fopenmp-transaction-stm', '-fchecking=' + checking, '-c', root / source,
                '-o', out / (label + '.o')], 0)
            checks += 1
            for linkage in ('dynamic', 'archive'):
                binary = out / (label + '-' + linkage)
                libs = ([f'-L{gomp}', f'-Wl,-rpath,{gomp}', '-lgomp'] if linkage == 'dynamic'
                        else [gomp / 'libgomp.a'])
                run(label + '-' + linkage + '-link', [gcc / 'xgcc', f'-B{gcc}/', out / (label + '.o'),
                    *libs, '-pthread', '-o', binary], 0)
                run(label + '-' + linkage + '-run', [binary], 0)
                checks += 2
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        for mode in ('registration_copy', 'registration_self_copy'):
            label = f'{mode}-{language.replace("+", "p")}-{checking}'
            run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O0', '-fopenmp',
                '-fopenmp-transaction-stm', '-fchecking=' + checking, f'-fplugin={cfg_plugin}',
                '-fplugin-arg-cfg_abi_probe-mode=' + mode, '-c', root / 'memory_registration.c',
                '-o', out / (label + '.o')], 0)
            checks += 1
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        label = f'memory-copy-{language.replace("+", "p")}-{checking}'
        run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O0', '-fopenmp',
            '-fopenmp-transaction-stm', '-fchecking=' + checking, f'-fplugin={cfg_plugin}',
            '-fplugin-arg-cfg_abi_probe-mode=memory_copy', '-c', root / 'memory_static_access.c',
            '-o', out / (label + '.o')], 0)
        checks += 1
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        label = f'offset-copy-{language.replace("+", "p")}-{checking}'
        run(label, [gcc / 'xgcc', f'-B{gcc}/', '-x', language, '-O0', '-fopenmp',
            '-fopenmp-transaction-stm', '-fchecking=' + checking, f'-fplugin={cfg_plugin}',
            '-fplugin-arg-cfg_abi_probe-mode=offset_copy', '-c', root / 'memory_static_array.c',
            '-o', out / (label + '.o')], 0)
        checks += 1
lto_plugin = out / 'lto_boundary_probe.so'
run('lto-plugin', ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
    f'-I{gcc}', f'-I{src / "gcc"}', f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
    root / 'lto_boundary_probe_plugin.cc', '-o', lto_plugin], 0)
checks += 1
for language in ('c', 'c++'):
    for checking in ('0', '2'):
        label = f'lto-{language.replace("+", "p")}-{checking}'
        obj, linked = out / (label + '.o'), out / (label + '-linked.o')
        frontend = run(label + '-frontend', [gcc / 'xgcc', f'-B{gcc}/', '-x', language,
            '-O0', '-flto', '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
            f'-fplugin={lto_plugin}', '-c', root / 'graph_cycle.c', '-o', obj], 0)
        match = re.search(r'NEWTM_LTO_FINISH 0 (\d+) (\d+)', frontend)
        assert match and int(match[1]) > 0 and int(match[2]) > 0, frontend
        linked_log = run(label + '-link', [gcc / 'xgcc', f'-B{gcc}/', '-O0', '-flto',
            '-fuse-linker-plugin', f'-fplugin={lto_plugin}', '-r', obj, '-o', linked], 0)
        for stage in ('START', 'IPA', 'FINISH'):
            assert f'NEWTM_LTO_{stage} 1 0 0' in linked_log, linked_log
        checks += 2
for checking in ('0', '2'):
    label = f'real-nested-{checking}'
    message = run(label, [gcc / 'xgcc', f'-B{gcc}/', f'-I{src / "include"}', '-x', 'c',
                  '-O0', '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                  f'-fplugin={plugin}', '-fplugin-arg-graph_probe-real_nested=1',
                  '-c', root / 'calls.c', '-o', out / (label + '.o')], 0)
    assert 'GRAPH_NESTED observed' in message, message
    checks += 1
for source in ('calls.c', 'graph_cycle.c'):
    for language in ('c', 'c++'):
        for checking in ('0', '2'):
            for mode in ('normal', 'invalidate', 'post', 'mutate', 'signature', 'signature_args'):
                label = f'{source[:-2]}-{language.replace("+", "p")}-{checking}-{mode}'
                args = [gcc / 'xgcc', f'-B{gcc}/', f'-I{src / "include"}', '-x', language,
                        '-O0', '-fopenmp', '-fopenmp-transaction-stm', '-fchecking=' + checking,
                        *([f'-fplugin={plugin}'] if mode != 'normal' else []),
                        *(['-fplugin-arg-graph_probe-post=1'] if mode == 'post' else []),
                        *(['-fplugin-arg-graph_probe-mutual=1'] if mode == 'post' and source == 'graph_cycle.c' else []),
                        *(['-fplugin-arg-graph_probe-mutate=1'] if mode == 'mutate' else []),
                        *(['-fplugin-arg-graph_probe-signature=1'] if mode == 'signature' else []),
                        *(['-fplugin-arg-graph_probe-signature_args=1'] if mode == 'signature_args' else []),
                        '-c', root / source, '-o', out / (label + '.o')]
                message = run(label, args, 1 if mode == 'invalidate' else 0)
                if mode == 'invalidate':
                    assert ('transaction graph lacks an approved local verification' in message
                            or 'transaction graph lacks its verified controller dependency' in message), message
                if mode == 'post':
                    assert 'GRAPH_POST invalidated' in message, message
                if mode == 'mutate':
                    assert 'GRAPH_MUTATION invalidated' in message, message
                if mode in ('signature', 'signature_args'):
                    assert 'GRAPH_SIGNATURE invalidated' in message, message
                checks += 1
print(f'Grafo local: {checks} verificações aprovadas; recursão e membro invalidado, C/C++, checking 0/2.')
