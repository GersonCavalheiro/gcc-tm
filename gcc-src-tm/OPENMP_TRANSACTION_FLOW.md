# Representação do fluxo — subetapa 6.4.1

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

Implementada em 2026-10-31 em `gcc/newtm-flow.{cc,h}`, integrada à
[inspeção da 6.3](OPENMP_TRANSACTION_INSPECTION.md) e ao
[contrato de verificação](OPENMP_TRANSACTION_VERIFICATION.md).

## Escopo e critério de conclusão

A entrega constrói um grafo próprio sobre o High GIMPLE completo, sem baixar
o corpo nem construir/modificar o CFG normal do GCC. Representa sequência,
condições, goto direto, switch, retornos, chamadas terminais e limpezas
TRY_FINALLY. Indexa também instruções inalcançáveis; estruturas e destinos
não admitidos são recusados. Ciclos convergem por estados canônicos, sem
desenrolar iterações. A construção participa da inspeção de produção antes
do fechamento da janela, inclusive com checking 0.

Esses critérios foram atendidos. As [provas de guardas da 6.4.2](OPENMP_TRANSACTION_GUARDS.md)
e a [disponibilidade por status da 6.4.3](OPENMP_TRANSACTION_STATUS.md)
também estão implementadas. A [6.4.4](OPENMP_TRANSACTION_LIFETIME.md) acrescenta
inicialização, vida útil e publicação. Falhas/aninhamento, protocolo e
composição da tentativa ainda pertencem às **6.4.5–6.4.7**.
Construir o grafo não demonstra que uma guarda presente é suficiente ou que
uma guarda necessária não foi removida. Não se emitem COBERTA nem certificados
de aprovação local/transitiva nesta subetapa.

## Representação e duração

`newtm_flow_graph` contém um índice de instruções, escopos lexicais, estados
de execução e arestas com predecessores/sucessores. As arestas distinguem
sequência, verdadeiro/falso, case/default, goto, retorno, desenrolamento de
limpeza, retomada e término. Cases conservam faixa e destino como seletores.

Há saídas separadas para conclusão normal do escopo analisado, retorno da
função e término por chamada noreturn. O controle de chamada admite a
continuação normal ou o término indicado por seus atributos; chamadas sinalizadas com
controle não local ou returns_twice são recusadas. A classificação semântica
das chamadas permanece na inspeção da 6.3 e nos contratos admitidos. Não são
modeladas exceções: o subconjunto continua excluindo esse suporte e handlers
TRY_CATCH são recusados.

BIND, eval e cleanup possuem identidades lexicais distintas. Um goto pode
alcançar um BIND ordinário sem executar inicializadores anteriores ao alvo;
o grafo conserva esse fato. A disponibilidade das saídas de chamadas por
caminho é verificada na 6.4.3; vida útil e inicialização dos transportes estão na 6.4.4.
Entradas por desvio que contornem a estrutura de um eval/cleanup são recusadas.

Somente estados alcançáveis são expandidos. Uma continuação pode ter um nó
sintético reservado que permaneça inalcançável; `node.reachable` distingue
esse caso. O índice permite consultar presença e alcançabilidade da instrução
original, mesmo sem nó de execução alcançável. As duas possibilidades não
devem ser confundidas por consumidores futuros.

O grafo é temporário e mantém referências emprestadas ao IR da janela atual.
Não altera instruções, rótulos, BINDs, CFG, SSA ou propriedades de `cfun`.
Deve ser destruído antes de retirar as evidências operacionais. A integração
o constrói dentro de `newtm_obligation_verify`, depois da classificação
estrutural, e o destrói antes de `newtm_obligation_end`, tree-nested e omplower.
Não consulta descritores e não autoriza sua recuperação posterior.

Somente a contagem escalar cumulativa de nós por declaração fica disponível
como histórico em `newtm_obligation_flow_nodes`. Essa contagem não é prova,
não conserva um grafo e não autoriza consumidores após remapeamento/LTO.
Cada grafo aceita uma construção; novas inspeções exigem um objeto novo.

## Continuações de TRY_FINALLY

Uma saída de eval executa o cleanup correspondente antes de retomar o destino
pendente. Retorno e goto que saiam de vários níveis executam as limpezas
aplicáveis de dentro para fora. Saltos internos ao mesmo eval não executam
prematuramente a limpeza.

A mesma instrução física de cleanup pode possuir vários nós de análise,
separados por continuação. Dois retornos não se misturam ao atravessar uma
limpeza compartilhada. Não se copia GIMPLE: o nó de retorno original representa
a avaliação de seu operando antes das limpezas; a saída sintética conserva
a origem como informação, sem reavaliar o operando. Essa distinção deverá
ser respeitada pela futura análise de definições/usos.

Um retorno ou goto que saia do próprio cleanup substitui seu destino pendente.
As continuações de cleanups externos que ainda contêm o novo destino continuam
válidas. Um cleanup noreturn não retoma o destino antigo. Uma chamada noreturn
no eval também não cria uma saída normal que execute cleanup; exceções e
desenrolamento excepcional continuam fora do contrato admitido.

Destinos pendentes são reservados sem ativá-los antecipadamente. A worklist
só os visita quando uma aresta alcançável de retomada é criada. Assim, um
cleanup terminal não torna o antigo retorno alcançável apenas porque esse
destino já foi registrado.

## Ciclos e recusa

Estados são identificados por posição e continuação. Transferências são
internadas por escopo/destino; continuações por cleanup/estado de retomada.
O contexto pendente acompanha o aninhamento lexical de limpezas, não o número
de iterações. Desvios não podem reentrar em eval/cleanup contornando sua entrada.
A worklist expande cada estado alcançável uma única vez; ciclos reutilizam
os estados existentes. Uma busca iterativa também identifica arestas de ciclo,
sem recursão sobre as voltas do programa.

Antes da expansão, são validados todos os alvos, inclusive em código morto.
Rótulos ausentes, duplicados ou externos, goto computado, condições sem alvo,
switch malformado, repetição de instância GIMPLE, controle não local, handlers
e estruturas desconhecidas produzem recusa V02/V07/V08. Nenhum caminho é
silenciosamente omitido por não possuir ocorrência ou por não ser reconhecido.

## Ajustes de classificação necessários

O caso real com continue revelou GIMPLE_PREDICT, anotação de probabilidade
sem efeito de execução. Ela passou a ser explicitamente admitida como nó
sequencial pela inspeção e pelo grafo. Isso não cria dispensa para operações
desconhecidas nem amplia o subconjunto de efeitos.

O caso real com switch revelou que `operand_equal_p` não é um comparador
adequado para cópias de CASE_LABEL_EXPR, da classe statement. A inspeção agora
compara explicitamente faixa, destino, tipo e encadeamento desses rótulos.
Cases intactos são aceitos; alterações de faixa ou destino continuam recusadas.
Contraprovas executam o verificador real sobre IR sintético fresco e restauram
o IR ao final, sem alterar funções executáveis.

## Validação

`make -C transaction_test check-fluxo` compila a sonda de teste e o caso real
`flow.c`. São **73 verificações, 16 execuções e 816 cenários sintéticos**:
34 cenários por compilação em 24 configurações. A matriz cobre C/C++ O0/O2,
LTO, DSO/archive, checking 0/2 e GC forçado; o modo de rastreamento e stubs
recusados também são compilados.

Os cenários verificam sequências vazias, condições, junções, ciclos infinitos
e com múltiplas entradas, switch/default/faixas, saídas normais e abruptas de
try, limpezas aninhadas, continuação distinta por retorno, substituição de
retorno por cleanup, noreturn, laço dentro de cleanup e goto para BIND que
salta a inicialização. Incluem as recusas acima e mutações de cases.

Cada cenário compara a representação textual do GIMPLE antes/depois e verifica
que CFG e propriedades de `cfun` permanecem iguais. GC é forçado com o índice
operacional aberto, inclusive com o grafo vivo. A sonda posterior a gimplificação
consulta somente as contagens históricas da produção; seus grafos sintéticos
usam instruções novas, sem reutilizar descritores de funções remapeadas.

O executável real compara a função original e a preparada em dez entradas,
com for, continue, switch, acessos privados e retorno antecipado. Logs e
relatório JSON ficam em `transaction_test/.build/flow/`.

Construção: `make -C gcc-build -j4 all-gcc`. As matrizes existentes
`check-inspecao` e `check-obrigacoes` passaram após a integração, com 73
verificações e 16 execuções cada. A 6.3 conserva suas 552 checagens locais.
A integração final passou: controlador 225 verificações/106 execuções mais
integração ampliada 1078/930, totalizando **1303 verificações e 1036 execuções**.
As regressões de clones (209), preparação (37), leituras (52), escritas (56),
chamadas (75), ocorrências (63), descritores (36), subconjunto (201) e ABI (88)
também passaram. Não foi executada a suíte DejaGNU integral.

ABI, runtime e layouts de bindings permanecem preservados. A alternativa B
de ambiente léxico continua futura. Certificação transitiva e validade após
remapeamentos continuam na 6.5; a aceitação integral do verificador, na 6.6.
