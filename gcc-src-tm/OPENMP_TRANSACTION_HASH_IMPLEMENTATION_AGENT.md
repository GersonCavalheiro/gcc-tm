# Orientação vigente — Implementação e manutenção da hash

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

Não transformar as primitivas genéricas em instrumentação STM ou ativar a ponte
como consequência de manutenção da hash. O índice de intervalos reside em
`libgomp/newtm-transaction.c`, separado da hash, e deve cobrir aquisição pendente,
rollback e cleanup após callback. Preservar D05–D12 e os testes de concorrência,
reservas, commit e liberação única. Para F08, medir a estrutura linear antes de
propor substituição e conservar a ordem de locks/limpeza; nenhuma estrutura de
substituição nem política de justiça foi escolhida.

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

## Falha, descarte e retry no runtime interno — 2026-10-29

Escopo confirmado pelo usuário: manter esta etapa no runtime interno, com blocos
explicitamente preparados. O compilador e a ponte GOMP não foram integrados.

`newtm_execute(body, argument)`, privado em newtm-context.h, controla a região
externa inteira: adquire lease/contexto novo, invoca o corpo preparado uma vez,
confere status efetivo e balanceamento de nesting e confirma somente tentativa
ACTIVE de profundidade um. Em falha usa exatamente um epílogo consumidor.
Conflito limpo reinicia na admissão/entrada do corpo inteiro, incluindo registros,
temporários automáticos e funções aninhadas preparadas, nunca no acesso interno.
O contador pertence ao controle externo e satura em SIZE_MAX. OK só após commit
e limpeza completos; INVALID ou tags desconhecidas nunca autorizam repetição.

`newtm_after_failure(slot, attempt)` salva causa/diagnóstico por valor, descarta
todos os níveis e só então faz diagnóstico fatal de INVALID ou backoff de
CONFLICT. Causa permanente domina conflito e um OK fictício do callback. Entrada
ACTIVE, conflito com contador zero ou nesting não balanceado são uso permanente
inválido; com contexto válido, limpar antes de fatal. Slots inválidos/alheios e
reentrada em publicação/teardown são violações sem limpeza especulativa.
`newtm_backoff` implementa espera ativa portátil via cpu_relax: 64 iterações na
primeira espera, crescimento exponencial até 65536, com clamp antes do shift.
Zero retorna INVALID; SIZE_MAX continua limitado. Sem locks, alocações, estado
global de contagem, escalonamento de tarefas ou garantia de justiça/progresso.

O callback recebe contexto emprestado, registra/acessa objetos explicitamente,
verifica cada status e propaga falhas antes de consumir resultados. Não pode
consumir o slot externo, reentrar no executor, guardar ponteiros privados além
da tentativa, lançar exceções/longjmp ou produzir efeitos externos. Argument é
entrada/controle estável do chamador, não um resultado especulativo que o runtime
possa restaurar. Resultados do programa são publicados nos objetos registrados.
Regiões internas compartilham o mesmo contexto; sucesso deve balancear begin/end.

Testes permanentes: check-retry inclui check-failure. Retry usa conflito real em
função aninhada e pausa o concorrente até a limpeza da primeira tentativa; prova
reexecução do prefixo externo, registros recriados e uma liberação por referência/
lease em cada tentativa. Failure executa dez processos, verifica fatal depois de
limpeza, INVALID dominante, tags desconhecidas, nesting inválido e consumo duplo
recusado sem segunda limpeza. Teste instrumenta contadores/slots somente para
observação, sem ampliar os efeitos admitidos no código do programa.

Não há retry automático do #pragma omp transaction nesta entrega; geração de
código preparado, propagação no compilador e integração da ponte continuam
pendentes. Nenhuma nova ABI pública GOMP foi introduzida.

## Confirmação externa implementada — 2026-10-29

`newtm_end` externo agora valida todos os registros e suas reservas antes de
PUBLISHING. Confere identidade, representação escalar/agregada, intervalos
normalizados e independentes, modified contido em valid, buffer presente quando
necessário e reserva compatível. As verificações de metadados usam visitas curtas
sob cada bucket; reservas próprias permanecem estáveis depois de desbloquear.
Erro anterior à publicação preserva slot, profundidade e recursos para descarte
explícito. Tentativa condenada não publica; saída interna só reduz profundidade.

Depois da validação, ACTIVE -> PUBLISHING é irrevogável. Não há alocação ou
retorno de erro recuperável nessa fase: copia escalares modificados e somente
intervalos modificados de agregados. Objetos/faixas não modificados não são
acessados. TODAS as reservas são mantidas até terminar TODAS as cópias. Em seguida,
o teardown compartilhado com discard marca CLOSED, desliga o slot, retira
reservas, libera referências/buffers/listas/contexto e solta a lease por último.
OK somente após a limpeza completa; falha estrutural na limpeza é fatal.

`make -C transaction_test -B check-commit` verifica confirmação vazia, escalar,
aninhamento, leitores compartilhados, slot copiado, intervalos modificados com
páginas vizinhas inacessíveis, objetos somente leitura/sem acesso e validação
recusada sem publicação parcial. Wrappers restritos ao teste pausam depois da
primeira cópia: duas tentativas por objeto ainda conflitam em read e write.
Verificam ausência de alocação depois da primeira cópia, slot desligado antes
de releases e tabela drenada antes da última liberação da lease.

Vida útil, escrita permitida nas faixas modificadas e objetos disjuntos continuam
pré-condições. A ponte GOMP ainda descarta seu contexto de rastreamento e não
chama confirmação externa; integração no compilador/ponte, backoff e retry
continuam pendentes. Nenhuma nova ABI ou suporte de linguagem foi introduzido.

## Contrato de acessos read/write consolidado — 2026-10-29

O caminho comum newtm_prepare_access agora valida contexto proprietário/estado,
registro, faixa e buffer, e obtém a reserva antes de entregar o registro ao
caminho de cópia. Read observa a visão privada, incluindo escritas anteriores;
write não altera a memória original. O suporte aos valores já foi implementado
na etapa anterior; esta entrega consolida entrada, diagnóstico e aceitação.

Conflitos em subacessos registram object/offset/size da operação solicitada,
em vez da extensão inteira do objeto. Chamadas posteriores em tentativa já
condenada preservam a primeira causa e sua faixa. Em erro, destino de leitura,
origem de escrita, dados privados anteriores e memória original são preservados;
reservas/referências preexistentes permanecem para descarte explícito.

`make -C transaction_test -B check-access` inclui check-values e uma nova matriz
com 20 casos para cada operação: contexto nulo/alheio/condenado/não ativo,
profundidade inválida, objeto ausente, tamanho/offset inválidos ou extremos e
sobreposição com objetos e armazenamento interno conhecido. Verifica também
conflitos em subfaixas sem perder escritas privadas anteriores de outro objeto,
sem produzir resultados ou registros de dados parciais. Estados/depth artificiais
são restaurados somente nos testes para permitir o descarte real dos recursos.

Validade/vida útil/extensão real dos ponteiros e aliases fora do contexto seguem
pré-condições; OOM continua fatal. Não há nova ABI, integração com GOMP/compilador,
publicação, commit ou retry nesta etapa.

## Valores privados e intervalos implementados — 2026-10-29

`newtm_read/write` agora operam sobre valores privados usando as reservas
internas existentes. Escalares exigem offset zero e tamanho integral, com buffer
sob demanda e flags valid/modified. Leitura carrega uma vez; escrita substitui
somente a representação privada, sem ler o original. Leituras posteriores
observam as escritas da tentativa, inclusive no aninhamento.

Agregados alocam buffer de extensão completa sem inicialização integral.
Read copia do original somente lacunas da faixa solicitada, sob reserva, sem
sobrescrever intervalos já válidos. Write copia somente source para a faixa e
não lê o original. Listas válidas/modificadas independentes representam uniões
normalizadas de intervalos [begin,end), ordenados, disjuntos, com adjacentes
unidos; modificados são sempre válidos. Nós e buffer são preparados antes de
alterar bytes/listas; não há falha retornável depois de começar a cópia.

Owner/estado, registro, limites por subtração, tamanho integral de escalares e
aliases verificáveis do buffer privado são checados antes das reservas/cópias.
CONFLICT/INVALID preservam destino de read e escritas anteriores, mantendo
recursos para descarte. OOM continua fatal. Extensão/vida útil real dos buffers,
aliases fora do contexto e validação de tipos/const/volatile/atômicos continuam
pré-condições do chamador/compilador. Diagnósticos, alocações e cópias ficam fora
dos mutexes. Descarte libera buffers/listas sem publicar valores.

Teste permanente `make -C transaction_test -B check-values`: 500 operações contra
modelo em bytes, formas de união/adjacência, páginas PROT_NONE ao redor de um
campo, leitura de caches após proteger o original, escrita sem leitura original,
falhas preservando dados/listas e duas threads com leitores, conflito e promoção
em 32 rodadas. A ponte GOMP/compilador continua sem chamar esses acessos; commit,
publicação, backoff e retry permanecem pendentes. A entrega não ativa STM nos
programas compilados nem amplia o subconjunto de linguagem.

## Metadados e reservas internos implementados — 2026-10-29

`newtm_reserve(ctx, object, mode)`, declarado privadamente em newtm-context.h,
retorna enum newtm_access_status e exige contexto proprietário ACTIVE e objeto
já registrado. READ e WRITE são os únicos modos solicitáveis. A função traduz
estados/status explicitamente; desconhecidos, objeto não registrado e modo
inválido condenam a tentativa válida com INVALID. Tentativa já condenada conserva
a causa; contexto alheio é recusado sem alteração.

A primeira solicitação cria metadados sob demanda: visita inicial identifica
ausência, candidato zerado é alocado fora do bucket, segunda visita revalida e
instala um único candidato. Concorrentes liberam candidatos excedentes fora do
mutex. Não há alocação, espera, E/S ou acesso a valores dentro do callback.
OOM permanece fatal. Registro continua sem criar metadados ou obter reservas.

A visita concede leitura compartilhada e escrita exclusiva, reutiliza reservas
próprias sem duplicar leitores ou rebaixar WRITE e promove READ para WRITE
somente quando o registro é o único leitor. Conflito condena apenas a solicitante;
promoção recusada preserva READ até descarte explícito. As reservas persistem
até limpeza; descarte remove participações sob o bucket, e a última referência
destrói metadados fora dele, com lease ainda ativa. Nenhum mutex acompanha o corpo.

Teste `make -C transaction_test -B check-reservations`: matriz de conflitos,
promoção, nesting, descarte, estados/argumentos inválidos, página PROT_NONE e
quatro threads em 16 rodadas por modo. Interposição restrita ao teste força a
preparação concorrente de quatro candidatos antes da instalação, sem hooks no
runtime. O teste inspeciona os metadados somente em entry_visit. A interface
permanece oculta na DSO. Read/write continuam stubs; não há cópia de valores,
commit, retry ou integração com a ponte GOMP/compilador nesta entrega.

## Registro STM por tentativa implementado — 2026-10-29

`newtm_register` agora valida contexto proprietário/estado, endereço não nulo,
tamanho positivo e kind explícito. Mantém lista linear de registros estáveis por
tentativa, compartilhada pelo aninhamento, e uma referência física à hash por
objeto canônico distinto. Repetição do mesmo endereço/tamanho/kind é idempotente;
incompatibilidade condena a tentativa com INVALID. Falhas retornáveis da hash
não vinculam candidatos nem deixam referências adicionais; depth é preservado.
A primeira causa permanente é conservada para limpeza e diagnóstico externo.

Registro não lê valores, não cria buffers/metadados de política nem adquire
reservas. Descarte libera os registros/referências. OOM continua fatal. A ponte
GOMP mantém seu registro de rastreamento separado: classificação e integração no
compilador/ponte, reservas, read/write, publicação e retry continuam pendentes.
Kind é validado nas duplicatas desta tentativa; a hash verifica tamanho por
endereço exato e não detecta sobreposições entre objetos distintos.

Contrato executável: `libgomp/newtm-transaction.h` e `newtm-transaction.c`.
Teste permanente: `make -C transaction_test -B check-register`, com identidade,
deduplicação aninhada, referências entre contextos, página PROT_NONE sem leitura,
ausência de reservas/buffers/metadados e falhas de tamanho/kind/overflow sem
registros parciais. As seções anteriores a esta entrega mantêm relatos históricos.

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

## Organização escolhida para o contexto

Cada tentativa externa mantém uma lista de registros, compartilhada pelos níveis
aninhados do mesmo contexto. Cada registro corresponde a um objeto canônico
registrado e permanece em endereço estável até sua remoção na limpeza. Repetir o
registro do mesmo objeto reutiliza o registro existente; a busca inicial é linear.
Não introduzir agora vetor de registros nem índice privado adicional.

O contexto contém controle da tentativa, profundidade, causa e lease do
proprietário. Seus registros mantêm referências à hash, participação/reserva e
representação privada. Metadados compartilhados de leitores/escritor permanecem
associados à entrada da hash e sincronizados pelo mutex do bucket; não confundir
esses metadados com o estado privado da tentativa.

Escalares usam representação simples. Para agregados, escolher buffer da extensão
completa do objeto, preenchido sob demanda, e listas de intervalos válidos e
modificados. Alocar a extensão não implica ler ou inicializar todo o objeto.
Intervalos modificados devem estar contidos nos válidos; leituras completam
somente trechos solicitados ausentes e commit publica somente modificados.
A unidade de conflito e os endereços lógicos originais permanecem preservados.

Esta escolha define a organização inicial. Campos C, armazenamento dos escalares,
representação dos limites/união de intervalos e ordem detalhada de liberação
precisam ser especificados antes da implementação. Avaliar otimizações de busca
e alocação após obter funcionamento correto. Nenhuma estrutura executável foi
introduzida por esta atualização documental.

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

# Agente de implementação e manutenção da hash de transaction

## Estado entregue — 2026-10-28

As nove primitivas de tabela estão operacionais: create/destroy,
acquire/lookup_acquire/release, entry_address/entry_size/entry_visit e count.
Há encadeamento de colisões, mutex por bucket, referências e metadados opacos.
A ponte usa handles reais e verifica falhas; o proprietário admite leases e
encerra por OPEN -> CLOSING -> CLOSED, com um coordenador e sem esperar usuários
no finalizador. A participação cobre todo o contexto, callbacks, limpeza e log
inicial. Não reimplementar essas entregas como se ainda fossem esqueletos.

Mistura fmix32/fmix64 por UINTPTR_MAX, módulo para capacidade arbitrária,
remoção na última referência, destrutor fora do lock e soma saturada já foram
adotados. Printf/fflush permanecem obrigatórios fora dos mutexes de buckets e
do proprietário. OOM é fatal depois de unlock; calloc parcial é desfeito pelo
helper sem diagnóstico. Consulte os algoritmos revisados nos cabeçalhos das
funções em libgomp/newtm-hash.c para o fluxo executado, distinguindo pré-condições
assumidas das validações efetivamente presentes.

A carga transaction_carga recebe threads, W:R e repetições por thread; make check
inclui os cenários 1:9, 1:1 e 9:1. Critical interno protege os valores enquanto
a STM não oferece isolamento; a carga testa registros/vida útil, não desempenho
ou correção de STM. Read/write e ciclo de vida da tentativa continuam esqueletos.

## Missão e autorização

Orientar manutenção da estrutura já implementada, revisão dos seus invariantes
e evolução expressamente solicitada. Preservar a separação entre hash, proprietário
da tabela e futura política transacional. Este guia não autoriza implementar
funcionalidades futuras de STM nem criar ABI pública por consequência.

Referências normativas do projeto:

- [Guia geral](OPENMP_AGENT.md).
- [Panorama e estado atual](OPENMP_TRANSACTION_PROJECT_AGENT.md).
- [Contratos da hash e da ponte](OPENMP_TRANSACTION_HASH_AGENT.md).
- [Inventário e histórico](OPENMP_TRANSACTION_IMPLEMENTATION.md).
- [Identidade e tipos](OPENMP_TRANSACTION_SCOPE_AGENT.md).
- [Acessos futuros](OPENMP_TRANSACTION_ACCESS_AGENT.md).
- [Abort e reinício futuros](OPENMP_TRANSACTION_ABORT.md).

Conferir os fontes antes de modificar. Fontes em gcc-src-tm/, build separado
em gcc-build/ e testes em transaction_test/, relativos à raiz do repositório
(/usr/src/gcc-tm no contêiner). Resultados datados são evidência das execuções
registradas, não substituem os testes pertinentes a uma nova alteração.

## Resultado entregue e limites

A tabela armazena entradas reais e compartilha a mesma geração entre
referências simultâneas à chave. Estrutura, referências e callbacks usam mutexes
por bucket; a ponte retém referências válidas até a saída externa, com flat
nesting. Preservar essas garantias e a drenagem das participações na manutenção.

Registro não concede reserva de leitura/escrita e não acessa o valor do objeto.
O corpo da diretiva continua acessando memória comum. Permanecem futuros:
instrumentação de acessos, reservas, cópias privadas, commit, abort, rollback,
retry e backoff. newtm_read/write e os seis serviços de ciclo de vida
permanecem esqueletos internos. Não ligá-los à ponte como operações funcionais. Não ampliar
gramática, tipos, tasking, suporte a dispositivos ou ABI pública nesta etapa.

## Decisões adotadas na implementação

A entrada é removida na última referência física, não no término do último
participante semântico. Metadados pertencem à geração e são destruídos fora do
lock. Acquire aloca sob mutex; antecipação da alocação permanece otimização
futura condicionada a medições, nova busca e descarte de candidatos. O proprietário
da tabela global controla admissão e drenagem; tabelas independentes continuam
sob quiescência externa. Essas decisões descrevem o código entregue, não propostas
pendentes. Mudanças devem explicitar impacto nos contratos e na validação.

## Decisões semânticas posteriores à entrega estrutural

O usuário exige agora registro explícito para todos os acessos preexistentes;
escalares/arrays/agregados restritos com filtros recursivos, excluindo volatile,
atômicos, VLA, unions, bit-fields, membros flexíveis e casos sem contrato; e
proibição de escritas const. Instâncias de classes, inclusive struct C++, ficam
para trabalho futuro. Também definiu identidade canônica demonstrável, índices
dinâmicos com limites em execução, rejeição de ponteiros desconhecidos sem
fallback e proibição de registros distintos sobrepostos. Campos/elementos usam
entrada/offset do agregado; ponteiro listado não registra a memória apontada.

O usuário também definiu versões transacionais internas com contexto e status
explícitos, resultado preenchido só em sucesso e assinatura pública preservada.
Falha segue aos epílogos pela cadeia preparada, sem valor fictício. Chamadas
indiretas/externas sem preparação são restritas; pure/const não prova acesso à
visão privada. A arquitetura foi escolhida, mas geração/linkage/ABI
continuam por implementar; não adicionar funções públicas da hash por consequência.
Exceções/cleanups do usuário foram rejeitados inicialmente, com suporte futuro
condicionado a protocolo de acesso dos destrutores em tentativa condenada.
A limpeza interna (inclusive destrutores de metadados/leases) permanece
obrigatória. Não converter exceção em CONFLICT, confirmar no desenrolamento ou
relaxar read/write condenados para permitir cleanup de usuário.

A representação privada também foi definida: escalares simples; agregados com
faixas válidas e modificadas, sem inicialização/acesso integral obrigatório para
operar sobre campos. Modificadas são subconjunto das válidas; commit publica
somente modificadas e descarte não publica. Buffers não escapam como endereços:
`&x` preserva o endereço lógico original. Isso não muda a unidade de conflito do
objeto nem a hash. Foram escolhidas listas de registros por tentativa e listas
de intervalos sobre buffer completo sob demanda; campos e implementação pendem.

São contratos de integração do compilador/contexto, não novas funções da hash
ou validações já executadas por ela. Não adicionar resolução dinâmica de
intervalos por consequência desta decisão nem tratar ausência no runtime como
leitura comum automática. Aplique os filtros no futuro caminho funcional e
preserve as nove primitivas genéricas. Detalhes e exemplo a += b no agente hash.

## Estrutura e invariantes

- Uma tabela global por instância da libgomp na ponte; capacidade inicial 1.024.
- Criação da primitiva aceita qualquer capacidade positiva representável.
- Colisões por encadeamento, capacidade imutável e sem redimensionamento.
- Chave por igualdade exata do endereço; tamanho positivo imutável por entrada.
- Converter endereço para uintptr_t e usar fmix32/fmix64 do MurmurHash3,
  selecionando por UINTPTR_MAX igual a UINT32_MAX/UINT64_MAX. Usar todos os
  bits, operações sem sinal na largura correta e módulo bucket_count sem
  truncamento prévio ou máscara; não exigir potência de dois. acquire e
  lookup usam a mesma função; release/visit usam o índice imutável da entrada.
  Outras larguras exigem diagnóstico na compilação da implementação operacional,
  até extensão explícita. Algoritmos/constantes e evidências exploratórias em
  [Decisão de mistura](OPENMP_TRANSACTION_HASH_AGENT.md#decisão-de-mistura-de-endereços).
- Uma referência física por aquisição bem-sucedida. Referência não é reserva
  transacional nem prolonga a vida do objeto do programa.
- Bucket protege lista, contadores, referências e acesso aos metadados.
- Nenhum mutex permanece adquirido após retorno normal. Nenhum mutex de bucket
  envolve o corpo da diretiva ou espera por outra transação.
- Apenas a contagem diagnóstica adquire todos os buckets, em ordem crescente,
  liberando-os em ordem inversa. Não há contador global com mutex por aquisição.
- Destruição exige impedir novas operações e concluir operações, visitas e
  destruidores em andamento, mantendo a exclusão até retornar. Referências
  ociosas podem permanecer: suas entradas causam BUSY, sem consumo ou alteração.
  count zero não impede novas operações, não exclui usuários da tabela nem
  destruidores de entradas já retiradas; não autoriza destroy.

## Primitivas implementadas e contratos de manutenção

Preservar assinaturas internas e os contratos completos do agente da hash.

| Primitiva | Resultado exigido |
| --- | --- |
| newtm_hash_create | Inicializa tabela e todos os mutexes antes de publicar *out; rejeita zero/nulos e overflow. |
| newtm_hash_destroy | Libera tabela vazia; BUSY preserva integralmente tabela não vazia. |
| newtm_hash_acquire | Insere ou retém a entrada existente sob lock; incompatibilidade de tamanho não altera referências. |
| newtm_hash_lookup_acquire | Retém uma entrada existente sem inserção; ausência retorna NOT_FOUND. |
| newtm_hash_release | Consome uma referência adquirida com sucesso; na última, desliga sob lock e conclui destrutor/free fora dele antes de retornar. Decremento positivo não garante sobrevivência após unlock sem outra referência independente. Tabela viva até retorno completo; count zero não basta para destroy. |
| newtm_hash_entry_address | Consulta chave imutável com referência retida. |
| newtm_hash_entry_size | Consulta tamanho imutável com referência retida. |
| newtm_hash_entry_visit | Executa callback curto uma vez sob lock, preservando a referência. |
| newtm_hash_count | Obtém fotografia consistente do número de entradas, saturada em SIZE_MAX. |

Em acquire, alocar a entrada ausente sob o mutex do bucket na implementação
inicial. Depois de a tabela estar operacional e validada, avaliar por medições
se compensa antecipar a alocação. Essa possível otimização exige nova busca sob
lock e descarte de candidatos não utilizados, preservando unicidade, tamanho,
overflow, referências e falhas sem alterações parciais. Não impor alocação
supérflua no caminho de aquisição de entrada existente. A otimização não é
requisito desta etapa; detalhes no
[contrato de alocação](OPENMP_TRANSACTION_HASH_AGENT.md#alocação-inicial-e-possível-otimização-de-acquire).

Nas operações com estado, falhas retornáveis usam NEWTM_HASH_INVALID,
NOT_FOUND, SIZE_MISMATCH, BUSY ou OVERFLOW conforme o contrato. Saída out válida
fica NULL na falha; não deixar inserções ou incrementos parciais. Verificar
overflow de alocações, referências e contadores antes da alteração.
Falta de memória continua fatal via gomp_fatal, mas somente após liberar os
locks. A tabela usa helper interno sem E/S baseado em malloc/calloc; acquire
usa malloc sob o bucket e desbloqueia antes do diagnóstico de falha. Não criar
estado recuperável fictício na API. Violações com ponteiros pendentes não têm
garantia de diagnóstico seguro.

policy_data pertence à entrada após instalação. visit deve retornar
normalmente, sem exceção/longjmp, e não pode reentrar na tabela, esperar por
outra transação, executar código do programa, inverter ordem de locks, consumir
a referência ou liberar o mutex. O endereço do slot não pode escapar; ponteiros
copiados não conservam proteção após retorno e exigem protocolo explícito de
vida útil/sincronização para uso posterior. Não há rollback do callback. Ao substituir
metadados, responde pelo destino dos anteriores. destroy_policy é obrigatório
quando houver metadados não nulos; executa exatamente uma vez por geração
retirada, sem lock de bucket, sem reentrada e sem alterar o objeto do programa.
Uma nova geração com a mesma chave pode existir durante essa destruição.
O callback retorna normalmente, sem exceção/longjmp; release exige tabela viva
até concluir callback e free. Retirar somente participações/reservas sustentadas
pela referência consumida; nenhuma pode sobreviver à última referência que a
sustenta. Cada sucesso de acquire/lookup exige uma release; cópias de ponteiro e
falhas não criam referências. Contador zero, tabela errada, dupla liberação e
ponteiro pendente são violações sem diagnóstico garantido. Não prometer que a
entrada continua viva após unlock/retorno sem outra referência independente.

## Integração implementada da ponte: garantias a preservar

1. Inicializar a tabela uma vez sob o lock existente, usando helper comum
   sem E/S ou diagnóstico fatal. Publicar newtm_trace_initialized apenas com
   tabela completamente válida; liberar o lock antes do log create ou de
   gomp_fatal. Na falha, o helper desfaz alocação parcial e não publica estado.
2. Em begin, preservar contexto por thread, flat nesting e proteção de overflow
   de profundidade. Continuam exigidas execução síncrona e ausência de suspensão
   ou troca de tarefa durante a região dinâmica.
3. Em register, verificar contexto ativo, endereço não nulo e tamanho positivo.
   Duplicata de mesmo tamanho permanece idempotente; tamanho diferente é fatal.
4. Para um registro novo, obter uma referência válida antes de publicar o nó na
   lista do contexto. Falha não pode deixar registro parcial. Definir e cumprir
   a limpeza do nó temporário nos caminhos de erro.
5. A ponte atual retorna void: erros estruturais detectados usam diagnóstico
   fatal claro, sem retry e sem aceitar handle NULL como sucesso. Não mudar ABI
   para introduzir propagação de conflito transacional nesta etapa.
6. Em end interno, reduzir apenas a profundidade. Em end externo, liberar cada
   referência uma vez, limpar registros e contexto e zerar o ponteiro da thread.
   Manter a limpeza dos caminhos excepcionais que chegam ao finally existente.
7. O proprietário admite participações somente em OPEN; CLOSING rejeita
   novos usuários e permite aos existentes concluir. Contexto externo retém
   lease até terminar toda a limpeza; consumidores diretos da tabela global
   também precisam de lease. Owner_close não aguarda drenagem: BUSY mantém
   CLOSING com usuários/coordenador/entradas residuais. Retry após término/join;
   somente um coordenador chama destroy fora do lock e publica CLOSED em OK.
   O finalizador apenas tenta close, sem fatal, espera ou remoção forçada;
   mantém controle/mutex para rejeitar chamadas tardias. Não garante dlclose.
   Contrato completo em
   [admissão](OPENMP_TRANSACTION_HASH_AGENT.md#admissão-e-participações-da-camada-proprietária).

O mutex global de inicialização não deve passar a proteger operações normais
da hash. O serviço não pode ser apresentado como sincronização dos valores do
programa, mesmo quando entradas de threads diferentes forem compartilhadas.

## Arquivos e documentação

Implementação principal: libgomp/newtm-hash.c e libgomp/newtm-hash.h, incluindo
a ponte provisória hoje localizada nesse fonte. Conferir mutexes dos targets e
as regras de inicialização/destruição. Alterar Makefile.am/Makefile.in somente
se a integração ou os testes exigirem, mantendo-os coerentes.

Preservar visibilidade interna de newtm_hash_* e newtm_read/write e os símbolos
GOMP_* existentes. Não exportar serviços internos para permitir testes.
Não modificar o compilador sem necessidade demonstrada pela integração.

Decisão do usuário: manter os printf/fflush para depuração na implementação.
Preservar os formatos atuais enquanto usados pelos verificadores. Não executar printf/fflush ou outra E/S de
diagnóstico sob mutexes de buckets, inclusive em visit e count. Se preservados,
capturar dados seguros sob lock e imprimir após desbloquear, sem desreferenciar
entradas liberadas ou metadados sem proteção. Ordem de impressão concorrente não
comprova ordem das operações. Logs na ponte seguem a mesma restrição.
Atualizar conjuntamente testes de saída e documentação quando mensagens mudarem;
transaction_test/verificar.py depende de contagens e posições dos logs hash.
Validar resultados, referências e callbacks além das mensagens, sem alterar
expectativas para esconder regressões. Logs dos stubs de acesso/ciclo de vida
permanecem um contrato separado. Detalhes em
[restrição de logs](OPENMP_TRANSACTION_HASH_AGENT.md#restrição-de-logs-na-implementação-operacional).

Atualizar panorama, agente da hash, inventário de implementação, manual e
transaction_test/README.md quando o comportamento mudar. Remover descrições
obsoletas de handles nulos e hash sem implementação, mantendo explícita a
ausência de isolamento transacional. Identificar o histórico como histórico.

## Validação para manutenção e novas alterações

Os testes privados existentes incluem o fonte real e headers/configuração
do build Linux x86_64 e ligam o archive local, sem ampliar a ABI. Manter esse
mecanismo reproduzível no Makefile ou justificar uma substituição compatível;
não depender exclusivamente de fontes em /tmp. Interposição/injeção de falhas
e contadores permanecem restritas aos testes, sem hooks de produção.

A partir da raiz, no ambiente compatível com o xgcc construído:

```sh
make -C gcc-build -j"$(nproc)"
make -C transaction_test -B check-hash-create-destroy check-hash-acquire \
  check-hash-entry check-hash-count check-hash-diagnostics check-hash-owner
make -C transaction_test -B check check-lifecycle
make -C transaction_test check-carga CARGA_THREADS=4 CARGA_REPETICOES=1000
```

Check já inclui check-carga (1:9, 1:1, 9:1); o último comando mostra execução
isolada. A carga também pode ser chamada como
transaction_test/.build/transaction_carga THREADS W:R REPETICOES. Uma repetição
é uma transação por thread; escrita é read-modify-write. São
floor(N*W/(W+R)) escritas por thread. Consultar ../transaction_test/README.md para
limites dos parâmetros, logs e timeouts. Todos os testes privados de hash usam
configuração Linux x86_64; isso não comprova suporte dos testes em outros targets.

| Grupo | Evidência exigida |
| --- | --- |
| Estrutura | Capacidades 1 e não potência de dois, colisões, chaves distintas, aquisição repetida, consulta ausente, identidade e tamanho. |
| Falhas | Nulos previstos, tamanho zero, SIZE_MISMATCH sem alteração, out limpo, BUSY preservando tabela e overflow antes de alterar estado. |
| Referências | Liberação parcial/final, consulta retendo entrada após outra release e remoção somente na última referência. |
| Metadados | visit com exclusão correta, substituição com propriedade definida e destrutor exatamente uma vez fora do lock. |
| Concorrência | Mesma chave, colisões, progresso em buckets distintos e contagem concorrente sem corrupção ou deadlock. |
| Ponte C/C++ | Registro antes do corpo, duplicatas, aninhamento lexical/por função, retenção até saída externa, threads distintas e limpeza excepcional suportada. |
| Integração | Templates, referências C++, cópias private/reduction externas, dumps e ligação com a libgomp reconstruída; não confundir registro aceito hoje com tipo STM admitido. |
| Subconjunto funcional futuro | Leitura externa sem controle, filtros recursivos, classes adiadas, vetor canônico com índice dinâmico e diagnóstico de limites, ponteiros sem identidade e registros distintos sobrepostos rejeitados, sem fallback de erro runtime; exceções/cleanups do usuário rejeitados com limpeza interna obrigatória. |

Validar a mistura nas larguras 32/64 com endereços consecutivos e alinhados,
capacidades 1 e não potências de dois e bases que exercitem bits superiores.
Separar simulação aritmética de execução nativa; a evidência exploratória não
substitui testes permanentes da implementação nem exige uniformidade perfeita.

Usar coordenação determinística em testes concorrentes, sem sleep como prova de
ordenação. Não executar count/reentrada a partir de visit para tentar provar
liberação do lock. Para overflow impraticável por volume, usar mecanismo interno
controlado e documentado; distinguir cobertura por teste de inspeção do código.
Se testar falta de memória, fazê-lo em processo separado com injeção controlada.

Reconstruir libgomp em gcc-build/ e executar os testes internos e os
subconjuntos DejaGNU C/C++ pertinentes a transaction. Reconstruir GCC apenas se
os seus fontes mudarem. Executar também os exemplos locais com recompilação
forçada. Conferir a biblioteca efetivamente carregada e os arquivos .sum/.log;
reportar PASS, FAIL, UNRESOLVED e UNSUPPORTED separadamente.

## Critérios de manutenção e tarefas remanescentes

A implementação estrutural e o protocolo do proprietário foram entregues.
Ao modificar, preservar os nove contratos, leases até a limpeza completa,
coordenador único e rejeição de chamadas tardias; executar a cobertura afetada
e atualizar os algoritmos em comentário e o inventário. Não declarar STM,
isolamento, commit, rollback ou retry implementados.

Pendências de validação/manutenção, distintas de reimplementar a hash:

- Reproduzir DejaGNU C/C++ com runtime local quando as ferramentas estiverem
  disponíveis; os resultados de 383 PASS da auditoria de 2026-10-21 são históricos.
- Validar nativamente 32 bits e outros backends de mutex/targets antes de
  anunciar cobertura além do ambiente Linux x86_64 já exercitado.
- Medir contenção antes de antecipar alocação ou alterar distribuição/capacidade;
  separar custo dos logs obrigatórios e do critical da carga dos custos da hash.
- Antes de instalar metadados de política na tabela global, configurar seu
  destrutor imutável na criação: owner já configura newtm_policy_destroy.
  Não alterar a configuração após publicação; criar metadados sob demanda.
- Ao integrar contexto STM, preservar leases durante todas as referências,
  tentativas/cleanups em andamento; não substituir quiescência por count zero.
  dlclose permanece sob exclusão externa de código, sem garantia automática.

As funcionalidades de STM pendentes estão na sequência do agente de projeto;
a carga parametrizada continuará usando critical até haver isolamento funcional.

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
