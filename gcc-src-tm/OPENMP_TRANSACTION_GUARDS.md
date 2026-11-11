# Guardas de identidade e aritmética — subetapa 6.4.2

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

**Atualização 6.4.4 — 2026-11-04:** a
[prova de inicialização, vida útil e publicação](OPENMP_TRANSACTION_LIFETIME.md)
está implementada. Declarações de temporários de descritores pertencem à
ativação preparada; avaliações conservam seus pontos originais. Ponteiro e
binding publicados possuem igualdade verificada antes de ambas as saídas.
As pendências históricas abaixo não alteram esse estado; seguem as 6.4.5–6.4.7.

**Atualização 6.4.3 — 2026-11-03:** a
[prova de status e disponibilidade](OPENMP_TRANSACTION_STATUS.md) está
implementada. As referências abaixo à pendência de status descrevem o escopo
histórico da entrega 6.4.2. Vida útil/publicação e demais provas seguem nas
6.4.4–6.4.7; nenhuma dessas análises emite COBERTA ou certificado transitivo.

Implementada em 2026-11-01 em `gcc/newtm-guard.{cc,h}`, sobre o
[grafo temporário da 6.4.1](OPENMP_TRANSACTION_FLOW.md), dentro de
`newtm_obligation_verify`. Segue o
[contrato de verificação](OPENMP_TRANSACTION_VERIFICATION.md), especialmente V03.

## Escopo e conclusão

A inspeção reconstrói predicados matemáticos de identidade e verifica que
valem antes dos usos dependentes, em todos os caminhos alcançáveis do High
GIMPLE. Não aceita a presença de `D_VALID`, o nome de um temporário ou uma
marcação do emissor como prova suficiente. Uma marcação de cálculo identifica
uma obrigação adicional; a aprovação vem da análise do IR e das condições
efetivamente executadas. A análise também funciona com checking 0.

O critério local desta subetapa foi atendido: guardas ausentes, invertidas,
tardias, enfraquecidas ou referentes a outro valor são recusadas; junções,
ciclos e continuações de cleanup participam da prova. Não se emitem COBERTA
nem certificados locais/transitivos novos. Status/resultados (6.4.3), vida
útil/publicação (6.4.4), falhas/aninhamento (6.4.5), protocolo do controlador
(6.4.6), integração das provas (6.4.7) e certificação/remapeamento (6.5)
continuam separados.

## Valores, definições e caminhos

`newtm_guard_analysis` calcula dominadores e identifica definições escalares.
Quando uma variável tem várias definições físicas, busca retrospectivamente
a definição que alcança aquele uso. A expansão exige uma definição dominante
e ausência de redefinição relevante entre captura e uso. Cópias, conversões,
comparações e cálculos puros são normalizados; leituras de memória permanecem
snapshots, sem serem transformadas em novas leituras do referido. A formação
de ponteiros não é expandida como uma igualdade aritmética já autorizada.

Uma guarda capturada antes de alterar seu índice não prova o índice atual.
Já `old = index; index++` pode conservar o valor antigo quando a definição
capturada permite demonstrá-lo. Escritas diretas invalidam fatos dependentes;
stores indiretos e chamadas desconhecidas invalidam dependências que podem
escapar. Distinção entre objetos automáticos locais só é usada quando sua
identidade e contexto demonstram que são objetos diferentes. A ABI de leitura
pode escrever seu buffer; serviços que não escrevem memória do programa
preservam os fatos conforme seus contratos existentes.

Cada átomo requerido percorre o grafo com estados provado/não provado.
Uma operação só passa se nenhum caminho alcançável chegar a ela sem o fato.
Condições dos ramos estabelecem fatos; redefinições os retiram. Uma definição
reexecutada num laço inicia nova captura. Ramos constantes impossíveis são
descartados. Junções e cleanups não herdam aprovação de apenas um predecessor.
As continuações distintas da mesma instrução física são verificadas; expansões
ambíguas permanecem opacas e podem causar recusa conservadora.

A normalização admite as formas necessárias ao GIMPLE gerado: conjunções,
negações, limites constantes mais fortes, complementos sem sinal, flags de
overflow combinadas por OR igual a zero e a compactação de intervalo de índice
com sinal em comparação sem sinal. Esta última exige conversão sem perda de
bits e limite dentro da faixa positiva do tipo de origem. Conversão estreita,
OR diferente de zero ou AND igual a zero não autorizam inferir a condição de
cada operando. Limites de expansão preservam dependências e causam perda de
precisão, sem retirar obrigações.

## Requisitos independentes

Para objeto `o`, extensão `e`, deslocamento `d`, tamanho `n` e endereço inteiro
`a`, a prova exige, além da validade local da proveniência:

- `o != NULL`, `e != 0`, kind e armazenamento dentro das tags admitidas;
- `max(e, 1) - 1 <= PTR_MAX - a` e `d <= PTR_MAX - a`;
- `d <= e` e `n <= e - min(d, e)`;
- acesso escalar não vazio cobre exatamente o objeto escalar;
- endereço lógico do acesso corresponde a `a + d`.

As formas com min/max permitem construir o próprio predicado sem overflow.
A exigência sobre `d` inclui o endereço um após o fim: representar todos os
bytes do objeto não basta para representar esse endereço.

Índices efetivos de ARRAY_REF são verificados separadamente: não negativos,
representáveis em size_t antes de reduzir largura e inferiores à contagem
de elementos; a formação do endereço admite igualdade para um após o fim.
Componentes e arrays aninhados conservam os requisitos das bases. Endereços
constantes de declarações e elementos estaticamente dentro da faixa têm
prova estrutural própria.

Subtrações de metadados exigem subtraendo menor ou igual ao minuendo antes
do cálculo; somas exigem margem antes do cálculo. Multiplicação não checada
de identidade é recusada. Resultados de ADD/SUB/MUL_OVERFLOW exigem flag zero
antes de seu uso dependente. Conversões de identidade marcadas exigem prova
da faixa anterior à redução ou mudança de sinal. Isso não muda a semântica
modular das conversões expressamente feitas pelo código fonte.

As provas se aplicam aos acessos READ/WRITE, braços diretos, registros,
formações de endereço e transporte de argumentos associados à instância
correta. A mesma expressão lógica em duas operações não mistura seus valores.
Buffers incoming/result precisam de guarda de ponteiro não nulo antes do
primeiro acesso a seus campos. Disponibilidade por status pertence à 6.4.3.

## Ajustes de emissão necessários

A emissão agora guarda POINTER_PLUS e endereços de referências antes da
instrução original. A validação de faixa inclui a representabilidade do
deslocamento no endereço, inclusive quando o tamanho do acesso é zero.
Não reexecuta índice, incremento ou expressão de origem.

O prefixo de descritores do controlador funcional é transportado num BIND
temporário e incorporado após entrada bem-sucedida, dentro da tentativa.
Assim suas definições pertencem ao grafo inspecionado e são refeitas no retry.
A representação imutável de referência C++ tem snapshot identificado pela
instrução e pela captura real do auxiliar: a inspeção admite somente essa
cópia tipada da representação, sem autorizar leitura comum do referido.

## Janela, GC e limites

Grafo, dominadores, expansões, dependências, marcações matemáticas e vínculos
de emissão vivem somente durante a inspeção/preparação. Árvores sintetizadas
possuem raízes GC por análise, removidas no destrutor. Prefixos e snapshots
são consumidos/retirados com os descritores. Tudo termina antes de fechar a
janela, tree-nested e omplower. Não há consulta tardia a descritores nem
serialização destas provas para LTO; só os históricos escalares anteriores
permanecem. GIMPLE, CFG, SSA e propriedades da função não são alterados pelo
analisador de guardas.

O domínio é deliberadamente limitado às expressões e relações demonstráveis.
Não é um solucionador aritmético geral. Predicados não demonstrados geram
recusa V03, inclusive se forem verdadeiros por uma relação não reconhecida.
ABI/runtime, layouts, opções e limites do subconjunto permanecem preservados.
Ambiente léxico explícito, alternativa B, continua otimização futura.

## Validação

`make -C transaction_test check-guardas`: **102 verificações, 24 execuções e
1036 cenários sintéticos**, com 37 cenários por inspeção isolada/integrada.
Inclui C/C++, O0/O2, LTO, DSO/archive, checking 0/2 e GC forçado. A biblioteca
comum dos probes conserva as verificações de imutabilidade de IR/CFG.

Os negativos cobrem guarda ausente/tardia/invertida, operando/limite errado,
conversão estreita, dependência profunda, redefinição, alias, chamada,
junção incompleta, laço e cleanup. Os positivos incluem conjunções distribuídas,
captura preservada, guardas refeitas no ciclo e overflow checado. Um teste
executável usa endereço extremo sem desreferenciá-lo, com sanitizador de
overflow de ponteiros em modo trap: a falha retorna INVALID antes da formação,
preserva ambas as saídas e permite descarte. Outro confirma um após o fim
legítimo. O resolver de clone é exclusivo de teste e não altera guardas.

Logs e JSON em `transaction_test/.build/guards/`. Passaram também, com o
compilador reconstruído:

| Matriz | Verificações | Execuções explicitamente contadas |
| --- | ---: | ---: |
| Controlador + integração | 1303 | 1036 |
| Fluxo | 73 | 16 |
| Inspeção | 73 | 16 |
| Inventário de obrigações | 73 | 16 |
| Clones | 209 | 48 |
| Preparação | 37 | 8 |
| Leituras | 52 | 12 |
| Escritas | 56 | 12 |
| Chamadas | 75 | 18 |
| Ocorrências | 63 | — |
| Descritores | 36 | — |
| Subconjunto | 201 | — |
| ABI v1, símbolos/versionamento e falhas | 88 | — |

O alvo de ABI é `check-abi-v1`, executado separadamente. Esses resultados
não substituem as provas das subetapas ainda pendentes. Validação obtida em
Linux x86_64; não anunciar cobertura nativa de 32 bits ou outros backends.
