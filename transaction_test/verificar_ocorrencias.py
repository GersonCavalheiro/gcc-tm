"""Verify source-to-GIMPLE bindings without enabling functional STM lowering.

Execution checks evaluation counts/results; dumps check actual evaluated refs,
call arguments, RMW grouping, prepared candidates, copies and metadata lifetime.
"""
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
out = root / '.build' / 'occurrences'
out.mkdir(parents=True, exist_ok=True)
cc = build / 'gcc' / 'xgcc'
gomp = build / 'x86_64-pc-linux-gnu' / 'libgomp'


def region(objects, body):
    return f'\n#pragma omp transaction ({objects})\n{{{body}}}\n'


cases = [
    ('post-index', ('c', 'c++'), '', 'int a[3]={0},i=0;', 'a,i',
     'a[i++]=7;', 'i==1&&a[0]==7&&a[1]==0'),
    ('rmw-index', ('c', 'c++'), '', 'int a[3]={10,0,0},i=0,v=3;', 'a,i,v',
     'a[i++]+=v;', 'i==1&&a[0]==13&&a[1]==0'),
    ('post-value', ('c', 'c++'), '', 'int x=2,r=0;', 'x,r',
     'r=x++;', 'x==3&&r==2'),
    ('pre-value', ('c', 'c++'), '', 'int x=2,r=0;', 'x,r',
     'r=++x;', 'x==3&&r==3'),
    ('two-reads', ('c', 'c++'), '', 'int a=3,r=0;', 'a,r',
     'r=a;r+=a;', 'r==6&&a==3'),
    ('call-args', ('c', 'c++'), 'int combine(int a,int b){return a*10+b;}',
     'int i=1,j=2,r=0;', 'i,j,r', 'r=combine(i++,j++);', 'r==12&&i==2&&j==3'),
    ('pointer-callee', ('c', 'c++'), 'int change(int*p){return ++*p;}',
     'int a=3,r=0;int*p=&a;', 'a,r,p', 'r=change(p);', 'a==4&&r==4&&p==&a'),
    ('field-rmw', ('c',), 'struct S{int a[3];int n;};',
     'struct S s={{10,0,0},3};int i=0;', 's,i', 's.a[i++]+=s.n;',
     'i==1&&s.a[0]==13&&s.n==3'),
    ('nested', ('c', 'c++'), '', 'int a=1,b=2;', 'a',
     region('b', 'b+=a;')+'a+=b;', 'a==4&&b==3'),
    ('conditional', ('c', 'c++'), '', 'int a[2]={0},i=0,yes=1;', 'a,i,yes',
     'if(yes)a[i++]=1;else a[i++]=2;', 'i==1&&a[0]==1&&a[1]==0'),
    ('constant', ('c', 'c++'), '', 'const int k=3;int r=0;', 'k,r',
     'r=k+2;', 'r==5'),
    ('reference', ('c++',), 'int change(int&r){return ++r;}',
     'int a=3,r=0;int&alias=a;', 'a,r', 'r=change(alias);', 'a==4&&r==4'),
    ('aggregate-copy', ('c',), 'struct S{int a[2];int b;};',
     'struct S a={{1,2},3},b={{4,5},6};', 'a,b', 'a=b;',
     'a.a[0]==4&&a.a[1]==5&&a.b==6'),
    ('pointer-offset', ('c', 'c++'), 'int change(int*p){return ++*p;}',
     'int a[2]={3,0},i=0,r=0;int*p=&a[0];', 'a,i,r,p',
     'r=change(p+i++);', 'a[0]==4&&a[1]==0&&i==1&&r==4'),
    ('call-lvalue-rmw', ('c', 'c++'), 'int*at(int*p,int k){return p+k;}',
     'int a[2]={10,0},i=0,v=3;int*p=&a[0];', 'a,i,v,p',
     '*at(p,i++)+=v;', 'i==1&&a[0]==13&&a[1]==0'),
    ('parallel', ('c', 'c++'), '', 'int sum=0;', '', '', ''),
]

bindings = re.compile(r'/\* newtm-bind op=(\d+) source=(\d+) parent=(\d+) group=(\d+) origin=(\w+) mode=([\w-]+)(.*?) \*/')
report = []
for name, languages, prefix, declarations, registered, body, expected in cases:
    for language in languages:
        for optimization in ('-O0', '-O2'):
            label = f'{name}-{language.replace("+", "p")}-{optimization[1:]}'
            if name == 'parallel':
                source = prefix + '''int main(void){int sum=0;
#pragma omp parallel num_threads(2) reduction(+:sum)
{int a[2]={0},i=0;
#pragma omp transaction(a,i)
{a[i++]=7;}sum+=a[0]+i;}
return sum!=16;}'''
            else:
                source = prefix + 'int main(void){' + declarations + region(registered, body) + f'return !({expected});}}'
            path = out / (label + ('.C' if language == 'c++' else '.c'))
            path.write_text(source)
            binary = out / label
            command = [str(cc), f'-B{build / "gcc"}/', f'-B{gomp}/', '-fopenmp', f'-I{gomp}',
                       optimization, '-fno-inline', '-fdump-tree-gimple', '-fdump-tree-omplower',
                       '-fdump-tree-lower', str(path), f'-L{gomp / ".libs"}',
                       f'-Wl,-rpath,{gomp / ".libs"}', '-o', str(binary)]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            reasons = []
            logs = compiled.stdout + compiled.stderr
            if compiled.returncode:
                reasons.append('compilation failed')
            else:
                run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
                logs += run.stdout + run.stderr
                if run.returncode or run.stderr: reasons.append('incorrect execution/evaluation count')
                dump_paths = list(out.glob(label + '*.gimple'))
                text = ''.join(p.read_text() for p in dump_paths)
                rows = bindings.findall(text)
                if not rows: reasons.append('missing bindings')
                ids = [row[0] for row in rows]
                if len(ids) != len(set(ids)): reasons.append('operation IDs reused')
                if any('++' in row[6] or '--' in row[6] for row in rows if row[5] != 'source-only'):
                    reasons.append('source effect reused in evaluated operands')
                if not any(row[5] == 'register' for row in rows): reasons.append('missing registration argument binding')
                if name in ('post-index', 'rmw-index', 'field-rmw'):
                    stores = [row for row in rows if row[5] in ('store','load-store') and re.search(r'write-ref=.*a\[', row[6])]
                    if not stores or any(re.search(r'\[i\]', row[6]) for row in stores):
                        reasons.append('index not bound to evaluated operand')
                if name == 'rmw-index':
                    reads = [row for row in rows if row[5] in ('load','load-store') and re.search(r'read-ref=a\[', row[6])]
                    writes = [row for row in rows if row[5] in ('store','load-store') and re.search(r'write-ref=a\[', row[6])]
                    refs_read = [re.search(r'read-ref=(a\[[^]]+\])', row[6]).group(1) for row in reads]
                    refs_write = [re.search(r'write-ref=(a\[[^]]+\])', row[6]).group(1) for row in writes]
                    if not refs_read or refs_read != refs_write: reasons.append('RMW read/write uses different evaluated index')
                    if not reads or not writes or reads[0][3] != writes[0][3] or reads[0][3] == '0':
                        reasons.append('RMW evaluation group lost')
                if name in ('call-args','pointer-callee','reference'):
                    callee = 'combine' if name == 'call-args' else 'change'
                    if not any(row[5] == 'call' and f'callee={callee}' in row[6] and 'arguments=[' in row[6] for row in rows):
                        reasons.append('missing evaluated call arguments')
                    helper = text.split('int main')[0]
                    if 'newtm-bind' not in helper: reasons.append('prepared candidate body not tracked')
                    if 'mode=return result=' not in helper: reasons.append('prepared candidate return not bound')
                if name == 'two-reads':
                    loads = [row for row in rows if row[5] == 'load' and 'read-ref=a ' in row[6]]
                    if len(loads) != 2: reasons.append('distinct same-line reads merged')
                if name == 'aggregate-copy' and not any(row[5] == 'load-store' and 'read-ref=b write-ref=a ' in row[6] for row in rows):
                    reasons.append('aggregate copy did not bind both actual refs')
                if name == 'call-lvalue-rmw':
                    calls = [row for row in rows if row[5] == 'call' and 'callee=at ' in row[6]]
                    if len(calls) != 1: reasons.append('saved lvalue call not evaluated exactly once')
                if name == 'constant' and not any(row[5] == 'source-only' and 'witness=k ' in row[6] and 'result=none' in row[6] for row in rows):
                    reasons.append('folded semantic read lost or assigned fictitious result')
                lower = ''.join(p.read_text() for p in out.glob(label + '*.lower'))
                if 'newtm-bind' in lower: reasons.append('bindings leaked beyond High GIMPLE')
                if 'GOMP_transaction_stm' in text or 'newtm_read (' in text:
                    reasons.append('functional integration unexpectedly enabled')
            (out / (label + '.log')).write_text(logs + '\n'.join(reasons))
            report.append({'case': label, 'passed': not reasons, 'reasons': reasons, 'command': command})
            if reasons: print(f'FAIL: {label}: {reasons}\n{compiled.stderr}')
# Independent consumer: copy and change evaluated operands, then check that
# metadata follows the new instruction and the original remains unchanged.
plugin = out / 'occurrence_copy_plugin.so'
plugin_command = ['g++', '-std=gnu++11', '-shared', '-fPIC', '-fno-rtti',
                  f'-I{build / "gcc"}', f'-I{root.parent / "gcc-src-tm/gcc"}',
                  f'-I{root.parent / "gcc-src-tm/include"}',
                  f'-I{root.parent / "gcc-src-tm/libcpp/include"}',
                  str(root / 'occurrence_copy_plugin.cc'), '-o', str(plugin)]
compiled_plugin = subprocess.run(plugin_command, capture_output=True, text=True, timeout=60)
(out / 'plugin-build.log').write_text(compiled_plugin.stdout + compiled_plugin.stderr)
report.append({'case': 'copy-consumer-build', 'passed': compiled_plugin.returncode == 0,
               'command': plugin_command})
if compiled_plugin.returncode:
    print('FAIL: copy-consumer-build\n' + compiled_plugin.stderr)
else:
    for language in ('c', 'c++'):
        for optimization in ('-O0', '-O2'):
            label = f'copy-consumer-{language.replace("+", "p")}-{optimization[1:]}'
            path = out / (label + ('.C' if language == 'c++' else '.c'))
            path.write_text('int combine(int a,int b){return a*10+b;}int main(void){'
                            'int a[2]={10,0},i=0,j=2,r=0;'
                            + region('a,i,j,r', 'a[i++]+=3;r=combine(i++,j++);')
                            + 'return !(a[0]==13&&i==2&&j==3&&r==12);}')
            binary = out / label
            command = [str(cc), f'-B{build / "gcc"}/', f'-B{gomp}/', '-fopenmp',
                       optimization, f'-fplugin={plugin}', str(path), f'-L{gomp / ".libs"}',
                       f'-Wl,-rpath,{gomp / ".libs"}', '-o', str(binary)]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            passed = (compiled.returncode == 0 and 'newtm-copy-store:' in compiled.stderr
                      and 'write-ref=a[0]' in compiled.stderr and 'copy-of=' in compiled.stderr
                      and 'newtm-copy-call:' in compiled.stderr and 'arguments=[99,' in compiled.stderr)
            logs = compiled.stdout + compiled.stderr
            if passed:
                run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
                passed = run.returncode == 0
                logs += run.stdout + run.stderr
            (out / (label + '.log')).write_text(logs)
            report.append({'case': label, 'passed': passed, 'command': command})
            if not passed: print(f'FAIL: {label}\n{compiled.stderr}')
(out / 'resultado.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'{sum(row["passed"] for row in report)}/{len(report)} verificacoes de ocorrencias')
raise SystemExit(not all(row['passed'] for row in report))
