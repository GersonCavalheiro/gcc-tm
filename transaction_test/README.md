## Bloco 8 — reconstrução em LTO

`make -C transaction_test check-grafo-lto` reexecuta as provas sobre os corpos
carregados em WPA/LTRANS, LTO monolítico e incremental. Nenhuma aprovação é
transportada. A matriz cobre C/C++, O0/O2, checking 0/2, GC, DSO/archive,
recursão, GNU, EH, parallel, transações aninhadas, múltiplas unidades e partições,
COMDAT, funções inativas sem aprovação, versão antiga e objetos fat.
Mutações atingem envelope, mapas, identidade/SSA, layout, cobertura, guardas,
status, proprietário e canais; restaurar IR não revive aprovação.

O [contrato do bloco 8](../gcc-src-tm/OPENMP_TRANSACTION_LTO_GRAPH.md) explicita
a preservação temporária da unidade de prova e a restauração das opções de
otimização depois da reconstrução final. O formato é LTO 16.1: recompilar
objetos 16.0. Isso preserva a ABI v1 do runtime. É possível executar só uma
seção com `python3 verificar_grafo_lto.py ../gcc-build positive` (ou negative,
fixtures, units, format, templates). A aceitação integral continua na 6.6.
Matriz final: 426 verificações e 122 execuções. Resultados e logs no checkpoint
de retomada. O controle externo manual da ABI, fora do modo funcional, conserva
compatibilidade sem receber aprovação de grafo.

## Bloco 7 — certificado atual do grafo

Resultado do novo teste: **543 verificações e 212 execuções aprovadas**.

`make -C transaction_test check-grafo-atual` verifica composição atômica,
fechamento recursivo compartilhado, dependências zeradas, revogação e novas
gerações. Mutações alcançam corpos, assinatura, layout, CFG, controlador e
produtores de outlining; provas isoladas não publicam aprovação. A matriz
inclui C/C++, O0/O2, checking 0/2, GC, DSO/archive, GNU, EH e parallel.
A autoridade é revogada antes de IPA e não é herdada por LTO. Contrato e
limites no [relatório do bloco 7](../gcc-src-tm/OPENMP_TRANSACTION_CURRENT_GRAPH.md).
O bloco 8 acrescenta reconstrução LTO independente. Resta a aceitação integral da 6.6. Resultados no checkpoint.

## Bloco 6 — cobertura residual no CFG remapeado

`make -C transaction_test check-cobertura` fecha a classificação de cada
leitura, escrita e chamada nos corpos preparados e nos subgrafos proprietários
atuais. A memória local exige identidade e intervalo; PRIVATE exige operação
atual comprovada; canais exigem direção e extensão; chamadas exigem assinatura
e identidade de origem, consumida uma única vez. O lowering GNU possui
proveniência própria de declarações de frame/CFA, sem liberar por nome ou tipo.

A matriz injeta acessos globais/aliases, memória em endereços, condições,
construtores e retornos, efeitos antes da inicialização do slot, volatile com
flags retirados, builtins e chamadas originais/indiretas/extras, CFA sem produtor,
asm e duplicação de operações PRIVATE. Cada base precisa passar antes da
mutação. Positivos incluem escalares, endereços sem leitura e IFNs de overflow
válidos; executam em C/C++, O0/O2, checking 0/2, DSO/archive, GC e parallel.

Resultado: 695 verificações e 156 execuções. O
[contrato do bloco 6](../gcc-src-tm/OPENMP_TRANSACTION_CURRENT_COVERAGE.md)
detalha categorias e limites. O resultado é DEPENDENT com IDENTITY | CHANNELS |
STATUS | OWNER (15); não publica current_approved. Restam os blocos 7–8 e a 6.6.

## Bloco 5 — propriedade no CFG remapeado

`make -C transaction_test check-proprietario-atual` reconstrói cada protocolo
proprietário após lowering/outlining: slot, admissão, outermost, auxiliar,
STATUS/END, causa, limpeza, contador saturado e retry completo somente após
conflito. As mutações são aplicadas ao CFG atual e verificadas diretamente
pela rotina de produção. Positivos incluem cópia de contexto, comparação
invertida e sobrescrita de um resultado morto de FAIL.

A matriz cobre C/C++, O0/O2, checking 0/2, GC forçado, DSO/archive, aninhamento,
laços, múltiplos controladores, funções GNU e parallel. Recusa bypass, entradas
no meio do protocolo, retorno antecipado, status falsificado, alias sem
TREE_ADDRESSABLE, vida útil encerrada, contexto consumido e retry parcial.
O [contrato do bloco 5](../gcc-src-tm/OPENMP_TRANSACTION_CURRENT_OWNER.md)
explica as fronteiras de vida útil e as dependências CHANNELS | STATUS.
O componente não publica `current_approved`; cobertura/composição/LTO continuam
nos blocos 6–8 e a aceitação integral, na 6.6.
Resultado: 553 verificações e 88 execuções; a base passa antes de cada mutação.

## Bloco 4 — status no CFG remapeado

`make -C transaction_test check-status-atual` verifica sucesso por definição de
chamada, disponibilidade independente de valor/binding, alternativas PRIVATE
comprovadas e STATUS final da própria publicação. Falhas precisam conservar
a causa, passar por FAIL e retornar seu resultado efetivo; não podem circular
no epílogo ou prosseguir com memória/chamadas normais.

Os testes incluem os checks de BEGIN/auxiliar/END do controlador, inclusive
após parallel; aliases sem depender de TREE_ADDRESSABLE; seletores globais
alterados por chamadas; status desconhecidos; e seletores de cleanup GNU.
Os positivos executam em C/C++, O0/O2, checking 0/2 e DSO/archive, com GC
forçado. A mutação do cleanup vazio ocorre em O0, onde esse IR está presente.

O [contrato do bloco 4](../gcc-src-tm/OPENMP_TRANSACTION_CURRENT_STATUS.md)
registra limites, validações e a dependência OWNER. Não há aprovação integral
atual; o protocolo proprietário remapeado está documentado no bloco 5 acima.
Resultado: 449 verificações e 100 execuções. Passaram também as regressões
de grafo, canais, memória, controlador, integração e todas as provas anteriores.

## Bloco 3 — canais no CFG remapeado

`make -C transaction_test check-canais` verifica incoming/result, produtores de
slots e publicações, buffers tipados, extensão, inicialização, vida útil,
empréstimos e não sobreposição após ompexp. Inclui a entrada do controlador
extraído por parallel, cópias equivalentes e aliases legítimos dos dados.
Mutações com GIMPLE válido precisam ser recusadas pelo verificador de produção.

Resultado: 241 verificações e 44 execuções; C/C++, O0/O2, checking 0/2, GC
forçado e DSO/archive. O [contrato dos canais](../gcc-src-tm/OPENMP_TRANSACTION_CHANNELS.md)
explica a reconstrução lexical e os resultados condicionais a STATUS | OWNER.
Aprovação integral atual e reconstrução da certificação em LTO seguem pendentes.
Exceções lançadas e capturadas fora da transação preservam o comportamento do
código C++ comum; exceções dentro dos corpos preparados continuam recusadas.

## Bloco 2 — modelo comum de memória no CFG

`make -C transaction_test check-memoria` testa o modelo atual de identidade,
instância e armazenamento após ompexp. Inclui tuplas transportadas, slots,
PRIVATE/REGISTERED/DYNAMIC, aliases, layouts, inicializadores privados, índices,
conversões, endereços e frames. Pares positivos e cópias equivalentes executam
com ligação dinâmica/estática, C/C++, O0/O2, checking 0/2 e GC forçado. Subobjetos
de agregados do programa e frames GNU respeitam o domínio C já admitido.

Os negativos precisam ser recusados pela inspeção de produção; os probes
mantêm GIMPLE válido. Consultas isoladas também exercitam alias sem flag de
exposição e armazenamento de outra ativação com origem artificialmente igual.
O [contrato vigente](../gcc-src-tm/OPENMP_TRANSACTION_MEMORY.md) especifica os
resultados condicionais e as dependências dos blocos 3–5. Nenhuma dessas provas
concede aprovação integral atual ou reconstrução LTO. Os relatos parciais abaixo
foram superados por esse modelo comum.

Resultado da versão concluída: 281 verificações e 32 execuções em
`check-memoria`; 317 verificações em `check-transitivo`. As regressões de
controlador, integração, fluxo, guardas, composição, status, vida útil, falhas,
propriedade, ABI e chamadas também passaram.

## Bloco 2 — operações checadas do deslocamento (histórico)

`check-transitivo` remove a prova de overflow após ompexp, troca projeção ou
operador, substitui multiplicação checada por operação comum e ignora o cálculo
com um deslocamento constante. A inspeção deve recusar os casos, mantendo uma
cópia legítima aceita. C/C++, checking 0/2. O resumo matemático é consumido no
CFG atual sem consultar descritores retirados. O alcance permanece restrito
às dependências reconhecidas dos acessos estáticos; o bloco 2 segue incompleto.

## Bloco 2 — acessos STM de origem estática

`check-transitivo` confere origem, tamanho e faixa de READ/WRITE após lowering,
recusando troca de objeto com guardas coerentes, deslocamento fora do array,
tamanho/opcode alterados, duplicação e READ introduzido em clone antes sem
acessos. Pares positivos executam escalar e array com bibliotecas dinâmica e
estática. Identidades dinâmicas e classificação PRIVATE/DYNAMIC seguem pendentes;
consultar `gcc-src-tm/OPENMP_TRANSACTION_MEMORY_PROGRESS.md`.

## Bloco 2 — origem estática dos registros

`check-transitivo` troca o objeto registrado por outro inteiro e ajusta suas
guardas, modifica extensão/kind para valores válidos porém diferentes do
original, duplica registros e desloca ponteiros. Cópias e autocópias legítimas
precisam continuar aceitas. A associação é reconstruída no CFG atual; tuplas
dinâmicas continuam pendentes. O contrato está em
`gcc-src-tm/OPENMP_TRANSACTION_MEMORY_PROGRESS.md`.

## Bloco 2 — guardas de registro no CFG

`check-transitivo` inclui objeto nulo, extensão zero/máxima e kind inválido em
REGISTER após ompexp, com C/C++ e checking 0/2. O par positivo usa transação
aninhada em laço e é executado com bibliotecas dinâmica e estática.
A reconstrução integral de identidade e armazenamento continua pendente;
consultar `gcc-src-tm/OPENMP_TRANSACTION_MEMORY_PROGRESS.md`.

## Correspondência semântica das capturas — bloco 1

`check-transitivo` testa trocas de capturas e formais de mesmo tipo, campos
compartilhados trocados no produtor/consumidor e truncamento de ponteiro após
remapeamento. O par positivo é executado com bibliotecas dinâmica e estática.
O contrato e os limites constam em
`gcc-src-tm/OPENMP_TRANSACTION_REMAP_CORRESPONDENCE.md`.
A correspondência não concede a prova integral de memória ou a aprovação LTO.

## Correspondência estrutural dos controladores após outlining

`check-transitivo` testa chamada do controlador removida ou duplicada, canais
incompatíveis, origem de outlining conflitante, parâmetro com proprietário
alterado e campo desconhecido. A certificação inspeciona os CFGs reais e usa
as origens registradas no evento de extração. Correspondência semântica entre
capturas e campos de mesmo tipo e prova profunda de layouts continuam pendentes.

## Contexto emprestado no CFG transformado

`check-transitivo` testa identidade do contexto depois de `ompexp`: ponteiro
nulo com tipo correto, parâmetro sobrescrito, escrita por alias e cópia válida.
A análise de ponto fixo usa os operandos e arestas atuais, exclui variáveis
expostas e exige o mesmo contexto em chamadas preparadas e serviços
emprestados. Seu alcance atual é o CFG pré-SSA do grafo preparado no frontend;
não concede certificação integral de memória ou do grafo LTO.

## Chamadas preparadas após lowering

`check-transitivo` altera destinos, cria chamada indireta e modifica o tipo
argumento de contexto depois de `ompexp`. A inspeção do CFG preparado, integrada
à certificação, deve recusar cada corpo em C/C++, checking 0/2. Essa validação
não concede certificação integral ao IR remapeado ou ao grafo LTO.

## Auditoria de chamadas no CFG

`check-transitivo` inclui mutação de uma chamada READ após `ompexp`, recusada
pela auditoria de produção em C/C++, checking 0/2. A mesma auditoria de tipos
ABI é executada depois da leitura dos corpos LTO. Ela não substitui as provas
de identidade, status, armazenamento e cobertura integral ainda pendentes.

## Fronteiras de remapeamento — progresso da 6.5.3

`make check-transitivo` verifica o registro da passagem real por `tree-nested`
e `omplower` e a propagação da marca aos chamadores, inclusive recursivos.
A consulta distingue validade histórica de aprovação atual, que permanece falsa.
A matriz executa também um link LTO real e verifica a ausência dos registros
históricos no processo LTO. Inspeção após remapeamento, aceitação atual e
certificação do IR em LTO continuam pendentes.

## Invalidação e revalidação — subetapa 6.5.2

`make check-transitivo` cobre publicação e revogação dos certificados históricos,
revalidação de invalidação propagada e recusa após alterações de aresta ou
assinatura; também verifica que falhas não publicam uma revisão. O contrato e
seus limites constam em `gcc-src-tm/OPENMP_TRANSACTION_INVALIDATION.md`.

## Certificação transitiva — subetapa 6.5.1

`make -C transaction_test check-transitivo` valida a
[certificação pré-lowering](../gcc-src-tm/OPENMP_TRANSACTION_GRAPH.md): 33
verificações em C/C++, checking 0/2, com recursão simples/mútua, componentes,
arestas divergentes, fechamento, invalidação e mutação de dependências. Logs:
`.build/graph/`. Remapeamento e certificação através de LTO seguem na 6.5.3.

## Composição local — subetapa 6.4.7

`make -C transaction_test check-composicao` valida a
[composição e os diagnósticos](../gcc-src-tm/OPENMP_TRANSACTION_COMPOSITION.md):
93 verificações, 16 execuções e 816 cenários sintéticos. Inclui recusa atômica,
obrigações retiradas, cópias, derivações, dependências, diagnósticos, C/C++,
checking 0/2, LTO, ligação dinâmica/estática e GC forçado. Logs em
`.build/composition/`. A certificação transitiva permanece na 6.5.

## Protocolo proprietário — subetapa 6.4.6

`make -C transaction_test check-proprietario` valida a
[prova do controlador](../gcc-src-tm/OPENMP_TRANSACTION_OWNERSHIP.md):
77 verificações, 16 execuções e 2356 cenários sintéticos. Cobre
admissão/outermost, slot exclusivo e estável, auxiliar/STATUS/END, condenação e
limpeza únicas, saturação do contador, fatal e retry integral. Inclui definição
fresca do incoming e dependências, escopos/clobbers, escapes, escritas indiretas,
contexto consumido e ciclos sem readmissão. Mutações negativas passam também
pelo inspetor completo; os positivos completos são os controladores reais.
C/C++ O0/O2, LTO, DSO/archive, checking 0/2, GC e imutabilidade de IR/CFG.
Logs/JSON: `.build/owner/`. Composição segue na 6.4.7; não há certificado novo.

## Falhas e aninhamento — subetapa 6.4.5

`make -C transaction_test check-falhas` valida a
[prova dos epílogos e do contexto emprestado](../gcc-src-tm/OPENMP_TRANSACTION_FAILURES.md):
77 verificações, 16 execuções e 1856 cenários sintéticos. Cobre causa substituída
ou antiga, status desconhecido, retorno efetivo de FAIL, precedência de INVALID,
guardas permanentes, junções, dois níveis, laços balanceados, ciclos condenados,
cleanups e contexto alterado/exposto. Inclui positivos e negativos pelo inspetor
completo, C/C++ O0/O2, LTO, DSO/archive, checking 0/2, GC forçado e imutabilidade
do IR/CFG/propriedades. Logs/JSON: `.build/failures/`. O protocolo proprietário
do controlador e a composição seguem nas 6.4.6–6.4.7; não há certificado novo.

## Inicialização, vida útil e publicação — subetapa 6.4.4

`make -C transaction_test check-vida-util` valida a
[prova dos transportes e saídas](../gcc-src-tm/OPENMP_TRANSACTION_LIFETIME.md):
126 verificações, 32 execuções e 1708 cenários sintéticos. Cobre campos/operandos
indefinidos, construtores aninhados/parciais, arrays inteiros, sobreposição,
clobbers, escopos, aliases, efeitos de chamadas, escapes e publicação parcial.
Inclui o inspetor completo, C/C++ O0/O2, LTO, DSO/archive, checking 0/2 e GC
forçado. O teste executável de inconsistência entre valor e binding exige
INVALID com ambas as saídas preservadas; referências C++ condicionais confirmam
vida útil e commit. Logs/JSON: `.build/lifetime/`. Provas restantes nas
6.4.5–6.4.7; não há COBERTA nem certificado novo.

## Status e disponibilidade de saídas — subetapa 6.4.3

`make -C transaction_test check-status` valida a
[prova por definição de chamada](../gcc-src-tm/OPENMP_TRANSACTION_STATUS.md):
77 verificações, 16 execuções e 1680 cenários sintéticos. Cobre status reutilizado,
guarda ausente/invertida/tardia ou da chamada errada, cópias, valor/binding,
sobrescritas/aliases, chamadas, END, junções, laços, cleanup e produtor PRIVATE
alternativo. Inclui negativos pelo inspetor completo, C/C++ O0/O2, LTO,
DSO/archive, checking 0/2, GC forçado e imutabilidade de IR/CFG/propriedades.
Logs/JSON: `.build/status/`. Inicialização integral, vida útil e publicação
estão implementadas na 6.4.4 acima; não há COBERTA nem certificado novo.

## Guardas de identidade e aritmética — subetapa 6.4.2

`make -C transaction_test check-guardas` valida as
[provas independentes de guardas](../gcc-src-tm/OPENMP_TRANSACTION_GUARDS.md):
102 verificações, 24 execuções e 1036 cenários sintéticos. Cobre limites,
overflow, guardas incorretas/tardias, redefinições, aliases, junções, ciclos,
cleanup, conversões e formas compactadas pelo GCC; C/C++ O0/O2, LTO,
DSO/archive, checking 0/2 e GC forçado. O endereço um após o fim é testado
com sanitizador de overflow de ponteiros em modo trap, incluindo endereço
extremo, saídas preservadas em falha e caso válido. Logs/JSON: `.build/guards/`.
Status e disponibilidade de saídas estão implementados na 6.4.3 acima.

## Representação do fluxo — subetapa 6.4.1

`make -C transaction_test check-fluxo` valida o
[grafo de High GIMPLE](../gcc-src-tm/OPENMP_TRANSACTION_FLOW.md), com 73
verificações, 16 execuções e 816 cenários sintéticos. Cobre condições, switch,
laços, retornos, noreturn e continuações distintas de TRY_FINALLY, incluindo
substituição de destinos e ausência de retomada após cleanup terminal.
Confere imutabilidade do GIMPLE/CFG/propriedades, recusas estruturais, cases
alterados, C/C++ O0/O2, LTO, DSO/archive, checking 0/2 e GC forçado. Logs e
relatório JSON: `.build/flow/`. As provas de guardas estão na 6.4.2 acima;
status está implementado na 6.4.3 acima.

## Inspeção independente — etapa 6.3

`make -C transaction_test check-inspecao` valida a
[inspeção do GIMPLE final](../gcc-src-tm/OPENMP_TRANSACTION_INSPECTION.md):
73 verificações, 16 execuções e 552 checagens locais/negativas de IR, com
C/C++ O0/O2, LTO, DSO/archive, checking 0/2 e GC forçado. Cobre acessos sem
ocorrência, rótulos sem prova, chamadas residuais, alterações de operandos,
cópias, faixas e duração da janela. Logs e JSON: `.build/inspection/`.
Não certifica fluxo, grafo ou remapeamento; esses trabalhos seguem nas 6.4–6.6.

## Validação integrada — etapa 5.6

O [inventário da 6.2](../gcc-src-tm/OPENMP_TRANSACTION_OBLIGATIONS.md) é validado
separadamente por `make -C transaction_test check-obrigacoes`: fatos históricos,
operações sem vínculo, derivações, reconciliação, recusa, cópias e invalidação,
com GC forçado e checking 0/2. Logs e `report.json`: `.build/obligations/`.
Essa matriz valida o inventário; a inspeção independente é testada separadamente.

`make -C transaction_test check-integracao` inclui `check-controlador` e a matriz
nova: 1303 verificações e 1036 execuções aprovadas no total. C/C++ O0/O2, LTO,
DSO/archive e GC forçado; falhas em cada posição das operações, dois conflitos
consecutivos, laços/ramificações, PROT_NONE, agregados/aliases C, chamadas e
capturas em parallel e limites de 64 bits. Usa o pragma real e wrappers de
teste, sem invocar clones por plugin. Logs, dumps e relatório de cobertura ficam
em `.build/integration/`. Ver a
[revisão de cobertura e limitações](../gcc-src-tm/OPENMP_TRANSACTION_INTEGRATION_VALIDATION.md).
A tarefa 5 está concluída; o verificador de cobertura da tarefa 6 é separado.
Seu [contrato de obrigações/evidências da 6.1](../gcc-src-tm/OPENMP_TRANSACTION_VERIFICATION.md)
orienta a implementação; a 6.3 acrescenta uma barreira estrutural local,
sem certificados completos de fluxo/grafo/remapeamento.

## Controle externo gerado — etapa 5.5

`make -C transaction_test check-controlador` executa o pragma real com
`-fopenmp -fopenmp-transaction-stm`, sem plugin de invocação. São 225 verificações
e 106 execuções C/C++ O0/O2, LTO e DSO/archive: retry integral, conflito no corpo
e no end, referências, chamadas, capturas em parallel, certificação e limpeza
fatal comprovada por inspeção interna. Logs, dumps e `report.json` ficam em
`.build/controller/`. A opção é explícita; o padrão conserva rastreamento.
Ver o [contrato da etapa 5.5](../gcc-src-tm/OPENMP_TRANSACTION_CONTROLLER.md).

## Integração de chamadas preparadas — etapa 5.4

`make -C transaction_test check-chamadas` verifica chamadas emitidas, bindings
por ativação, capturas, resultados ponteiro, propagação de status e certificação
transitiva. São 75 verificações/18 execuções C/C++ O0/O2, DSO/archive e LTO,
incluindo recusas de callee incompleto e frame GNU incompatível. Usa sonda
exclusiva de teste e controle externo explícito; não ativa o pragma de produção.
Logs, dumps e `report.json` ficam em `.build/calls/`. Ver o
[contrato da etapa 5.4](../gcc-src-tm/OPENMP_TRANSACTION_CALLS.md).

## Vínculos de ocorrências na gimplificação

`make -C transaction_test check-ocorrencias` verifica avaliação única e os
metadados `newtm-bind` nos dumps High GIMPLE, sem ativar STM no pragma.
Inclui C/C++ em O0/O2, RMW, argumentos com efeitos, ponteiros/referências,
agregados e parallel. Um plugin de teste compilado com g++ força GC e verifica
cópias com operandos alterados, sem inseri-las no programa. Requer os headers e
o suporte a plugins do build GCC local. Comandos, logs, dumps e resultado.json
ficam em `.build/occurrences/`; timeout de 60s por compilação e 30s por execução.
O [contrato da etapa 1](../gcc-src-tm/OPENMP_TRANSACTION_OCCURRENCES.md) explica
proveniência, operandos atuais, âncoras source-only e a janela anterior a CFG/SSA.

## Exemplos funcionais com o runtime interno

[transaction_interno_simples.c](transaction_interno_simples.c) executa uma região
OpenMP parallel: cada thread faz cem incrementos transacionais de uma única
variável compartilhada. Ao final, verifica `valor == threads * 100`.

[transaction_interno_carga.c](transaction_interno_carga.c) é o equivalente STM de
transaction_carga.c: aceita `THREADS ESCRITAS:LEITURAS REPETICOES`, conserva a
sequência ponderada e verifica a invariância `inverse == ~value`. Registra o
agregado inteiro e acessa seus dois campos separadamente com offsetof/read/write.
Somente operações confirmadas entram nos contadores de leituras/escritas.

Ambos usam corpos explicitamente preparados para `newtm_execute`, com registro,
visão privada, reservas, commit, descarte e retry reais. Cada status de acesso é
traduzido explicitamente para lifecycle e propagado antes de consumir resultados.
O executor repete o corpo inteiro em conflito; temporários são recriados. Não há
critical ou E/S nos callbacks. No teste de carga, o descritor passado ao executor
é entrada/controle estável e não recebe resultados especulativos. Os objetos
compartilhados só são inspecionados diretamente depois do término da região
paralela; então o proprietário da hash é fechado explicitamente.

Para compilar e executar no ambiente do build, a partir da raiz:

```sh
make -C transaction_test exemplos-internos
OMP_NUM_THREADS=4 transaction_test/.build/transaction_interno_simples > /tmp/interno-simples.log
transaction_test/.build/transaction_interno_carga 4 1:9 1000 > /tmp/interno-carga.log
make -C transaction_test check-internos INTERNO_REPETICOES=1000
```

O compilador local e os caminhos aceitam GCC_BUILD/TRANSACTION_CC como os demais
alvos. A compilação usa -fopenmp; a ligação é uma etapa separada, sem -fopenmp,
com o archive local libgomp.a e -pthread. Isso dá acesso aos símbolos hidden sem
exportá-los e evita acrescentar uma segunda libgomp compartilhada pelo driver.
Não há ativação da integração STM do pragma.

`all` inclui os dois programas; `check` inclui `check-internos`. O alvo específico
verifica o contador com 1/2/4 threads por padrão, a carga com uma e CARGA_THREADS
threads nas proporções 1:9, 1:1, 9:1, 0:1 e 1:0, além de argumentos inválidos.
INTERNO_REPETICOES tem padrão 100; CARGA_THREADS tem padrão 4. Logs ficam em
`.build/interno-*.log`, com timeout de 60 segundos por execução. Não se exige um
número específico de conflitos: o escalonamento é variável. Confere-se balanço
de referências entre todas as tentativas e fechamento único da hash. Os logs
internos são verbosos e não constituem medição de desempenho.

Verificação desta entrega: -O2 -Wall -Wextra -Werror, contador com 1/2/4 threads
e cargas com 1/4 threads, mil operações por thread nas cinco proporções. Ligação
conferida sem dependência da DSO libgomp do sistema.

## Diagnósticos, identidade e sobreposição

`make -C transaction_test check-subset` compila a matriz C/C++ em O0/O2,
executa casos de índice avaliado uma vez com/sem LTO e verifica planos GIMPLE.
Resultados e comandos ficam em `.build/subset/resultado.json`; os logs são
separados por caso. Os planos não ativam execução STM no pragma.

`make -C transaction_test -B check-overlap` testa adjacência, incompatibilidade,
sobreposições nas duas ordens, deduplicação, aquisição pendente concorrente,
32 disputas de admissão e remoção do índice em commit/descarte.

`python3 transaction_test/verificar_regressao_compilador.py gcc-build` compila e
executa os casos existentes da extensão. É alternativa de verificação direta
quando DejaGNU não está disponível: não interpreta os scans Tcl de dg-final.
Os exemplos da ponte usam critical externo e listam também os dados preexistentes locais.
A carga de rastreamento já não mantém registros concorrentes dentro do critical;
a concorrência da hash e do índice é verificada pelos testes internos separados.

## Falha, descarte e retry de corpos preparados

`make -C transaction_test -B check-retry` usa a libgomp reconstruída e inclui
check-failure; aceita GCC_BUILD. Logs `.build/retry.log` e `.build/failure-*.log`.
Timeout de 60s no retry e 10s por processo de falha. O teste de retry cria conflito
real dentro de função aninhada, aguarda limpeza antes de liberar o concorrente
e verifica reexecução do corpo externo inteiro e liberação única de referências/
leases. Os dez processos de failure verificam fatal após limpeza e ausência de
retry de INVALID, incluindo status falso/desconhecido e nesting não balanceado.
Os wrappers são exclusivos dos testes e não acrescentam hooks ao runtime.

O executor newtm_execute é privado e recebe corpo explicitamente preparado:
registro, read/write e propagação de status são manuais. A assinatura está em
newtm-context.h. Não testa nem ativa retry do pragma compilado. Backoff é espera
ativa progressiva com teto, sem unidades de tempo ou garantia de progresso.

## Confirmação externa

`make -C transaction_test -B check-commit` usa o archive local da libgomp
reconstruída, aceita GCC_BUILD e grava `.build/commit.log` (timeout de 60s).
Cobre publicação escalar/parcial de agregados, ausência de publicação em falhas,
leitores compartilhados, nesting e limpeza completa. Wrappers de memcpy,
alocadores e liberações, restritos ao teste, pausam entre duas publicações e
confirmam conflito de read/write em ambos os objetos até terminar as cópias.
Páginas protegidas detectam escrita de faixas não modificadas. O teste não integra
a ponte GOMP, não testa retry e não injeta OOM ou ponteiros pendentes.

## Aceitação dos acessos read/write

`make -C transaction_test -B check-access` (a partir da raiz) executa check-values
e a matriz em access.c contra a libgomp reconstruída; aceita GCC_BUILD. Timeout
individual de 60 segundos, log `.build/access.log`. Cobre falhas de contexto,
registro, limites e buffers para ambas as operações, preservação das escritas
anteriores e do destino de leitura e diagnóstico da subfaixa conflitante. A
matriz confere a memória original após cada erro e descarte. OOM é fatal e não
é injetado por esse alvo. Ponteiros pendentes não são entradas de teste válidas.

## Valores privados e intervalos

`make -C transaction_test -B check-values` (a partir da raiz) usa o archive local
da libgomp reconstruída; aceita GCC_BUILD. Timeout de 60 segundos, log em
`.build/values.log`. Cobre armazenamento escalar, agregados parciais, uniões de
intervalos válidos/modificados (inclusive adjacência), read-your-writes, nesting,
limites/aliases conhecidos e falhas preservando dados. Um modelo de 500 operações
confere cada faixa válida/modificada; páginas protegidas detectam leitura de
campos vizinhos ou recarga indevida de dados já válidos. Duas threads verificam
leitura compartilhada, falha de promoção e escrita privada após descarte do outro
leitor, em 32 rodadas. Não testa publicação, retry ou integração com o compilador.

## Metadados e reservas internos

`make -C transaction_test -B check-reservations` (a partir da raiz) usa o archive
local da libgomp reconstruída, aceita GCC_BUILD e grava `.build/reservations.log`
com timeout de 60 segundos. Cobre criação sob demanda, leitores compartilhados,
escritor exclusivo, promoção, conflitos, reuso, nesting e descarte. Quatro threads
executam 16 rodadas por modo; --wrap=gomp_malloc_cleared, somente no executável de
teste, força candidatos concorrentes antes da instalação e detecta alocação sob
bucket por timeout. Página PROT_NONE garante ausência de acesso aos valores.
O alvo é separado do checkpoint e não testa read/write, publicação ou retry.

## Registro STM interno por tentativa

`make -C transaction_test -B check-register` (a partir da raiz) usa o archive da
libgomp reconstruída e os headers privados Linux x86_64. Aceita `GCC_BUILD`.
Verifica identidade/tamanho/kind, deduplicação inclusive aninhada, referências
compartilhadas entre contextos, descarte, contexto alheio/condenado e falhas sem
registros parciais (incluindo overflow injetado/restaurado). Uma página PROT_NONE
verifica que registrar não acessa valores; inspeções verificam ausência de
reservas, buffers e metadados. Timeout de 60 segundos; log `.build/register.log`.
O alvo é separado do checkpoint de oito alvos. Não valida acessos ou isolamento
STM, nem integra o registro interno à ponte pública de rastreamento.

# Exemplos da diretiva experimental transaction

Execute neste diretorio:

```sh
make check
```

O Makefile usa o GCC em `../gcc-build` pela localizacao relativa desta
arvore. Para outro build, use `make check GCC_BUILD=/caminho/do/build`.
O build deve conter C e a libgomp com os simbolos `GOMP_transaction_*`.
Os executaveis e logs ficam em `.build/`; o antigo `a.out` nao e utilizado.
Recompile os executaveis ao mudar de compilador ou flags, por exemplo:
`make -B check CFLAGS='-O0 -Wall -Wextra'`.

- `transaction_exemplo.c`: procura o maximo em paralelo, compara valor e posicao
  com uma referencia sequencial e testa um empate proposital. Lista os objetos
  efetivamente alterados (`maior` e `posicao`). Um `critical` externo sincroniza
  a atualizacao, pois transaction ainda nao oferece isolamento.
- `transaction_aninhamento.c`: executa uma regiao interna lexical e outra em
  funcao chamada. Exercita argumento repetido, reutilizacao do objeto externo e
  inclusao de um novo objeto na lista interna. Os objetos permanecem vivos ate
  a saida externa. Nao ha concorrencia neste caso.
- `transaction_carga.c`: carga parametrizada concorrente, com uma região
  transaction e uma operação lógica de leitura ou escrita por repetição.
  `verificar_carga.py` executa os três cenários de leitura/escrita do Makefile.
- `verificar.py`: verifica o resultado do maximo com 1, 2 e 4 threads solicitadas,
  as contagens dos logs e, no exemplo aninhado, a ausencia de liberacao nas saidas
  internas. Aceita equipes menores impostas pelo ambiente e relata seu tamanho.

Estes exemplos verificam o registro e a vida util da ponte de rastreamento.
A hash ja cria/destroi tabelas e adquire/consulta/libera referencias reais.
Address/size, visit e count também estão implementadas; as nove primitivas da
hash estão operacionais. Eles nao comprovam atomicidade, isolamento,
commit, abort ou reinicio. Os logs e sua verificacao precisarao ser revistos
quando a hash e a STM forem implementadas. Os `puts` internos do exemplo
aninhado servem apenas a esta etapa; nao representam E/S aprovada para uma
transacao com repeticao. Testes negativos de sintaxe, escopo e categorias de
identificadores continuam na suite DejaGNU do GCC (`transaction-*`).
## Carga parametrizada de leituras e escritas

O programa recebe exatamente três parâmetros, nesta ordem:

```sh
make all
.build/transaction_carga 4 1:9 1000
```

1. Número de threads a lançar, positivo e representável por int.
2. Relação **escritas:leituras**, com pesos inteiros não negativos e soma
   positiva representável por uint32_t. Por exemplo, 1:9 significa poucas
   escritas; 1:1, proporções iguais; 9:1, muitas escritas. Também aceita 0:1 e 1:0.
3. Número positivo de transações/repetições **por thread**. O total de
   transações precisa caber em unsigned long long.

Cada repetição executa uma região transaction sobre um objeto compartilhado.
Uma transação classificada como escrita faz read-modify-write; a relação não
conta instruções individuais de acesso. A sequência intercala os tipos de forma
determinística. Com N repetições e pesos W:R, cada thread faz
floor(N*W/(W+R)) escritas e as demais leituras; se N não completa um período,
a proporção é arredondada para baixo nas escritas.

Como ainda não há isolamento STM, um critical **dentro** da região protege os
valores compartilhados; o registro e a liberação de objetos continuam podendo
concorrer. O teste verifica contagens, valor final e consistência de um valor e
seu complemento. Não é um benchmark de STM nem comprovação de isolamento.
Desabilita equipes dinâmicas e falha claramente se o runtime não lançar a
quantidade pedida de threads, por exemplo por OMP_THREAD_LIMIT.

O alvo abaixo executa 1:9, 1:1 e 9:1 e confere também os logs da hash:

```sh
make check-carga
make check-carga CARGA_THREADS=8 CARGA_REPETICOES=10000
```

Os padrões são 4 threads e 1.000 repetições por thread. `make check` inclui
`check-carga` automaticamente. Aceita GCC_BUILD como os outros testes; use -B
para recompilar ao trocar o compilador ou flags. Logs em
`.build/transaction_carga-1-9.log`, `-1-1.log` e `-9-1.log`. O verificador limita
cada cenário a 60 segundos e usa Python com checagens explícitas, sem depender
de assert. `make run` continua executando apenas os exemplos sem argumentos.

## Criação e destruição da tabela hash

Execute `make -B check-hash-create-destroy` após reconstruir a libgomp.
O alvo aceita GCC_BUILD e usa headers/configuração privados do build Linux
x86_64, incluindo o fonte real e ligando o archive local, sem ampliar a ABI.
Verifica capacidades 1/3/1024, tabelas independentes, argumentos inválidos,
overflow, mutexes inicializados, BUSY repetido sem destruição parcial e liberação
da tabela vazia. Uma entrada ociosa é preparada apenas pelo teste para exercer
BUSY de forma independente do caminho de aquisição. Não testa inserção real ou estabelece
quiescência: esta continua sendo obrigação externa. Logs de depuração mantidos,
com saída em `.build/hash_create_destroy.log`.

## Aquisição, consulta e liberação de entradas

Execute `make -B check-hash-acquire` após reconstruir a libgomp. O teste inclui
o fonte real, usa headers/configuração privados do build Linux x86_64 e liga o
archive local. Cobre capacidades 1/3/1024, colisões, identidade compartilhada,
referências, NOT_FOUND sem inserção, SIZE_MISMATCH, nulos e overflow por injeção
interna controlada. Exercita concorrência com quatro threads e barreiras, sem
sleep, e liberação final/destrutor fora do mutex. Nenhum símbolo público ou
hook de produção foi acrescentado. Testa a mistura nativa com endereços
numéricos consecutivos/alinhados e capacidades 1/3/1009/1024/1536; não executa
nativamente a variante de outra largura. Logs em `.build/hash_acquire.log`,
com limite de 60 segundos. Aceita GCC_BUILD como os demais alvos.

## Consultas e visitas aos metadados

Execute `make -B check-hash-entry`. Usa configuração privada do build Linux
x86_64 e verifica endereço/tamanho imutáveis com referência retida, callback
sob mutex, referência preservada, instalação/substituição de metadados e
argumento nulo. Quatro threads atualizam um contador exclusivamente por visitas;
outro teste mantém um bucket bloqueado externamente enquanto uma visita em
outro bucket completa. Nenhum callback espera ou reentra na hash. A inspeção
do mutex Linux é restrita ao teste, sem hook/ABI novos. Log em
`.build/hash_entry.log`, limite de 60 segundos; aceita GCC_BUILD.

## Contagem consistente de entradas

Execute `make -B check-hash-count`. Conta entradas, não referências, com todos os
mutexes em ordem crescente e soma saturada em SIZE_MAX; libera em ordem inversa.
O teste verifica capacidades 1/3/1024, vazio, retenções repetidas, colisões,
saturação por injeção interna restaurada e concorrência com quatro threads.
Um destrutor com coordenação exclusiva de teste demonstra count zero durante
uma release ainda em andamento; destroy só é chamado depois de join. Não chamar
count dentro de callbacks/destruidores ou com mutex de bucket retido. Logs em
`.build/hash_count.log`, limite de 60 segundos, configuração privada Linux x86_64
e seleção de GCC_BUILD como nos demais testes. Printf/fflush fora dos locks.

## Diagnósticos sem inversão de locks

```sh
make -B check-hash-diagnostics
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

## Admissão e encerramento da tabela global

```sh
make -B check-hash-owner
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

<<<<<<< HEAD

## Teste dos esqueletos de ciclo de vida

Depois de reconstruir a libgomp do build selecionado:

```sh
make check-lifecycle
```

`lifecycle_stubs.c` mantém seu nome histórico, mas agora testa contextos reais:
criação, transferência exclusiva da lease, aninhamento, rejeição de slot copiado
ou outra thread, overflow, INVALID dominante e limpeza sem publicação. Prepara
registros por acesso privado para testar buffers escalares/agregados, listas de
intervalos, leitores compartilhados e escritor; registro/reservas/acessos STM
normais continuam pendentes. A preparação manual não substitui esses serviços.

O verificador também executa processos que exigem diagnóstico fatal para
metadados com participações residuais e registro inválido na ponte, verificando
neste último a liberação antes da finalização. Os printf permanecem ativos,
incluindo kind escalar/agregado/tag desconhecida nos stubs de registro. O teste
liga ao archive libgomp.a e verifica ausência dos serviços internos na API
dinâmica. Requer compilador C, pthreads, Python 3 e nm.

`GCC_BUILD=/caminho/do/build` seleciona outro build. Logs em
`.build/lifecycle_stubs.log` e `.build/context-*.log`. O teste não comprova commit,
read/write STM, backoff ou isolamento. `make check` verifica exemplos e carga de
rastreamento, com sincronização auxiliar ainda necessária.

Os estados declarados em `enum newtm_attempt_state` e o protocolo de consumo
único estão especificados em
[ABORT](../gcc-src-tm/OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa).
Este teste não executa transições nem libera recursos: verifica que os stubs
continuam sem efeitos e sem anunciar commit ou limpeza concluída.

O verificador lifecycle exige asserções Python habilitadas e recusa -O/-OO ou
PYTHONOPTIMIZE, evitando aprovar a validação de logs/símbolos sem executá-la.
Resultados da verificação geral e comandos DejaGNU com a libgomp local estão em
[OPENMP_TRANSACTION_AUDIT.md](../gcc-src-tm/OPENMP_TRANSACTION_AUDIT.md).
=======
>>>>>>> 775307665b55aaf0c7f28e25bcb02f20d9019458

## Checkpoint: contexto pronto para registro

`checkpoint_contexto_pronto_para_registro.py` reúne oito alvos: hash
(criação/destruição, acquire/lookup/release, consultas/visitas e contagem),
diagnósticos, owner, contexto e exemplos/carga. Seu cabeçalho descreve a cobertura
e as limitações. É uma referência de regressão da base atual antes de implementar
registro STM; não fecha as lacunas de cobertura identificadas para o contexto.

Reconstrua primeiro a libgomp do build escolhido. Depois, no contêiner gcc-tm:

```sh
make -C transaction_test check-checkpoint-contexto CFLAGS='-O2 -Wall -Werror'
```

O checkpoint força recompilação dos testes, executa os alvos sequencialmente e
interrompe na primeira falha ou timeout (180 segundos por alvo). Mantém os printf
nos logs e retorna código diferente de zero em falha. Variáveis GCC_BUILD,
TRANSACTION_CC, CFLAGS, CPPFLAGS, LDFLAGS, LDLIBS e parâmetros da carga são
repassados aos alvos. Não reconstrói a libgomp nem comprova correspondência entre
fontes alterados e um archive antigo.

Os logs individuais e `resultado.json` ficam em
`.build/checkpoint_contexto_pronto_para_registro/`. O relatório identifica as
variáveis usadas, comandos, resultados e o estado global da execução. Uma nova
execução invalida imediatamente a aprovação anterior. A mensagem final de
aprovação exige os oito alvos concluídos com sucesso.

Não verifica registro/acessos STM funcionais, isolamento, publicação ou retry.
Registros de limpeza são preparados manualmente; a carga ainda usa critical.
Sanitizadores e novos testes para lacunas do contexto não estão incluídos.

## Descritores de identidade do compilador

`make check-descritores` executa `verificar_descritores.py` com um plugin de teste
em High GIMPLE. Verifica campos calculados, avaliação única, identidade lógica,
kind/extensão de referências a elementos, limites por dimensão, índices extremos
e overflow em C/C++ O0/O2, canais de chamadas e descarte de metadados/cópias.
A sonda retorna antes do acesso inválido; essa proteção existe somente no teste.
Não valida integração funcional STM do pragma. Contrato e limites em
[descritores](../gcc-src-tm/OPENMP_TRANSACTION_DESCRIPTORS.md).
Relatórios e logs ficam em `.build/descriptors/`.

## Fronteira executável STM v1

`make check-abi-v1` valida a ABI exportada por DSO e archive em C/C++ O0/O2.
Testa símbolos/versionamento, layout/GC e chamadas produzidas pelo construtor
GIMPLE mediante plugin exclusivo de teste, aninhamento, conflitos entre threads,
saídas preservadas e confirmação/descarte. Interposição estática força falhas de
entrada e tags desconhecidas e observa limpeza antes de fatal. Relatórios e
logs ficam em `.build/abi-v1/`. Os corpos são explicitamente preparados; o
pragma continua em rastreamento. Contrato e limites em
[etapa 3](../gcc-src-tm/OPENMP_TRANSACTION_ABI_EXECUTABLE.md).

## Clones preparados — etapa 4

`make check-clones` testa funções geradas pelo compilador a partir de GENERIC.
A entrada nos clones dormentes e a injeção de falhas usam somente um plugin de
teste. A matriz cobre C/C++, O0/O2, DSO/archive, recursão, templates, referências,
capturas GNU, memória privada, avaliação única, resultados preservados e GC
forçado. Duas TUs verificam inline/COMDAT e identidade de objeto compartilhado,
com/sem LTO. Logs e relatório ficam em `.build/clones/`.

O layout de identidade anterior foi preservado. Contrato do transporte específico,
assinaturas, estados e limites em [clones](../gcc-src-tm/OPENMP_TRANSACTION_CLONES.md).
O pragma continua no caminho antigo; a instrumentação STM é a próxima tarefa.

## Preparação da transformação — etapa 5.1

Execute `make check-preparacao` para validar auxiliares GENERIC, capturas
explícitas e classificação dos acessos na janela anterior a tree-nested.
A matriz usa C/C++, O0/O2 e libgomp local dinâmica/archive, com GC forçado,
parallel, avaliação única, status e diagnósticos de recusa. A sonda chama
corpos dormentes; não ativa instrumentação STM no pragma. Consulte
[contrato da preparação](../gcc-src-tm/OPENMP_TRANSACTION_PREPARATION.md).

## Leituras geradas — etapa 5.2

Execute `make check-leituras`: C/C++, O0/O2, ligação dinâmica/archive e LTO,
com GC forçado, chamada real da ABI, valores privados, referência privada C++,
const/macro, agregados, página original PROT_NONE e conflito entre threads.
Falhas na segunda leitura e status desconhecido preservam as saídas. As
alterações privadas são preparadas explicitamente pela ABI write nos testes;
a geração de escritas segue na etapa 5.3. Os plugins apenas abrem entradas de
teste nos corpos dormentes; o pragma de produção permanece em rastreamento.
Consulte [contrato das leituras](../gcc-src-tm/OPENMP_TRANSACTION_READS.md).

## Escritas geradas — etapa 5.3

Execute `make check-escritas`: C/C++, O0/O2, ligação dinâmica/archive e LTO,
com GC forçado e chamadas read/write reais. Exercita read-modify-write,
avaliação única, stores derivados de chamadas/cópias agregadas, aliases,
página original PROT_NONE, status desconhecido, falhas intermediárias e conflito
real de promoção. Verifica memória original intacta antes do commit, descarte
sem publicação parcial e repetição explícita do corpo inteiro. O pragma de
produção continua em rastreamento. Consulte o
[contrato de escritas](../gcc-src-tm/OPENMP_TRANSACTION_WRITES.md).

As menções acima a escritas ainda comuns descrevem o estado histórico da 5.2.
As regressões de preparação/leitura agora consultam valores privados antes de
confirmar; incluem novamente mutação do ponteiro capturado seguida de uso.
