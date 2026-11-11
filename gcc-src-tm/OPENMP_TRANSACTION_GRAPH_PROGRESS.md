# Etapa 6.5 — certificação transitiva e invalidação

**Estado vigente em 2026-11-11: etapa 6.6 aprovada e concluída no domínio
experimental documentado.** O [parecer final](OPENMP_TRANSACTION_ACCEPTANCE_FINAL.md)
prevalece sobre notas históricas de pendência; suporte e requisitos não foram
ampliados. Os relatos seguintes mantêm seu valor nos próprios escopos.

## Estado vigente — bloco 8 concluído; 6.5.3 concluída no domínio documentado

A [reconstrução em LTO](OPENMP_TRANSACTION_LTO_GRAPH.md) transporta contratos
versionados, reconcilia declarações entre streams/unidades e reexecuta as provas
sobre o SSA carregado em WPA, LTRANS e LTO monolítico/incremental. As raízes
vêm das chamadas atuais. Nenhuma aprovação é serializada; auxiliares inativos
não recebem certificados fictícios. A autoridade de cada fase é revogada antes
de novas transformações.

O domínio inicial preserva a unidade de prova até a reconstrução final;
as opções originais são restauradas depois. Há custo de otimizações prévias,
materialização e colocação conjunta em LTRANS, explicitado no contrato.
O formato desta árvore é LTO 16.1 e recusa objetos 16.0 para recompilação.
ABI/runtime/layouts e incoming/result permanecem preservados.

Passou a matriz integral final de LTO (426 verificações/122 execuções),
incluindo modo de origem, COMDAT e remoção da própria invocação da região.
Passaram as regressões nativas, integração, ABI e chamadas. O controle manual
da ABI permanece fora da aprovação de grafo, com compatibilidade validada.
Resultados e logs estão no checkpoint. Os blocos 1–8 fecham a 6.5.3 no domínio
documentado; permanece a aceitação integral da 6.6. Este estado prevalece
sobre os relatos históricos abaixo.

## Estado anterior — bloco 7 da 6.5.3 concluído

Os blocos 1–7 estão implementados no domínio do frontend. A
[composição atual do grafo](OPENMP_TRANSACTION_CURRENT_GRAPH.md) reexecuta o
fechamento de corpos, controlador e produtores de outlining, resolve as cinco
classes de dependências e publica atomicamente uma geração por raiz.
`newtm_current_get` concede current_approved apenas ao certificado atual válido;
os componentes continuam condicionais e o getter histórico permanece falso.

Mutações ou rechecagem isolada revogam raízes dependentes; restaurar o IR não
reanima uma geração. Revalidação completa cria outra geração. A autoridade é
retirada antes dos passes IPA e não atravessa streaming/LTO. Restam o bloco 8
(reconstrução LTO) e a etapa 6.6 (aceitação integral). `check-grafo-atual`: 543 verificações e 212 execuções. Passaram cobertura,
propriedade, status e memória atuais, canais, grafo histórico, controlador,
integração (1078/930), ABI (88) e chamadas (75/18). Build sem avisos.
Este estado prevalece sobre os relatos históricos abaixo. Resultados e logs
no checkpoint de retomada.

## Estado anterior — bloco 6 da 6.5.3 concluído

Cada efeito atual recebe uma categoria demonstrada: local com intervalo,
PRIVATE com operação reconstruída, canal com direção/extensão, chamada com
assinatura e identidade de origem ou manutenção nativa de frame com proveniência
registrada no construtor do GCC. Os mapas de corpo e controlador são separados;
nenhuma instrução, SSA_NAME ou descritor é retido na captura de permissões.

O percurso alcança condições, endereços, construtores, retornos e argumentos.
A entrada começa na abertura de vida útil do slot, incluindo o prefixo antes
da inicialização. Debug/NOP/labels/PREDICT são anotações; efeitos desconhecidos,
volatile, asm, chamadas e acessos não justificados são recusados. Código comum
ao redor da transação permanece fora do subgrafo proprietário.

`check-cobertura`: 695 verificações e 156 execuções; base aceita antes das
mutações, C/C++ O0/O2, checking 0/2, GC, DSO/archive, GNU, EH externo e parallel.
O resultado é DEPENDENT com IDENTITY | CHANNELS | STATUS | OWNER (15).
As dependências anteriores são preservadas até a composição no bloco 7;
`current_approved` continua falso. Restam blocos 7–8 e aceitação integral na 6.6.
Este estado prevalece sobre os relatos históricos abaixo.

## Estado anterior — bloco 5 da 6.5.3 concluído

O protocolo proprietário é reconstruído por controlador no CFG atual:
slot/frame, admissão e outermost, auxiliar, STATUS/END, causa, condenação,
limpeza, contador saturado e retry integral somente após conflito efetivo.
Entrada falha considera slot vazio e slot retido; status desconhecidos não
podem contornar limpeza. Conflito limpo precisa repetir o bloco completo.

As fronteiras explícitas de vida útil do slot são operações nativas sem chamada
nova de runtime. A abertura permite reentrada em laços; o encerramento exige
commit bem-sucedido. Não há hipótese de sobrevivência de rótulos ou descritores.
O subgrafo conserva os caminhos e operações atuais, recusa entradas no interior,
retornos, sumidouros e uso externo dos transportes; separa múltiplos controladores
e o código comum ao redor, inclusive EH de C++ e extração por parallel.

O resultado de propriedade é DEPENDENT com CHANNELS | STATUS (3). Os demais
componentes conservam suas dependências até a composição do bloco 7.
`check-proprietario-atual`: 553 verificações e 88 execuções, com base aceita
antes de cada mutação, C/C++ O0/O2, checking 0/2, GC, DSO/archive e parallel.
`current_approved` permanece falso. Cobertura residual, composição/aceitação
atual e LTO continuam nos blocos 6–8; aceitação integral, na 6.6.
Este estado prevalece sobre os relatos históricos abaixo.

## Estado anterior — bloco 4 da 6.5.3 concluído

Status por definição de chamada, disponibilidade independente de valor/binding,
alternativas PRIVATE e publicação sob o STATUS final próprio são reconstruídos
no CFG atual. Retornos reais, seletores de cleanup, causas e resultado efetivo
de FAIL são verificados. Constantes globais não sobrevivem a possíveis
escritores, e uma falha não pode autorizar publicação global no controlador.

Os estados de status e publicação são separados nas junções compartilhadas
do lowering. Buffers desconhecidos não recebem aprovação por tipo ABI; a
forma fora do domínio de disponibilidade é recusada. Nenhum descritor ou
operando operacional anterior ao remapeamento é consultado.

`check-status-atual`: 449 verificações e 100 execuções, C/C++ O0/O2,
checking 0/2, GC, DSO/archive, aninhamento, controlador e parallel.
Passaram grafo (317), canais (241/44), memória (281/32), controlador (225/106),
integração (1078/930), vida útil (126/32/1708) e as demais provas, ABI e chamadas.
Build all-gcc sem avisos. Limites e resultados no contrato do bloco 4.

O resultado de status é DEPENDENT com OWNER. Propriedade, admissão, limpeza,
confirmação/descarte e retry do controlador remapeado continuam no bloco 5;
cobertura residual, composição/aceitação atual e LTO, nos blocos 6–8.
A 6.5.3 continua em andamento: blocos 1–4 concluídos, 5–8 pendentes.
`current_approved` permanece falso. Este estado prevalece sobre os relatos abaixo.

## Estado anterior — bloco 3 da 6.5.3 concluído

Incoming/result e buffers são confrontados no CFG atual: identidade dos slots,
produtores de valor e binding, extensão efetiva, inicialização, vida útil,
empréstimos, direção e não sobreposição. A entrada do controlador é inspecionada
também no corpo extraído por outlining, com confronto dos produtores do frame.
Os resumos não conservam descritores ou instruções para consulta tardia.

O número de bindings vem do protótipo preservado, mesmo após remover o corpo
original. BLOCKs do GIMPLE atual permitem reconstruir reentrada lexical e
clobbers em laços. Código C++ comum ao redor do controlador conserva suas
arestas de unwind; exceções nos corpos preparados continuam fora do domínio.

`check-canais`: 241 verificações e 44 execuções, C/C++ O0/O2, checking 0/2,
GC forçado, DSO/archive e parallel. Memória (281/32), grafo (317), controlador
(225/106), integração (1078/930) e vida útil (126/32/1708) passaram.
Passaram também fluxo, guardas, composição, status, falhas, propriedade, ABI
e chamadas. Build all-gcc sem avisos; limites no contrato do bloco 3.

O resultado dos canais é DEPENDENT, com STATUS | OWNER. Sucesso da instância
correta e disponibilidade das saídas seguem no bloco 4; propriedade, limpeza
e retry, no 5; cobertura residual, composição/aceitação atual e LTO, nos 6–8.
A 6.5.3 continua em andamento: blocos 1–3 concluídos, 4–8 pendentes.
`current_approved` permanece falso. Este estado prevalece sobre os relatos abaixo.

## Estado anterior — bloco 2 da 6.5.3 concluído

As quatro entregas estão implementadas: modelo de instâncias/layouts,
reconstrução de tuplas e versões com decisões de controle, acessos estáticos,
transportados e privados, aritmética e recusas de produção. Os resumos são
consumidos antes do fechamento da preparação; não há consulta tardia a
descritores ou retenção de SSA_NAME.

`check-memoria`: 281 verificações e 32 execuções, C/C++ O0/O2, checking 0/2,
GC forçado e DSO/archive. `check-transitivo`: 317 verificações. Controlador
(225/106) e integração (1078/930) passaram; all-gcc sem avisos.

O resultado de memória é condicional às provas de canais, status e propriedade
dos blocos 3–5. Cobertura residual, aceitação atual e LTO permanecem nos blocos
6–8. A 6.5.3 continua em andamento: blocos 1–2 concluídos, 3–8 pendentes;
current_approved permanece falso. Este estado prevalece sobre os relatos abaixo.

A primeira subetapa foi concluída para o grafo histórico pré-lowering. O
[contrato e os testes](OPENMP_TRANSACTION_GRAPH.md) descrevem revisões,
dependências, componentes fortemente conexos, publicação atômica e limites.

A segunda subetapa implementa propagação, revogação e revalidação conservadora
do grafo histórico; os limites e testes estão no [contrato de invalidação](OPENMP_TRANSACTION_INVALIDATION.md).
A interface de revalidação é exercitada pelo plugin; o pipeline ainda não a
aciona automaticamente após transformações. A integração com transformações
remapeadas continua na terceira subetapa.

A terceira subetapa está em andamento. As entradas reais de `tree-nested` e
`omplower` registram que as provas locais e de seus chamadores atravessaram uma
fronteira, sem invalidar o significado histórico do certificado. O registro é
propagado inclusive para resumos consolidados depois do evento. Não há consulta
tardia aos descritores. `check-transitivo` inclui passagem real por ambas as
fronteiras e propagação em ciclos, com checking 0/2.

A interface do grafo agora expõe separadamente a validade histórica e a
aprovação atual: esta permanece falsa porque ainda não existe verificador do
IR remapeado. A matriz LTO executa o frontend e o processo LTO reais em C/C++,
checking 0/2; o frontend produz registros históricos, enquanto o processo LTO
os inicia e termina ausentes. Isso comprova isolamento dos registros nos casos
testados, não a correção dos corpos carregados ou sua certificação.

Essas marcas são evidência de invalidação, não uma nova verificação nem uma
barreira de aceitação integrada. Permanecem necessários a inspeção independente
do IR remapeado, um mecanismo de aceitação que a consuma e a certificação do IR
carregado ou transformado em LTO. A terceira subetapa não está concluída.

A versão atual passou em `check-transitivo` (317 verificações, C/C++, checking
0/2), composição (93 verificações/16 execuções/816 cenários), proprietário
(77/16/2356), falhas (77/16/1856), integração com controlador (1303/1036) e
chamadas (75/18). O build all-gcc passou; self-tests internos estão desabilitados.
Resultados limitados ao ambiente nativo Linux x86_64. ABI/runtime/layouts foram
preservados.

## Trabalho ainda necessário para aprovação atual

1. Construir uma inspeção sobre o CFG e os operandos da função transformada,
   incluindo os corpos separados por outlining e seus frames, sem recorrer às
   associações ou aos descritores descartados. Ela precisa reconstruir a
   classificação de armazenamento e os canais incoming/result, verificar
   acessos residuais e reproduzir as garantias V01–V08 pertinentes.
2. Associar cada controlador e cada chamada preparada aos corpos efetivamente
   executados após remapeamento, verificando assinaturas, layout, arestas e
   ciclos. Uma fotografia rasa de tipos ou uma aprovação histórica não supre
   essa associação. Publicar uma revisão atual somente depois da análise
   completa e invalidá-la a cada transformação relevante.
3. Integrar essa revisão à barreira que decide aceitar o caminho funcional,
   com recusa conservadora e diagnóstico nos casos não demonstrados. A consulta
   `current_approved`, atualmente sempre falsa, é uma distinção de estado e não
   constitui essa barreira.
4. Definir como o LTO reconhece as raízes e dependências de transações nos
   corpos carregados, reconstruir a prova na fase adequada e testar WPA/LTRANS,
   incluindo alterações e chamadas entre unidades. O teste de link relocável
   realizado até agora demonstra ausência de registros históricos, não essa
   reconstrução.

## Auditoria executável do CFG — chamadas ABI

`newtm_abi_verify_cfg` inspeciona as chamadas builtin STM do CFG real após o
lowering em `cgraphunit.cc` e após os fixups de leitura em
`lto-streamer-in.cc`. Confere quantidade de argumentos, papéis ponteiro/size_t/
uint32_t, protótipo da declaração e da chamada e tipo do destino de retorno.
O contrato esperado vem da ABI versionada; não consulta descritores ou toma
o certificado histórico como evidência. Divergência produz erro e impede a
compilação. Chamadas não reconhecidas como builtins STM não são certificadas
por essa inspeção.

O teste negativo modifica o tipo do deslocamento de READ depois de `ompexp`.
C/C++, checking 0/2 devem receber o diagnóstico da auditoria de produção. Isso
não demonstra identidade/faixa, vida útil dos buffers, canais de clones,
dominação de status ou ausência de acessos residuais. A aprovação integral
atual permanece falsa; esta é uma parte da inspeção pendente, não sua conclusão.

## Chamadas preparadas no CFG transformado

Antes de publicar o grafo histórico, `check_local_graph` agora inspeciona os
blocos reais de cada função preparada alcançável. Exige CFG disponível,
recusa chamadas indiretas e destinos externos não preparados, e exige que
cada destino preparado observado pertença às dependências verificadas.
Confere também protótipo, quantidade/tipos de argumentos e destino de retorno.
A remoção de chamadas pelo lowering é admitida; o resumo continua fornecendo
uma aproximação conservadora das dependências. Builtins e chamadas internas
são tratados pelas verificações específicas, não por esta checagem de arestas.

As mutações de teste ocorrem depois de `ompexp`, em C/C++, checking 0/2:
READ trocado por função externa, READ tornado indireto e argumento de contexto
de uma chamada preparada trocado por inteiro. A certificação recusa esses
corpos; nenhuma consulta a descritores ocorre. Esta checagem usa declarações
e resumos do processo frontend e não foi estendida ao grafo carregado em LTO
nem ao corpo inteiro das funções que contêm os controladores. Aprovação
integral atual continua pendente, inclusive identidade, armazenamento, fluxo
de status, vida útil e cobertura dos acessos remapeados.

## Identidade do contexto após lowering

A inspeção dos corpos preparados agora executa uma análise de ponto fixo sobre
os blocos e arestas do CFG atual, antes de SSA. A origem é o primeiro parâmetro
da assinatura atual. Cópias e conversões sem efeito preservam a identidade;
escritas a retiram. Junções usam interseção dos fatos de todos os predecessores
alcançáveis. Loops convergem sem desenrolamento. Endereços expostos são
reconstruídos dos operandos reais; locais addressable também são excluídos.
Uma variável exposta não é fonte de prova contra escritas por alias.

Cada chamada preparada e cada serviço de contexto emprestado reconhecido
precisa receber essa mesma identidade. Tipo compatível, isoladamente, não
satisfaz a regra. Divergência produz erro durante a certificação. Os bitmaps
usam uma arena temporária própria, destruída ao terminar a inspeção; não há
alteração do IR ou consulta tardia a descritores.

Os probes modificam chamadas depois de `ompexp`: contexto nulo com tipo
correto, parâmetro sobrescrito, cópia válida e sobrescrita por alias. Esta
análise é restrita ao CFG pré-SSA das funções preparadas alcançadas no
frontend. Não fornece a reconstrução em LTO, nem a prova completa dos canais
de saída, dos controladores ou da memória remapeada. A aprovação integral
atual permanece pendente.

## Bloco 1 — correspondência estrutural após outlining

O evento real de extração em `expand_omp_taskreg` registra a origem da função
resultante por declarações. Antes da certificação, o compilador procura a
invocação do auxiliar em todos os CFGs definidos, exige uma única ocorrência
e confere a origem do corpo contendo essa chamada. A origem precisa ser o
proprietário do controlador ou uma função extraída cuja origem esteja
registrada; conflito ou ciclo de proveniência impede essa correspondência.
O tipo real da chamada precisa corresponder à assinatura do auxiliar.

Cada função preparada também confronta seus parâmetros atuais, na ordem,
com a fotografia de tipos do protótipo preparado e exige a propriedade das
declarações pelo corpo atual. Referências a parâmetros de outro corpo ou a
campos ausentes do record do operando são recusadas. Esses dados são resumos
de declarações: não conservam instruções ou descritores para consulta tardia.
As checagens participam da decisão de publicar o grafo histórico.

`check-transitivo`: 117 verificações. As mutações após `ompexp` cobrem remoção,
duplicação e canais do controlador, origem de outlining conflitante,
proprietário de parâmetro alterado e identidade de campo desconhecida, em
C/C++, checking 0/2. Passaram também composição, controlador, integração e
chamadas; `parallel` é exercitado na matriz funcional. Build sem avisos.

A pendência semântica desse bloco foi implementada posteriormente; consultar o
[contrato de correspondência](OPENMP_TRANSACTION_REMAP_CORRESPONDENCE.md).
O bloco 1 conclui a correspondência no domínio demonstrável do frontend, com
recusa conservadora de formas desconhecidas. Os blocos 2–8 permanecem pendentes;
a aprovação integral atual e a reconstrução LTO não são concedidas por ele.
