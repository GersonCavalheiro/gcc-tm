# Status por operação e disponibilidade de saídas — subetapa 6.4.3

Atualização 6.5.3: a [prova no CFG remapeado](OPENMP_TRANSACTION_CURRENT_STATUS.md)
reconstrói instâncias de chamadas, alternativas PRIVATE, canais e publicações
após lowering/outlining. Conserva estados separados por fase nos cleanups
compartilhados; o protocolo proprietário completo continua no bloco 5.

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
está implementada. A associação por status agora também exige a definição
final da própria instância de publicação. Pendências abaixo relativas à 6.4.4
descrevem o escopo histórico da entrega 6.4.3; seguem abertas as 6.4.5–6.4.7.

Implementada em 2026-11-03 em `gcc/newtm-status.{cc,h}` e integrada a
`newtm_obligation_verify`, depois das
[provas de guardas](OPENMP_TRANSACTION_GUARDS.md). Segue a regra V05 do
[contrato de verificação](OPENMP_TRANSACTION_VERIFICATION.md).

## Escopo e critério de conclusão

Uma comparação com zero só autoriza a operação que produziu a definição de
status observada. Reutilizar a variável do controlador não reutiliza a prova.
Valor retornado e binding de resultado são acompanhados como saídas separadas;
nenhuma pode ser consumida antes de demonstrar sucesso da chamada correspondente.
Uma escrita ou exposição que possa alterar o buffer retira sua disponibilidade.

A prova participa do caminho de produção, inclusive com checking 0. Não depende
de uma comparação imediatamente adjacente à chamada: cópias e instruções
intermediárias seguras são admitidas quando o fato continua demonstrado em
todos os caminhos. Ausência de prova produz recusa V05. O critério local da
6.4.3 foi atendido; não há novo estado COBERTA nem certificado transitivo.

Inicialização integral dos transportes, vida útil e publicação (6.4.4),
correção completa de epílogos/aninhamento (6.4.5), protocolo proprietário do
controlador (6.4.6), composição das provas (6.4.7), certificação/remapeamento
(6.5) e aceitação integral por mutações (6.6) continuam pendentes.

## Definições e prova por caminhos

Cada chamada admitida com status explícito uint32_t constitui uma operação
distinta. O analisador percorre o grafo temporário de High GIMPLE, separadamente
para cada definição física de chamada e para todas as suas instâncias de fluxo,
incluindo continuações de cleanup. A reexecução da mesma chamada num ciclo
inicia nova produção e invalida aliases do status da execução anterior.

Os estados de controle distinguem operação ainda ausente, resultado pendente,
sucesso demonstrado e falha demonstrada. Cópias escalares conservam a associação
com a definição, assim como conversões inteiras que preservam seus bits e
booleanos derivados de igualdade/desigualdade com zero. Conversões estreitas,
valores constantes escritos sobre o status ou comparações com outra chamada
não criam uma prova. Escritas por alias também retiram associações possíveis.

Uma junção conserva somente associações escalares iguais em todos os
predecessores e reúne as possibilidades de controle e de disponibilidade.
Portanto, um caminho sem guarda não é autorizado pelo caminho vizinho que
testou o status. A worklist converge sobre estados finitos, sem desenrolar
iterações. Ramos de comparação entre constantes impossíveis são descartados;
relações não reconhecidas permanecem conservadoramente possíveis.

Antes da próxima operação admitida, de acesso de memória dependente, de retorno
de sucesso ou da conclusão normal, nenhum caminho pode conservar status
pendente ou de falha daquela operação. Retornar o próprio status é propagação,
sem autorizar consumo de saídas. Um retorno não zero ou do serviço FAIL também
pode propagar falha; provar a causa exata e o epílogo correto pertence à 6.4.5.
Retornar uma variável preenchida com zero não substitui a guarda ausente.

Uma consulta posterior STATUS não autoriza retroativamente READ, WRITE,
registro ou chamada preparada sem a verificação de sua própria definição.

## Saídas e produtores alternativos

O inspetor lê os operandos reais da chamada:

| Operação | Saídas acompanhadas |
| --- | --- |
| READ | Buffer privado tipado do argumento de saída. |
| Clone com retorno não void | Objeto privado do valor retornado. |
| Clone com retorno ponteiro | Valor e transporte de binding, separadamente. |
| BEGIN | Campo de saída outermost; propriedade do slot fica para a 6.4.6. |
| WRITE, REGISTER, STATUS, NEST_BEGIN, NEST_END, END | Status antes da continuação; sem buffer de resultado próprio. |

Ponteiros de saída só são resolvidos por endereço explícito ou cadeia de
cópias/conversões com definição única, não exposta e dominante. Os objetos
devem ser automáticos e os tipos devem corresponder à assinatura. A inspeção
estrutural anterior conserva os requisitos de extensão e direção do transporte.

O estado de cada saída distingue ausência de produção, inicialização direta,
produção ainda sem sucesso, produção liberada, invalidação e substituição por
outro produtor conhecido. A validação do segundo produtor é independente;
sobrescrever um buffer não transfere a aprovação do produtor anterior.
As provas de disponibilidade de valor e binding não se confundem.

Uma leitura DYNAMIC pode produzir o mesmo buffer no braço PRIVATE, sem executar
READ naquele caminho. A produção alternativa só é reconhecida quando a
inspeção estrutural comprova o acesso privado e sua instância de origem,
confronta a referência lógica e demonstra a cadeia de cópias dominante até
o buffer completo. Nomes e rótulos do emissor não autorizam essa alternativa.
Isso permite alternar os braços num laço sem aceitar uma sobrescrita comum
como se fosse um resultado de READ.

## Escritas, aliases e chamadas

Stores diretos ou através de aliases possíveis invalidam a saída afetada.
O analisador percorre definições de ponteiros e endereços, inclusive ciclos,
para conservar a possibilidade de alias quando uma definição é ambígua.
Tomar o endereço não consome o conteúdo; desreferenciar, copiar campos ou
entregar o buffer como entrada a uma chamada pode consumi-lo.

Objetos automáticos distintos cujo endereço não foi exposto são separados dos
objetos do programa acessados por indireção. A exposição é examinada no IR real:
stores de endereços, agregados, argumentos de chamadas e retornos. Serviços da
ABI obedecem seus contratos de empréstimo e não retenção de buffers; REGISTER
é tratado separadamente porque conserva a identidade registrada. Chamadas
preparadas recebem os canais explícitos já admitidos pela inspeção estrutural;
seus argumentos de programa que possam alcançar um buffer o invalidam.
O contexto sozinho não expõe um objeto automático distinto do frame chamador.

Uma chamada desconhecida é conservadora quanto a alterações possíveis.
Se o endereço já escapou, uma chamada posterior pode invalidar o buffer mesmo
sem recebê-lo novamente. Uma possível alteração invalida; não é necessário
provar que ocorreu. Isso não amplia o conjunto de chamadas admitidas em
produção nem substitui a verificação geral de escape e vida útil da 6.4.4.

AFTER_FAILURE e DISCARD encerram a disponibilidade dos resultados da tentativa
anterior. Esse limite local não prova propriedade/consumo do slot ou legalidade
do retry: essas verificações continuam na 6.4.6. FAIL e FATAL não constituem
continuação normal autorizada por um status anterior.

Esta é uma análise conservadora do domínio emitido, não um solucionador geral
de equivalências entre expressões. Associações perdidas em junções ou formas
não reconhecidas podem recusar código correto. Nenhuma falta de precisão
autoriza fallback ou transforma uma saída indisponível em disponível.

## Janela e preservação

Grafo, estados, associações e informações de alias são temporários e mantêm
somente referências emprestadas ao IR vivo. Os operandos continuam enraizados
pelo inventário da janela; não são criadas árvores operacionais persistentes.
A análise não altera GIMPLE, CFG, SSA ou propriedades de `cfun`.

Descritores são consultados apenas dentro da janela para confrontar o produtor
PRIVATE com seu acesso. Tudo é destruído antes de `newtm_obligation_end`,
tree-nested e omplower. Nenhum consumidor posterior recupera esses descritores
ou usa um histórico escalar como aprovação. ABI, runtime, layouts, opções e
limites do subconjunto permanecem preservados; alternativa B continua futura.

## Validação

O alvo `make -C transaction_test check-status` contém cenários sintéticos
isolados, testes negativos através do inspetor completo e execução do pragma
real. A matriz inclui C/C++, O0/O2, LTO, DSO/archive, checking 0/2 e GC forçado.
Os probes verificam também imutabilidade do IR/CFG/propriedades e não inserem
os corpos sintéticos em funções executáveis.

Cobertura: guarda ausente/invertida/tardia, status antigo, variável reutilizada,
cópia válida ou sobrescrita, conversão estreita, disponibilidade independente
de valor/binding, junção incompleta, laço, cleanup, sobrescrita direta/indireta,
chamada que pode alterar ou reter o endereço, produtor PRIVATE alternativo,
propagação do próprio status e retorno de sucesso indevido.

`check-status`: **77 verificações, 16 execuções e 1680 cenários sintéticos**,
com 60 cenários por compilação isolada/integrada. Oito cenários por compilação
chamam o inspetor de produção completo e conferem recusa V05, sem depender
de uma declaração de tratamento do emissor. Os casos de endereço retido e
possível publicação por END também possuem pares positivo/negativo.

Logs e JSON: `transaction_test/.build/status/`. A integração e as regressões
anteriores também passaram com a versão final do compilador:

| Matriz | Verificações | Execuções explicitamente contadas |
| --- | ---: | ---: |
| Controlador + integração | 1303 | 1036 |
| Fluxo | 73 | 16 |
| Inspeção | 73 | 16 |
| Inventário de obrigações | 73 | 16 |
| Guardas | 102 | 24 |
| Clones | 209 | 48 |
| Preparação | 37 | 8 |
| Leituras | 52 | 12 |
| Escritas | 56 | 12 |
| Chamadas | 75 | 18 |
| Ocorrências | 63 | — |
| Descritores | 36 | — |
| Subconjunto | 201 | — |
| ABI v1, símbolos/versionamento e falhas | 88 | — |

Os canais sintéticos são separados; layouts reais de binding e retornos
ponteiro são exercitados também por clones, chamadas e integração. Não
anunciar cobertura nativa de 32 bits, outros backends ou a suíte completa do
GCC: a validação local usa Linux x86_64 e as matrizes descritas.
