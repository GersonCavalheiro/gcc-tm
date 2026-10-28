# GCC-TM: extensão experimental OpenMP transaction

Este projeto desenvolve uma extensão do GCC para memória transacional em
software (STM), com a diretiva própria `#pragma omp transaction`. A árvore de
fontes incluída identifica sua versão base como **16.2.0** em
`gcc-src-tm/gcc/BASE-VER`. O desenvolvimento atual concentra-se em C e C++ no
host; a diretiva não faz parte do padrão OpenMP e não corresponde à extensão
GNU TM (`__transaction_atomic`, `-fgnu-tm` e libitm).

```c
int a = 0, b = 0;
#pragma omp transaction (a, b)
{
  a++;
  b += a;
}
```

A intenção é instrumentar acessos, manter escritas privadas e confirmar ou
repetir a tentativa externa diante de conflitos. **Isso ainda não está
implementado.** Atualmente há reconhecimento e validação da diretiva em C/C++,
representação intermediária e chamadas `GOMP_transaction_begin/register/end`
para rastrear objetos e aninhamento na libgomp. A criação e destruição da
tabela hash, aquisição/consulta com referências e liberação estão implementadas.
Consultas de endereço/tamanho e visita sincronizada de metadados também estão
implementadas, assim como a contagem consistente e saturada de entradas.
As nove primitivas da hash estão operacionais. Criação, aninhamento e limpeza do
contexto também estão implementados, com transferência da lease e destrutor da
política. Registro STM, acessos, confirmação e retry continuam pendentes. O corpo continua acessando memória
comum, sem isolamento, commit, rollback ou retry. O exemplo concorrente usa
`critical` externo para sincronizar as atualizações. Passar nos testes atuais
comprova os contratos de rastreamento/esqueletos, não uma STM funcional.

## Organização da árvore

| Caminho | Conteúdo |
| --- | --- |
| `gcc-src-tm/` | Árvore completa dos fontes do GCC modificados e documentação do projeto. |
| `gcc-src-tm/gcc/` | Front-ends, árvores/GIMPLE, passes de compilação e testes do compilador. |
| `gcc-src-tm/libgomp/` | Runtime OpenMP, ponte de rastreamento, fontes `newtm-*` e testes de execução. |
| `gcc-src-tm/gcc/testsuite/c-c++-common/gomp/` | Testes compartilhados C/C++, incluindo `transaction-*` e `critical-*`. |
| `gcc-src-tm/gcc/testsuite/g++.dg/gomp/` | Casos específicos de C++. |
| `gcc-src-tm/libgomp/testsuite/` | Testes de runtime, inclusive `libgomp.c` e `libgomp.c++`. |
| `gcc-build/` | Diretório separado para configuração, objetos, compiladores e bibliotecas gerados; artefatos ignorados pelo Git. |
| `transaction_test/` | Exemplos, verificadores Python e Makefile que seleciona o build local. |
| `Dockerfile.gcc-tm` | Ambiente Ubuntu 24.04 para compilar os fontes montados do host; não compila nem instala o GCC automaticamente. |
| `Dockerfile.gcc-source-ai` | Receita alternativa que ainda baixa outro repositório (`gcc-omp-transaction`); não é a receita indicada para obter esta árvore. |

Os documentos de agentes são guias de arquitetura e contratos, não programas
executáveis. Comece pelo [panorama do projeto](gcc-src-tm/OPENMP_TRANSACTION_PROJECT_AGENT.md).
O [guia geral](gcc-src-tm/OPENMP_AGENT.md) acompanha a implementação de diretivas
e o [agente critical](gcc-src-tm/OPENMP_CRITICAL_AGENT.md) descreve a construção
usada como referência. Os agentes de
[escopo](gcc-src-tm/OPENMP_TRANSACTION_SCOPE_AGENT.md),
[hash](gcc-src-tm/OPENMP_TRANSACTION_HASH_AGENT.md),
[implementação da hash](gcc-src-tm/OPENMP_TRANSACTION_HASH_IMPLEMENTATION_AGENT.md),
[acessos](gcc-src-tm/OPENMP_TRANSACTION_ACCESS_AGENT.md) e
[abort](gcc-src-tm/OPENMP_TRANSACTION_ABORT.md) detalham responsabilidades e
etapas futuras. Consulte o [inventário de primitivas](gcc-src-tm/OPENMP_TRANSACTION_PRIMITIVES.md),
o [estado de implementação](gcc-src-tm/OPENMP_TRANSACTION_IMPLEMENTATION.md) e
a [auditoria](gcc-src-tm/OPENMP_TRANSACTION_AUDIT.md) para distinguir contratos
planejados, código existente e resultados históricos. Caminhos antigos como
`/usr/src/gcc-omp-transaction` nesses documentos devem ser adaptados à sua cópia.

## Baixar a versão em trabalho

```sh
git clone --branch main https://github.com/GersonCavalheiro/gcc-tm.git
cd gcc-tm
git rev-parse HEAD
```

O clone já contém os fontes modificados em `gcc-src-tm`; não é necessário baixar
um GCC separado. `main` é a versão em trabalho, não uma versão estável de STM.
Registre o hash acima para reproduzir uma compilação. Para atualizar uma cópia
sem alterações locais pendentes:

```sh
git switch main
git pull --ff-only
```

## Preparar o ambiente

Em Ubuntu 24.04, instale as dependências de compilação e dos testes:

```sh
sudo apt-get update
sudo apt-get install -y build-essential binutils bison flex libgmp-dev \
  libmpfr-dev libmpc-dev libisl-dev zlib1g-dev libzstd-dev python3 perl \
  texinfo gawk dejagnu expect tcl git ca-certificates xz-utils
```

Alternativamente, a partir da raiz do repositório, prepare o contêiner:

```sh
docker build -f Dockerfile.gcc-tm -t gcc-tm-dev .
docker run --rm -it -v "$PWD:/usr/src/gcc-tm" -w /usr/src/gcc-tm gcc-tm-dev
```

Execute a configuração e compilação abaixo dentro desse shell. Para os testes
DejaGNU, instale também `dejagnu expect tcl` na imagem (a receita atual não os
inclui); isso pode ser feito acrescentando-os à lista de pacotes do Dockerfile
antes de construir a imagem. O volume preserva fontes e resultados no host.
Use o mesmo ambiente para construir e executar: binários gerados no contêiner
podem exigir uma glibc mais nova que a disponível no host.

## Compilar fora dos fontes

Todos os comandos seguintes partem da raiz do repositório, salvo indicação.
Configure um diretório de build novo, sem reutilizar configurações ou binários
de outra máquina/ambiente:

```sh
mkdir -p gcc-build
cd gcc-build
../gcc-src-tm/configure \
  --prefix=/usr/local/gcc-tm \
  --enable-languages=c,c++ \
  --enable-libgomp \
  --disable-multilib \
  --disable-bootstrap \
  --disable-libsanitizer \
  --disable-libquadmath \
  --disable-libssp \
  --disable-libvtv
make -j"$(nproc)"
cd ..
```

Essa configuração corresponde à usada no build local de desenvolvimento e evita
dependências de bibliotecas de 32 bits. `--disable-bootstrap` reduz o trabalho de
compilação, mas não realiza a validação de um bootstrap completo. Ajuste o número
de tarefas à memória disponível. Se `gcc-build` já contiver um build incompatível,
use outro diretório vazio e adapte os comandos, inclusive `GCC_BUILD` nos exemplos.

Não é necessário instalar o compilador para desenvolver ou testar. Após mudanças,
`make -C gcc-build -j"$(nproc)"` atualiza o build. Para alterações restritas às
respectivas camadas, os relatórios também usam:

```sh
make -C gcc-build/gcc -j"$(nproc)" all
make -C gcc-build/"$(gcc-build/gcc/xgcc -dumpmachine)"/libgomp -j"$(nproc)" all
```

## Confirmar que está usando o GCC em desenvolvimento

Use caminhos explícitos: `gcc --version` sozinho pode mostrar o GCC do sistema,
e o número 16.2.0 não comprova a presença da extensão. Na raiz do projeto:

```sh
export GCC_TM_ROOT="$PWD"
export GCC_TM_BUILD="$GCC_TM_ROOT/gcc-build"
export GCC_TM_CC="$GCC_TM_BUILD/gcc/xgcc"
export GCC_TM_TARGET="$("$GCC_TM_CC" -dumpmachine)"
export GCC_TM_GOMP="$GCC_TM_BUILD/$GCC_TM_TARGET/libgomp"

"$GCC_TM_CC" -B"$GCC_TM_BUILD/gcc/" -v
"$GCC_TM_CC" -B"$GCC_TM_BUILD/gcc/" -print-prog-name=cc1
"$GCC_TM_CC" -B"$GCC_TM_BUILD/gcc/" -B"$GCC_TM_GOMP/" \
  -L"$GCC_TM_GOMP/.libs" -print-file-name=libgomp.so
command -v gcc
```

O `cc1` deve apontar para `gcc-build/gcc/` e a libgomp para o build selecionado.
`xgcc` é o driver ainda não instalado; `-B` seleciona seus componentes. Alterar
apenas o `PATH` para incluir `gcc-build/gcc` não faz `gcc` virar `xgcc`.

Compile um exemplo real e confira também a biblioteca carregada em execução:

```sh
"$GCC_TM_CC" -B"$GCC_TM_BUILD/gcc/" -B"$GCC_TM_GOMP/" \
  -I"$GCC_TM_GOMP" -fopenmp -ftransaction-debug \
  transaction_test/transaction_aninhamento.c \
  -L"$GCC_TM_GOMP/.libs" -Wl,-rpath,"$GCC_TM_GOMP/.libs" \
  -o /tmp/gcc-tm-aninhamento
LD_LIBRARY_PATH="$GCC_TM_GOMP/.libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ldd /tmp/gcc-tm-aninhamento
LD_LIBRARY_PATH="$GCC_TM_GOMP/.libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  /tmp/gcc-tm-aninhamento
nm -D --defined-only "$GCC_TM_GOMP/.libs/libgomp.so" | rg 'GOMP_transaction_'
```

Na saída de `ldd`, `libgomp.so.1` deve resolver para o diretório do build. A
biblioteca deve exportar `GOMP_transaction_begin`, `GOMP_transaction_register`
e `GOMP_transaction_end` (versão `GOMP_NEWTM_1.0`). `-ftransaction-debug` habilita
os diagnósticos específicos do compilador; os logs do runtime são independentes.
Erros de símbolo/versão `GOMP_NEWTM_1.0` geralmente indicam uma libgomp antiga ou
do sistema. As primitivas internas `newtm_*` não devem ser exportadas na DSO.
O comando `rg` acima pode ser substituído por `grep` se ripgrep não estiver instalado.

## Executar os testes

### Exemplos e rastreamento

Depois de concluir o build:

```sh
make -C transaction_test -B check
```

O Makefile usa `../gcc-build/gcc/xgcc`, inclui `-fopenmp`, seleciona a libgomp
local e grava executáveis/logs em `transaction_test/.build/`. O verificador
exercita máximo paralelo com 1, 2 e 4 threads solicitadas, empate, aninhamento e
registro/liberação dos objetos. Para outro build:

```sh
make -C transaction_test -B check GCC_BUILD=/caminho/absoluto/do/build
```

O Makefile atual fixa o alvo `x86_64-pc-linux-gnu`; em outra plataforma, adapte
`GOMP_BUILD` ao alvo informado por `xgcc -dumpmachine`. Use `-B` no make para
recompilar ao trocar compilador ou flags. `make -C transaction_test run` apenas
executa os exemplos; `check` verifica seus resultados.

O alvo `check-lifecycle` testa criação e limpeza reais do contexto, transferência
da lease, aninhamento, falhas, leitores compartilhados e descarte sem publicação.
Liga ao archive estático da libgomp selecionada e usa os cabeçalhos privados em
`gcc-src-tm/libgomp/`. Registro/acesso STM e commit continuam pendentes:

```sh
make -C transaction_test -B check-lifecycle
# Para outro build:
make -C transaction_test -B check-lifecycle GCC_BUILD=/caminho/absoluto/do/build
```

Execute Python sem `-O`/`-OO` e sem `PYTHONOPTIMIZE`: o verificador depende de
asserções e recusa sua desativação. Esse teste verifica retornos, preservação
de dados, logs e visibilidade dos símbolos, sem executar transações reais.

### Carga parametrizada de leituras e escritas

```sh
make -C transaction_test check-carga
make -C transaction_test check-carga CARGA_THREADS=8 CARGA_REPETICOES=10000
transaction_test/.build/transaction_carga 4 1:9 1000
```

O executável recebe três parâmetros: número de threads, relação
**escritas:leituras** e repetições **por thread**. O Makefile executa 1:9, 1:1 e
9:1, por padrão com 4 threads e 1.000 transações por thread, também incluídas em
`make check`. O teste verifica contagens, valor final e consistência do objeto;
o verificador confere ainda registros/liberações da hash. Logs em
`transaction_test/.build/transaction_carga-*.log`.

Uma repetição corresponde a uma transação de leitura ou de escrita
(read-modify-write). Para N repetições e pesos W:R, cada thread executa
floor(N*W/(W+R)) escritas. Os pesos e sua soma devem caber em uint32_t, e a soma
deve ser positiva. Threads/repetições devem ser positivas, e o total de
transações deve caber em unsigned long long. Falha se o ambiente limitar a
equipe abaixo do número solicitado. Como ainda não há isolamento STM, um
critical interno protege os valores; os registros podem concorrer. Isso
exercita a infraestrutura atual, sem comprovar isolamento transacional.

### Criação e destruição da tabela hash

```sh
make -C transaction_test -B check-hash-create-destroy
```

O teste usa os fontes reais e headers privados do build Linux x86_64, ligado ao
archive local, para verificar argumentos inválidos, overflow, capacidades
1/3/1024, mutexes utilizáveis e preservação integral em BUSY. Uma entrada retida
é preparada internamente para isolar esse teste do caminho de aquisição.
Saída em `transaction_test/.build/hash_create_destroy.log`; aceita `GCC_BUILD`
como os outros alvos. Não comprova quiescência, concorrência ou STM funcional.
Os printf/fflush de depuração foram mantidos fora dos mutexes dos buckets.

### Aquisição e consulta de entradas

```sh
make -C transaction_test -B check-hash-acquire
```

O teste interno verifica inserção/reutilização, consulta sem inserção, tamanho
incompatível, nulos, overflow por injeção controlada, colisões e concorrência com
quatro threads. Verifica também liberação parcial/final e destruição de metadados
fora do lock. A mistura nativa é exercitada com endereços numéricos consecutivos
e alinhados, capacidades arbitrárias e bits superiores; isso não é execução
nativa da variante de outra largura. Saída em `.build/hash_acquire.log` dentro
de `transaction_test`; aceita `GCC_BUILD`. A ponte usa referências reais e verifica
os retornos, preservando os printf fora dos mutexes. Não há STM funcional.

### Consultas e visita de metadados

```sh
make -C transaction_test -B check-hash-entry
```

Endereço e tamanho são imutáveis e consultados sem lock enquanto o chamador
mantém uma referência. A visita executa o callback uma vez sob o mutex do bucket,
preservando a referência. O teste verifica os campos, instalação/substituição de
metadados, argumento nulo, destruição final, visitas concorrentes com quatro
threads e progresso em outro bucket. Usa o build privado Linux x86_64 e admite
GCC_BUILD. Callbacks não fazem E/S, espera ou reentrada; printf/fflush da primitiva
ficam fora do lock. Log em `transaction_test/.build/hash_entry.log`, com limite
de 60 segundos. Não comprova STM funcional.

### Contagem consistente de entradas

```sh
make -C transaction_test -B check-hash-count
```

Count adquire todos os mutexes em ordem crescente, conta entradas (não
referências), satura em SIZE_MAX e libera em ordem inversa. O teste cobre tabela
vazia, referências repetidas, colisões, capacidades 1/3/1024, saturação por
injeção interna restaurada e contagem concorrente com quatro threads. Verifica
também zero durante destruição de metadados ainda em andamento, sem chamar
destroy nesse intervalo: zero não autoriza destruição nem comprova quiescência.
Usa configuração privada Linux x86_64, aceita GCC_BUILD e grava
`transaction_test/.build/hash_count.log`, com limite de 60 segundos.
Printf/fflush ficam fora dos locks; não chamar count de callbacks de visita ou
destrutores, nem com mutex de bucket já adquirido.

### Diagnósticos sem inversão de locks

```sh
make -C transaction_test -B check-hash-diagnostics
```

O teste interno Linux x86_64 mantém stdout ou stderr bloqueado enquanto outra
thread inicializa a ponte ou recebe falha de alocação injetada. Verifica que
nenhum log/diagnóstico fatal retém o lock global ou o mutex do bucket, mantendo
os formatos printf existentes. Quatro casos isolados: inicialização, OOM de
entrada, tabela e buckets. OOM continua fatal (saída 1); cada caso tem limite
de 10 segundos. Logs em `transaction_test/.build/hash_diagnostics-*.log`
(relativos à raiz); aceita GCC_BUILD. Interposição restrita ao teste, sem hooks
ou ABI novos. Em concorrência, o log create pode sair depois de operações que
já observaram a tabela publicada. A finalização agora usa o controle de admissão e participações descrito abaixo;
tabelas independentes continuam exigindo quiescência externa.

### Admissão e encerramento da tabela global

```sh
make -C transaction_test -B check-hash-owner
```

O proprietário usa OPEN -> CLOSING -> CLOSED. O contexto externo retém uma
participação até concluir toda a limpeza; aninhamentos reutilizam-na. Durante
CLOSING novos usuários são recusados, mas os existentes podem terminar.
`newtm_hash_owner_close`, interface interna oculta, é não bloqueante quanto à
drenagem: retorna BUSY com usuários/coordenador/entradas residuais e pode ser
repetida após conclusão/join. Um único coordenador destrói a tabela drenada;
CLOSED não reinicializa e chamadas repetidas não destroem duas vezes.
Consumidores internos diretos da tabela global devem manter uma lease por todo
o intervalo de uso e referências; não podem destruir a tabela emprestada.

O finalizador automático faz uma tentativa: se ocupado, conserva a tabela e o
controle para recuperação pelo processo, sem esperar ou forçar limpeza. O mutex
de admissão sobrevive à tabela para rejeitar chamadas tardias. Isso não garante
segurança de dlclose: impedir execução atual/futura da biblioteca continua sendo
responsabilidade externa. Não há nova ABI pública GOMP ou semântica STM.

O teste Linux x86_64 cobre onze casos em processos separados, com limite de
15 segundos: sem uso, aninhamento, coordenadores concorrentes, limpeza e log de
criação pendentes, entradas residuais, corrida de admissão, erros/overflow,
chamada tardia e finalização com usuário ativo. Logs em
`transaction_test/.build/hash_owner-*.log` (relativos à raiz); aceita GCC_BUILD.

### Suítes DejaGNU do compilador e da libgomp

Com DejaGNU/Expect/Tcl instalados e as variáveis `GCC_TM_*` acima definidas:

```sh
export LD_LIBRARY_PATH="$GCC_TM_GOMP/.libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export OMP_NUM_THREADS=4
export OMP_DYNAMIC=FALSE

make -C "$GCC_TM_BUILD/gcc" check-gcc \
  TESTSUITEDIR=testsuite-tm-c RUNTESTFLAGS='gomp.exp=transaction-*'
make -C "$GCC_TM_BUILD/gcc" check-g++ \
  TESTSUITEDIR=testsuite-tm-cxx RUNTESTFLAGS='gomp.exp=transaction-*'
make -C "$GCC_TM_GOMP" check \
  RUNTESTFLAGS='c.exp=transaction-*.c c++.exp=transaction-*.c'
```

É essencial selecionar a libgomp local também na execução: passar apenas
`GCC_UNDER_TEST`/`GXX_UNDER_TEST` como variáveis make não bastou na auditoria.
Confira os comandos efetivos em `.log` e os resultados em:

- `gcc-build/gcc/testsuite-tm-c/gcc/gcc.sum` e `gcc.log`;
- `gcc-build/gcc/testsuite-tm-cxx/g++/g++.sum` e `g++.log`;
- `gcc-build/<alvo>/libgomp/testsuite/libgomp.sum` e `libgomp.log`.

Para verificar regressões em `critical`, repita os três comandos separadamente,
substituindo `transaction-*` por `critical-*` e usando novos `TESTSUITEDIR`, por
exemplo `testsuite-critical-c` e `testsuite-critical-cxx`. Preserve os `.sum` e
`.log` da libgomp antes de repetir, pois a chamada seguinte os sobrescreve.
Não repita o mesmo driver `.exp` com filtros diferentes numa única chamada:
um filtro pode substituir o outro.

Procure `FAIL`, `XPASS`, `UNRESOLVED` e erros de infraestrutura; confira também
se houve testes executados. A auditoria registra resultados históricos de
subconjuntos, não aprovação da suíte completa nem evidência de STM funcional.
Para ampliar a cobertura, `make -C gcc-build -k check` executa as suítes dos
componentes configurados e pode demorar bastante.

### Checkpoint da base atual

Após reconstruir a libgomp, execute no ambiente do build:

```sh
make -C transaction_test check-checkpoint-contexto CFLAGS='-O2 -Wall -Werror'
```

O checkpoint `contexto_pronto_para_registro` reúne hash, owner, criação/limpeza de
contexto e rastreamento. Logs e relatório JSON ficam em
`transaction_test/.build/checkpoint_contexto_pronto_para_registro/`.
O [cabeçalho do teste](transaction_test/checkpoint_contexto_pronto_para_registro.py)
delimita sua cobertura; aprovação não certifica isolamento ou STM funcional.
