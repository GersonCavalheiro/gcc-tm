"""Execute dormant prepared clones through a test-only entry-point plugin.
No activation or claim of automatic STM access instrumentation is made here.
"""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
src = root.parent / 'gcc-src-tm'
gcc = build / 'gcc'
gomp = build / 'x86_64-pc-linux-gnu/libgomp/.libs'
out = root / '.build/clones'
out.mkdir(parents=True, exist_ok=True)
report = []
def run(label, args, expected=0):
    r = subprocess.run(list(map(str,args)), capture_output=True, text=True, timeout=90)
    (out / (label+'.log')).write_text(r.stdout+r.stderr)
    report.append({'case':label,'passed':r.returncode==expected})
    if r.returncode != expected:
        print(r.stdout+r.stderr)
        raise RuntimeError(label)
    return r.stdout + r.stderr
try:
    plugin = out / 'clone_probe.so'
    run('plugin', ['g++','-std=gnu++11','-shared','-fPIC','-fno-rtti',f'-I{gcc}',
         f'-I{src / "gcc"}',f'-I{src / "include"}', f'-I{src / "libcpp/include"}',
         root/'clone_probe_plugin.cc','-o',plugin])
    for lang in ('c','c++'):
        for opt in ('-O0','-O2'):
            for linkage in ('dynamic','archive'):
                for target, cause in (('none',0),('fail_leaf',1),('fail_leaf',2),('fail_pointer',1),('fail_pointer',2)):
                    label=f'{lang.replace("+","p")}-{opt[1:]}-{linkage}-{target}-cause{cause}'
                    obj=out/(label+'.o')
                    flags=[str(gcc/'xgcc'),f'-B{gcc}/','-Wall','-Wextra','-Werror',
                           '-UNDEBUG',f'-I{src / "include"}', '-fopenmp','-fchecking=2',opt]
                    inject=[] if not cause else [f'-fplugin-arg-clone_probe-inject={target}',
                                                 f'-fplugin-arg-clone_probe-cause={cause}']
                    run(label+'-compile', flags+['-x',lang,f'-DCAUSE={cause if target == "fail_leaf" else 0}',
                        f'-DPOINTER_CAUSE={cause if target == "fail_pointer" else 0}',f'-fplugin={plugin}',
                        *inject,'-fdump-tree-gimple','-fdump-tree-omplower','-c',root/'clones.c','-o',obj])
                    binary=out/label
                    libs=([f'-L{gomp}',f'-Wl,-rpath,{gomp}','-lgomp'] if linkage=='dynamic'
                          else [gomp/'libgomp.a','-pthread'])
                    if lang == 'c++':
                        libs.append(subprocess.check_output(['g++', '-print-file-name=libstdc++.so'], text=True).strip())
                    run(label+'-link', [f for f in flags if f not in ('-fopenmp', '-fchecking=2')] + [obj,*libs,'-o',binary])
                    run(label+'-run',[binary])
                    symbols=run(label+'-symbols',['nm',binary])
                    assert '.newtm.v1' in symbols
                    assert '__newtm_publish_result' not in symbols
    # Two translation units share inline/COMDAT originals and an inline object.
    # Clones remain local; generated bodies/bindings must survive LTO unaided.
    common = out / 'shared.hh'
    common.write_text("""#include "newtm-abi.h"
#include <assert.h>
inline int shared_object = 5;
template<class T> inline T shared_value(T n) { return n+1; }
inline int *shared_pointer(void) { return &shared_object; }
extern "C" uint32_t invoke_shared_value(struct newtm_context*, const struct newtm_clone_binding_v1*, int, int*);
extern "C" uint32_t invoke_shared_pointer(struct newtm_context*, const struct newtm_clone_binding_v1*, int**, struct newtm_clone_binding_v1*);
""")
    units=[]
    for unit, n in [('a',7),('b',9)]:
        source=out/(unit+'.C')
        source.write_text(f"""#include "shared.hh"
__attribute__((used)) void select_{unit}(void){{int r=0;int*p=&shared_object;
#pragma omp transaction(r,p,shared_object)
{{r=shared_value({n});p=shared_pointer();}}
(void)r;(void)p;
}}
extern "C" int run_{unit}(struct newtm_context *ctx){{
int result=-91;int*p=0;struct newtm_clone_binding_v1 binding;
assert(invoke_shared_value(ctx,0,{n},&result)==0);
assert(invoke_shared_pointer(ctx,0,&p,&binding)==0);
assert(p==&shared_object && binding.identity.object==&shared_object);
assert(binding.identity.object_size==sizeof(int) && binding.identity.offset==0);
assert(binding.identity.kind==NEWTM_ABI_SCALAR && binding.storage==NEWTM_STORAGE_REGISTERED);
assert(shared_value({n})=={n+1} && shared_pointer()==&shared_object);
return result;
}}
""" + ("""
extern "C" int run_b(struct newtm_context*);
int main(void){struct newtm_context*slot=0;uint32_t outer=0;
assert(GOMP_transaction_stm_v1_begin(&slot,&outer)==0 && outer);
assert(GOMP_transaction_stm_v1_register(slot,&shared_object,sizeof(int),NEWTM_ABI_SCALAR)==0);
assert(run_a(slot)==8 && run_b(slot)==10);
assert(GOMP_transaction_stm_v1_end(&slot)==0 && !slot);return 0;}
""" if unit=='a' else ''))
        units.append(source)
    for opt in ('-O0','-O2'):
        for linkage in ('dynamic','archive'):
            for lto in (False,True):
                label=f'odr-{opt[1:]}-{linkage}-lto{int(lto)}'
                flags=[str(gcc/'xgcc'),f'-B{gcc}/','-std=c++17','-Wall','-Wextra','-Werror',
                       '-UNDEBUG',f'-I{src / "include"}',opt,*(['-flto'] if lto else [])]
                objects=[]
                for source in units:
                    obj=out/(label+'-'+source.stem+'.o');objects.append(obj)
                    run(label+'-'+source.stem+'-compile',flags+['-fopenmp','-fchecking=2',
                        f'-fplugin={plugin}','-c',source,'-o',obj])
                libs=([f'-L{gomp}',f'-Wl,-rpath,{gomp}','-lgomp'] if linkage=='dynamic'
                      else [gomp/'libgomp.a','-pthread'])
                libs.append(subprocess.check_output(['g++','-print-file-name=libstdc++.so'],text=True).strip())
                binary=out/label
                run(label+'-link',flags+objects+libs+['-o',binary])
                run(label+'-run',[binary])
                symbols=run(label+'-symbols',['nm',binary])
                assert '.newtm.v1' in symbols
                assert '__newtm_publish_result' not in symbols
    for language in ('c','c++'):
        bad=out/('bad-return-'+language.replace('+','p')+'.c')
        bad.write_text('int g;int bad(void){\n#pragma omp transaction(g)\n{return 1;}return 0;}\n'
                       'void choose(void){int r=0;\n#pragma omp transaction(g,r)\n{r=bad();}(void)r;}\n')
        message=run('source-structured-'+language.replace('+','p'),
             [str(gcc/'xgcc'),f'-B{gcc}/','-x',language,'-fopenmp','-fchecking=2',
              '-c',bad,'-o',out/'bad.o'],expected=1)
        assert 'structured block' in message and 'internal compiler error' not in message
    tree_tests=[('c-c++-common/gomp/transaction-clone-1.c',('c','c++')),
                ('c-c++-common/gomp/transaction-clone-2.c',('c','c++')),
                ('g++.dg/gomp/transaction-clone-template-1.C',('c++',)),
                ('gcc.dg/gomp/transaction-clone-capture-1.c',('c',))]
    for number,(name,languages) in enumerate(tree_tests):
        for language in languages:
            run(f'tree-{number}-{language.replace("+","p")}',
                [str(gcc/'xgcc'),f'-B{gcc}/','-x',language,'-fopenmp','-fchecking=2','-g',
                 '-c',src/'gcc/testsuite'/name,'-o',out/f'tree-{number}-{language.replace("+","p")}.o'])
    print(f'Clones: {len(report)} checks passed, 48 executions (C/C++, O0/O2, DSO/archive, failures, ODR/LTO).')
finally:
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
