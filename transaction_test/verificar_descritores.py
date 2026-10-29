"""Execute generated descriptors against an independent integer range oracle.

The test-only plugin checks fields and returns BEFORE invalid original accesses.
The production tracing bridge still does not consume descriptor validity.
"""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
out = root / '.build' / 'descriptors'
out.mkdir(parents=True, exist_ok=True)
cc = build / 'gcc' / 'xgcc'
gomp = build / 'x86_64-pc-linux-gnu' / 'libgomp'
plugin = out / 'descriptor_probe_plugin.so'
command = ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
           f'-I{build / "gcc"}', f'-I{root.parent / "gcc-src-tm/gcc"}',
           f'-I{root.parent / "gcc-src-tm/include"}',
           f'-I{root.parent / "gcc-src-tm/libcpp/include"}',
           str(root / 'descriptor_probe_plugin.cc'), '-o', str(plugin)]
r = subprocess.run(command, capture_output=True, text=True, timeout=60)
(out / 'plugin-build.log').write_text(r.stdout + r.stderr)
if r.returncode:
    print(r.stderr)
    sys.exit(1)
report = []
# All targets in this environment are LP64; no claim of native 32-bit coverage.
values = [-2**63, -2**62, -3, -1, 0, 1, 2, 3, 4, 2**61, 2**62, 2**63-1]
uvalues = [0, 1, 2, 3, 4, 2**61, 2**62, 2**63, 2**64-1]
for name, index_type, body, items in [
    ('array', 'long', 'a[i]=7;', values),
    ('unsigned-array', 'unsigned long', 'a[i]=7;', uvalues),
    ('pointer', 'long', '*(p+i)=7;', values),
    ('unsigned-pointer', 'unsigned long', '*(p+i)=7;', uvalues),
    ('middle-pointer', 'long', '*(p+i)=7;', values),
    ('once', 'long', 'a[i++]=7;', [0, 1, 2]),
    ('field', 'long', 'a.items[i]=7;', values),
    ('matrix', 'long', 'a[0][i]=7;', values),
    ('reference-element', 'long', 'alias=7;', [0]),
]:
    field = name == 'field'
    matrix = name == 'matrix'
    reference = name == 'reference-element'
    base_offset = 4 if name == 'middle-pointer' or field else 0
    extent = 4 if reference else 24 if matrix else 16 if field else 12
    checks = []
    for v in items:
        pos = 0 if reference else base_offset + v * 4
        valid = 0 <= v < 3 if 'pointer' not in name else 0 <= pos and pos + 4 <= extent
        if name == 'middle-pointer': valid = 0 <= pos and pos + 4 <= extent
        literal = ('(-9223372036854775807L-1)' if v == -2**63 else str(v) + ('UL' if index_type.startswith('unsigned') else 'L'))
        checks.append(f'want={int(valid)};expected_offset={max(pos,0) if valid else 0}UL;'
                      f'if(probe({literal})!={7 if valid else -100})__builtin_abort();')
    for language in (('c++',) if reference else ('c',) if field else ('c', 'c++')):
        for opt in ('-O0', '-O2'):
            label = f'{name}-{language.replace("+", "p")}-{opt[1:]}'
            source = out / (label + ('.C' if language == 'c++' else '.c'))
            declaration = 'struct S{int prefix;int items[3];};struct S a={0};' if field else 'int a[3]={0};'
            if matrix: declaration = 'int a[2][3]={0};'
            if reference: declaration += 'int&alias=a[1];'
            pointer = 'int*p=&a[1];' if name == 'middle-pointer' else 'int*p=&a[0];'
            registered = 'alias' if reference else 'a,i' if field or matrix else 'a,i,p'
            total = 'a.items[0]+a.items[1]+a.items[2]' if field else 'a[0]+a[1]+a[2]'
            if matrix: total = 'a[0][0]+a[0][1]+a[0][2]'
            source.write_text(f'''static void*expected_base;static unsigned long expected_offset;
static int want,seen;
__attribute__((used,noinline)) void inspect_descriptor(void*o,unsigned long n,
unsigned long off,unsigned kind,bool valid){{
if(o!=expected_base||n!={extent}||kind!={0 if reference else 1}||valid!=!!want||(want&&off!=expected_offset))__builtin_abort();++seen;}}
__attribute__((noinline)) int probe({index_type} i){{{declaration}
{'' if field or matrix or reference else pointer}expected_base={'&a[1]' if reference else '&a'};
#pragma omp transaction({registered})
{{{body}}}
return {total};}}
int main(void){{{''.join(checks)}return seen!={len(items)};}}
'''.replace('bool valid', '_Bool valid' if language == 'c' else 'bool valid'))
            binary = out / label
            cmd = [str(cc), f'-B{build / "gcc"}/', f'-B{gomp}/', '-fopenmp', opt,
                   f'-fplugin={plugin}', '-fchecking=2', '-fdump-tree-gimple', str(source),
                   f'-L{gomp / ".libs"}', f'-Wl,-rpath,{gomp / ".libs"}', '-o', str(binary)]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            logs = result.stdout + result.stderr
            ok = result.returncode == 0
            if ok:
                run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
                logs += run.stdout + run.stderr
                ok = run.returncode == 0
            (out / (label + '.log')).write_text(logs)
            report.append({'case': label, 'passed': ok, 'boundary_probes': len(items)})
            print(('PASS ' if ok else 'FAIL ') + label)
# Inspect all descriptor roles, canonical types, private storage and explicit
# incoming/result channels. These channels must not carry invented extents.
for language in ('c', 'c++'):
    for opt in ('-O0', '-O2'):
        label = f'schema-{language.replace("+", "p")}-{opt[1:]}'
        source = out / (label + ('.C' if language == 'c++' else '.c'))
        source.write_text("""int*at(int*p,int i){return p+i;}
int step(int*p){int scratch[1]={0};scratch[0]=1;*p+=scratch[0];return *p;}
int main(void){int a[2]={0},i=0,r=0;const int k=3;int*p=&a[0];
#pragma omp transaction(a,i,r,p,k)
{r=step(at(p,i++));r+=k;}
return !(a[0]==1&&i==1&&r==4&&p==&a[0]);}
""")
        binary = out / label
        cmd = [str(cc), f'-B{build / "gcc"}/', f'-B{gomp}/', '-fopenmp', opt,
               f'-fplugin={plugin}', '-fchecking=2', '-fplugin-arg-descriptor_probe_plugin-schema',
               '-fdump-tree-gimple', '-fdump-tree-omplower', str(source),
               f'-L{gomp / ".libs"}', f'-Wl,-rpath,{gomp / ".libs"}', '-o', str(binary)]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        logs = result.stdout + result.stderr
        ok = result.returncode == 0
        if ok:
            run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            logs += run.stdout + run.stderr
            ok = run.returncode == 0
        lower = ''.join(p.read_text() for p in out.glob(label + '*.omplower'))
        ok = ok and 'newtm-descriptor' not in lower
        (out / (label + '.log')).write_text(logs)
        report.append({'case': label, 'passed': ok})
        print(('PASS ' if ok else 'FAIL ') + label)
(out / 'resultado.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'{sum(r["passed"] for r in report)}/{len(report)} descriptor tests passed')
sys.exit(0 if all(r['passed'] for r in report) else 1)
