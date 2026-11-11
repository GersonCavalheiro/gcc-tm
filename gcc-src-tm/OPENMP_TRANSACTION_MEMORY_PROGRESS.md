# Bloco 2 da 6.5.3 — identidade e armazenamento no CFG

Estado vigente: bloco 2 concluído como componente do frontend.

Atualização posterior: o [bloco 3 de canais](OPENMP_TRANSACTION_CHANNELS.md)
também foi concluído como componente do frontend. Confere buffers e produtores
no CFG remapeado, com dependências STATUS | OWNER; a composição e a aprovação
atual continuam pendentes. O [bloco 4](OPENMP_TRANSACTION_CURRENT_STATUS.md)
também foi concluído com dependência OWNER. Próximo trabalho: bloco 5 da 6.5.3.

O contrato atual está em [identidade e armazenamento no CFG](OPENMP_TRANSACTION_MEMORY.md).
O módulo comum reconstrói tuplas estáticas e transportadas, instâncias, layouts,
armazenamento, efeitos privados, construtores e aritmética no CFG real. Registros
dinâmicos não são liberados pela checagem estática isolada: seguem obrigatoriamente
para essa inspeção. O resultado conserva dependências explícitas dos blocos 3–5;
current_approved permanece falso. Os registros abaixo descrevem avanços históricos.

Validação do modelo comum: `check-memoria` passou em 281 verificações e 32
execuções; `check-transitivo`, em 317 verificações. Controlador (225/106) e
integração (1078/930) passaram. Build all-gcc concluído sem avisos; self-tests
internos desabilitados. Os detalhes, limites e requisitos externos estão no
contrato vigente, que prevalece sobre as pendências históricas abaixo.
Regressões finais: fluxo (73/16/816), guardas (102/24/1036), composição
(93/16/816), status (77/16/1680), vida útil (126/32/1708), falhas
(77/16/1856), propriedade (77/16/2356), ABI (88) e chamadas (75/18), todas
aprovadas na versão final.

## Implementado

`newtm_flow_graph::build_cfg` cria uma visão temporária, somente de leitura,
das instruções, blocos e arestas do CFG pré-SSA real. Conserva as arestas
verdadeira/falsa e os ciclos, calcula alcançabilidade e não sintetiza High
GIMPLE nem infere escopos lexicais. PHIs, arestas excepcionais/anormais,
controle não local, efeitos internos sem regra e outras instruções não
classificadas são recusados. A visão serve às provas matemáticas; não fornece
as fronteiras lexicais e de cleanup da análise antiga de High GIMPLE.

A certificação dos corpos preparados usa essa visão e o mecanismo de guardas
para verificar REGISTER a partir dos argumentos atuais. Exige objeto não
nulo, extensão não nula, kind admitido e representabilidade do último byte no
espaço de endereços. Os predicados não fazem subtração inválida nem formam
ponteiro fora da faixa. Fatos precisam valer em todos os caminhos que chegam
à chamada. A análise usa os valores e as definições atuais, com a retirada de
fatos implementada pelo analisador de guardas. Não consulta descritores nem
suas marcações matemáticas retiradas; usa somente a interface require.

A divergência produz erro durante a certificação. A checagem matemática isolada não comprova
que o objeto registrado é o objeto lógico pretendido ou que sua extensão e
kind correspondem ao objeto canônico. Também não demonstra sucesso do registro.

## Validação

Os probes alteram objeto, extensão (zero e máximo do tipo) e kind após
ompexp, mantendo a assinatura ABI bem formada. C/C++, checking 0/2 precisam
receber o diagnóstico da nova checagem. O par positivo contém registro em
transação aninhada dentro de um laço e é executado com bibliotecas dinâmica
e estática. O adaptador conserva a execução única das expressões: ele não
modifica instruções nem gera avaliações no programa.

## Pendências para concluir o bloco

- Modelo de objeto e instância de armazenamento distinto da origem de declaração.
- Reconstrução coerente dos campos de identidade usados por READ/WRITE e registros.
- Relação entre objeto canônico, extensão, deslocamento, tamanho, kind e endereço lógico.
- Provas de índices, aritmética dependente e overflow dos acessos e formações de endereço.
- Classificação e comprovação de PRIVATE, REGISTERED e DYNAMIC no CFG remapeado.
- Layouts, intervalos de subobjetos e relação com a instância correta dos frames.
- Dependências explícitas de inicialização/vida útil, status e propriedade,
  a serem fechadas pelos blocos 3–5; nenhuma aprovação pode ser inferida delas.
- Ampliação da matriz de mutações e integração de todos esses resultados.

current_approved permanece falso. O bloco 1 e o certificado histórico não
substituem as propriedades acima. O bloco 2 não está concluído.

A versão final passou em 193 verificações transitivas, guardas
(102 verificações/24 execuções/1036 cenários), composição (93/16/816),
controlador (225/106), integração (1078/930) e chamadas (75/18).
O build all-gcc passou sem avisos; self-tests internos estão desabilitados.

## Associação dos registros com origem e metadados estáticos

Antes de fechar a preparação, a emissão completa de cada função é resumida
por instância de chamada, identificada pela declaração do canal de status.
O resumo conserva apenas essa declaração e, quando demonstráveis, origem do
objeto no papel de endereço, extensão constante e kind constante. Não conserva
GIMPLE ou descritores. Registros de tupla dinâmica são explicitamente distintos
dos registros cuja associação estática pode ser demonstrada.

No CFG atual, cada registro precisa corresponder a uma instância emitida e não
pode duplicá-la. Para os casos estáticos, a reconstrução deve recuperar a mesma
origem, extensão e kind. Cópias e conversões que preservam a representação são
admitidas. A busca usa definições que dominam o uso e não foram invalidadas;
cada operando é resolvido no ponto de sua definição, para não confundir versões
sucessivas da mesma variável. Casos não resolvidos de um resumo estático são
recusados. Estado dinâmico pendente não é convertido em prova estática.

As mutações trocam o objeto por outro inteiro, inclusive ajustando todas as
guardas para o objeto novo; mudam extensão/kind para valores numericamente
válidos, duplicam o registro e deslocam o ponteiro. Pares positivos inserem
cópia e autocópia legítimas. Os probes ocorrem após ompexp, em C/C++, checking
0/2. São recusas efetivas na certificação, independentes de checking.

Este avanço não fecha o modelo de objeto dinâmico/instância, a coerência de
tuplas recebidas por canais, a relação com layouts atuais ou a classificação
PRIVATE/REGISTERED/DYNAMIC dos acessos. Os registros dinâmicos continuam
pendentes e current_approved permanece falso. O bloco 2 segue incompleto.

A revisão de associação estática passou em 221 verificações transitivas,
guardas (102/24/1036) e composição (93/16/816). O build passou sem avisos.

## READ/WRITE com origem estática reconhecida

A preparação agora resume também as instâncias READ e WRITE, pela declaração
do canal de status, conservando opcode e, quando demonstráveis, origem no
papel de endereço, extensão constante do objeto de origem, tamanho e kind.
Um deslocamento constante demonstrável também é conservado. Esses resumos
contêm declarações e constantes, não instruções ou descritores retirados.

No CFG atual, os acessos precisam corresponder a uma instância emitida, sem
duplicação nem troca entre READ e WRITE. Isso também é conferido quando a
função originalmente não possuía esses acessos. Para a parcela estática,
a origem e o tamanho precisam continuar correspondentes; deslocamentos
constantes conhecidos não podem ser trocados. Cópias são resolvidas pelas
definições dominantes e atuais, nos respectivos pontos de uso.

Para esses objetos, o analisador de guardas exige antes da chamada: objeto
não nulo, representabilidade do último byte, representabilidade de objeto +
deslocamento, deslocamento dentro da extensão e tamanho dentro do restante.
A subtração usa min para evitar underflow no próprio predicado. Acesso escalar
não vazio precisa cobrir o objeto inteiro. São condições sobre os argumentos
atuais da chamada; não são aprovação obtida dos resumos históricos.

As mutações alteram o objeto e suas guardas coerentemente, tamanho, deslocamento,
opcode e quantidade de operações; outra insere READ em clone antes sem acesso.
Um array com índice dinâmico testa deslocamento máximo, além dos pares positivos
executáveis com escalar e array, em C/C++, checking 0/2 e ligação dinâmica/estática.
Uma cópia legítima do ponteiro também precisa continuar aceita.

Limites: acesso de origem dinâmica continua pendente; não se reconstrói ainda a
tupla completa de bindings. A extensão estática é relativa à declaração de
origem reconhecida, sem fechar o modelo de instâncias/frames e layouts atuais.
A guarda da faixa final não comprova, por si só, ausência de overflow nos cálculos
que produziram o deslocamento, validade de formações de endereço anteriores ou
correspondência de índice dinâmico com a operação lógica original. PRIVATE,
DYNAMIC, buffers, status e vida útil continuam nas pendências indicadas acima.
current_approved permanece falso; o bloco 2 continua incompleto.

A revisão de acessos estáticos passou em 293 verificações transitivas,
guardas (102/24/1036), composição (93/16/816), controlador (225/106),
integração (1078/930) e chamadas (75/18). Build all-gcc sem avisos.

## Dependências de overflow do deslocamento

A janela de preparação fornece uma consulta somente de leitura das obrigações
matemáticas enquanto elas ainda estão ativas. Antes de fechá-la, o resumo
conserva, para as projeções de operações checadas reconhecidas, as declarações
de resultado/par, o operador de overflow, a largura e o sinal do componente.
Cada acesso conserva também as identidades das projeções necessárias a seu
deslocamento. Os resumos não contêm GIMPLE ou descritores e não conferem aprovação.

Depois do lowering, a análise percorre as definições atuais nos pontos de uso.
Uma projeção matemática reconhecida precisa continuar usando a parte real do
mesmo par, produzido pela mesma operação checada, com largura e sinal
compatíveis. A flag da própria instância deve ser demonstrada como zero em
todos os caminhos alcançáveis até o acesso. A lista de dependências impede
que substituir todo o cálculo por um deslocamento constante apague a obrigação.
Cópias legítimas continuam admitidas. Esgotar a profundidade de prova ou perder
a proveniência exigida provoca recusa. Nenhuma consulta aos descritores antigos
ou às suas marcações matemáticas ocorre durante esta inspeção do CFG.

Os testes depois de ompexp falsificam a flag com zero, trocam a parte real pela
imaginária, trocam MUL_OVERFLOW por ADD_OVERFLOW, substituem a multiplicação
checada por multiplicação comum com flag zero e ignoram o cálculo usando um
deslocamento constante. O par positivo insere uma cópia do deslocamento.
Todos são exercitados em C/C++, checking 0/2.

Limites: a prova cobre as operações checadas reconhecidas do deslocamento dos
acessos estáticos desta etapa. Não é uma prova geral da aritmética do código
fonte, não fecha todas as conversões estreitas ou índices, não demonstra a
validade de cada formação de ponteiro anterior e não recompõe identidades
dinâmicas, instâncias, layouts ou PRIVATE/DYNAMIC. O bloco 2 permanece incompleto
e current_approved permanece falso.

Resultado da revisão: 317 verificações transitivas, guardas (102/24/1036),
composição (93/16/816), controlador (225/106), integração (1078/930) e chamadas
(75/18). Build all-gcc sem avisos; self-tests internos seguem desabilitados.
