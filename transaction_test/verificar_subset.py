"""Compile the admitted subset and reject unsafe source before GOMP integration.

No executable STM access is expected: positive execution checks ordinary
semantics/index evaluation, while dumps check the non-executable canonical plan.
"""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent
build = Path(sys.argv[1]).resolve()
output = root / ".build" / "subset"
output.mkdir(parents=True, exist_ok=True)
cc = build / "gcc" / "xgcc"
gomp = build / "x86_64-pc-linux-gnu" / "libgomp"

def region(objects, body):
    return f"\n#pragma omp transaction{f'({objects})' if objects else ''}\n{{{body}}}\n"

# name, languages, source, diagnostic (None means accepted).
cases = [
    ("scalar", ("c", "c++"), "void f(void){int a=0;" + region("a", "a++;") + "}", None),
    ("private", ("c", "c++"), "void f(void){int a=0;" + region("a", "const int b=2;for(int i=0;i<4;i++)if(i&1)a+=b;") + "}", None),
    ("missing", ("c", "c++"), "void f(int a,int b){" + region("a", "a+=b;") + "}", "requires explicit registration"),
    ("missing-const", ("c", "c++"), "void f(void){int a=0;const int b=2;" + region("a", "a+=b;") + "}", "requires explicit registration"),
    ("const", ("c", "c++"), "void f(void){int a=0;const int b=2;" + region("a,b", "a+=b;") + "}", None),
    ("volatile", ("c", "c++"), "void f(void){volatile int a=0;" + region("a", "a++;") + "}", "unsupported type"),
    ("union", ("c", "c++"), "union U{int a;double b;};void f(void){union U u;" + region("u", "u.a=1;") + "}", "unsupported type"),
    ("bitfield", ("c",), "struct S{unsigned b:1;};void f(void){struct S s;" + region("s", "s.b=1;") + "}", "unsupported type"),
    ("flexible", ("c",), "struct S{int n;int v[];};void f(struct S*s){" + region("s", "s->n=1;") + "}", "provable canonical object"),
    ("class", ("c++",), "struct S{int x;};void f(void){S s;" + region("s", "s.x=1;") + "}", "unsupported type"),
    ("vla", ("c", "c++"), "void f(int n){int a[n];" + region("a", "a[0]=1;") + "}", "unsupported type"),
    ("atomic", ("c",), "void f(void){_Atomic int a=0;" + region("a", "a++;") + "}", "unsupported type"),
    ("external", ("c", "c++"), "extern int external(void);void f(void){int a=0;" + region("a", "a=external();") + "}", "same-unit prepared definition"),
    ("indirect", ("c", "c++"), "void f(int(*p)(void)){int a=0;" + region("a,p", "a=p();") + "}", "same-unit prepared definition"),
    ("pure-external", ("c", "c++"), "extern int external(void) __attribute__((pure));void f(void){int a=0;" + region("a", "a=external();") + "}", "same-unit prepared definition"),
    ("forward", ("c", "c++"), "int helper(int);void f(void){int a=0;" + region("a", "a=helper(a);") + "}int helper(int v){return v+1;}", None),
    ("callee-global", ("c", "c++"), "int g;int helper(void){return g;}void f(void){int a=0;" + region("a", "a=helper();") + "}", "requires explicit registration"),
    ("callee-const-global", ("c", "c++"), "const int g=1;int helper(void){return g;}void f(void){int a=0;" + region("a", "a=helper();") + "}", "requires explicit registration"),
    ("callee-registered", ("c", "c++"), "int g;int helper(void){return g;}void f(void){int a=0;" + region("a,g", "a=helper();") + "}", None),
    ("unknown-pointer", ("c", "c++"), "void f(int*p){" + region("p", "*p=1;") + "}", "provable canonical object"),
    ("alias-pointer", ("c", "c++"), "void helper(int*p){(*p)++;}void f(void){int a=0;int*p=&a;" + region("a,p", "helper(p);*p+=1;") + "}", None),
    ("known-pointer-result", ("c", "c++"), "int g;int*helper(void){return &g;}void f(void){" + region("g", "*helper()=1;") + "}", None),
    ("unknown-pointer-result", ("c", "c++"), "int*helper(int*p){return p;}void f(int*p){" + region("p", "int*q=helper(p);(void)q;") + "}", "provable canonical identity"),
    ("reference", ("c++",), "void helper(int&v){v++;}void f(void){int a=0;int&r=a;" + region("a", "helper(r);r++;") + "}", None),
    ("reference-result", ("c++",), "int g;int&helper(void){return g;}void f(void){" + region("g", "helper()=1;") + "}", "cannot return a reference"),
    ("exception", ("c++",), "void f(void){" + region("", "throw 1;") + "}", "exceptions or user cleanups"),
    ("catch", ("c++",), "void f(void){" + region("", "try{}catch(...){ }") + "}", "exceptions or user cleanups"),
    ("cleanup", ("c",), "void clean(int*p){(void)p;}void f(void){" + region("", "int a __attribute__((cleanup(clean)))=1;") + "}", "exceptions or user cleanups"),
    ("destructor", ("c++",), "struct S{~S(){}};void f(void){" + region("", "S s;") + "}", "unsupported type"),
    ("assembly", ("c", "c++"), "void f(void){" + region("", '__asm__ volatile("":::"memory");') + "}", "external assembly effects"),
    ("lock", ("c", "c++"), "void f(void){" + region("", "\n#pragma omp critical\n{}") + "}", "OpenMP effects"),
    ("conditional-register", ("c", "c++"), "void f(int a,int b){" + region("a", "if(a){" + region("b", "b++;") + "}b++;") + "}", "requires explicit registration"),
    ("before-register", ("c", "c++"), "void f(int a,int b){" + region("a", "a+=b;" + region("b", "b++;")) + "}", "requires explicit registration"),
    ("after-register", ("c", "c++"), "void f(int a,int b){" + region("a", region("b", "b++;") + "a+=b;") + "}", None),
    ("array", ("c", "c++"), "void f(void){int v[4]={0};int i=1;" + region("v,i", "v[i++]=1;v[0]=v[1];") + "}", None),
    ("fields", ("c",), "struct S{int v[4];int x;};void f(void){struct S s={{0},0};int i=1;" + region("s,i", "s.x=s.v[i++];") + "}", None),
    ("template", ("c++",), "template<class T>void helper(T&v){v++;}void f(void){int a=0;" + region("a", "helper(a);") + "}", None),
    ("template-const-missing", ("c++",), "const int g=1;template<int N>int helper(){return g+N;}void f(void){int a=0;" + region("a", "a=helper<2>();") + "}", "requires explicit registration"),
    ("constructor-missing", ("c", "c++"), "void f(int b){" + region("", "int v[2]={b,0};(void)v;") + "}", "requires explicit registration"),
    ("constructor-private", ("c", "c++"), "void f(int b){" + region("b", "int v[2]={b,0};v[1]=v[0];") + "}", None),
    ("const-cast-write", ("c", "c++"), "void f(void){const int a=1;int*p=(int*)&a;" + region("a,p", "*p=2;") + "}", "const canonical object"),
    ("const-field-write", ("c",), "struct S{const int a;int b;};void f(void){struct S s={1,0};" + region("s", "int*p=(int*)&s.a;*p=2;") + "}", "const canonical object"),
    ("const-field-read", ("c",), "struct S{const int a;int b;};void f(void){struct S s={1,0};" + region("s", "s.b=s.a;") + "}", None),
    ("ref-subobject", ("c++",), "void f(void){int a[2]={0};int&r=a[1];" + region("r", "r++;") + "}", None),
    ("ref-subobject-missing", ("c++",), "void f(void){int a[2]={0};int&r=a[0];" + region("r", "a[1]++;") + "}", "requires explicit registration"),
    ("ref-array", ("c++",), "void f(void){int a[2]={0};int(&r)[2]=a;" + region("r", "r[1]++;") + "}", None),
    ("ref-overlap", ("c++",), "void f(void){int a[2]={0};int&r=a[1];" + region("a,r", "r++;") + "}", "registered objects overlap"),
    ("ref-equal", ("c++",), "void f(void){int a=0;int&r=a;" + region("a,r", "r++;") + "}", None),
    ("callee-register", ("c++",), "void helper(int&r){" + region("r", "r++;") + "}void f(void){int a=0;" + region("", "helper(a);a++;") + "}", None),
    ("recursive", ("c", "c++"), "int helper(int n){return n?helper(n-1):0;}void f(int a){" + region("a", "a=helper(a);") + "}", None),
    ("sizeof-no-read", ("c", "c++"), "void f(void){const int b=1;int a=0;" + region("a", "a=sizeof(b);") + "}", None),
    ("address-no-read", ("c", "c++"), "void f(void){const int b=1;" + region("", "const int*p=&b;(void)p;") + "}", None),
    ("macro-const-missing", ("c", "c++"), "#define GET_B b\nvoid f(void){const int b=1;int a=0;" + region("a", "a+=GET_B;") + "}", "requires explicit registration"),
    ("multiline-const-missing", ("c", "c++"), "void f(void){const int b=1;int a=0;" + region("a", "a+=\n b;") + "}", "requires explicit registration"),
    ("conditional-const-missing", ("c", "c++"), "void f(int a){const int b=1;" + region("a", "if(a){" + region("b", "a+=b;") + "}a+=b;") + "}", "requires explicit registration"),
    ("private-pointer-escape", ("c", "c++"), "int*p;void f(void){" + region("p", "int local=0;p=&local;") + "}", "cannot escape a private object"),
    ("unknown-pointer-assignment", ("c", "c++"), "int*p;void f(int*q){" + region("p,q", "p=q;") + "}", "pointer assignment has no provable canonical identity"),
    ("pointer-aggregate", ("c",), "struct S{int*p;};void f(void){struct S s={0};" + region("s", "(void)s;") + "}", "unsupported type"),
    ("pointer-integer-escape", ("c", "c++"), "unsigned long p;void f(void){" + region("p", "int local=0;p=(unsigned long)&local;") + "}", "pointer representation conversions"),
    ("partial-complex", ("c",), "void f(void){_Complex double a=0;" + region("a", "__real__ a=1;") + "}", "partial scalar access"),
]

report = []
for name, languages, source, diagnostic in cases:
    for language in languages:
        for optimization in ("-O0", "-O2"):
            label = f"{name}-{language.replace('+','p')}-{optimization[1:]}"
            path = output / (label + (".C" if language == "c++" else ".c"))
            path.write_text(source)
            command = [str(cc), f"-B{build / 'gcc'}/", "-fopenmp", optimization,
                       "-fdump-tree-gimple", "-fno-diagnostics-show-caret",
                       "-c", str(path), "-o", str(output / (label + ".o"))]
            run = subprocess.run(command, capture_output=True, text=True, timeout=30)
            (output / (label + ".log")).write_text(run.stdout + run.stderr)
            passed = (run.returncode == 0 if diagnostic is None
                      else run.returncode == 1 and diagnostic in run.stderr)
            passed &= "internal compiler error" not in run.stderr
            report.append({"case": label, "passed": bool(passed), "command": command})
            if not passed:
                print(f"FAIL: {label}: {run.returncode}\n{run.stderr}")
# Execute ordinary semantics and check that metadata never evaluates an index.
for language in ("c", "c++"):
    for lto in (False, True):
        label = f"execute-{language.replace('+', 'p')}-lto{int(lto)}"
        path = output / (label + (".C" if language == "c++" else ".c"))
        path.write_text("int helper(int n){return n+1;}int main(void){int a[3]={0};int i=0;"
                        + region("a,i", "a[i++]=helper(4);a[i++]=a[0]+1;")
                        + "return !(i==2&&a[0]==5&&a[1]==6&&a[2]==0);}")
        binary = output / label
        command = [str(cc), f"-B{build / 'gcc'}/", f"-B{gomp}/", "-fopenmp", "-O2",
                   *(["-flto"] if lto else []), str(path),
                   f"-L{gomp / '.libs'}", f"-Wl,-rpath,{gomp / '.libs'}", "-o", str(binary)]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
        executed = (subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
                    if compiled.returncode == 0 else compiled)
        passed = compiled.returncode == 0 and executed.returncode == 0
        (output / (label + ".log")).write_text(compiled.stderr + executed.stdout + executed.stderr)
        report.append({"case": label, "passed": passed, "command": command})
        if not passed: print(f"FAIL: {label}: {compiled.stderr}{executed.stderr}")

# A reference to a single element retains scalar extent, with its physical offset.
for name, expected in (("ref-subobject", ("base-offset=4", "extent=4", "kind=0")),
                       ("fields", ("kind=1", "offset=")),
                       ("forward", ("prepare(helper)",))):
    language = "cpp" if name == "ref-subobject" else "c"
    dumps = list(output.glob(f"{name}-{language}-O0.*.gimple"))
    content = "".join(path.read_text() for path in dumps)
    passed = bool(dumps) and all(token in content for token in expected)
    passed &= "GOMP_transaction_stm" not in content and "newtm_read (" not in content
    report.append({"case": "plan-" + name, "passed": passed})
    if not passed: print(f"FAIL: plan-{name}: expected {expected}")

if not all(row["passed"] for row in report):
    (output / "resultado.json").write_text(json.dumps(report, indent=2) + "\n")
    raise SystemExit(1)
(output / "resultado.json").write_text(json.dumps(report, indent=2) + "\n")
print(f"OK: {len(report)} verificacoes C/C++ em O0/O2")
