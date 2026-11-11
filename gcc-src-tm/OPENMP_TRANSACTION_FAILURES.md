# Falhas, contexto emprestado e aninhamento — subetapa 6.4.5

Atualização 6.5.3: a [inspeção atual de status e epílogos](OPENMP_TRANSACTION_CURRENT_STATUS.md)
reconstrói as saídas reais dos retornos no CFG, conserva guardas e causa de
falha e exige o retorno efetivo de FAIL. Status desconhecidos de serviços e
chamadas preparadas são tratados conservadoramente. A prova proprietária do
controlador remapeado continua no bloco 5.

**Atualização 6.4.7 — 2026-11-04:** a [composição local](OPENMP_TRANSACTION_COMPOSITION.md) foi implementada. Pendências históricas referentes à 6.4.7 estão superadas; certificação transitiva e remapeamento seguem na 6.5, e aceitação integral na 6.6.

**Atualização 6.4.6 — 2026-11-04:** a
[prova do protocolo proprietário](OPENMP_TRANSACTION_OWNERSHIP.md) está implementada.
Demonstra admissão, propriedade/vida útil do slot, confirmação, condenação e
limpeza, contador saturante, fatal e retry integral com preparação fresca.
As pendências históricas abaixo relativas à 6.4.6 estão superadas. Composição
segue na 6.4.7, certificação/remapeamento na 6.5 e aceitação integral na 6.6.
Não há COBERTA nem certificado novo; ABI, runtime e layouts preservados.

Implementada em 2026-11-04 em `gcc/newtm-failure.{cc,h}`, integrada a
`newtm_obligation_verify` depois das provas de guardas, status e vida útil.
Segue V07 e as exigências de permanência/reconstrução V01/V08 do
[contrato de verificação](OPENMP_TRANSACTION_VERIFICATION.md).

## Critério de conclusão

Todo caminho de falha demonstrável em clone ou auxiliar precisa preservar a
causa, passar por FAIL e devolver seu resultado efetivo. Nenhuma operação
normal, publicação ou encerramento normal de aninhamento pode continuar após
a falha. Toda saída de sucesso precisa conservar o contexto emprestado e
balancear os níveis abertos pela própria ativação. A prova usa caminhos do
High GIMPLE real, sem depender de nomes ou posições textuais dos rótulos.

O critério local foi atendido para o domínio abaixo, inclusive com checking 0.
Equivalências não reconhecidas são recusadas; nenhuma marca do emissor aprova
um caminho. Não há novo estado COBERTA nem certificado transitivo. O protocolo
proprietário do controlador permanece na 6.4.6, a composição com o inventário
na 6.4.7 e a certificação transitiva/remapeamento na 6.5.

## Causa e resultado efetivo

Cada nó do grafo admite sete partições: execução saudável, falha de chamada
CONFLICT/INVALID/desconhecida, falha de validação e resultado efetivo de FAIL
CONFLICT/INVALID. Valores escalares transportam conjuntos de classes de status
e testemunhos separados de causa e resultado efetivo. Cópias e conversões
integrais sem estreitamento preservam o testemunho; cálculos, substituições e
junções sem definição comum o retiram.

Uma chamada que falha invalida testemunhos anteriores e identifica sua própria
causa. Como outra operação normal é proibida a partir daí, causas de chamadas
distintas só se reúnem corretamente quando cada predecessor preenche o mesmo
transportador. Reutilizar a variável de status não reaproveita uma causa antiga.
O classificador estrutural independente precisa ter admitido cada chamada
runtime/preparada antes desta análise.

O argumento de FAIL precisa preservar a causa da operação. Não basta devolver
a causa, uma constante não zero ou um status de outra chamada: o retorno deve
conservar o resultado efetivo de FAIL. O modelo admite INVALID como resultado
de FAIL(CONFLICT), preservando a precedência de erro permanente e as validações
do runtime. FAIL com OK ou causa desconhecida resulta em INVALID, conforme a
ABI existente. Validações locais não podem inventar CONFLICT para provocar retry.

O operando de RETURN é capturado antes dos cleanups, respeitando TRY_FINALLY.
Uma chamada FAIL posterior no cleanup não corrige um retorno já capturado.
Sobrescrever o temporário depois de capturar o resultado não altera esse valor.

## Aninhamento e contexto

A análise mantém a profundidade relativa de cada caminho saudável. NEST_BEGIN
incrementa somente quando tem sucesso; NEST_END exige um nível aberto nesta
ativação e decrementa somente em sucesso. Junções e ciclos com profundidades
incompatíveis são recusados, assim como sucesso com nível pendente. Dois ou
mais níveis e laços balanceados não exigem desenrolar iterações.

Uma falha abandona a profundidade relativa: a limpeza integral pertence ao
controlador proprietário. NEST_END após falha é recusado, inclusive quando
pretende apenas balancear um BEGIN anterior. As chamadas preparadas admitidas
têm contrato de preservar a profundidade do chamador em sucesso; comprovar
transitivamente esse contrato permanece responsabilidade da 6.5.

O contexto formal não pode ser sobrescrito nem ter seu endereço exposto.
Cópias escalares demonstráveis conservam a identidade; junções com outra origem
perdem a prova. Cada operação precisa usar o contexto original. Armazená-lo em
memória ou passá-lo como dado é recusado. BEGIN, END proprietário, DISCARD,
AFTER_FAILURE e FATAL são proibidos nestes frames emprestados.

## Caminhos condenados e janela de verificação

Cada guarda de validação emitida registra uma obrigação da sua aresta falsa.
A inspeção confere sua presença e destino reais e prova que esse caminho
termina pelo epílogo permanente. Retirar evidência com forget/retire não apaga
essa obrigação. Guarda desaparecida gera V01; destino alterado sem reconstrução
gera V08. No controlador, esta subetapa confere a permanência das guardas;
o protocolo completo de seus destinos será demonstrado na 6.4.6.

Depois de uma falha só são admitidos transporte escalar, controle e clobbers
internos necessários ao epílogo. Acessos de programa, chamadas normais, novas
validações e FAIL duplicado no mesmo frame são recusados. Um segundo exame do
grafo produto exige ausência de ciclos alcançáveis nos estados condenados:
um caminho não pode girar indefinidamente em vez de devolver a falha. Laços
saudáveis continuam admitidos; não se afirma terminação geral do programa.

Grafo, estados, operandos e mapa GC de arestas existem somente na janela de
inspeção, antes de tree-nested/omplower. Não se consultam descritores após
outlining. A análise não altera GIMPLE, CFG ou SSA. ABI, runtime, layouts de
`newtm_binding_v1`/`newtm_clone_binding_v1` e canais incoming/result permanecem
preservados; a alternativa B de transporte lexical continua futura.

## Validação reproduzível

`make -C transaction_test check-falhas` compila o plugin de mutações sintéticas
e executa a matriz C/C++, O0/O2, LTO, DSO/archive, checking 0/2 e GC forçado.
Os corpos mutados são inspecionados em janelas temporárias, sem execução ou
inserção no corpo funcional. Confere também imutabilidade de IR/CFG/propriedades
e histórico escalar da inspeção de produção, sem consultar descritores tardios.

Inclui propagação de causas conhecidas/desconhecidas, cópias, substituições,
retorno efetivo, guardas permanentes, definição comum em junções, profundidade
incompatível, dois níveis, laços balanceados, falhas de entrada/saída aninhada,
ciclos condenados, contexto incorreto/exposto e serviços proprietários. Doze
casos por compilação funcional/tracing também passam pelo inspetor completo,
com positivos e recusas V07, V01 e V05 quando a prova anterior já detecta o
retorno de sucesso indevido. O programa real usa funções void preparadas,
chamadas, laços e registro aninhado, confirmando os resultados publicados.

Logs e relatório JSON ficam em `transaction_test/.build/failures/`.
Resultado: **77 verificações, 16 execuções e 1856 cenários sintéticos**
(56 casos diretos em 28 compilações, mais 12 casos do inspetor completo nas
24 compilações com preparação, inclusive tracing). Build `all-gcc` aprovado.
As matrizes anteriores de inventário, inspeção, fluxo, guardas, status e vida
útil passaram. Integração: 1303 verificações e 1036 execuções, incluindo a
matriz do controlador. A validação refere-se ao ambiente nativo Linux x86_64;
não representa o testsuite completo do GCC nem validação de outros targets.

Também passaram clones (209 verificações/48 execuções), preparação (37/8),
leituras (52/12), escritas (56/12), chamadas (75/18), ocorrências (63),
descritores (36), subconjunto (201) e ABI v1 (88, incluindo ligação dinâmica,
archive, símbolos/versionamento e injeção de falhas).

Comandos de regressão executados, além de `check-falhas`:

```sh
make -C transaction_test check-vida-util check-status check-guardas check-fluxo check-inspecao check-obrigacoes
make -C transaction_test check-integracao check-clones check-preparacao check-leituras check-escritas check-chamadas check-ocorrencias check-descritores check-subset check-abi-v1
```
