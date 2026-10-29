# Etapa 1: ocorrências e operandos avaliados — 2026-10-29

Implementado: associação de ocorrências GENERIC às instruções e aos operandos
GIMPLE realmente produzidos. Esta etapa prepara a instrumentação: não emite
read/write STM, clones preparados, transporte de kind ou epílogos de falha.
A ponte executável permanece de rastreamento. A [etapa 2](OPENMP_TRANSACTION_DESCRIPTORS.md)
acrescenta descritores internos sem mudar essa ABI pública.

## Representação e pontos de captura

`gcc/newtm-occurrence.cc` mantém uma tabela privada com chave **gimple***.
O valor descreve a origem, e não uma cópia executável da expressão original.
`gcc/newtm-occurrence.h` declara o contrato dessa janela de preparação.

Ao entrar em gimplify_expr, um escopo RAII cria o site de origem: identificador,
token estável da ocorrência, código e localização capturados, função, site pai e grupo
de avaliação. Não há busca por texto, posição de linha ou igualdade de expressão.
Duas invocações para um nó compartilhado podem ter sites distintos. Desde a
etapa 2 não se retém o próprio nó: helpers GENERIC podem ser explicitamente
liberados pelo gimplifier. O site conserva token e tipo de registro capturado;
não oferece expressão para obter índices ou valores.

A emissão de instruções registra o site ativo. Ao concluir uma gimplificação,
também são percorridas as instruções recém-adicionadas às filas pre/post, cobrindo
caminhos auxiliares que não usam o append principal. Um vínculo já criado pelo
site interno é conservado; não se substitui sua origem pela do contêiner externo.

Os grupos conservam a avaliação externa enquanto helpers criam INIT_EXPRs e
outros temporários. Assim, load, cálculo de endereço/valor e store de um RMW
permanecem relacionados. O tratamento cobre a transformação no mesmo nó de
EXPR_STMT pelo front-end C++, além das formas GENERIC usadas por C.

A seleção transitiva em newtm-analysis marca as funções diretas admitidas para
captura, inclusive seus corpos fora de uma região lexical própria. Isso registra
operandos formais e retornos reais da função candidata; não confunde referências
específicas do chamador, presentes no plano diagnóstico, com o corpo de um clone.
A pilha separa contextos de funções e é balanceada em retornos/erros do gimplifier.

## Dados vinculados

| Campo do vínculo | Conteúdo |
| --- | --- |
| 0 | ID da operação emitida, único na unidade de compilação |
| 1 | ID do site de origem |
| 2 | ID do site pai, ou zero |
| 3 | ID do grupo de avaliação, ou zero |
| 4–6 | Código de árvore, localização e função de origem |
| 7 | Flags: load=1, store=2, cálculo=4, chamada=8, registro de trace=16, testemunho source-only=32, retorno=64, desvio=128 |
| 8 | ID da operação copiada, ou zero |
| 9 | Testemunho de leitura de origem, somente para source-only |
| 10 | Site: ID, pai, grupo, código, localização, função, token estável e tipo de registro capturado |

IDs e layout são internos ao compilador, não ABI pública de programa/runtime.
Flags load/store indicam operações de memória, não uma decisão de que o objeto
é preexistente ou privado. Essa classificação e os descritores estão na [etapa 2](OPENMP_TRANSACTION_DESCRIPTORS.md).

Operandos e resultados executáveis são consultados **na própria instrução**:

- Atribuição: lhs/rhs e demais operandos GIMPLE. Cópia de agregado pode conter
  simultaneamente origem de leitura e destino de escrita.
- Chamada: função, argumentos avaliados e lhs efetivo, ou ausência de resultado.
- Retorno: valor do GIMPLE_RETURN, ou ausência de valor para void.
- Desvio: operandos avaliados do GIMPLE_COND.
- Registro: argumentos avaliados da chamada de rastreamento existente; isso
  ainda não acrescenta kind/contexto/status à ABI executável.

Os dumps gimple e omplower mostram `/* newtm-bind ... */` com referências,
resultados e argumentos consultados na instrução atual. Não constroem endereços
nem acrescentam avaliações. O plano `canonical-plan` antigo permanece diagnóstico
não executável, inclusive quando suas receitas ainda contêm efeitos.

## Exemplo: a[i++] += v

O compilador salva o índice antigo e calcula o novo valor uma vez. O vínculo da
leitura aponta para `a[i.2]`; o da escrita também aponta para `a[i.2]`. Ambos
conservam o mesmo grupo, e o store usa o resultado `_6` já calculado:

```text
newtm-bind ... group=24 ... mode=load read-ref=a[i.2] result=_4 ...
_4 = a[i.2];
...
_6 = _4 + v.3_5;
newtm-bind ... group=24 ... mode=store write-ref=a[i.2] ... operands=[_6]
a[i.2] = _6;
```

Uma futura emissão de read/write deve usar esses operandos. Não deve calcular
novamente `i++`, repetir uma chamada que forneceu a lvalue ou executar o offset
do plano diagnóstico. `combine(i++, j++)` é vinculado aos argumentos temporários
já escolhidos pelo gimplifier, conservando a ordem permitida pela linguagem.

## Leituras simplificadas e ausência de valores fictícios

Marcadores semânticos de leituras constantes recebem uma âncora GIMPLE_NOP com
vínculo `source-only`, `evaluated=no`, `result=none`. Isso conserva a ocorrência
observada pelo front-end mesmo quando o acesso desaparece por simplificação.
Marcadores de origem podem ser redundantes; não representam necessariamente um
load adicional ou uma correspondência um a um com instruções sobreviventes.

A âncora é não executável e não inventa um temporário, endereço avaliado ou
resultado de leitura. Um passe de instrumentação posterior deverá reconciliar
o testemunho com os acessos/constantes e o controle de registro, ou diagnosticar
o caso. Não está autorizado a tratá-lo como load já avaliado ou a liberar acesso
à memória comum. Esta etapa não implementa esse passe de instrumentação.

## Cópias, coleta e fim da janela

`gimple_copy` propaga a proveniência, atribui ID de operação novo e registra
copy-of. Origem/site permanecem os da fonte; operandos operacionais são sempre
os da nova instrução, inclusive depois de seu remapeamento. Não se guarda uma
lista velha de argumentos/índices que apontaria para variáveis da instrução antiga.

Tabela, sites, testemunhos e pilha de sites são rastreados pelo GC do GCC.
A pilha auxiliar de fatos escalares não contém ponteiros GC não rastreados.

A janela de consumo é High GIMPLE, antes de construir CFG/SSA. Os vínculos são
retirados ao iniciar lower_function_body, após omplower e seu dump. Ao encerrar
a análise inicial da unidade, o reset libera também raízes de instruções antigas
substituídas durante lowering. Não há promessa de sobrevivência a otimizações,
streaming LTO ou serialização de módulos. A instrumentação deverá consumir esses
dados antes de transformações/folds ordinários que eliminem as operações; uma
etapa futura que prolongue a janela precisará atualizar substituições e verificar
cobertura, sem interpretar metadados antigos como prova de operação sobrevivente.

## Verificação desta etapa

`make -C transaction_test check-ocorrencias` grava comandos, resultados e dumps em
transaction_test/.build/occurrences/, com relatório resultado.json. A matriz cobre
C/C++ em O0/O2: pré/pós-incremento, RMW indexado, duas leituras na mesma linha,
campos, cópia de agregados, chamadas, argumentos com efeitos, referências,
deslocamento de ponteiro, chamada que fornece lvalue, aninhamento, desvios,
constantes e região parallel.

Um plugin exclusivo de teste é compilado com o g++ do ambiente e os headers do
build local. Antes de omplower, força coleta de lixo, copia um store e uma chamada,
altera índice/argumento apenas nas cópias e confere IDs/proveniência independentes.
O dump deve mostrar os operandos novos; a execução original deve permanecer igual.
As cópias não são inseridas no programa e o plugin não acrescenta hooks ao runtime.

Resultados: 63 verificações de ocorrências, 201 verificações do subconjunto e
39 casos existentes por execução direta; check dos exemplos da ponte e internos.
Esses resultados não certificam a integração STM, que continua sem ativação.
DejaGNU e a suíte inteira do GCC não foram executados nesta etapa. Há um novo
caso DejaGNU mantido na árvore, também executado pelo verificador direto.
