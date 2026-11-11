# Agente orientador: implementação de diretivas OpenMP no GCC

Estado vigente da 6.5.3: blocos 1–8 concluídos e validados no domínio documentado. Ver [progresso consolidado](OPENMP_TRANSACTION_GRAPH_PROGRESS.md) e
[reconstrução em LTO](OPENMP_TRANSACTION_LTO_GRAPH.md). Há gerações atuais
independentes no frontend e nas fases LTO, com revogação antes de transformações.
O getter histórico permanece falso. Resta a aceitação integral da 6.6. Este estado prevalece sobre o histórico abaixo.

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


## Decisões específicas de transaction — 2026-10-29

Consultar o [registro vigente e extensões futuras](OPENMP_TRANSACTION_DECISIONS.md)
e o [agente de projeto](OPENMP_TRANSACTION_PROJECT_AGENT.md) ao trabalhar na
extensão transaction. A avaliação mais recente de F01 está no [relatório de prontidão](OPENMP_TRANSACTION_INTEGRATION_READINESS.md):
o usuário autorizou avaliar e implementar se viável, mas os bloqueios técnicos
identificados ainda impedem ativar a integração com as garantias solicitadas.
Diagnósticos/plano canônico estão no compilador; o runtime STM é exercitado por
interfaces internas e corpos explicitamente preparados.

`critical` dentro de transaction é efeito OpenMP rejeitado pelo subconjunto.
Os exemplos podem usar critical externo enquanto a ponte só rastreia; isso não
prova isolamento STM nem autoriza locks do programa em uma tentativa. Manter a
semântica própria de critical e não usá-lo como substituto da futura integração.
Extensões de linguagem, outros targets e Fortran seguem F05–F09 e precisam de
escopo/contratos próprios.

## Etapa 1 de geração de código concluída — 2026-10-29

Os [vínculos de ocorrências](OPENMP_TRANSACTION_OCCURRENCES.md) associam sites
GENERIC às instruções e operandos GIMPLE emitidos, incluindo corpos de funções
selecionadas para preparação. Consultar argumentos, índices, referências e
resultados na instrução atual; nunca executar receitas do plano diagnóstico.
Grupos conservam RMW; cópias recebem IDs novos; source-only não fornece valor
avaliado. A janela termina antes de CFG/SSA. Essa etapa não ativa a ponte STM;
descritores/kind, instrumentação, clones e epílogos continuam pendentes.

## Missão e escopo

Você é um agente de desenvolvimento trabalhando nos fontes do GCC. Implemente o
tratamento de novas diretivas `#pragma omp` de ponta a ponta: reconhecimento,
análise sintática, validação semântica, representação intermediária, geração de
código e testes. Não considere uma diretiva implementada apenas porque seu
parser aceita a entrada.

Este guia foi elaborado por inspeção da árvore local, cujo `gcc/BASE-VER` contém
`16.2.0`. Os caminhos abaixo são relativos à raiz dos fontes. Localize funções
pelo nome: números de linha mudam. O foco é C e C++; Fortran usa outro front-end
e a sintaxe `!$omp`, exigindo trabalho próprio quando incluído no escopo.

Não é necessário que `#pragma omp` comece na primeira coluna: o reconhecimento
segue as regras de diretivas do pré-processador, inclusive espaços iniciais.
Considere também `_Pragma` e as formas por atributos OpenMP suportadas nesta
árvore. Não implemente reconhecimento com busca textual de linhas.

## Fluxo identificado no código

```text
fonte C/C++: #pragma omp <diretiva> <cláusulas>
  -> init_pragma: registro no namespace omp
  -> libcpp: CPP_PRAGMA + identificador, tokens e CPP_PRAGMA_EOL
  -> parser C ou C++: contexto, diretiva, cláusulas e corpo associado
  -> análise semântica: tipos, escopos, restrições e construção de árvores
  -> gimplificação: árvores OpenMP para GIMPLE e tratamento das variáveis
  -> diagnose_omp_blocks / omplower: restrições e transformação de regiões
  -> ompexp: fluxo de controle, funções auxiliares e chamadas de execução
  -> passes posteriores e geração de código
  -> libgomp, quando a semântica exige suporte de execução
```

Esse é o caminho típico de construções executáveis com região. Diretivas
declarativas podem modificar declarações ou atributos; outras viram chamadas
diretamente. Nem toda diretiva cria um nó `OMP_*`, um código `GIMPLE_OMP_*` ou
uma nova função de runtime.

O diagrama organiza responsabilidades, não chamadas estritamente separadas:
parsing e validação semântica se intercalam. O registro acontece na inicialização
do front-end, antes do processamento das diretivas do arquivo. `_Pragma` entra
pelo pré-processador; atributos OpenMP são reconhecidos pelos parsers e
convertidos em sequências de tokens de pragma para reutilizar a análise.

### 1. Registro e pré-processamento

- `gcc/c-family/c-pragma.h`: `enum pragma_kind`, identificadores
  `PRAGMA_OMP_*` e enumeração das cláusulas `PRAGMA_OMP_CLAUSE_*`.
- `gcc/c-family/c-pragma.cc`: tabelas `omp_pragmas`, `omp_pragmas_simd` e
  função `init_pragma`.
- `libcpp/directives.cc`: `cpp_register_deferred_pragma` registra o pragma
  adiado; o processamento do pragma entrega `CPP_PRAGMA` com seu identificador.
  O conteúdo é analisado posteriormente pelo front-end.

`init_pragma` registra `omp_pragmas` quando `flag_openmp` está ativo e
`omp_pragmas_simd` quando `flag_openmp || flag_openmp_simd`. Os registros
OpenMP permitem expansão de macros nos argumentos e no reconhecimento do nome.
O modo `directives_only` tem tratamento próprio no registro.

Ao acrescentar um identificador, mantenha-o dentro do intervalo
`PRAGMA_OMP__START_` a `PRAGMA_OMP__LAST_`, atualizando os aliases se mudar a
primeira ou a última entrada. Há consumidores que detectam OpenMP por esse
intervalo, inclusive verificações de código interveniente em laços. Preserve
também o limite de armazenamento dos identificadores: o token do front-end C
reserva oito bits, como documentado em `c_register_pragma_1`.

As tabelas registram a primeira palavra: `parallel for simd`, por exemplo,
começa com `PRAGMA_OMP_PARALLEL`; as palavras seguintes são consumidas pelo
parser. Uma nova forma de `target` ou `declare` pode reutilizar um identificador
existente. A presença em `omp_pragmas_simd` não significa que toda a construção
tenha efeito com `-fopenmp-simd`: os parsers também controlam esse comportamento.

### 2. Despacho e análise sintática

| Responsabilidade | C | C++ |
| --- | --- | --- |
| Arquivo principal | `gcc/c/c-parser.cc` | `gcc/cp/parser.cc` |
| Despacho por contexto | `c_parser_pragma` | `cp_parser_pragma` |
| Construções com corpo, em geral | `c_parser_omp_construct` | `cp_parser_omp_construct` |
| Identificação de cláusulas | `c_parser_omp_clause_name` | `cp_parser_omp_clause_name` |
| Lista e máscara de cláusulas | `c_parser_omp_all_clauses` | `cp_parser_omp_all_clauses` |
| Exemplo de região paralela | `c_parser_omp_parallel` | `cp_parser_omp_parallel` |

O despacho distingue contextos como nível de arquivo, bloco e posição de
instrução. Diretivas independentes, como `barrier`, têm caminhos específicos;
não force todas as diretivas pelo dispatcher de construções com corpo.

Uma rotina de parser deve consumir a sintaxe da diretiva até o fim do pragma,
validar as cláusulas permitidas por sua máscara, analisar o corpo associado
quando existir e preservar a localização para diagnósticos. Use os mecanismos
existentes de recuperação até `CPP_PRAGMA_EOL`; uma entrada inválida não deve
corromper a análise das próximas instruções.

As máscaras `OMP_*_CLAUSE_MASK` são definidas nos respectivos arquivos de parser.
O tipo compartilhado `omp_clause_mask` e `OMP_CLAUSE_MASK_1` estão em
`gcc/c-family/c-common.h`. Alterar somente a máscara de C não habilita a cláusula
em C++; altere e valide os dois caminhos.

### 3. Semântica, classificação e cláusulas

- `gcc/c/c-typeck.cc`: `c_finish_omp_clauses`, `c_begin_omp_parallel`,
  `c_finish_omp_parallel` e outros finalizadores C.
- `gcc/cp/semantics.cc`: `finish_omp_clauses`, `finish_omp_parallel` e
  finalizadores C++; tratam particularidades de tipos e objetos C++.
- `gcc/cp/pt.cc`: substituição e instanciação de templates, incluindo
  `tsubst_omp_clauses` e tratamento de árvores OpenMP.
- `gcc/c-family/c-omp.cc`: utilitários compartilhados, finalizadores comuns,
  `c_omp_split_clauses` e tabela `c_omp_directives`.
- `gcc/c-family/c-common.h`: interfaces compartilhadas e classificações.

`c_omp_directives` descreve diretivas individuais, inclusive nomes de até três
palavras, identificador, categoria e o campo booleano `simd`. Não é uma lista
de todas as construções combinadas: `c_omp_categorize_directive` identifica a
diretiva inicial e os parsers analisam a combinação restante. As categorias
incluem declarativa, informativa, independente, construção, utilitária e meta.
Sua ordenação é
explícita: ordem alfabética pela primeira palavra; entradas com segunda/terceira
palavra precedem as entradas correspondentes com valores nulos. Revise essa
tabela e seus consumidores para atributos, metadiretivas e cláusulas
`absent`/`contains` quando adicionar uma forma de diretiva. Defina `simd`
coerentemente com a semântica pretendida: consumidores de atributos usam esse
campo para decidir o processamento quando `flag_openmp` está desativado. Ele
é distinto da escolha entre as tabelas de registro de pragmas.

Construções combinadas usam máscaras acumuladas e repartição de cláusulas por
`c_omp_split_clauses`, com índices `C_OMP_CLAUSE_SPLIT_*`. Não basta aceitar uma
cláusula: ela precisa alcançar a região correta e manter sua semântica nas
combinações. Preserve as marcações de construções combinadas usadas nas fases
seguintes.

### 4. Árvores, GIMPLE e passes

| Camada | Arquivos e pontos de partida |
| --- | --- |
| Códigos de árvores | `gcc/tree.def` |
| Cláusulas e estruturas de árvores | `gcc/tree-core.h`, `gcc/tree.h`, `gcc/tree.cc` |
| Códigos e estruturas GIMPLE | `gcc/gimple.def`, `gcc/gimple.h`, `gcc/gimple.cc` |
| Gimplificação | `gcc/gimplify.cc`: `gimplify_omp_parallel`, varredura e ajuste de cláusulas |
| Contextos e transformação de regiões | `gcc/omp-low.cc`: `scan_omp_*`, `lower_omp_*`, `execute_lower_omp` |
| Expansão de regiões | `gcc/omp-expand.cc`: `expand_omp`, `expand_omp_taskreg`, `execute_expand_omp` |
| Ordem dos passes | `gcc/passes.def` |
| Seleção de variantes e dispositivos | `gcc/omp-general.cc`, `gcc/omp-offload.cc` |

No exemplo `parallel`, `c_finish_omp_parallel` cria `OMP_PARALLEL` com corpo,
cláusulas e localização. `gimplify_omp_parallel` examina as cláusulas, gimplifica
o corpo, ajusta compartilhamento de dados e cria `GIMPLE_OMP_PARALLEL` por
`gimple_build_omp_parallel`. `scan_omp_parallel` e `lower_omp_taskreg` participam
da preparação de contextos, dados e função auxiliar; `expand_omp_taskreg` e o
código de chamadas paralelas materializam a execução.

Em `gcc/passes.def`, `pass_diagnose_omp_blocks` precede `pass_lower_omp`;
`pass_lower_cf`, o tratamento de exceções e `pass_build_cfg` aparecem antes de
`pass_expand_omp`. A expansão trabalha sobre o grafo de fluxo de controle.
Existe ainda `pass_expand_omp_ssa` (`ompexpssa`) em outros pontos do pipeline;
avalie esse caminho quando a transformação puder deixar ou gerar OpenMP nessa
fase. Não é necessário criar um passe novo para cada diretiva.

As restrições não se concentram nos finalizadores: em `gcc/omp-low.cc`,
`check_omp_nesting_restrictions` é chamada durante a varredura das instruções
OpenMP; `diagnose_omp_structured_block_errors` verifica transferências de
controle ilegais. Inclua a nova construção nas verificações pertinentes, além
de implementar sua transformação.

Ao criar uma representação nova, audite também impressão, walkers, cópia,
operandos, verificação e serialização/LTO. Pontos usuais incluem
`gcc/tree-pretty-print.cc`, `gcc/gimple-pretty-print.cc`, `gcc/tree-inline.cc`,
`gcc/tree-cfg.cc` e `gcc/gimple-walk.cc`; localize os casos da representação
análoga em toda a árvore para decidir quais realmente precisam mudar.
Para LTO, examine também `gcc/tree-streamer-in.cc`, `gcc/tree-streamer-out.cc`,
`gcc/gimple-streamer-in.cc` e `gcc/gimple-streamer-out.cc` conforme os dados
que sobrevivem até a serialização. Não presuma que toda árvore do front-end
precise de serialização própria: verifique quando ela é eliminada ou convertida.

Novas cláusulas exigem coerência entre a enumeração em `tree-core.h`, acessores
em `tree.h` e tabelas `omp_clause_num_ops`/`omp_clause_code_name` em `tree.cc`,
além dos consumidores pertinentes. Não suponha a existência de um arquivo
`omp-clause.def`: esta árvore mantém essas definições nos arquivos mencionados.

### 5. Runtime e exemplo de caminho curto

`gcc/omp-builtins.def` declara os builtins usados para chamadas `GOMP_*`.
Uma região paralela comum pode usar `GOMP_parallel`, implementada em
`libgomp/parallel.c` e exportada em `libgomp/libgomp.map`. Combinações e outras
semânticas podem selecionar funções diferentes.

Como contraste entre caminhos reais, `c_finish_omp_barrier`, em
`gcc/c-family/c-omp.cc`, cria diretamente uma chamada ao builtin
`BUILT_IN_GOMP_BARRIER`. Portanto, não use `parallel` como modelo universal.

Se precisar de API interna nova, examine `libgomp/libgomp_g.h`,
`libgomp/libgomp.h`, a implementação pertinente e o mapa de símbolos. Preserve
ABI, assinaturas e regras de versionamento existentes. Offload exige avaliação
própria das bibliotecas de dispositivo, plugins e mapeamento de dados; executar
corretamente no host não prova suporte no dispositivo.

## Procedimento obrigatório para uma nova diretiva

1. **Defina a semântica antes da edição.** Registre nome, versão OpenMP ou
   natureza de extensão, gramática, categoria, corpo associado, cláusulas,
   contextos legais, combinações, sincronização e regras de dados. Se depender
   de texto normativo ausente, consulte a especificação oficial pertinente e
   documente a versão e a referência; não infira regras de memória só pelo nome.
2. **Escolha uma diretiva análoga.** Rastreie seu caminho completo. Decida se a
   nova forma reutiliza nós e runtime existentes, altera declarações, vira uma
   chamada ou necessita de uma representação nova.
3. **Registre somente o necessário.** Adicione `PRAGMA_OMP_*` e uma entrada de
   registro se houver uma primeira palavra nova, preservando o intervalo e
   os limites dos identificadores. Uma nova forma de palavra inicial existente
   normalmente exige alteração do seu parser, sem novo registro. Decida explicitamente o
   comportamento com `-fopenmp`, `-fopenmp-simd` e sem essas opções. Atualize a
   classificação compartilhada, incluindo `simd`, respeitando a ordenação.
4. **Implemente C e C++.** Acrescente despacho no contexto correto, parser,
   máscara, diagnóstico e recuperação. Para uma nova cláusula, implemente o
   reconhecimento e análise nos dois front-ends e a representação compartilhada.
5. **Complete a semântica.** Valide duplicatas, exclusões, tipos, escopos,
   referências, nesting e structured blocks conforme aplicável. Verifique
   templates, expressões dependentes, construtores e destrutores em C++.
6. **Garanta o percurso intermediário.** Implemente ou reutilize gimplificação,
   tratamento de variáveis, verificações de aninhamento e blocos estruturados,
   lowering e expansão conforme a representação escolhida. Audite consumidores de novos
   códigos; não deixe nós sem tratamento chegando a passes posteriores.
7. **Complete a execução se necessária.** Reutilize builtins existentes ou
   implemente a interface e runtime novos. Trate cancelamento, exceções,
   barreiras, memória e offload conforme a semântica escolhida.
8. **Teste e documente.** Adicione testes relevantes, execute os subconjuntos
   adequados e descreva limitações. Atualize documentação aplicável, como
   `libgomp/libgomp.texi` e manuais em `gcc/doc`, se o comportamento exigir.

## Contratos das primitivas acrescentadas por uma extensão

Para cada nova primitiva ou callback, documentar argumentos/unidades, contexto e
proprietário, vida útil, limites/aliasing, pré-condições verificáveis, efeitos em
sucesso e falha, saída inicializada ou preservada, referências consumidas/retidas,
locks/ordenação e ponto de invocação. Distinguir violação de contrato de falha
recuperável; não prometer validação de ponteiros pendentes. Toda saída deve ter
propriedade de limpeza definida e não deixar lock retido inadvertidamente.

Na extensão transaction, consultar o inventário em
[OPENMP_TRANSACTION_PRIMITIVES.md](OPENMP_TRANSACTION_PRIMITIVES.md) e
os contratos especializados de hash, acesso e abort ali indicados. Funções
existentes usadas como exemplo não viram novas APIs; protótipos ilustrativos
não constituem ABI aprovada. Atualizar o contrato quando um esqueleto passar a
implementar operações reais, sem relatar logs como prova de funcionalidade.

## Validação e critérios de conclusão

Use DejaGNU e os diretórios existentes:

- `gcc/testsuite/c-c++-common/gomp/`: casos compartilhados C/C++.
- `gcc/testsuite/gcc.dg/gomp/`: particularidades de C.
- `gcc/testsuite/g++.dg/gomp/`: C++, especialmente templates e objetos.
- `libgomp/testsuite/libgomp.c-c++-common/`, `libgomp.c/` e `libgomp.c++/`:
  execução e sincronização.

Os drivers `gcc.dg/gomp/gomp.exp` e `g++.dg/gomp/gomp.exp` também executam casos
compartilhados. Siga as convenções locais de `dg-do`, `dg-options`, `dg-error`,
`dg-warning`, `scan-tree-dump` e requisitos de target.

Cubra a sintaxe válida mínima e as cláusulas relevantes; entradas inválidas e
recuperação; contexto e combinações; modo SIMD e opções desabilitadas; macros e
formas alternativas aplicáveis; templates quando pertinentes. Testes de execução
devem verificar resultados, participação de threads e sincronização com
assertivas determinísticas, evitando depender de tempos de execução. Teste LTO
ou dispositivo quando a mudança alcançar essas camadas.

Execute comandos de build e testes no **diretório de construção**, separado
dos fontes. Exemplos, a ajustar à configuração e ao nome dos testes criados:

```sh
make -j4 all-gcc
make -j4 check-gcc RUNTESTFLAGS='gomp.exp=nova-diretiva-*.c'
make -j4 check-g++ RUNTESTFLAGS='gomp.exp=nova-diretiva-*.C'
make -j4 check-g++ RUNTESTFLAGS='gomp.exp=nova-diretiva-*.c'
make -j4 check-target-libgomp RUNTESTFLAGS='c.exp=nova-diretiva-*.c'
make -j4 check-target-libgomp RUNTESTFLAGS='c++.exp=nova-diretiva-*.C'
make -j4 check-target-libgomp RUNTESTFLAGS='c++.exp=nova-diretiva-*.c'
```

Esses exemplos partem da raiz do diretório de construção; use somente os
seletores correspondentes aos arquivos realmente criados. Os comandos C++ com
`.c` selecionam os testes compartilhados, que seriam excluídos por um filtro
restrito a `.C`. Os drivers C/C++ da `libgomp` também incluem seu diretório
compartilhado. Verifique que C e C++ estão habilitados e que DejaGNU está
disponível. O driver C++ da `libgomp` exige a `libstdc++` apropriada ao build;
na sua ausência ele pode deixar de executar os testes.

O build de `all-gcc` sozinho não constrói um runtime modificado; construa também
os componentes de target necessários antes dos testes de execução. Confira os
arquivos `.sum` e `.log`, distinguindo `PASS`, `FAIL`, `UNRESOLVED`, `UNSUPPORTED`
e resultados esperados. Um teste não executado não é evidência de sucesso.

Para rastrear um caso com o compilador **construído a partir desta árvore**, use
as opções `-fopenmp -fdump-tree-original -fdump-tree-gimple
-fdump-tree-omplower -fdump-tree-ompexp`. Para investigar o caminho SIMD,
substitua `-fopenmp` por `-fopenmp-simd`. Inspecione árvores, captura de dados,
função auxiliar e chamadas resultantes; nem toda diretiva produzirá regiões
visíveis em todos os dumps.

A tarefa está concluída quando reconhecimento, semântica e execução prevista
estão coerentes; os testes pertinentes passam; modos alternativos têm
comportamento definido; e o relatório lista arquivos alterados, testes
efetivamente executados e limitações remanescentes. Não apresente aceitação
sintática como suporte completo.

## Buscas úteis para começar

```sh
rg -n 'omp_pragmas|init_pragma' gcc/c-family/c-pragma.cc
rg -n 'PRAGMA_OMP_PARALLEL|c_parser_omp_parallel' gcc/c/c-parser.cc
rg -n 'cp_parser_omp_parallel|cp_parser_omp_construct' gcc/cp/parser.cc
rg -n 'c_omp_directives|c_omp_split_clauses' gcc/c-family/c-omp.cc
rg -n 'OMP_PARALLEL|GIMPLE_OMP_PARALLEL' gcc --glob '*.{cc,h,def}'
rg -n 'GOMP_parallel' gcc/omp-builtins.def gcc/omp-expand.cc libgomp
```

Troque `parallel` e seus identificadores pela diretiva análoga escolhida. Leia
as funções encontradas e seus chamadores antes de editar.

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
