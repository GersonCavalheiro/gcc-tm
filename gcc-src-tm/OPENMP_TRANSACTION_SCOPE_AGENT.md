# Orientação vigente — Escopo, tipos e identidade no compilador

## Subetapa 6.5.1 — grafo transitivo histórico — 2026-11-04

Concluída a [certificação transitiva pré-lowering](OPENMP_TRANSACTION_GRAPH.md):
revisões locais, controlador, arestas do preflight, fechamento, componentes
fortemente conexos e publicação atômica. Resultados históricos não autorizam
IR remapeado ou reutilização em LTO. Invalidação de membros/arestas revoga o
resultado; reconstrução/revalidação e remapeamento seguem nas 6.5.2–6.5.3.

`check-transitivo`: 33 verificações, recursão simples/mútua, grafos incompletos,
arestas divergentes, mutação e invalidação; C/C++, checking 0/2. Regressões de
integração, chamadas e provas locais passaram. ABI/runtime/layouts preservados.
Esta atualização prevalece sobre pendências históricas da primeira subetapa.

## Subetapa 6.4.7 — composição local — 2026-11-04

Implementada a [composição das provas](OPENMP_TRANSACTION_COMPOSITION.md):
requisitos e evidências por obrigação, escopo e geração; aprovação local
atômica; dependências transitivas explícitas; invalidação integral do escopo e
diagnóstico estruturado da primeira recusa. A reconciliação inclui obrigações
sem ocorrência e retiradas, produtoras e derivadas. Não há certificação de grafo
nem consulta tardia a descritores. ABI/runtime/layouts preservados.

`check-composicao`: 93 verificações, 16 execuções e 816 cenários sintéticos;
C/C++ O0/O2, LTO, DSO/archive, checking 0/2, GC e diagnósticos negativos.
Regressões anteriores e integração passaram. Próxima etapa: 6.5, certificação
transitiva e remapeamento; aceitação integral por mutações segue na 6.6.
Esta atualização prevalece sobre pendências históricas da 6.4.7 abaixo.

## Subetapa 6.4.6 — protocolo proprietário — 2026-11-04

Implementada a [prova do controlador proprietário](OPENMP_TRANSACTION_OWNERSHIP.md)
em `gcc/newtm-owner.{cc,h}`, integrada à inspeção antes de fechar a janela.
Verifica slot exclusivo e estável, admissão/outermost, auxiliar, STATUS e END,
causa preservada, condenação/limpeza únicas, contador positivo com saturação,
fatal e retry integral. Cada admissão invalida os temporários e campos da
preparação anterior. Recusa contexto consumido, escapes/escritas indiretas,
vida útil/clobbers incompatíveis e ciclos sem readmissão autorizada.

`check-proprietario`: 77 verificações, 16 execuções e 2356 cenários sintéticos;
C/C++ O0/O2, LTO, DSO/archive, checking 0/2, GC e negativos pelo inspetor completo.
A prova usa as pós-condições da ABI, incluindo término possível em
AFTER_FAILURE. ABI/runtime/emissão funcional/layouts e canais incoming/result
permanecem preservados. Não há consulta tardia a descritores, COBERTA ou
certificado novo. Alternativa B continua futura. Próxima: 6.4.7, composição
das provas com inventário e diagnósticos; certificação/remapeamento na 6.5
e mutações integrais na 6.6. Resultados e limites constam no relatório.
Esta atualização prevalece sobre as pendências históricas da 6.4.6 abaixo.

## Subetapa 6.4.5 — falhas e aninhamento — 2026-11-04

Implementada a [prova dos epílogos e do contexto emprestado](OPENMP_TRANSACTION_FAILURES.md)
em `gcc/newtm-failure.{cc,h}`, integrada à inspeção antes de fechar a janela.
Conserva a causa de cada operação, exige o resultado efetivo de FAIL, recusa
continuação normal/ciclos condenados e verifica profundidade relativa em
sucesso, junções e laços. Falhas abandonam os níveis para limpeza externa;
NEST_END após falha e serviços proprietários em clones/auxiliares são recusados.
Guardas emitidas têm obrigação persistente da aresta de aborto, sem aprovação
por marcação. Retirar evidência não apaga essa obrigação.

`check-falhas`: 77 verificações, 16 execuções e 1856 cenários sintéticos;
C/C++ O0/O2, LTO, DSO/archive, GC forçado, checking 0/2 e negativos pelo inspetor
completo. Nenhuma consulta tardia a descritores, COBERTA ou certificado novo.
ABI/runtime/layouts e alternativa B futura preservados. Próxima: 6.4.6,
protocolo proprietário do controlador; composição na 6.4.7, certificação e
remapeamento na 6.5, mutações integrais na 6.6. Esta atualização prevalece
sobre as pendências históricas da 6.4.5 abaixo.

## Subetapa 6.4.4 — inicialização, vida útil e publicação — 2026-11-04

Implementada a [prova dos transportes e saídas](OPENMP_TRANSACTION_LIFETIME.md)
em `gcc/newtm-lifetime.{cc,h}`, integrada à inspeção antes de fechar a janela.
Confere campos e operandos definidos, construtores, arrays completos,
empréstimos sem sobreposição, escopos/clobbers, origens possíveis e escapes.
A publicação exige o status final da própria instância e saídas completas
sob sucesso, preservadas em falha. Checking 0 conserva as recusas V04/V05.

A emissão verifica a igualdade entre ponteiro e binding antes de qualquer
saída. Declarações de temporários de descritores pertencem à ativação do
clone; suas avaliações e os escopos dos objetos do programa são preservados.
Não há consulta tardia a descritores, COBERTA ou certificado novo.
ABI/runtime/layouts e alternativa B futura permanecem preservados.

`check-vida-util`: 126 verificações, 32 execuções e 1708 cenários sintéticos;
C/C++ O0/O2, LTO, DSO/archive, GC forçado, checking 0/2 e negativos pelo inspetor
completo. A fronteira executável recusa valor/binding inconsistentes sem
alterar as duas saídas. Integração e regressões anteriores também passaram.
Próxima subetapa: 6.4.5, falhas e aninhamento. Controlador/composição seguem nas
6.4.6–6.4.7; certificação/remapeamento e mutações integrais, nas 6.5–6.6.
Esta atualização prevalece sobre as pendências históricas da 6.4.4 abaixo.

## Subetapa 6.4.3 — status e disponibilidade implementados — 2026-11-03

Implementada a [prova independente de status e saídas](OPENMP_TRANSACTION_STATUS.md)
em `gcc/newtm-status.{cc,h}`, integrada à inspeção de produção, inclusive com
checking 0. Cada definição de chamada possui status próprio; reutilizar uma
variável não reutiliza a aprovação. Valor e binding são acompanhados
separadamente, com recusa V05 para consumo sem sucesso ou após invalidação.

Junções, ciclos, cópias de status, sobrescritas e aliases participam da prova.
O produtor PRIVATE alternativo exige acesso e cadeia de cópias comprovados
pela inspeção estrutural. Endereços expostos conservam efeitos possíveis de
chamadas e END. O próprio status pode ser propagado sem liberar as saídas.

A análise não modifica GIMPLE/CFG/SSA. Grafo, estados e vínculos terminam antes
do fechamento da janela, tree-nested e omplower; nenhuma consulta tardia a
descritores. ABI/runtime/layouts e alternativa B futura permanecem preservados.
Não há COBERTA nem certificado novo. Próxima subetapa: 6.4.4, inicialização,
vida útil e publicação; falhas/aninhamento, controlador e composição seguem
nas 6.4.5–6.4.7, certificação/remapeamento na 6.5 e mutações integrais na 6.6.
Testes e critérios constam no relatório da 6.4.3. Esta atualização prevalece
sobre a pendência histórica de status nos registros abaixo.

## Subetapa 6.4.2 — guardas e aritmética verificadas — 2026-11-01

Implementadas as [provas independentes de guardas](OPENMP_TRANSACTION_GUARDS.md)
em `gcc/newtm-guard.{cc,h}`, integradas à inspeção do High GIMPLE real.
Validade/faixa/overflow e índices são demonstrados antes dos usos dependentes,
em todos os caminhos; redefinições/aliases invalidam fatos. Marcas do emissor
identificam obrigações, sem aprová-las. Guardas ausentes/tardias/incorretas
geram recusa V03, inclusive com checking 0.

A emissão protege a formação de ponteiros e seu endereço um após o fim.
O prefixo de descritores do controlador integra cada tentativa inspecionada;
a representação de referência C++ tem snapshot tipado ligado à captura real.
Grafo e provas terminam antes de fechar a janela e antes de tree-nested/omplower.
Nenhuma consulta tardia a descritores, COBERTA ou certificado novo.

`check-guardas`: 102 verificações, 24 execuções e 1036 cenários sintéticos,
com C/C++ O0/O2, LTO, DSO/archive, GC, checking 0/2 e sanitizador de ponteiros.
Integração completa: 1303 verificações/1036 execuções aprovadas.
ABI/runtime/layouts e alternativa B futura preservados.
Próxima subetapa: 6.4.3, status e disponibilidade de resultados; demais provas
seguem nas 6.4.4–6.4.7 e certificação/remapeamento na 6.5.
Esta atualização prevalece sobre a pendência histórica das guardas abaixo.

## Subetapa 6.4.1 — representação do fluxo implementada — 2026-10-31

Implementado o [grafo temporário de High GIMPLE](OPENMP_TRANSACTION_FLOW.md)
em `gcc/newtm-flow.{cc,h}`, integrado à inspeção antes do fechamento da janela.
Representa condições, switch, goto direto, ciclos, retornos, noreturn e
continuações distintas de TRY_FINALLY, sem modificar GIMPLE/CFG/SSA.
Estruturas/alvos desconhecidos são recusados, inclusive em código morto;
ciclos reutilizam estados canônicos, sem desenrolamento de iterações.
GIMPLE_PREDICT é anotação sem efeito; cases copiados têm comparação estrutural.

`check-fluxo`: 73 verificações, 16 execuções e 816 cenários sintéticos,
com imutabilidade, continuações, recusas, GC forçado e checking 0/2.
Inspeção/inventário, integração (1303 verificações/1036 execuções) e regressões
das etapas anteriores passaram. ABI, runtime e layouts permanecem preservados.
Provas de guardas/status/publicação/protocolo seguem nas 6.4.2–6.4.7;
certificação e remapeamento, na 6.5. Não há COBERTA nem certificado novo.
Esta atualização prevalece sobre a pendência histórica da representação do fluxo.

## Etapa 6.3 — inspeção independente do GIMPLE implementada — 2026-10-31

A [inspeção estrutural local](OPENMP_TRANSACTION_INSPECTION.md) percorre o corpo
final de clones/auxiliares e controladores, incluindo entrada e epílogo, antes
de fechar a janela e antes de tree-nested/omplower. Classifica também operações
sem ocorrência; confronta substituições, operandos atuais, buffers tipados,
transporte por assinatura e braços PRIVATE. Rótulos do emissor e nomes
artificiais não autorizam acessos. Falhas estruturais geram recusa/diagnóstico,
inclusive com checking 0; snapshots operacionais são descartados no fechamento.

Não emite COBERTA nem certificados completos. Fluxo/status/publicação (6.4),
grafo e remapeamento (6.5) e aceitação integral por mutações (6.6) seguem pendentes.
ABI, layouts, opções, runtime e limites permanecem preservados. Alternativa B
continua futura. `check-inspecao`: 73 verificações, 16 execuções e 552 checagens
locais/negativas; detalhes e limites no contrato da 6.3. Esta atualização
prevalece sobre a pendência histórica da inspeção nos registros abaixo.

## Etapa 6.2 — inventário de obrigações implementado — 2026-10-31

Implementado o [inventário e acompanhamento](OPENMP_TRANSACTION_OBLIGATIONS.md)
em `gcc/newtm-obligation.{cc,h}`: O01–O07, instâncias/escopos/gerações,
fatos históricos sem operandos expirados, tratamentos declarados, reconciliação,
stores derivados, recusa persistente e cópias com evidência invalidada.
As âncoras GIMPLE são retiradas ao fechar a janela; forget/retire não apagam
obrigações. Invalidação alcança categorias da mesma instrução, derivações e
operações dependentes. A marcação GC percorre operandos das chaves GIMPLE.

Nenhum registro vira COBERTA nem há certificado novo de aprovação nesta etapa.
O preflight e a certificação funcional existentes não foram substituídos pelo
inventário. A inspeção independente e o verificador completo seguem nas
6.3–6.6. ABI, layouts, opções e limites de suporte permanecem preservados.
`check-obrigacoes` valida os fatos e estados; não é prova de cobertura integral.
Esta atualização prevalece sobre a pendência histórica da 6.2 abaixo.

## Etapa 6.1 — contrato do verificador definido — 2026-10-31

Concluído o [contrato de obrigações e evidências](OPENMP_TRANSACTION_VERIFICATION.md):
categorias O01–O07, identidade por origem/instância/geração, estados separados
para obrigações, evidências e certificados, derivações e invalidações explícitas.
Regras V01–V08 exigem inspeção do IR real, inclusive operações sem ocorrência;
retirar metadados não resolve obrigação e copiar IR não transfere aprovação.
Consumir descritores na janela válida; resumos posteriores não são planos de
acesso nem autorização para consulta tardia a operandos anteriores ao remapeamento.

Esta entrega é documental: não implementa inventário, passes, diagnósticos ou
nova barreira de aceitação. Etapas 6.2–6.6 permanecem pendentes. ABI, opções,
comportamento funcional e recusas da tarefa 5 são preservados; alternativa B
continua futura. Contagens anteriores de testes não são validação de um
verificador executável da tarefa 6.

## Etapa 5.6 — validação integrada concluída — 2026-10-31

Concluída a [validação integrada](OPENMP_TRANSACTION_INTEGRATION_VALIDATION.md).
`make -C transaction_test check-integracao` inclui a matriz da 5.5: 1303
verificações/1036 execuções aprovadas. Os cenários novos cobrem falhas em todas
as posições de operação, dois retries consecutivos, visões privadas PROT_NONE,
agregados/aliases C, laços/ramificações, chamadas/capturas em parallel e limites
de 64 bits. C/C++ O0/O2, LTO, DSO/archive e GC forçado, com pragma real e sem
plugin de invocação. Nenhuma correção no compilador/runtime foi necessária.

A tarefa 5 está concluída para o caminho funcional opcional e os cenários
validados. O padrão conserva rastreamento; limites e recusas permanecem,
inclusive ponteiro global sem canal de identidade executável. Alternativa B
continua futura. A tarefa 6, verificador integral de operações residuais,
permanece pendente; a matriz de testes não o substitui. Esta atualização
prevalece sobre pendências históricas da 5.6 abaixo.

## Etapa 5.5 — controlador externo funcional opcional — 2026-10-31

Implementado o [controlador gerado](OPENMP_TRANSACTION_CONTROLLER.md), ativado por
`-fopenmp -fopenmp-transaction-stm`. O padrão conserva rastreamento. No modo
funcional, bindings são consumidos antes dos remapeamentos e corpos incompletos
são erros, sem fallback. Slot proprietário estável; sucesso confirma, conflito
limpa e repete todo o bloco, erro permanente limpa e termina. Certificação
transitiva usa resumos persistentes após a preparação de todos os corpos.

Referências C++ usam representação privada separada do referido. Definições de
corpos originais removidos não fornecem proveniência a tentativas posteriores.
Novo serviço noreturn fatal sem contexto em GOMP_NEWTM_2.1; layouts e versões
anteriores preservados. Frames GNU incompatíveis continuam recusados; alternativa
B permanece futura. `check-controlador`: 225 verificações/106 execuções, com
pragma real, C/C++ O0/O2, LTO, DSO/archive, parallel, retry e limpeza fatal.
Todas as regressões da integração passaram. Validação ampliada (5.6) e
verificador integral de cobertura (6) permanecem pendentes. Esta atualização
prevalece sobre descrições históricas do pragma somente em rastreamento.

## Etapa 5.4 — chamadas e bindings — 2026-10-31

Implementada a [integração de chamadas preparadas](OPENMP_TRANSACTION_CALLS.md).
O preflight exige implementação e todos os canais explícitos/implícitos por
chamada; resultados ponteiro usados exigem canal result. Dependências entre
clones são registradas na janela dos descritores. Exigir um auxiliar agora
certifica todo seu grafo após a preparação dos corpos, por resumos persistentes,
sem consultar descritores retirados. Status precede consumo de saídas e falhas
propagam sem publicar resultados nem consumir o slot.

Preservados ABI/layouts e limites do subconjunto. Frames GNU incompatíveis
continuam recusados; transporte explícito do ambiente léxico (alternativa B)
fica como possibilidade futura. O pragma de produção permanece em rastreamento:
controlador/ativação (5.5), validação integrada (5.6) e cobertura (6) continuam
pendentes. Esta atualização prevalece sobre pendências históricas de chamadas.

## Etapa 5.3 — escritas da visão privada — 2026-10-31

Implementadas [escritas STM](OPENMP_TRANSACTION_WRITES.md) nos clones/auxiliares.
Decisões aprovadas: buffers privados tipados, segunda caminhada na mesma janela
e ABI existente. A segunda caminhada consome também stores derivados de leituras
e chamadas, antes de fechar a preparação e antes dos remapeamentos.

Validar endereço/faixa, materializar somente o RHS GIMPLE já avaliado, chamar
write e conferir status antes de continuar. PRIVATE mantém store direto;
DYNAMIC despacha pela tag validada. Falhas seguem ao epílogo sem consumir slot
ou publicar resultados. Não há fallback à memória comum nem nova operação RMW.
Leituras observam as escritas privadas; confirmação/descarte permanecem externos.
Restrições de identidade, vida útil, tipos e escape privado foram preservadas.

`check-escritas`: 56 verificações e 12 execuções C/C++ O0/O2, DSO/archive e LTO,
com página PROT_NONE, aliases, agregados, chamadas, falhas e conflito real de
promoção. Os testes verificam ausência de publicação antes de commit e perda de
todas as alterações após descarte, com repetição explícita do corpo inteiro.
Preparação, leituras e clones também passaram com seus novos critérios de visão
privada. Esta entrega não ativa o pragma de produção: integração restante de
bindings/chamadas (5.4), controlador gerado (5.5) e cobertura (6) seguem pendentes.
Esta atualização prevalece sobre pendências históricas de escritas abaixo.

## Etapa 5.2 — leituras da visão privada — 2026-10-31

Implementadas [leituras STM](OPENMP_TRANSACTION_READS.md) nos clones e auxiliares
preparados. Validar faixa e correspondência do endereço lógico, ler em buffer
privado pela ABI e verificar status antes de consumir resultados. PRIVATE provado
mantém acesso direto; DYNAMIC despacha pela tag validada. Falhas seguem ao epílogo
comum. Corpos incompletos são recusados antes da transformação, sem fallback.

A janela continua no fim da gimplificação, antes de tree-nested/omplower.
Resultados de leituras são definidos antes de descritores que dependam deles;
ponteiros capturados vêm da visão privada antes de acessar seus apontados.
A representação privada de parâmetros ponteiro/referência tem raiz distinta do
apontado. Registros não mudam a classificação de armazenamento; aliases privados
não geram registros preexistentes. As restrições de vida útil permanecem.

Testemunhos const só são reconciliados com leituras reais por prova estrutural
na mesma sequência. C++ deduplica visitas à mesma posição/referência e distingue
parênteses semânticos de conversões de representação. Testemunhos sem leitura
correspondente continuam recusados, sem reexecutar expressões de origem.

`check-leituras`: 52 verificações e 12 execuções C/C++ O0/O2, DSO/archive e LTO,
com página PROT_NONE, conflito real, avaliação única, macros, agregados e falhas
preservando saídas. Regressões: preparação 37/8 execuções; clones 209/48;
ocorrências 63, descritores 36, subconjunto 201 e ABI 88, todas aprovadas.

A ponte original continua em rastreamento. Escritas STM (5.3), integração restante
de bindings/chamadas (5.4), controlador externo (5.5) e cobertura (6) permanecem
pendentes. Não afirmar isolamento/rollback completo de corpos com escritas ainda
comuns. Esta atualização prevalece sobre pendências históricas abaixo.

## Etapa 5.1 — preparação dos acessos e corpos auxiliares — 2026-10-30

Implementada a [preparação da transformação](OPENMP_TRANSACTION_PREPARATION.md).
Escolha do usuário: corpo externo em auxiliar preparado; o futuro controlador
mantém slot/status, retry e limpeza. Auxiliares GENERIC usam endereços explícitos
de captura e bindings por ativação, preservando locais de parallel para outlining.
Não usam cadeia estática nativa. Clones comuns mantêm a ABI da etapa 4.

Classificação operacional no fim da gimplificação: REGISTERED, PRIVATE,
DYNAMIC ou REJECTED, com identidade/faixa/validade completas e operandos atuais.
A API recusa consulta fora dessa janela, antes de tree-nested/omplower; depois
só há resumos escalares. Unknown/source-only/stale/canais incompletos não viram
acessos comuns. Auxiliares incompletos são recusados por inteiro; exigir sua
ativação produz diagnóstico. Chamadas GNU que exigiriam outro frame são
recusadas até resolver seu transporte. Falhas usam epílogo comum por função.

A ponte original permanece em rastreamento. Read/write STM (5.2/5.3), integração
de chamadas/capturas (5.4), controlador externo (5.5) e cobertura (6) continuam
pendentes. Os testes invocam auxiliares somente por plugin exclusivo de teste.
Esta atualização prevalece sobre pendências históricas de preparação abaixo.

## Etapa 4 — clones GENERIC e bindings por ativação — 2026-10-30

Implementados [clones preparados](OPENMP_TRANSACTION_CLONES.md) antes da
gimplificação, com protótipos de todo o grafo selecionado, contexto emprestado,
status uint32_t e resultados por saída somente em sucesso. Funções originais e
a ponte de rastreamento permanecem disponíveis; o pragma ainda não chama clones.
Chamadas entre clones usam versões preparadas e propagam falhas antes de ler
resultados. Regiões internas usam nest_begin/register/nest_end sem consumir o
slot externo. Consulta GOMP_transaction_stm_v1_status acrescentada à versão 2.0.

Decisão do usuário: preservar newtm_binding_v1. O transporte novo
newtm_clone_binding_v1 contém essa identidade e storage (registrado/privado).
Incoming/result conservam seus canais e são resolvidos por ativação nos clones.
Capturas GNU têm canais separados para objeto e, quando necessário, valor
ponteiro; pais, locais, parâmetros, endereços e testemunhos são remapeados.

Consumidores operacionais rodam no fim da gimplificação, antes também da
conversão de funções GNU aninhadas. Descritores não sobrevivem a remapeamento:
usar antes ou reconstruir; a retirada na entrada de omplower permanece.
Instruções substituídas perdem imediatamente seus vínculos/descritores; copies
GIMPLE continuam stale. Fonte-only/unknown não recebem identidade fictícia.

Leituras/escritas preexistentes dos corpos continuam comuns nesta etapa.
Instrumentação, controle externo/retry gerado e verificação de cobertura seguem
nas tarefas 5–6. Esta atualização prevalece sobre pendências históricas de clones.


## Etapa 3 — ABI executável, pragma ainda em rastreamento — 2026-10-30

Implementada a [fronteira STM v1](OPENMP_TRANSACTION_ABI_EXECUTABLE.md): dez
serviços `GOMP_transaction_stm_v1_*`, exportados em `GOMP_NEWTM_2.0`, builtins
com status de 32 bits e construtor de chamadas High GIMPLE. O controle escolhido
usa slot proprietário estável e status explícito; entrada externa adquire a
lease e aninhamentos compartilham o slot. A ponte antiga permanece intacta.

`include/newtm-abi.h` fixa status/kind, contexto opaco, slot e esquema nativo de
`newtm_binding_v1`; `gcc/newtm-abi.{cc,h}` constrói o mesmo layout no target.
Produção/consumo dos bindings incoming/result, clones e instrumentação ficam
para as próximas etapas. Não consultar descritores após sua retirada na entrada
de omplower: consumir antes ou reconstruir após remapeamento. A ABI não prolonga
a vida das tabelas nem transforma receitas de diagnóstico em código.

`make -C transaction_test check-abi-v1` passou: 88 verificações, C/C++ O0/O2,
ligação dinâmica/archive, símbolos/versionamento, chamadas emitidas pelo
construtor do compilador mediante plugin exclusivo de teste, layout/GC,
conflito entre threads, aninhamento, saídas preservadas, confirmação/descarte,
tradução de tags e limpeza de entrada falha/fatal. Isso valida a fronteira,
não ativa STM automático no pragma. Esta atualização prevalece sobre as
pendências históricas de ABI abaixo.


## Etapa 2 de geração de código — estado vigente

[Descritores de registro e identidade](OPENMP_TRANSACTION_DESCRIPTORS.md)
implementados em High GIMPLE: chave lógica original, extensão, deslocamento,
kind e validade estrutural/de faixa, usando operandos avaliados e aritmética
checada. Referência a elemento preserva extensão/kind do elemento registrado.
Parâmetros/resultados ainda dependentes da ABI usam canais explícitos, sem
inventar identidade ou sucesso. Unknown/source-only/stale exigem resolução ou
rejeição pelo consumidor futuro; nunca fallback à memória comum.
Descritores são retirados na entrada de omplower; cópias exigem reconstrução.
Temporários ficam no BIND da região OpenMP correspondente. O pragma permanece
de rastreamento: faltam ABI executável, clones, acessos STM e epílogos. Ver o
contrato para limites e testes; este estado atualiza as pendências históricas.


Atualizado em 2026-10-29. Ler primeiro o [registro de decisões e extensões](OPENMP_TRANSACTION_DECISIONS.md).
Os IDs D/F abaixo remetem a esse registro. Esta orientação prevalece sobre
estados antigos de stubs ou funcionalidades pendentes no histórico deste arquivo.

A análise atual está em `gcc/newtm-analysis.cc`, antes da gimplificação. Preservar
leituras constantes por marcadores semânticos, registros alcançados e interseção
nos desvios. Chamadas diretas da mesma unidade são validadas transitivamente;
clones executáveis ainda não são gerados. Campos/elementos conservam base física,
extensão registrada e offsets. Planos TREE/GIMPLE são receitas não executáveis;
remapear suas declarações explicitamente e marcar endereçabilidade antes de
gimplificar usos anteriores. Manter D02–D05 e os limites atuais; F02–F05, F07 e
F09 exigem prova de identidade, vida útil e avaliação única antes de relaxá-los.

Consultar a [ABI especificada](OPENMP_TRANSACTION_ABI.md) e o
[relatório de revisão e validação](OPENMP_TRANSACTION_SUBSET_REVIEW.md).
A autorização mais recente permite avaliar e implementar a integração se viável.
A [avaliação de prontidão](OPENMP_TRANSACTION_INTEGRATION_READINESS.md) encontrou
bloqueios técnicos; o pragma permanece com rastreamento. A vinculação de
ocorrências da etapa 1 já está implementada. Faltam descritores/bindings
executáveis, instrumentação STM, clones e epílogos antes da ativação.
Em nova mudança, atualizar contratos e o estado das propostas/limitações.

## Etapa 1 de geração de código concluída — 2026-10-29

Os [vínculos de ocorrências](OPENMP_TRANSACTION_OCCURRENCES.md) associam sites
GENERIC às instruções e operandos GIMPLE emitidos, incluindo corpos de funções
selecionadas para preparação. Consultar argumentos, índices, referências e
resultados na instrução atual; nunca executar receitas do plano diagnóstico.
Grupos conservam RMW; cópias recebem IDs novos; source-only não fornece valor
avaliado. A janela termina antes de CFG/SSA. Essa etapa não ativa a ponte STM;
descritores/kind, instrumentação, clones e epílogos continuam pendentes.

## Histórico de etapas e referências técnicas

Os textos abaixo conservam decisões e evidências no momento em que foram
escritos. Descrições antigas de stubs, pendências e testes não substituem o
estado vigente acima nem o registro central.

## Contexto: criação e limpeza implementadas — 2026-10-28

Estruturas concretas em [newtm-context.h](libgomp/newtm-context.h), privadas à
libgomp. newtm_begin recebe lease*, slot e outermost: transfere exclusivamente
a lease da ponte ao contexto externo e preserva-a no aninhamento. O endereço do
slot canônico deve permanecer estável; contexto guarda slot e execução dona para
rejeitar consumo por cópia ou outra thread. Isso não valida ponteiros pendentes.

newtm_fail registra a causa, com INVALID dominante, sem limpar recursos.
newtm_discard desliga o slot e retira reservas sob o bucket; libera buffers,
listas de intervalos, referências e registros fora dele, depois contexto e lease.
O owner configura newtm_policy_destroy antes de publicar a tabela; o callback
exige leitores/escritor vazios e libera somente metadados compartilhados.
Todos os printf/fflush e diagnósticos permanecem fora dos mutexes.

A ponte GOMP usa esse contexto para entrada e limpeza do rastreamento. Sua lista
legada de objetos ainda é separada dos registros privados STM e é drenada antes
do descarte. newtm_end só implementa saída interna; saída externa marca INVALID
sem publicar, para descarte explícito. A ponte externa usa descarte de trace,
não commit. Registro STM, criação sob demanda de metadados, reservas, valores
privados, read/write, publicação e retry continuam pendentes. As estruturas de
registros já existem para permitir limpeza, mas os testes as preparam por acesso
privado: isso não significa que registro/acesso STM estejam implementados.

check-lifecycle agora testa contextos reais, incluindo lease, aninhamento,
slot copiado, outra thread, overflow, falhas dominantes, leitores compartilhados,
limpeza de buffers/intervalos sem publicação e caminhos fatais. Testes owner,
diagnósticos e exemplos/carga passaram. ASan/UBSan passaram no teste de contexto
com fontes de hash/contexto instrumentadas; não há nova execução DejaGNU.

## Lease e destrutor da política — decisões aceitas

Transferir exclusivamente a lease da ponte para o contexto externo, invalidando
o token de origem sem incrementar participações. Aninhamentos preservam a mesma
lease. Liberá-la somente após toda a limpeza, inclusive releases e callbacks;
na falha anterior à transferência, a ponte continua responsável pelo token.
Não publicar contexto parcialmente inicializado nem duplicar admissão/liberação.

Configurar o destrutor interno da política na criação da tabela global, antes
de publicá-la, substituindo o NULL atual; manter o callback imutável durante sua
vida útil. Criar metadados sob demanda: preparar candidato fora do bucket,
instalar um só em entry_visit e liberar candidato excedente fora do mutex.

Na limpeza, retirar leitores/escritor sob o bucket antes de liberar referências.
Na última referência, release executa o destrutor fora do bucket. O destrutor
verifica que leitores/escritor estão vazios e libera somente os metadados
compartilhados; buffers privados pertencem aos registros. Participações residuais
são violação interna fatal, nunca removidas à força. Não chamar código do usuário,
adquirir reservas, acessar valores dos objetos ou liberar contextos nesse callback.
A lease permanece ativa durante toda a execução do destrutor.

Procedimentos implementados na base do contexto: owner configura destrutor
interno e begin transfere a lease. Registro e acessos STM continuam pendentes.

## Interface de classificação e estrutura aceita — 2026-10-28

O usuário aceitou a [especificação das estruturas do contexto](OPENMP_TRANSACTION_CONTEXT_PROPOSAL.md):
lista simples de registros estáveis por tentativa, escalares com buffer sob demanda
e flags, agregados com buffer completo sob demanda e duas listas de intervalos,
participação intrusiva de leitores e metadados compartilhados por entrada.
As alternativas documentadas não substituem essa escolha inicial.

A interface interna newtm_register(ctx, object, size, kind) recebe agora
`enum newtm_object_kind`: NEWTM_OBJECT_SCALAR ou NEWTM_OBJECT_AGGREGATE.
O compilador deve classificar o objeto canônico inteiro após validar seu tipo;
arrays são AGGREGATE, ponteiros listados são SCALAR sem incluir o apontado.
Não inferir classificação por tamanho/alinhamento. O registro privado conserva
kind imutável; read/write consultam esse registro. Repetição exige tamanho e
kind compatíveis; tag desconhecida ou incompatível é INVALID no contrato
funcional, com limpeza segura e fatal para contexto válido, nunca retry.
Não altera as nove primitivas da hash nem a ABI pública GOMP existente.

A assinatura, o enum e o log numérico de kind já estão no código; newtm_register
continua stub: imprime, faz fflush e retorna INVALID sem examinar contexto ou
objeto. A estrutura aceita está em newtm-context.h, com criação e limpeza
implementadas. Integrar classificação e registro STM no compilador/ponte.

## Fechamento do subconjunto inicial — decisão do usuário

Todos os acessos, de leitura ou escrita, a objetos cuja vida começou antes da
tentativa externa exigem registro explícito já alcançado no contexto. Não há
registro implícito nem fallback à memória comum. Esta decisão substitui a
permissão anterior de leituras externas não listadas: em `transaction(a) { a += b; }`,
se b é preexistente, seu acesso exige registro explícito também. Temporários
automáticos privados são permitidos nos tipos admitidos, recriados a cada retry
e sem escapar da tentativa; não autorizam exceções ou cleanups do usuário.

INVALID detectado com contexto válido é falha permanente: impedir novos efeitos
normais, preservar a causa, propagá-la pelos epílogos preparados até o controle
externo, concluir a limpeza interna segura e somente então emitir diagnóstico
fatal. Não publicar valores nem executar retry. O descarte libera reservas,
referências, buffers, contexto e participação do proprietário conforme a
propriedade de cada recurso. Não introduzir cláusula nem resultado novo na
linguagem; os status são canais internos. Contextos inválidos/alheios não podem
ser desreferenciados ou limpos especulativamente. Falta de memória permanece
fatal; não há promessa de recuperação de OOM nem sua conversão em CONFLICT.
Essa decisão não muda os retornos das nove primitivas genéricas da hash.

Ficam fora do primeiro subconjunto E/S, chamadas de sistema, locks do usuário e
malloc/free executados pelo programa dentro da tentativa, inclusive por funções
chamadas. Admitir alocação exigirá rastreamento de recursos e contrato de descarte
ou liberação adiada. Alocação e sincronização internas do runtime continuam
necessárias; seus logs de depuração não concedem suporte a E/S do programa.

O critical do teste de carga é apoio ao rastreamento atual. Reavaliá-lo quando o
teste verificar isolamento STM real; o teste atual não comprova esse isolamento
nem constitui uma exceção às restrições do futuro subconjunto funcional.

As decisões semânticas, incluindo cópias privadas, estão fechadas para definir
o layout do contexto. Layout/ABI, filtros, diagnósticos e execução desse protocolo
ainda precisam de implementação; os stubs atuais não passam a implementá-los.

# Agente orientador: escopo e tipos dos identificadores de transaction

## Missão e limites

Oriente a implementação da resolução dos identificadores de
`#pragma omp transaction(a, b)` nesta árvore GCC 16.2.0, para verificar sua
visibilidade no ponto da diretiva e obter seus tipos. Leia também
[OPENMP_AGENT.md](OPENMP_AGENT.md),
[OPENMP_CRITICAL_AGENT.md](OPENMP_CRITICAL_AGENT.md) e
[OPENMP_TRANSACTION_IMPLEMENTATION.md](OPENMP_TRANSACTION_IMPLEMENTATION.md) e
[OPENMP_TRANSACTION_ACCESS_AGENT.md](OPENMP_TRANSACTION_ACCESS_AGENT.md).

Este documento foi elaborado por inspeção dos fontes locais. Os caminhos são
relativos a gcc-src-tm/, com build em gcc-build/ e testes em transaction_test/
na raiz do repositório (/usr/src/gcc-tm no contêiner). Localize funções pelo nome
e confira seus chamadores.
A criação deste guia não implementa a validação no compilador nem altera a
semântica da diretiva. Não acrescente sincronização ou memória transacional
como consequência da resolução dos nomes.

## Estado atual

Os parsers resolvem os nomes antes do corpo, aceitam VAR_DECL/PARM_DECL e
preservam as declarações em OMP_TRANSACTION_OBJECTS. C++ usa a busca do parser
com verificação de acesso e ambiguidade e processa referências automáticas
externas. A instanciação substitui a lista em cp/pt.cc. A gimplificação exige
tipo completo de tamanho constante, registra referências C++ pelo objeto
referido e converte os itens em chamadas runtime. A lista não permanece como
operando do marcador GIMPLE. Tipos e lowering só são impressos com
-ftransaction-debug.

As orientações abaixo são critérios de manutenção e integração futura, não
uma lista de funcionalidades ausentes. Conferir o estado em
OPENMP_TRANSACTION_IMPLEMENTATION.md antes de alterar representações.

## Momento correto e contrato semântico

Resolva cada nome durante a análise da lista, antes de chamar
`c_parser_omp_structured_block` ou `cp_parser_omp_structured_block`. Nesse ponto,
os bindings ativos correspondem ao contexto da diretiva. Uma variável declarada
somente dentro do corpo não pode tornar válido um argumento da diretiva.
Uma declaração posterior no bloco também não deve ser encontrada por uma
busca adiada. Respeite as regras próprias de C++ para contexto de classe e
templates, em vez de simular visibilidade por ordem textual.

Use a busca da linguagem: ela trata escopos externos e ocultação por declarações
mais internas. Não procure identificadores por texto nos fontes, não percorra
apenas os locais da função e não use `DECL_CONTEXT` como teste de visibilidade.
Um nome global pode estar visível dentro de uma função, e o contexto de uma
declaração não informa sozinho qual binding está ativo.

Antes de implementar rejeições, defina quais entidades transaction admite.
O pedido de conhecer tipos não determina se funções, constantes de enumeração,
typedefs ou membros são argumentos válidos. Separe três resultados:

1. O nome foi encontrado segundo as regras da linguagem?
2. A entidade encontrada pertence a uma categoria permitida pela extensão?
3. Seu tipo já é conhecido ou depende de instanciação?

Se o contrato escolhido for **variáveis e parâmetros**, aceite inicialmente
`VAR_DECL` e `PARM_DECL`; documente essa escolha. Não rejeite parâmetros por
testar apenas `VAR_DECL`. Defina separadamente membros não estáticos
(`FIELD_DECL`), structured bindings, constantes e outros casos C++.
Preserve a gramática atual de identificadores simples: usar um parser de
expressões ou listas OpenMP mais amplo não deve habilitar silenciosamente
`ns::x`, `obj.x`, `*p`, seções de arrays ou expressões.

## Resolução no front-end C

Referências concretas:

- `gcc/c/c-decl.cc`: `lookup_name(tree name)` consulta o binding ordinário
  atual (`I_SYMBOL_BINDING`), considera sua visibilidade e retorna a declaração
  ou `NULL_TREE`. Leia as exceções existentes, inclusive versões de funções.
- `gcc/c/c-parser.cc`: no processamento de listas OpenMP, o trecho que chama
  `lookup_name(c_parser_peek_token(parser)->value)` mostra o diagnóstico via
  `undeclared_variable` e a recuperação com `error_mark_node`.
- `gcc/c/c-parser.cc`: `c_parser_omp_var_list_parens` é uma entrada para
  estudar esse fluxo; suas regras completas não são a gramática de transaction.

Em `newtm_c_parser_transaction`, capture `value` e `location` do token antes
da chamada que o consome. Depois de confirmar `CPP_NAME`, use o identificador
em `lookup_name`. Para ausência, reutilize `undeclared_variable(loc, id)`
conforme o padrão local e marque o item como inválido. Para uma declaração
encontrada, valide sua categoria antes de extrair o tipo.

Pseudocódigo, a adaptar à recuperação existente:

```text
confirmar token CPP_NAME
salvar id = token->value e loc = token->location
consumir identificador
decl = lookup_name(id)
se decl é nulo: diagnosticar nome não declarado
senão se decl é error_mark_node: propagar falha sem diagnóstico duplicado
senão: classificar entidade e obter TREE_TYPE(decl), quando aplicável
continuar vírgula / fechamento / fim do pragma
```

O namespace ordinário de C inclui variáveis, funções e typedefs; encontrar um
nome não prova que é variável. Um tag `struct S` pertence a outro namespace e
não deve ser buscado como variável. Confira também `id_kind`: o parser
aceita `CPP_NAME` sem restringir a `C_ID_ID`, e um typedef pode chegar como nome
com outra classificação. Não transforme uma restrição de categoria em uma
mensagem enganosa de identificador inexistente.

## Resolução no front-end C++

Referências concretas em `gcc/cp/parser.cc`:

- `cp_parser_identifier`: análise do identificador simples, sem resolver nome.
- `cp_parser_lookup_name_simple(parser, name, location)`: wrapper local de
  `cp_parser_lookup_name` com checagem de dependência e busca usual de nome
  que não foi solicitado como tipo, template ou namespace.
- `cp_parser_lookup_name`: busca conforme o estado do parser, tratamento de
  ambiguidades e checagem de acessibilidade. Seu resultado não é sempre uma
  declaração simples; pode incluir sobrecargas e representações dependentes.
- `cp_parser_omp_var_list_no_open`: exemplo de resolução com o wrapper,
  diagnóstico por `cp_parser_name_lookup_error`, e tratamento de referências
  a variáveis automáticas externas por `outer_automatic_var_p` e
  `process_outer_var_ref`.
- `gcc/cp/name-lookup.cc`: implementação de `lookup_name` e infraestrutura
  de busca C++. Não confunda sua interface com a função homônima de C.

Em `newtm_cp_parser_transaction`, preserve a localização antes de
`cp_parser_identifier`, guarde seu retorno e resolva o nome com o mecanismo
do parser. O padrão de `cp_parser_omp_var_list_no_open` é o ponto de partida;
não substitua toda a rotina pela lista OpenMP existente, que aceita sintaxe e
aplica tratamentos específicos de cláusulas diferentes.

Trate `error_mark_node` da análise e da resolução. Siga o padrão de diagnóstico
do chamador e confira se a busca já emitiu erro, evitando mensagens duplicadas.
Não aplique `TREE_TYPE` indiscriminadamente a `OVERLOAD`, `BASELINK`,
`SCOPE_REF` ou qualquer resultado ainda não classificado.

Encontrar uma declaração e ter acesso permitido são condições distintas. A
infraestrutura de busca pode emitir um erro de acesso e ainda retornar uma
declaração; um retorno diferente de `error_mark_node` não certifica sozinho
que toda a validação semântica passou.

Para lambdas, funções locais e membros, defina se listar um nome conta como
uso ou captura. O processamento de variáveis externas das cláusulas OpenMP é
uma referência a estudar, não uma operação a copiar sem esse contrato.
Obter metadados de tipo não deve causar inadvertidamente uma avaliação do
objeto ou uma conversão lvalue-to-rvalue.

## Como obter e interpretar o tipo

Após uma resolução válida e classificação da entidade, o tipo declarado de
uma variável ou parâmetro está em `TREE_TYPE(decl)`, definido em `gcc/tree.h`.
Verifique valores nulos e `error_mark_node` antes de usar predicados de tipo.

| Informação | Ponto de partida |
| --- | --- |
| Categoria da entidade | `TREE_CODE(decl)`, `DECL_P(decl)` |
| Tipo declarado da variável/parâmetro | `TREE_TYPE(decl)` |
| Categoria do tipo | `TREE_CODE(type)` e predicados pertinentes de `tree.h` |
| Qualificadores | `TYPE_QUALS(type)` |
| Tipo apontado, referenciado ou elemento de array | `TREE_TYPE(type)`, depois de verificar a categoria |
| Limites do array | `TYPE_DOMAIN(type)` quando o tipo é `ARRAY_TYPE` |
| Dependência de tipo C++ | `dependent_type_p(type)`, declarado em `gcc/cp/cp-tree.h` |
| Impressão em diagnóstico | formato `%qT` com um nó de tipo válido |

Preserve o tipo original. Não aplique `TYPE_MAIN_VARIANT` antes de registrar
qualificadores, pois ele remove diferenças que podem ser relevantes.
Uma referência C++ tem tipo declarado diferente do tipo da expressão após
`convert_from_reference`. Arrays não devem sofrer decay só para consultar seu
tipo. Um parâmetro declarado com sintaxe de array já pode ter sido ajustado
para ponteiro pelas regras da linguagem.

Tipo conhecido não significa tipo completo ou tamanho constante. Ponteiros
para tipos incompletos, arrays de tamanho variável e tipos dependentes exigem
tratamento próprio. Não use `TYPE_SIZE_UNIT` como teste de existência do tipo
e não converta seu valor para inteiro sem validar que é constante e cabe no
formato de destino. Não force layout de tipos para a simples consulta.

## Contrato de identidade para instrumentação de acessos

Nome visível, entidade admitida e tipo instrumentável são verificações distintas.
A aceitação atual de VAR_DECL/PARM_DECL com tipo completo de tamanho constante
não comprova suporte a todas as operações desse tipo. Na integração funcional,
validar o subconjunto do agente de acesso, respeitando const, volatile, atômicos,
vida útil e propriedades C++; não apagar qualificadores para admitir escritas.
As restrições dependentes devem ser repetidas após instanciação.

Decisões do usuário em 2026-10-28: escalares, arrays e agregados restritos com
validação recursiva; excluir volatile, atômicos, VLA, unions, bit-fields, membros
flexíveis e casos sem contrato; proibir escritas const. Instâncias de classes
ficam para trabalho futuro (struct C++ também é tipo de classe). O parser atual
pode aceitar tipos fora desse subconjunto: implementar filtros na integração
funcional, não anunciar que a mera resolução do nome já os valida.

Identidade conhecida permite vetor[i] dinâmico, com avaliação única e limites
verificados em execução. Rejeitar ponteiros não identificáveis, sem fallback
comum, e registros distintos sobrepostos. Campos/elementos usam entrada do
agregado registrado e offset, não registros independentes. A hash não descobre
intervalos; a aplicação/diagnóstico dessa proibição ainda precisa de integração.
Leituras e escritas de objetos preexistentes exigem registro explícito;
rejeitar ponteiros desconhecidos sem fallback à memória comum.

Preservar dados suficientes para o compilador construir os argumentos propostos
`object`, `offset` e `size`, sem resolver novamente o nome no lowering:

| Informação | Responsabilidade |
| --- | --- |
| Declaração resolvida e localização | Identidade lexical e diagnóstico do argumento/acesso. |
| Tipo e qualificadores originais | Permissão de leitura/escrita e classificação dos acessos admitidos. |
| Objeto canônico e extensão registrada | Unidade inteira de conflito e validação dos limites de subacessos. |
| Subobjeto e deslocamento em bytes | Campo/elemento suportado dentro do objeto canônico. |
| Expressões de endereço/índice | Preservar avaliação única e instrumentar suas próprias leituras quando necessário. |

A resolução da lista fornece a identidade inicial; o mapeamento de cada acesso
para subobjeto/deslocamento pertence à instrumentação posterior. Não antecipar
endereços de execução no parser nem avaliar índices ou objetos para obter tipos.
Ponteiros listados não incluem automaticamente a memória apontada; referências
C++ designam o referido. Aliases suportados compartilham a mesma chave canônica;
a hash de igualdade exata não resolve aliases ou sobreposições por conta própria.

Ao transportar esses dados, remapear declarações em templates, inlining,
funções aninhadas e outlining. Usar a instância efetiva de private/reduction
externos. Preservar metadados antes da varredura OpenMP quando afetarem captura;
o marcador GIMPLE atual guarda somente o corpo. Atualizar walkers, cópia e dumps
quando o layout mudar, sem guardar ponteiros para tokens ou estruturas efêmeras.

Conhecer o binding não autoriza acesso: o registro precisa ter ocorrido na
execução antes da operação. Uma lista interna não torna transacionais acessos
anteriores. Todos os acessos preexistentes exigem registro explícito, inclusive
leituras. Rejeitar ponteiros não identificáveis, sem classificá-los como
externos por falta de prova. Não usar o novo contrato para
ampliar silenciosamente a gramática de identificadores simples.

No subconjunto inicial, rejeitar também exceções e cleanups do usuário,
inclusive objetos locais/temporários que os exijam mesmo quando não listados.
Não admitir handlers de cleanup por serem atributos conhecidos. Suporte futuro
precisa definir acesso de destrutores a dados com tentativa condenada;
read/write normais não são uma solução para esse estado. Limpeza interna do
runtime e suas referências/leases permanece obrigatória. Aceitação atual de
uma construção pelo parser ou teste de rastreamento não certifica essa validação.

## Contrato das funções preparadas

Decisão do usuário: versões transacionais internas recebem contexto e propagam
status explícito, com resultado preenchido somente em sucesso, preservando a
assinatura pública original. Remapear identidade canônica, referências e offsets
para essas versões; uma chamada preparada não pode resolver novamente nomes por
texto nem usar a visão comum para objetos registrados. Verificar status antes
de consumir resultado; falha segue ao epílogo e não publica um valor fictício.
Chamadas indiretas/externas sem preparação são restritas e pure/const não prova
acesso à cópia privada. Geração/linkage/ABI e representação de retornos por
referência ainda precisam de especificação técnica; esta atualização não os
implementa nem amplia o subconjunto de classes ou aliases já definido.

## Templates e preservação da informação

Um parâmetro `T &x` de template pode estar perfeitamente visível mesmo quando
seu tipo ainda depende de `T`. Não emita “não declarado” só por não conhecer o
tipo concreto. Faça imediatamente as verificações independentes e adie as
restrições que precisam da substituição.

Se a informação for necessária somente para diagnóstico imediato de entidades
não dependentes, a consulta pode ficar no front-end. Se for necessária na
instanciação ou no lowering, preserve declarações ou representações
dependentes e localizações na árvore da construção; strings isoladas não
preservam identidade nem bindings.

Nesse segundo caso, será necessário planejar alterações de representação:

1. Preservar o armazenamento e os acessores existentes em `OMP_TRANSACTION`,
   `newtm_finish_transaction` e seus chamadores.
2. Em `gcc/cp/pt.cc`, separar o tratamento de transaction quando necessário,
   substituir os novos dados com a infraestrutura de templates e repetir as
   verificações dependentes. O caso atual já substitui corpo e lista; preservar esse tratamento.
3. Se os dados precisarem chegar ao lowering, atualizar também o layout GIMPLE,
   `newtm_build_transaction`, a conversão em `gcc/gimplify.cc` e seus consumidores.
4. Auditar walkers, cópia, dumps, módulos C++ e eventual serialização, conforme
   o tempo de vida dos dados. Não guardar ponteiros para tokens ou vetores
   temporários em nós persistentes; usar armazenamento rastreado pelo GC.

Não volte a buscar uma string de nome no lowering: o escopo léxico original
já pode ter terminado, e a ocultação torna essa busca incorreta. Preserve a
declaração resolvida e faça o remapeamento necessário para templates, cópia
ou outlining. A visibilidade lexical também não determina sozinha qual cópia
de uma variável `private` de uma região OpenMP externa será usada em runtime.

## Recuperação e testes para uma implementação posterior

Preserve a recuperação até `CPP_PRAGMA_EOL` e analise o corpo mesmo após erro
na lista, conforme o fluxo atual. Um argumento inválido não deve impedir a
análise dos seguintes nem corromper a próxima construção. Defina a política
de duplicatas separadamente; nomes repetidos são aceitos e deduplicados por endereço no contexto runtime.

Adicione casos compartilhados em `gcc/testsuite/c-c++-common/gomp/` e casos
C++ específicos em `gcc/testsuite/g++.dg/gomp/`, incluindo:

- Variáveis globais, locais e parâmetros visíveis; tipo esperado de cada um.
- Ocultação: global `int x` e local `double x`; resolver a declaração local.
- Nome não declarado, local de bloco encerrado, declaração posterior e nome
  declarado somente no corpo de transaction.
- Categorias encontradas mas não permitidas pelo contrato escolhido: typedef,
  função, enumeração e, em C++, namespace ou conjunto de sobrecargas.
- Ponteiros, arrays, qualificadores, structs e, em C++, referências e classes.
- C++: acesso, membros, lambdas, ambiguidades e templates com tipo dependente;
  testar definição e instanciação, inclusive falhas que só surgem nesta última.
- Macros, `_Pragma`, listas com múltiplos itens e recuperação depois de erro.
- Formas alternativas e módulos C++ se o armazenamento novo atingir esses caminhos.

Verifique a identidade da declaração e o tipo com dumps apropriados ou uma
instrumentação temporária controlada. Compilar sem erro, por si só, não prova
que o tipo correto foi selecionado. Evite mensagens incondicionais no compilador,
que já são uma fonte de falhas registrada no relatório de transaction.

Execute rebuild e os subconjuntos C/C++ de transaction no diretório de build,
seguindo os comandos do relatório de implementação. Confira `.sum` e `.log`,
distinga falhas anteriores de novas regressões e relate os testes realmente
executados. Este guia não executou build ou testes do compilador.

## Pré e pós-condições do trabalho de resolução

Os contratos dos parsers e construtores estão reunidos em
[OPENMP_TRANSACTION_IMPLEMENTATION.md](OPENMP_TRANSACTION_IMPLEMENTATION.md).
A resolução exige bindings válidos no ponto da lista e árvores/localizações
com vida útil adequada; em sucesso preserva identidade e tipo sem avaliar o
objeto. Em erro, diagnostica/recupera sem transformar entidade inválida em objeto
registrável nem certificar a validade dos demais dados parcialmente construídos.
Não adquire reservas nem cria efeito runtime. Instanciação e remapeamento devem
preservar os contratos antes de invocar os serviços do agente de acesso.

## Critério de conclusão da implementação

A resolução usa o escopo do ponto da diretiva, respeita ocultação e regras da
linguagem, distingue ausência de categoria inválida, obtém tipos sem conversões
indevidas e trata dependência C++ no momento correto. Qualquer informação usada
depois do parsing tem representação persistente e seus consumidores atualizados.
Na integração funcional, verificar também identidade canônica, mapeamento dos
subacessos e permissões segundo o agente de acesso, com testes de campo/elemento,
referências, aliases suportados, tipos rejeitados e private/reduction externos.
Não apresentar validação de nomes como prova de cobertura da instrumentação.
Relate o contrato de entidades aceitas, os testes e limitações remanescentes.

## Inventário consolidado e sequência de trabalho

Consultar [OPENMP_TRANSACTION_PRIMITIVES.md](OPENMP_TRANSACTION_PRIMITIVES.md)
para assinaturas existentes, propriedade dos recursos, estados, efeitos atuais
e futuros e pontos de chamada. Os contratos especializados deste agente
permanecem detalhados aqui; mudanças de interface devem atualizar também o
inventário. Os nomes stm_* continuam conceituais, sem assinatura C ou ABI definida.

Estado consolidado em 2026-10-28: as nove primitivas da hash, a ponte com
referências reais e o controle de admissão/participações OPEN/CLOSING/CLOSED
estão implementados. Os testes permanentes incluem estrutura, concorrência,
metadados, contagem, diagnósticos, encerramento e carga parametrizada 1:9/1:1/9:1.
Não listar novamente a implementação da hash ou seu shutdown como tarefas futuras.
Read/write e os seis serviços de ciclo de vida da tentativa continuam esqueletos;
reservas, dados privados, instrumentação de acessos, propagação segura, commit,
descarte e retry funcionais continuam pendentes. A ordem de integração está em
[próximas etapas](OPENMP_TRANSACTION_PROJECT_AGENT.md#orientação-para-as-próximas-etapas).
Não confundir participação do proprietário com reserva/estado de tentativa STM.
Fontes: `gcc-src-tm/`; build: `gcc-build/`; testes: `transaction_test/`, na raiz do
repositório. No contêiner atual, a raiz é `/usr/src/gcc-tm`. Caminhos antigos em
relatos datados são históricos; adaptar comandos à cópia em uso.
