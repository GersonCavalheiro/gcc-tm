# Inspeção independente do High GIMPLE — etapa 6.3

Atualização da 6.5.3: o [bloco 6](OPENMP_TRANSACTION_CURRENT_COVERAGE.md) fecha
as categorias de efeitos no CFG remapeado. A inspeção High GIMPLE abaixo é
histórica e não substitui a prova atual. Composição/publicação e LTO seguem
nos blocos 7–8; current_approved permanece falso.

**Atualização 6.4.7 — 2026-11-04:** a [composição local](OPENMP_TRANSACTION_COMPOSITION.md) foi implementada. Pendências históricas referentes à 6.4.7 estão superadas; certificação transitiva e remapeamento seguem na 6.5, e aceitação integral na 6.6.

**Atualização 6.4.6 — 2026-11-04:** a
[prova do protocolo proprietário](OPENMP_TRANSACTION_OWNERSHIP.md) está implementada.
Demonstra admissão, propriedade/vida útil do slot, confirmação, condenação e
limpeza, contador saturante, fatal e retry integral com preparação fresca.
As pendências históricas abaixo relativas à 6.4.6 estão superadas. Composição
segue na 6.4.7, certificação/remapeamento na 6.5 e aceitação integral na 6.6.
Não há COBERTA nem certificado novo; ABI, runtime e layouts preservados.

**Atualização 6.4.5 — 2026-11-04:** a
[prova de falhas e aninhamento](OPENMP_TRANSACTION_FAILURES.md) está implementada.
Exige causa preservada, retorno efetivo de FAIL, contexto emprestado e níveis
balanceados em sucesso; recusa continuação normal e ciclos após falha.
As pendências históricas abaixo relativas à 6.4.5 estão superadas. Controlador
e composição seguem nas 6.4.6–6.4.7; certificação/remapeamento, na 6.5.
Não há COBERTA nem certificado novo.

**Atualização 6.4.4 — 2026-11-04:** a inspeção também executa a
[prova de inicialização, vida útil e publicação](OPENMP_TRANSACTION_LIFETIME.md).
Confere campos e seus operandos, todos os canais incoming, objetos expirados,
escapes e preservação das saídas em falha. A igualdade ponteiro/binding é
confrontada com os campos efetivamente publicados. Não há COBERTA nem novo
certificado; falhas/aninhamento, controlador e composição seguem nas 6.4.5–6.4.7.

**Atualização 6.4.3 — 2026-11-03:** a inspeção executa a
[prova de status e disponibilidade](OPENMP_TRANSACTION_STATUS.md), por definição
de chamada, inclusive quando a variável de status é reutilizada. Valor e binding
possuem disponibilidade independente; guardas incorretas e consumo antes do
sucesso ou após invalidação geram V05. O produtor alternativo PRIVATE exige
prova estrutural e cadeia de cópias correspondente. Provas restantes seguem
nas 6.4.4–6.4.7; não há COBERTA nem certificado novo.

**Atualização 6.4.2 — 2026-11-01:** a inspeção também executa as
[provas de validade/faixa/overflow](OPENMP_TRANSACTION_GUARDS.md) sobre os
operandos reais e caminhos alcançáveis, antes de fechar a janela. Guardas
ausentes, tardias ou invalidadas são recusadas por V03. O prefixo de descritores
do controlador integra a tentativa inspecionada; a cópia de representação
de referência C++ exige correspondência tipada com a captura real.
Não há COBERTA nem certificado novo. Provas restantes seguem nas 6.4.3–6.4.7.

**Atualização 6.4.1:** a inspeção agora também constrói o
[grafo de fluxo temporário](OPENMP_TRANSACTION_FLOW.md), sem modificar o IR.
GIMPLE_PREDICT foi classificado explicitamente como anotação sem efeito;
CASE_LABEL_EXPR tem comparação estrutural de faixa/destino nas cópias.
As provas de guardas estão na atualização acima; grafo e contagens não
emitem certificados nem autorizam consulta após remapeamento.

Implementada em 2026-10-31 em `gcc/newtm-obligation.{cc,h}`, segundo o
[contrato da 6.1](OPENMP_TRANSACTION_VERIFICATION.md), sobre o
[inventário da 6.2](OPENMP_TRANSACTION_OBLIGATIONS.md).

## Fronteira de execução

`newtm_obligation_verify` percorre novamente o corpo real completo. Nos clones
e auxiliares, roda depois das duas transformações, das validações de entrada,
das publicações e do epílogo comum. No controlador, roda depois de construir
todo o código de entrada, tentativa, limpeza, retry e encerramento, antes de
substituir a região original. O stub de um corpo recusado também é inspecionado;
as obrigações originais continuam RECUSADAS.

Todos esses pontos ficam no fim da gimplificação, antes de tree-nested e da
entrada de omplower. Snapshots operacionais de operandos, identidades e formas
de instrução são mantidos somente durante esse escopo aberto. Forget/retire
podem remover os vínculos originais sem apagar esses snapshots nem obrigações.
`newtm_obligation_end` libera todas as raízes operacionais da inspeção. Uma
consulta fora da janela é recusada; nenhum descritor antigo é consultado após
outlining. Permanecem apenas os fatos históricos da 6.2 e uma contagem escalar
de inspeções bem-sucedidas por declaração, sem autorização pós-remapeamento.

## Regras estruturais locais

O percurso independe de encontrar ocorrência. Inclui atribuições, operandos de
chamadas, argumentos e resultados em memória, retornos, desvios, estruturas de
limpeza e operações inseridas pelo compilador. Uma categoria desconhecida é
recusada. Não há dispensa geral por nome, DECL_ARTIFICIAL, tratamento EMITTED,
builtin, pure/const ou ausência de metadados.

| Operação | Evidência confrontada com o IR real |
| --- | --- |
| Memória automática direta | Objeto não estático, pertencente ao frame e aos BINDs da ativação; intervalo constante contido no objeto. Parâmetros por valor de clone têm armazenamento próprio. |
| Memória do controlador | Somente os objetos do novo BIND do controlador; um local preexistente da função original não se torna privado. |
| Transporte de entrada | Parâmetro exato de bindings, quantidade e layout derivados da assinatura; somente leitura, com deslocamento e extensão contidos no vetor. |
| Transporte de saída | Parâmetro exato de valor/binding de resultado, direção de escrita e extensão do tipo apontado. O status que autoriza a publicação é matéria da 6.4. |
| PRIVATE por identidade | Referência real igual à referência capturada e objeto canônico rastreável por cópias/conversões de definição única até um objeto automático da ativação. |
| Braço PRIVATE de DYNAMIC | Campo storage do transporte conhecido, definição única, comparação da mesma tag com 1 e acesso no braço local imediato, sem entrada alternativa por goto, cond ou switch. |
| READ/WRITE STM | Declaração/assinatura/contexto, relação de produtor com a instância original, objeto/deslocamento/tamanho efetivamente passados, buffer local tipado com extensão exata e substituição única da obrigação. |
| Registro | Substituição efetiva com o mesmo objeto/extensão/kind; omissão de registro privado exige objeto privado demonstrado. |
| Chamada preparada | Clone correspondente ao alvo original observado, argumentos já avaliados preservados, assinatura/contexto e relação de produção; substituição única. A região delegada usa o auxiliar capturado antes da substituição e sua invocação única. |
| Serviços de ciclo de vida | Lista explícita e assinatura/status; serviços que consomem slot ficam restritos ao controlador. |
| Helpers aritméticos | Somente ADD/SUB/MUL_OVERFLOW, dois operandos inteiros e resultado complexo inteiro; os IFNs admitem precisões/sinais diferentes. |

São recusados acessos diretos preexistentes sem prova, chamadas comuns ou
indiretas, builtins/helpers não classificados, volatile, handlers de exceção,
goto computado e estruturas OpenMP residuais. Limpezas TRY_FINALLY admitidas
têm seus corpos e efeitos inspecionados recursivamente.

As formas capturadas incluem cópias das árvores de operandos. Trocar um
operando, alterar em lugar uma referência, introduzir uma instrução sem
evidência atual, reutilizar a mesma instância duas vezes ou inserir uma cópia
invalidada não herda aprovação. Esses snapshots demonstram correspondência
estrutural; sua mera presença não classifica memória como privada.

## Obrigações retiradas e derivadas

A inspeção confronta também os registros de origem cujas âncoras já saíram.
Uma READ/WRITE/REGISTER não desaparece por forget: exige a chamada correspondente
ou a regra privada aplicável. Uma chamada preparada exige sua invocação real.
Uma região delegada exige a chamada ao auxiliar declarado. A aprovação transitiva
do corpo chamado ainda pertence à 6.5.

Stores derivados de leituras e chamadas podem ler um agregado do buffer privado
já checado. Essa READ adicional não é outro acesso preexistente apenas porque
o store possui ocorrência de origem. Sua aceitação exige intervalo privado e
leitura real do mesmo operando na mesma instância ou em sua substituição.
O mesmo vale para o agregado lido pela publicação de resultado. Não se apagam
essas obrigações por contagem ou por rótulo de emissão interna.

As recusas de produção geram erro de compilação, com regra V01/V02/V03/V04/V06/
V07/V08, causa, função, localização de operação ou escopo e espécie GIMPLE
quando disponível. A opção existente `-ftransaction-debug` acrescenta o IR ou
inventário pertinente à falha. Checking 0 executa as mesmas verificações.

## Alcance e pendências

Esta é uma barreira estrutural local adicional ao preflight da tarefa 5.
Não emite COBERTA, LOCAL_APROVADO ou GRAFO_APROVADO. A contagem de inspeções
não é um certificado e não deve ser usada para autorizar LTO/outlining.

Continuam pendentes:

- **6.4:** provas completas de validade antes de formar endereços, domínio de
  guardas/status, definições/usos, consumo de resultados, publicação, escape,
  vida útil por caminho e protocolo de falha/encerramento/retry. A estrutura
  local do despacho privado não substitui essas provas de fluxo.
- **6.5:** produção/consumo transitivo dos canais, certificação do grafo,
  invalidação de certificados e reconstrução/transferência após remapeamento.
- **6.6:** matriz completa de mutações negativas e aceitação do verificador
  integral. Os testes locais desta etapa não a substituem.

ABI compartilhada, layout de bindings, runtime, opções e subconjunto continuam
preservados. A alternativa B de ambiente léxico permanece otimização futura.

## Validação

`make -C transaction_test check-inspecao` constrói uma sonda exclusiva de teste.
A produção inspeciona o GIMPLE real antes de omplower; a sonda, posteriormente,
confere apenas a contagem histórica e abre escopos independentes para IR
sintético. Ela não reabre descritores das funções preparadas nem altera seus
corpos executáveis.

A matriz cobre C/C++, O0/O2, LTO, DSO/archive, checking 0/2 e GC forçado.
São 73 verificações e 16 execuções, incluindo 552 verificações locais/negativas
de IR: memória privada legítima, acessos sem ocorrência, disfarces PRIVATE e
EMITTED, nome artificial, objeto estático, chamadas comuns/indiretas/builtin,
READ/registro sem identidade, IFN mal tipado, alteração de operando e de árvore interna,
faixa excedida, local preexistente do controlador, inserção sem evidência,
cópia sem aprovação e consulta fora da janela. Logs e relatório JSON ficam em
`transaction_test/.build/inspection/`.

Construção final `make -C gcc-build -j4 all-gcc` aprovada. Os testes abaixo
passaram com esse compilador estabilizado, sem reconstruções concorrentes:

| Matriz | Verificações | Execuções explicitamente contabilizadas |
| --- | ---: | ---: |
| Inspeção 6.3 | 73, além de 552 checagens locais/negativas de IR | 16 |
| Inventário 6.2 | 73 | 16 |
| Controlador + integração 5.6 | 225 + 1078 = 1303 | 106 + 930 = 1036 |
| Clones | 209 | 48 |
| Preparação | 37 | 8 |
| Leituras | 52 | 12 |
| Escritas | 56 | 12 |
| Chamadas | 75 | 18 |
| Ocorrências | 63 | — |
| Descritores | 36 | — |
| Subconjunto | 201 | — |
| ABI executável | 88 | — |

As regressões usam seus executores existentes; não foi executada a suíte
DejaGNU integral. A checagem de whitespace das alterações em `gcc-src-tm` e
`transaction_test` passou. O Dockerfile preexistente não foi alterado nesta etapa.
