## Documentos de referência e integração futura

| Documento | Responsabilidade |
| --- | --- |
| [SCOPE_AGENT](OPENMP_TRANSACTION_SCOPE_AGENT.md) | Resolução, tipos e preservação de identidade para os acessos. |
| [HASH_AGENT](OPENMP_TRANSACTION_HASH_AGENT.md) | Estrutura da hash, referências e política transacional geral. |
| [ACCESS_AGENT](OPENMP_TRANSACTION_ACCESS_AGENT.md) | Contratos propostos de newtm_read/write, sincronização e pontos de chamada. |
| [ABORT](OPENMP_TRANSACTION_ABORT.md) | Propagação de falha, cleanups, descarte e reinício externo. |

As primitivas newtm_read/write possuem somente esqueletos em
libgomp/newtm-transaction.c/.h, com assinaturas e estados internos declarados.
O contrato funcional permanece proposto; não existe ABI consolidada da ponte
de acesso GOMP_*. Elas imprimem parâmetros e retornam INVALID, sem acessar os
objetos ou modificar buffers/contexto. Os testes históricos abaixo não são
validação funcional dessas primitivas.

A integração funcional depende de hash/contexto reais, preservação do mapeamento
objeto/deslocamento antes da eliminação do marcador GIMPLE, instrumentação com
remapeamento OpenMP correto, reservas e dados privados. Cada retorno precisa ser
verificado antes de usar o resultado. CONFLICT condena a tentativa externa;
INVALID não autoriza retry. As primitivas sinalizam falha sem executar a limpeza
completa: o controle de abort conduz cleanups, descarte e retirada de reservas.

Permanecem pendentes a propagação segura entre funções, os acessos por cleanups,
a definição da ponte e a distinção entre commit, descarte e retry no encerramento
atualmente representado por TRY_FINALLY. Tipos, aliases e operações sem suporte
precisam ser restringidos. Registro e rastreamento não comprovam STM funcional.

## Definição da máquina de estados — 2026-10-21

Acrescentado `enum newtm_attempt_state` em `libgomp/newtm-transaction.h`,
separado dos resultados de hash/acesso/ciclo de vida: ACTIVE, CONFLICTED,
INVALID, PUBLISHING e CLOSED, com prefixo NEWTM_ATTEMPT_. O contexto permanece
opaco; os corpos das oito funções permanecem stubs. Nenhuma assinatura de função,
ABI exportada, chamada da ponte GOMP ou integração do compilador foi alterada.

A especificação completa está em
[ABORT — máquina de estados](OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa):
transições permitidas, operações rejeitadas, precedência de INVALID sobre conflito,
publicação irrevogável após preparação falível e CLOSED terminal. Espera não é
estado da tentativa; após descarte por conflito, nova entrada cria outra tentativa.
Também foi corrigida a regra resumida de abort para não sugerir retry de toda
falha de commit, inclusive INVALID.

Detalhada a prevenção de dupla limpeza no contrato: um único slot proprietário,
empréstimos sem poder de liberação, end externo como consumidor após publicação
total ou discard como consumidor exclusivo do controle externo após cleanups.
O consumidor marca CLOSED, zera profundidade e retira o contexto do slot **antes
da primeira liberação**. Esta ordem explicita a retirada antecipada do handle;
erros detectáveis antes do consumo continuam preservando o slot. Somente o
consumidor conserva ponteiro privado até terminar; falha estrutural durante a
limpeza é fatal. Segundo consumo pelo slot vazio é INVALID, sem tocar memória
liberada. CLOSED/slot NULL não provam commit ou término da limpeza; OK exige
concluir a operação. Essas guardas são contrato para a implementação futura,
não comportamento já executado pelos stubs.

Atualizados inventário, agentes de panorama/hash/acesso e comentários dos
esqueletos para usar a mesma máquina. A hash deixa de descrever espera como
estado e retorno da mesma tentativa a ativa. O agente de abort preserva a
pendência do protocolo de cleanups e exige guardar causa efetiva após eles,
antes do descarte. `stm_*` continua exclusivamente conceitual.

Validação executada nesta etapa:

- `make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp -j3 all`: passou,
  recompilando newtm-transaction PIC/não PIC com -Wall -Werror e religando as
  bibliotecas. Log local: `/tmp/newtm-state-build.log`.
- `make -C transaction_test check-lifecycle check`: passou. O teste permanente
  lifecycle passou de 22 para 24 chamadas, acrescentando discard repetido e
  end após discard sobre o mesmo slot opaco. Todas retornam INVALID, preservam
  slots/canários e mantêm outermost=false; seis símbolos presentes no archive
  e ausentes da API dinâmica. Verificador e README atualizados.
- Exemplos com 1/2/4 threads e aninhamento lexical/por função passaram com o
  runtime reconstruído. Continuam verificando rastreamento, não STM.

Não houve rebuild do compilador nem execução DejaGNU nesta etapa: a alteração
de runtime limita-se à declaração do enum e aos comentários dos contratos.
Transições, exclusividade operacional do descarte, publicação e retry reais
precisarão de testes funcionais quando houver contexto/recursos implementados.
Os stubs continuam retornando INVALID, sem anunciar confirmação bem-sucedida.

## Auditoria geral após os esqueletos

Ver [OPENMP_TRANSACTION_AUDIT.md](OPENMP_TRANSACTION_AUDIT.md) para a verificação
executada em 2026-10-21: builds incrementais, 383 PASS DejaGNU em transaction e
critical (C/C++/libgomp), exemplos, contratos dos 17 serviços internos, ABI,
LTO, módulo C++ e remapeamento de objetos. Os resultados finais não contêm
FAIL, UNRESOLVED ou UNSUPPORTED; as primeiras falhas por carregamento da libgomp
do sistema foram diagnosticadas e repetidas com o runtime local.

Correção em verificar_lifecycle.py: recusar -O/-OO/PYTHONOPTIMIZE para não
aprovar logs/símbolos sem executar as asserções. O teste normal e a rejeição da
execução otimizada foram verificados. Essa correção não altera o runtime.
O relatório inclui comandos reproduzíveis; nesta configuração, passar
GCC_UNDER_TEST/GXX_UNDER_TEST somente como variáveis make não mudou o driver,
e LD_LIBRARY_PATH foi necessário para selecionar a biblioteca na execução.

## Esqueletos do ciclo de vida

Acrescentados em `libgomp/newtm-transaction.c/.h`, sem novos arquivos de runtime:

- Primitivas: `newtm_begin`, `newtm_register`, `newtm_end`, `newtm_discard`.
- Helpers internos compartilhados: `newtm_fail`, `newtm_backoff`.
- Saída interna e confirmação externa são fases de end; retry permanece fluxo
  gerado pelo compilador. Não há símbolos próprios de commit/leave/retry.
- `enum newtm_lifecycle_status` define OK, CONFLICT e INVALID em domínio próprio,
  distinto de acesso e hash. Não implementa a máquina de estados do contexto.

Os stubs imprimem parâmetros e fazem fflush(stdout), retornando sempre
NEWTM_LIFECYCLE_INVALID. begin escreve false em outermost não nulo; preserva
slot de entrada/saída. Nenhum deles inspeciona contexto, slot, tabela ou objeto,
aloca, adquire lock/reserva, confirma, descarta, registra falha ou espera.
O contexto permanece opaco. Não foram ligados à ponte GOMP nem ao compilador;
Makefile.am/in já incluem o fonte/cabeçalho e não precisaram de alteração.

Escolha de contrato interno para a implementação futura: end em falha preserva
slot/profundidade/recursos; o controle externo executa cleanups e chama discard
explicitamente, guardando a causa fora do contexto. Substitui a proposta de
end-false com descarte implícito; agente de abort e inventário foram atualizados.
OK de discard significa limpeza, não commit; só causa CONFLICT permite backoff.
newtm_fail recebe causa de acesso e devolve causa efetiva lifecycle, preservando
prioridade de INVALID. Backoff recebe contagem de conflitos positiva, primeira=1,
saturada em SIZE_MAX pelo chamador; fórmula da espera ainda futura. Nenhum
contrato funcional é executado pelos stubs. `stm_*` segue conceitual.

Validação executada nesta etapa:

- `make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp -j3 all`: passou;
  compilação PIC/não PIC com -Wall -Werror e ligação compartilhada/estática.
  Log local: `/tmp/newtm-lifecycle-build.log`.
- `make -C transaction_test check-lifecycle`: passou. Teste permanente
  `lifecycle_stubs.c` ligado ao archive do build, sem exportar funções internas.
  São 22 chamadas com nulos, slots nulo/preenchido, canários de contexto/tabela/
  objeto, causas OK/CONFLICT/INVALID/desconhecida, tamanhos 0/SIZE_MAX e contagens
  0/1/SIZE_MAX. Confere INVALID, slots/dados preservados e outermost=false.
- `verificar_lifecycle.py` verifica argumentos/contagens dos logs e usa nm para
  conferir os seis símbolos no archive e sua ausência da API dinâmica. Executa
  com timeout para detectar bloqueio acidental; não é prova de backoff funcional.
  Log reproduzível em `transaction_test/.build/lifecycle_stubs.log`.
- `make -C transaction_test -B check`: recompilou exemplos e passou com 1/2/4
  threads, aninhamento lexical/por função, duplicatas e liberação externa.
- readelf confirmou GLOBAL HIDDEN nos seis símbolos do objeto PIC; ldd confirmou
  a libgomp do build local carregada pelo exemplo. Assinaturas/enums do inventário
  e links locais também foram conferidos contra os fontes.

Não foi executada a suíte DejaGNU ou feito rebuild do compilador nesta etapa:
seus fontes e a ABI GOMP não mudaram. Os testes cobrem contratos dos stubs e a
compatibilidade do rastreamento; não demonstram isolamento/commit/retry reais.

## Esqueletos das primitivas de acesso

Criados `libgomp/newtm-transaction.c` e `.h` com newtm_read/newtm_write, contexto
opaco e enum newtm_access_status. Cada corpo faz um printf com ctx, object,
offset, destination/source e size, seguido de fflush(stdout), e retorna
NEWTM_ACCESS_INVALID. Não valida ou desreferencia parâmetros, não modifica
buffers/contexto e não implementa hash, reservas, cópias privadas ou abort.

Os arquivos estão nos fontes da libgomp em Makefile.am/Makefile.in, com objeto
e dependência de compilação correspondentes. O cabeçalho não é instalado e os
símbolos permanecem ocultos. Nenhuma chamada foi inserida no compilador ou na
ponte GOMP_transaction_*; não há ABI pública nova.

Validação desta etapa:

- `make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp -j3 all` passou,
  compilando os dois modos PIC/não PIC com -Wall -Werror e ligando bibliotecas
  compartilhada e estática. Log final: `/tmp/newtm-access-skeleton-build-final.log`.
- Teste C isolado ligado à biblioteca estática conferiu quatro chamadas,
  parâmetros impressos, retorno INVALID, buffers/contexto inalterados, ponteiros
  nulos, tamanho zero e SIZE_MAX. Fonte, executável e log estão em
  `/tmp/newtm-access-skeleton/`. O teste não usa contexto transacional real:
  verifica a ausência de operações do stub, não a semântica funcional.
- nm/readelf confirmaram que newtm_read/write não são exportadas pela biblioteca
  compartilhada; evidência em `/tmp/newtm-access-skeleton/symbols.log`.
- `make -C transaction_test check` passou com a libgomp reconstruída: máximo
  com 1/2/4 threads e caso de aninhamento lexical/por função.

Também foram revisados todos os agentes: contratos consolidados da hash e seus
callbacks, ponte atual, ciclo de vida conceitual, acessos, helpers do compilador
e serviços critical existentes. Assinaturas internas dos stubs estão declaradas;
contratos funcionais e a futura ABI de ponte continuam sujeitos às pendências
explicitadas. Não foi executada a suíte completa do GCC/libgomp nesta etapa.

## Inventário e localização dos contratos

O inventário central está em
[OPENMP_TRANSACTION_PRIMITIVES.md](OPENMP_TRANSACTION_PRIMITIVES.md): registra
as assinaturas internas existentes, proprietário dos recursos, estados, efeitos
atuais/funcionais futuros e ponto de chamada de cada operação. Abrange hash,
callbacks, acessos, ponte de rastreamento, esqueletos de ciclo de vida e os cinco
helpers do compilador. Os agentes especializados preservam os contratos completos.

A consolidação de 2026-10-21 foi documental, conferida contra declarações,
definições e chamadores. Nenhum símbolo, enum, ABI ou esqueleto novo foi criado.
Os nomes stm_* continuam conceituais e operações sem assinatura definida estão
marcadas como tal. Confirmação, backoff e retry não exigem novos símbolos por si.
Após essa consolidação inicial, os esqueletos de ciclo de vida foram
acrescentados conforme a seção acima; as operações reais permanecem futuras. Fontes atuais em
`/usr/src/gcc-omp-transaction`; caminhos antigos no histórico são preservados.

Funções genéricas do GCC/libgomp e primitivas critical são referências existentes,
não novas APIs STM. Não confundir retornos dos stubs com contratos funcionais.

### Contratos dos helpers atuais do compilador

Estes helpers executam na compilação, não sobre memória transacional do programa.
Erros de sintaxe/semântica seguem diagnósticos e recuperação do front-end; não
são CONFLICT/INVALID do runtime. Contratos de alocação/GC são os usuais do GCC.

| Helper | Pré-condições | Pós-condições atuais |
| --- | --- | --- |
| newtm_c_parser_transaction | Parser C válido na posição esperada pelo dispatcher; contexto de construção e bindings ativos; localização válida e if_p sob contrato do parser de bloco. | Consome a diretiva e analisa o corpo; resolve lista antes de entrar no corpo; preserva declarações aceitas na ordem; constrói/acrescenta OMP_TRANSACTION. Entradas inválidas são diagnosticadas e recuperadas até EOL, sem certificar validade de lista parcial. |
| newtm_cp_parser_transaction | Parser/token de pragma válidos e contexto do dispatcher; bindings e estado C++ coerentes. | Mesmo percurso, com busca C++, acesso/ambiguidade, referências externas e dados para instanciação. Diagnóstico já emitido não implica nó de sucesso semanticamente válido. |
| newtm_finish_transaction | Localização, corpo e lista de declarações como árvores GCC válidas/rastreadas; lista conforme representação aceita, possivelmente vazia. | Constrói OMP_TRANSACTION com tipo void, corpo e lista, acrescenta ao fluxo e retorna o nó. Não resolve nomes, registra endereços ou instrumenta acessos. |
| newtm_build_transaction | Sequência GIMPLE válida e pertencente ao corpo a transferir, possivelmente vazia. | Retorna novo GIMPLE_OMP_TRANSACTION/GSS_OMP com esse corpo; chamado na gimplificação e na cópia/remapeamento em tree-inline.cc. Não copia a lista GENERIC, não emite acesso ou commit. |
| newtm_lower_transaction | Iterador válido sobre GIMPLE_OMP_TRANSACTION, contexto omp existente e corpo preparado para lower_omp no contexto externo. | Transforma o corpo com ctx->outer e substitui o marcador pela sequência; chamadas de rastreamento já presentes são preservadas. Não consultar o marcador removido; não há instrumentação funcional. |

Na implementação futura, preservar/remapear metadados, atualizar consumidores e
inserir acessos conforme o agente de acesso antes de remover o marcador. Esse
trabalho altera as pós-condições e deve atualizar esta tabela na mesma entrega.

## Instrumentação de chamadas à hash (etapa atual)

Os parsers preservam as declarações dos objetos no segundo operando de
`OMP_TRANSACTION`; nomes que não designam variáveis/parâmetros são rejeitados.
A instanciação C++ substitui essa lista. Na gimplificação, a lista é convertida
em chamadas `GOMP_transaction_begin`, `GOMP_transaction_register(address, size)`
e `GOMP_transaction_end`; a saída usa `GIMPLE_TRY_FINALLY`. Referências C++
registram o objeto referido. Exigem-se tipos completos de tamanho constante.
As chamadas comuns passam pelo remapeamento de dados OpenMP existente, e a
lista não precisa sobreviver no marcador GIMPLE.

A ponte provisória na libgomp chama `newtm_hash_create` uma vez, no primeiro
uso, `newtm_hash_acquire` para cada objeto distinto no contexto, e
`newtm_hash_release` somente ao sair do nível externo. Aninhamento por chamada
de função também compartilha esse contexto. No encerramento normal da
biblioteca, chama `newtm_hash_destroy` se houve uso. As outras primitivas não
são chamadas artificialmente: ainda não há operações de acesso transacional,
metadados ou diagnóstico de contagem que as exijam.

As primitivas apenas imprimem seus argumentos. Os antigos `gomp_fatal` foram
substituídos por saídas definidas: estados INVALID, handles nulos e consultas
nulas/zero. A ponte ignora esses estados **apenas nesta etapa de instrumentação**;
não há tabela ou entradas reais, e os logs mostram handles nulos. Uma lista
mínima por execução controla as mensagens de aquisição/liberação e duplicatas;
não armazena valores dos objetos nem implementa hash, reservas ou conflitos.
O log consiste exclusivamente nos `printf` existentes, sem outro mecanismo.

O contexto acompanha execução síncrona na thread. Tasks com suspensão/migração
e dispositivos não estão suportados. O corpo continua acessando memória comum;
a saída excepcional apenas limpa o contexto, sem commit, abort ou rollback.
Os relatos abaixo são históricos e não descrevem integralmente esta etapa.

Validação desta etapa:

- Build GCC concluído com `make -C /usr/src/gcc-build/gcc -j6 all`, seguido
  de checagem incremental; libgomp reconstruída com `make -j3 all`.
- Novos testes `transaction-log-*`: C, 8 PASS; C++, 33 PASS; libgomp,
  6 PASS (compilação, execução e padrão de saída em C e C++).
- Regressão `critical-*`: C, 20 PASS; C++, 60 PASS.
- Conjunto `transaction-*`: C, 46 PASS/7 FAIL; C++, 165 PASS/21 FAIL.
  Sem UNRESOLVED nos resultados finais. Todas as linhas excedentes que
  causaram FAIL são mensagens `[newtm]` de depuração já presentes nos parsers
  e em omplower; não foram filtradas dos testes históricos para esconder falhas.
- Nove execuções adicionais passaram: três programas C/C++ em O0, O2 e
  O2+LTO, cobrindo quatro threads, duplicatas, aninhamento léxico/por função,
  tipos e tamanhos, referências, lambdas e cleanup durante exceção. Um teste
  adicional confirmou endereços das cópias private/reduction; outro confirmou
  exportação/importação e instanciação de template em módulo C++.
- Testes diretos da ponte confirmaram liberação somente na saída externa e
  80 pares acquire/release sob quatro threads, com uma criação/destruição global.

Os testes de execução precisam da libgomp reconstruída. Na regressão GCC,
foi necessário definir GCC_UNDER_TEST/GXX_UNDER_TEST com `-L` e rpath para
`/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/.libs`: o driver padrão estava
ligando a versão antiga e falhava com símbolos GOMP_transaction_* ausentes.
Não foi feita instalação do compilador ou runtime, nem executada a suíte completa.

Evidências: `/tmp/newtm-calls-build-final.log`,
`/tmp/newtm-calls-build-check.log`, `/tmp/newtm-calls-lambda-build.log`,
`/tmp/newtm-calls-runtime-final.log`, `/tmp/newtm-calls-verify.py`,
`/tmp/newtm-calls-regression.py`, `/tmp/newtm-calls-module/` e os resumos/logs
nos diretórios `testsuite-newtm-calls`, `testsuite-newtm-calls-all`
(critical) e `testsuite-newtm-calls-transaction` sob o build GCC. Os resultados
da libgomp estão em seu diretório `testsuite/libgomp.sum` e `.log`.

## Leitura do histórico

As seções a partir de “Histórico de implementação” registram etapas anteriores,
sem valor normativo para trabalho novo. Descrições de listas descartadas,
ausência de chamadas runtime e saída incondicional não se aplicam ao código
atual. Os resultados de testes ali registrados são históricos, não reproduzidos
por esta atualização. O estado atual é descrito no início deste arquivo.

## Ajustes de manutenção

- A resolução C++ usa `cp_parser_lookup_name_simple`, com verificações da
  linguagem e diagnóstico de busca quando necessário. C distingue ausência de
  declaração de entidade que não é variável. Os diagnósticos apontam o argumento.
- A saída de tipos e de lowering do compilador é opcional, habilitada por
  `-ftransaction-debug`, em stderr. Os logs do runtime continuam em stdout.
- A lista é consumida na gimplificação; o marcador GIMPLE guarda o corpo com
  as chamadas, não uma lista separada. Instrumentação futura deve planejar como
  preservar a identidade dos objetos quando precisar dela no lowering.
- O tratamento real dos estados de erro da hash é requisito da transição para
  uma hash funcional. Enquanto as primitivas forem stubs, INVALID e handles
  nulos são esperados exclusivamente pela ponte de rastreamento. Não reutilizar
  essa ponte como implementação funcional sem revisar seus chamadores.

## Validação dos ajustes de manutenção

Rebuild e checagem incremental concluídos com
`make -C /usr/src/gcc-build/gcc -j6 all`. Self-tests internos não estão
habilitados neste build. Não houve alteração de fontes da libgomp nesta etapa;
os testes usaram a biblioteca anteriormente reconstruída em
`/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/.libs`.

Resultados finais DejaGNU:

| Seleção | C | C++ |
| --- | --- | --- |
| `gomp.exp=transaction-*` | 56 PASS | 213 PASS |
| `gomp.exp=critical-*` | 20 PASS | 60 PASS |

Sem FAIL, UNRESOLVED ou UNSUPPORTED nesses resumos finais. Os novos testes
`transaction-lookup-1.c` e `.C` cobrem visibilidade, recuperação, acesso privado
e ambiguidade. As expectativas de nome ausente foram atualizadas para os
diagnósticos próprios de C/C++; não foram filtrados erros inesperados.

Passaram também 21 comandos de verificação direta: silêncio por padrão,
habilitação/desabilitação explícita de `-ftransaction-debug`, tipo da variável
local que oculta a global, distinção de nome ausente, templates/lambdas e
compilação/execução do teste de rastreamento em C/C++ com O0, O2 e LTO.
Esses resultados validam manutenção e rastreamento, não memória transacional.

Comandos reproduzíveis e evidências:

- `/tmp/newtm-adjust-build.log` e `/tmp/newtm-adjust-build-check.log`.
- `/tmp/newtm-adjust-regression.py`: comandos DejaGNU, passando o compilador e
  os caminhos de ligação via RUNTESTFLAGS. Uma tentativa inicial sem esse
  encaminhamento ligou uma libgomp antiga e falhou; os resultados finais usam
  a biblioteca correta, conforme os comandos dos logs.
- `/usr/src/gcc-build/gcc/testsuite-newtm-adjust-final/`: .sum/.log de transaction.
- `/usr/src/gcc-build/gcc/testsuite-newtm-adjust/`: .sum/.log de critical.
- `/tmp/newtm-adjust-verify.py` e `/tmp/newtm-adjust-verify/results.json`:
  verificações diretas e seus logs individuais.

A suíte completa não foi executada. Não houve instalação do compilador.

# Histórico de implementação (não normativo)



## Depuração de identificadores e tipos

Os parsers C e C++ agora consultam `lookup_name` durante a análise de cada
identificador, antes de entrar no corpo, e escrevem em `stderr`:

```text
[newtm] transaction: contador: int
[newtm] transaction: valor: double
[newtm] transaction: ausente: undef
```

A saída é incondicional para cada argumento, inclusive repetições. Não é
emitida uma linha para a forma sem argumentos. A ausência de declaração
visível imprime `undef`, sem emitir erro de nome não declarado. A consulta é
apenas para depuração: não cria uso avaliado, captura ou armazenamento na IR.
Os tipos são obtidos de `TREE_TYPE` de declarações válidas; C usa
`print_generic_expr` e C++ usa `type_as_string`, preservando referências e
tipos dependentes. Typedefs podem aparecer com seu nome de alias.

Em C++, a consulta usa os bindings ativos diretamente, evitando os diagnósticos
e efeitos do wrapper do parser para esta instrumentação. Ela não constitui uma
validação completa de acessibilidade. Sobrecargas sem tipo único aparecem como
`<overload>`; entidades encontradas sem tipo utilizável aparecem como
`<no type>`. Um namespace não tem tipo de objeto, mesmo que o nó interno
contenha `void_type_node`. Tipos dependentes são mostrados na definição do
template, por exemplo `T&`; não há nova mensagem na instanciação.

Validação: rebuild incremental em `/usr/src/gcc-build/gcc`; verificações
diretas de C/C++ confirmaram parâmetros, qualificadores, ponteiros, arrays,
structs, aliases, ocultação, repetições, nomes ausentes, escopos encerrados,
declarações posteriores e locais exclusivos do corpo. Também foram verificados
referências, membros e templates C++, além da recuperação após lista vazia.

Execução de `gomp.exp=transaction-*`, com
`TESTSUITEDIR=testsuite-newtm-scope-debug`: C, 36 PASS/7 FAIL; C++,
126 PASS/21 FAIL. As falhas são verificações de saída adicional: incluem as
mensagens preexistentes de `omp-low.cc` e a saída de depuração solicitada nos
testes `transaction-4.c` e `transaction-10.c`. Os testes de execução passaram.
Não foram modificadas as expectativas da suíte para ocultar essas mensagens.
Evidências: `/tmp/newtm-scope-debug/`,
`/tmp/newtm-scope-debug-build.log` e os `.sum`/`.log` no diretório de testes
acima. A suíte completa não foi executada.

## Resultado

A extensão está implementada em C e C++ nesta árvore GCC 16.2.0. Aceita
`#pragma omp transaction` seguido de um bloco estruturado, sem cláusulas. A sintaxe também aceita uma
lista opcional não vazia de identificadores separados por vírgula entre
parênteses: `#pragma omp transaction(a, b, c)`. Os nomes não são resolvidos
como variáveis, não são armazenados na representação intermediária e ainda
não têm efeito semântico.
O corpo executa normalmente para cada thread que encontrar a região, usando
o ambiente de dados da construção OpenMP externa. Regiões aninhadas são aceitas.

Foram consultados `OPENMP_AGENT.md` e `OPENMP_CRITICAL_AGENT.md`, e o percurso
foi conferido diretamente nos fontes. Esses dois documentos não foram alterados.

## Representação e processamento

```text
PRAGMA_OMP_TRANSACTION
  -> newtm_c_parser_transaction / newtm_cp_parser_transaction
  -> newtm_finish_transaction
OMP_TRANSACTION (um operando: corpo)
  -> gimplify_expr: caso OMP_TRANSACTION
  -> newtm_build_transaction
GIMPLE_OMP_TRANSACTION (layout GSS_OMP)
  -> verificações de bloco estruturado
  -> varredura OpenMP
  -> newtm_lower_transaction, no passe omplower
sequência GIMPLE comum, preservando binds e cleanups do corpo
  -> passes de controle, exceções, CFG, otimização e geração de código
```

O construtor GIMPLE está em `gcc/gimple.cc`, declarado em `gcc/gimple.h`.
A conversão é chamada pelo dispatcher de `gcc/gimplify.cc`.
O lowering específico está em `gcc/omp-low.cc`: transforma recursivamente o
corpo e substitui a instrução por essa sequência com `gsi_replace_with_seq`.

O código `GIMPLE_OMP_TRANSACTION` permanece explícito até esse lowering.
Não é convertido em `GIMPLE_OMP_CRITICAL`, `GIMPLE_TRANSACTION` ou outro alias.
Não sobrevive até `ompexp`, portanto não exige um caso novo em `omp-expand.cc`.

O passe de varredura registra um contexto para permitir o lowering de regiões
isoladas, mas percorre o corpo com o contexto externo. O lowering também usa
esse contexto externo. Assim, a extensão não oculta regras de aninhamento nem
o tratamento das variáveis da construção OpenMP envolvente.

## Funções novas e componentes reutilizados

As cinco funções específicas usam o prefixo exigido:

| Função | Arquivo | Responsabilidade |
| --- | --- | --- |
| `newtm_c_parser_transaction` | `gcc/c/c-parser.cc` | Parser C; rejeita tokens adicionais na diretiva e analisa o corpo. |
| `newtm_cp_parser_transaction` | `gcc/cp/parser.cc` | Parser C++; preserva a localização e analisa o corpo. |
| `newtm_finish_transaction` | `gcc/c-family/c-omp.cc` | Finalizador compartilhado que constrói `OMP_TRANSACTION`. |
| `newtm_build_transaction` | `gcc/gimple.cc` | Aloca `GIMPLE_OMP_TRANSACTION` e define seu corpo. |
| `newtm_lower_transaction` | `gcc/omp-low.cc` | Substitui a região pelo corpo sequencial transformado. |

Não foram criados tipos auxiliares, estruturas ou variáveis globais específicas.
A representação reutiliza `gimple_statement_omp`, pelo layout `GSS_OMP`.
Os identificadores novos foram pesquisados antes da implementação; não foram
encontrados conflitos com `newtm_` ou `OMP_TRANSACTION` nos fontes examinados.

Foram reutilizados o registro adiado de pragmas, os dispatchers OpenMP,
os parsers de blocos estruturados, `build1_loc`, `add_stmt`, `gimple_alloc`,
`gimple_omp_set_body`, os acessores genéricos de corpo, a cópia de sequências,
os walkers OpenMP, a impressão de regiões com apenas corpo e as verificações
existentes de blocos estruturados e aninhamento.

## Arquivos alterados

| Grupo | Arquivos |
| --- | --- |
| Registro e classificação | `gcc/c-family/c-pragma.h`, `gcc/c-family/c-pragma.cc`, `gcc/c-family/c-common.h`, `gcc/c-family/c-omp.cc` |
| Parsing | `gcc/c/c-parser.cc`, `gcc/cp/parser.cc` |
| GENERIC | `gcc/tree.def`, `gcc/tree.h`, `gcc/tree-pretty-print.cc` |
| GIMPLE | `gcc/gimple.def`, `gcc/gimple.h`, `gcc/gimple.cc`, `gcc/gimple-walk.cc`, `gcc/gimple-pretty-print.cc` |
| Conversão e lowering | `gcc/gimplify.cc`, `gcc/omp-low.cc`, `gcc/gimple-low.cc` |
| Cópia e funções aninhadas | `gcc/tree-inline.cc`, `gcc/tree-nested.cc` |
| Templates, módulos e classificação C++ | `gcc/cp/pt.cc`, `gcc/cp/module.cc`, `gcc/cp/constexpr.cc` |
| Manual | `gcc/doc/extend.texi` |

São 22 arquivos de implementação e um arquivo do manual. Foram adicionados
dez testes e este relatório. Não foram alterados fontes ou testes da `libgomp`
nem a infraestrutura de GNU Transactional Memory.

`CASE_GIMPLE_OMP`, em `gimple.h`, inclui o código novo; seus consumidores
genéricos, como o verificador de `tree-cfg.cc`, passam a reconhecê-lo sem
alterações locais. `gimple-low.cc` verifica a invariável de que a região já
deve ter sido eliminada em `omplower`. Os streamers de GIMPLE/LTO foram
examinados; a região é removida antes da serialização normal. Módulos C++
precisam reconhecer a árvore que pode permanecer em uma definição de template,
por isso `cp/module.cc` recebeu sua classificação explícita como OpenMP.

## Testes adicionados

| Arquivo | Cobertura |
| --- | --- |
| `gcc/testsuite/c-c++-common/gomp/transaction-1.c` | Execução, região vazia, declarações, atribuições, condição, laço, aninhamento e dump legível antes/depois do lowering. |
| `gcc/testsuite/c-c++-common/gomp/transaction-2.c` | Dump bruto com `gimple_omp_transaction`; ausência de alias e de chamadas GOMP/ITM; assembly sem essas chamadas. |
| `gcc/testsuite/c-c++-common/gomp/transaction-3.c` | Execução em `parallel`, redução externa, regiões aninhadas e worksharing dentro da região. |
| `gcc/testsuite/c-c++-common/gomp/transaction-4.c` | Rejeição de cláusulas e nome, com recuperação para a construção seguinte. |
| `gcc/testsuite/c-c++-common/gomp/transaction-5.c` | Contexto de arquivo inválido e corpo ausente. |
| `gcc/testsuite/c-c++-common/gomp/transaction-6.c` | Rejeição de saltos que entram ou saem da região. |
| `gcc/testsuite/c-c++-common/gomp/transaction-7.c` | Preserva a proibição de regiões críticas aninhadas com o mesmo nome, mesmo com transaction intermediária. |
| `gcc/testsuite/c-c++-common/gomp/transaction-8.c` | `_Pragma`, composição com critical e contagem das chamadas de sincronização explicitamente solicitadas. |
| `gcc/testsuite/gcc.dg/gomp/transaction-1.c` | Função aninhada C com acesso a variável externa. |
| `gcc/testsuite/g++.dg/gomp/transaction-1.C` | Template e execução de construtor/destrutor de objeto local. |

## Compilação e resultados

Foi utilizada a árvore existente `/usr/src/gcc-build`, no estágio 3,
configurada para C, C++ e LTO. DejaGNU foi instalado para executar a suíte.
O rebuild foi realizado com:

```sh
make -C /usr/src/gcc-build/gcc -j3 all
```

O comando terminou com sucesso e foi seguido de uma checagem incremental,
também bem-sucedida, para incorporar as últimas edições feitas durante o rebuild.
Houve um aviso de possível variável não inicializada em `gcc/expmed.cc`, arquivo
não alterado. O build informa que os self-tests internos não estão habilitados.
Não foi feita instalação do compilador reconstruído nem um novo bootstrap
completo de três estágios.

Comandos principais dos testes executados:

```sh
make -C /usr/src/gcc-build/gcc check-gcc TESTSUITEDIR=testsuite-newtm RUNTESTFLAGS='gomp.exp=transaction-*'
make -C /usr/src/gcc-build/gcc check-g++ TESTSUITEDIR=testsuite-newtm RUNTESTFLAGS='gomp.exp=transaction-*'
make -C /usr/src/gcc-build/gcc check-gcc TESTSUITEDIR=testsuite-newtm-critical RUNTESTFLAGS='gomp.exp=critical-*'
make -C /usr/src/gcc-build/gcc check-g++ TESTSUITEDIR=testsuite-newtm-critical RUNTESTFLAGS='gomp.exp=critical-*'
make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp check RUNTESTFLAGS='c.exp=critical-*.c c++.exp=critical-*.c'
```

| Conjunto | Resultado |
| --- | --- |
| Novos testes, C | 33 PASS |
| Novos testes, C++ | 117 PASS, com variações de padrão de linguagem executadas pelo driver |
| Regressão critical, C | 20 PASS; mesmos resultados da referência anterior ao rebuild |
| Regressão critical, C++ | 60 PASS; mesmos resultados da referência anterior ao rebuild |
| Regressão critical, libgomp | 28 PASS; mesmos resultados da referência anterior ao rebuild |
| Execuções adicionais | 12 PASS: C/C++, O0/O2/O2+LTO, uma/quatro threads |
| Módulo C++ | 1 PASS: exportação/importação e instanciação de template contendo transaction |

Os resumos desses subconjuntos não contêm FAIL, UNRESOLVED ou UNSUPPORTED.
Os testes de execução foram efetivamente executados, conforme registrado nos
arquivos `.sum` e `.log`. A suíte completa do GCC/OpenMP não foi executada.

Resultados e evidências locais:

- `/tmp/newtm-build.log` e `/tmp/newtm-build-final.log`: rebuild e checagem incremental.
- `/usr/src/gcc-build/gcc/testsuite-newtm/gcc/gcc.sum` e `gcc.log`.
- `/usr/src/gcc-build/gcc/testsuite-newtm/g++/g++.sum` e `g++.log`.
- `/usr/src/gcc-build/gcc/testsuite-newtm-critical/`: resumos/logs de critical.
- `/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/testsuite/libgomp.sum` e `libgomp.log`.
- `/tmp/newtm-critical-baseline-c.sum`, `/tmp/newtm-critical-baseline-cxx.sum`
  e `/tmp/newtm-critical-runtime-baseline.sum`: referência anterior ao rebuild.
- `/tmp/newtm-integration-checks/results.json`: verificações adicionais.
- `/tmp/newtm-implementation.patch`: patch de revisão dos fontes/manual/testes.

A inspeção direta produziu, antes do lowering:

```text
gimple_omp_transaction <
  BODY <
    gimple_assign <mem_ref, _1, *value, NULL, NULL>
    gimple_assign <plus_expr, _2, _1, 1, NULL>
    gimple_assign <ssa_name, *value, _2, NULL, NULL>
  > >
```

Depois de `omplower`, permaneceu somente:

```text
_1 = *value;
_2 = _1 + 1;
*value = _2;
```

Os dumps e assembly dessa inspeção estão em
`/tmp/newtm-representation.c.007t.gimple`,
`/tmp/newtm-representation.c.010t.omplower` e
`/tmp/newtm-representation.s`. Nenhum deles contém chamadas GOMP/ITM.

## Limitações e extensão futura

A diretiva é experimental e não pertence ao padrão OpenMP. Não oferece
atomicidade, isolamento, rollback, sincronização ou operações de memória
transacional. Não aceita cláusulas; aceita a lista opcional de identificadores descrita acima. É registrada por `-fopenmp`,
não por `-fopenmp-simd` isoladamente. A entrada foi implementada apenas em C e
C++; não foi adicionada sintaxe Fortran. Permanecem as restrições da
infraestrutura OpenMP, incluindo blocos estruturados, contextos de aninhamento
e rejeição em funções constexpr C++.

A validação foi feita em x86_64 GNU/Linux. Nenhum dispositivo de offload foi
validado, e não há promessa de identidade transacional entre host/dispositivo.
Com `-fopenmp`, o driver continua usando as regras usuais de ligação OpenMP;
a região transaction em si não adiciona chamadas ao runtime.

Para uma semântica transacional futura, os pontos explícitos de extensão são
`OMP_TRANSACTION`, `GIMPLE_OMP_TRANSACTION`, seu construtor e
`newtm_lower_transaction`. Campos, cláusulas, validações e mecanismos de
execução futuros devem ser acrescentados somente após definir essa semântica.
O código atual não escolhe locks, GNU TM ou qualquer runtime transacional como
implementação futura implícita.

## Extensão sintática: lista de identificadores

Os parsers C e C++ aceitam a forma sem argumentos e a forma
`transaction (identificador [, identificador] ...)`. Listas vazias,
expressões, separadores ausentes e vírgulas finais são rejeitados, com
recuperação até o fim do pragma e análise normal do corpo seguinte.
Os identificadores podem ser repetidos e não precisam estar declarados.
Esta alteração é apenas sintática: os operandos de GENERIC/GIMPLE e o
lowering permanecem os mesmos.

O teste `transaction-4.c` agora aceita `(named)`. Foram acrescentados
`transaction-9.c` (execução, lista simples/múltipla, macros, `_Pragma`,
aninhamento e compatibilidade com a forma original) e `transaction-10.c`
(diagnósticos e recuperação de erros), ambos compartilhados por C e C++.

Validação desta extensão: rebuild com `make -C /usr/src/gcc-build/gcc -j3 all`
concluído com sucesso. Executados `check-gcc` e `check-g++`, com
`TESTSUITEDIR=testsuite-newtm-identifiers` e `RUNTESTFLAGS='gomp.exp=transaction-*'`.
Resultados: C, 38 PASS e 5 FAIL; C++, 132 PASS e 15 FAIL. Todas as verificações
dos testes novos e do `transaction-4.c` atualizado passaram, incluindo execução
em C e C++ (gnu++98, gnu++20 e gnu++26). As falhas restantes são exclusivamente
saída inesperada das mensagens `[newtm] Inicio/Fim do tratamento das variaveis
da transacao`, provenientes de alterações já existentes em `gcc/omp-low.cc`,
preservadas nesta tarefa. O teste novo de execução filtra essas mensagens.
Não houve ERROR, UNRESOLVED ou UNSUPPORTED nos resumos finais.
Logs de build/testes: `/tmp/newtm-identifiers-build.log`,
`/tmp/newtm-identifiers-test-c.log` e `/tmp/newtm-identifiers-test-cxx.log`.
