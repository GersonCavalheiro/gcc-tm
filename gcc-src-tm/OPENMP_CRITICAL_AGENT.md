# Agente especializado no tratamento de `critical` no GCC

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

## Missão

Você é um agente de análise e manutenção da diretiva OpenMP `critical` nesta
árvore GCC 16.2.0. Use [OPENMP_AGENT.md](OPENMP_AGENT.md) para o funcionamento
geral das diretivas. Este documento especializa o percurso, as representações,
os arquivos envolvidos, as restrições e os testes de `critical`.

Os caminhos são relativos à raiz dos fontes. Localize as funções pelo nome e
leia seus chamadores antes de propor alterações. Este documento descreve o
código local, inclusive limitações observadas; não substitui a especificação
OpenMP. Na tarefa de elaboração deste agente, somente este arquivo é criado.
Não altere outros arquivos sem uma instrução posterior que autorize esse trabalho.

## Sintaxe e comportamento observável

```c
#pragma omp critical
{ /* corpo protegido pelo lock sem nome */ }

#pragma omp critical (contador) hint (0)
{ /* corpo protegido pelo lock associado ao nome contador */ }
```

Os parsers também aceitam vírgula entre `(nome)` e `hint`, como mostrado em
`gcc/testsuite/c-c++-common/gomp/critical-1.c`. Siga o tratamento de bloco
estruturado existente: vários testes desta árvore usam uma instrução única
como corpo, portanto não presuma que o parser exige sempre chaves.

O nome é armazenado como identificador, sem resolução como variável ou função
C/C++; é possível usar o mesmo texto de uma função. Regiões sem nome usam um
lock comum na instância do runtime. Regiões com o mesmo nome compartilham o
símbolo de lock correspondente, inclusive por meio da ligação de símbolos
comuns entre unidades de tradução. Nomes diferentes produzem locks distintos.
`critical` serializa a execução do corpo; não distribui trabalho, não cria uma
equipe de threads e não insere uma barreira coletiva ao sair.

O registro textual de `critical` está em `omp_pragmas`, habilitado por
`-fopenmp`, e não em `omp_pragmas_simd`. Na tabela compartilhada de diretivas,
sua categoria é `C_OMP_DIR_CONSTRUCT` e `simd` é `false`. Não atribua a
`-fopenmp-simd` o mesmo efeito de sincronização de `-fopenmp`.

## Percurso principal

```text
init_pragma / libcpp
  PRAGMA_OMP_CRITICAL + tokens da diretiva
    -> c_parser_pragma / cp_parser_pragma
    -> c_parser_omp_construct / cp_parser_omp_construct
    -> c_parser_omp_critical / cp_parser_omp_critical
    -> cláusula hint + corpo estruturado
    -> c_finish_omp_critical (compartilhado por C e C++)
  OMP_CRITICAL: corpo, cláusulas, nome
    -> gimplify_expr: caso OMP_CRITICAL
    -> gimple_build_omp_critical
  GIMPLE_OMP_CRITICAL / gomp_critical
    -> diagnose_omp_structured_block_errors
    -> scan_omp_1_stmt / check_omp_nesting_restrictions
    -> lower_omp_critical: símbolo de lock + chamadas start/end
    -> lowering de controle/exceções e construção do CFG
    -> expand_omp_synch: remoção dos marcadores da região
  chamadas GOMP_critical_* + corpo
    -> libgomp/critical.c
    -> gomp_mutex_lock / gomp_mutex_unlock do target
```

Não existe uma função especializada `gimplify_omp_critical` nesta árvore: a
conversão está em um caso do dispatcher de gimplificação. As chamadas de runtime
já são inseridas em `omplower`, antes de `ompexp`.

## Registro, despacho e formas alternativas

| Arquivo | Conteúdo pertinente |
| --- | --- |
| `gcc/c-family/c-pragma.h` | `PRAGMA_OMP_CRITICAL`, `PRAGMA_OMP_CLAUSE_HINT` e limites do intervalo OpenMP. |
| `gcc/c-family/c-pragma.cc` | Entrada `critical` em `omp_pragmas`; `init_pragma` registra o pragma adiado no namespace `omp`. |
| `libcpp/directives.cc` | `cpp_register_deferred_pragma` e processamento que entrega `CPP_PRAGMA`; o front-end recebe o conteúdo até `CPP_PRAGMA_EOL`. Também participa do caminho de `_Pragma`. |
| `gcc/c-family/c-omp.cc` | Entrada de `critical` em `c_omp_directives`, categorização por `c_omp_categorize_directive` e finalizador compartilhado. |
| `gcc/c-family/c-common.h` | Interface de `c_finish_omp_critical`, tipo de máscara e estruturas de classificação. |
| `gcc/c/c-parser.cc` | Contexto C, despacho, nome opcional, cláusulas, corpo e recuperação de erros; consumidores de atributos e metadiretivas. |
| `gcc/cp/parser.cc` | Caminho equivalente C++, com identificação e recuperação próprias da linguagem. |

`c_parser_omp_critical` consome o nome opcional entre parênteses, a vírgula
opcional, as cláusulas por `c_parser_omp_all_clauses` e o corpo por
`c_parser_omp_structured_block`. Chama `c_finish_omp_critical` com a localização
recebida do dispatcher.

`cp_parser_omp_critical` usa `cp_parser_identifier`, `matching_parens`,
`cp_parser_omp_all_clauses` e `cp_parser_omp_structured_block`; chama o mesmo
finalizador com `input_location`. Não há necessidade de inventar um
`finish_omp_critical` separado em `gcc/cp/semantics.cc`.

A tabela compartilhada também é consumida por atributos OpenMP,
metadiretivas e nomes em cláusulas `absent`/`contains`. A classificação de
`critical` determina que uma variante de metadiretiva necessita de corpo.
Ao mudar a sintaxe ou categoria, verifique essas entradas alternativas, além
do pragma textual.

## Cláusula `hint` e validação semântica

As duas definições de `OMP_CRITICAL_CLAUSE_MASK`, nos parsers C e C++, permitem
somente `PRAGMA_OMP_CLAUSE_HINT`. `private`, `shared`, `nowait` e outras cláusulas
não fazem parte dessa máscara de `critical`.

| Ponto | Tratamento observado |
| --- | --- |
| `c_parser_omp_clause_hint`, em `gcc/c/c-parser.cc` | Analisa a expressão, faz conversão de valor e folding; exige tipo integral, `INTEGER_CST` e valor não negativo; verifica duplicata. |
| `cp_parser_omp_clause_hint`, em `gcc/cp/parser.cc` | Analisa e dobra expressão não dependente; valida valores não dependentes como integrais, representáveis por `tree_fits_shwi_p` e não negativos; admite dependência para templates e verifica duplicata. |
| `c_finish_omp_clauses`, em `gcc/c/c-typeck.cc` | O caso `OMP_CLAUSE_HINT` não adiciona a validação específica já realizada pelo parser C. |
| `finish_omp_clauses`, em `gcc/cp/semantics.cc` | Verifica expressão integral e, fora da definição de template, exige expressão inteira constante; remove cláusulas com erros. |
| `c_finish_omp_critical`, em `gcc/c-family/c-omp.cc` | Rejeita nome ausente quando a expressão de `hint` é reconhecida como constante não zero; aceita a exceção de valor zero. Cria e acrescenta o nó da construção. |
| `gcc/cp/pt.cc` | `tsubst_omp_clauses` substitui expressões de cláusulas; o caso `OMP_CRITICAL` instancia corpo/cláusulas, copia a árvore e tem diagnóstico adicional para hint não zero. |

Há uma particularidade local que deve ser preservada como observação ao analisar
templates: no caso `OMP_CRITICAL` de `gcc/cp/pt.cc`, o teste de hint não zero
**não consulta `OMP_CRITICAL_NAME`** antes de emitir o diagnóstico que exige um
nome. `gcc/testsuite/g++.dg/gomp/critical-3.C` espera esse erro também para uma
instanciação nomeada. Isso descreve o código e os testes presentes; não conclua
que a mensagem representa uma regra geral correta para toda região nomeada.
Uma investigação ou correção desse comportamento exige tarefa própria.

O suporte de `hint` não implica validação normativa completa: comentários em
`libgomp/testsuite/libgomp.c-c++-common/critical-hint-1.c` apontam valores aceitos
mas inválidos, incluindo hints diferentes para o mesmo nome. Além disso,
`lower_omp_critical` não passa a expressão de hint às funções `GOMP_critical_*`.
Nesta implementação, as chamadas não selecionam uma estratégia de lock em
função desse valor.

## Representações de árvores e GIMPLE

| Arquivo | Conteúdo |
| --- | --- |
| `gcc/tree.def` | `OMP_CRITICAL`, classe `tcc_statement`, com três operandos. |
| `gcc/tree.h` | `OMP_CRITICAL_BODY`: operando 0; `OMP_CRITICAL_CLAUSES`: operando 1; `OMP_CRITICAL_NAME`: operando 2; acessor `OMP_CLAUSE_HINT_EXPR`. |
| `gcc/tree-core.h` | Código de cláusula `OMP_CLAUSE_HINT` e estruturas compartilhadas. |
| `gcc/tree.cc` | Quantidade de operandos e nome textual da cláusula nas tabelas OpenMP; hint possui um operando. |
| `gcc/gimple.def` | Código `GIMPLE_OMP_CRITICAL` com layout `GSS_OMP_CRITICAL`. |
| `gcc/gsstruct.def` | Associação do layout à estrutura `gomp_critical`. |
| `gcc/gimple.h` | `gomp_critical` deriva de `gimple_statement_omp`; mantém corpo pela base e campos `clauses` e `name`; getters, setters e ponteiros para esses campos. |
| `gcc/gimple.cc` | `gimple_build_omp_critical` aloca a instrução, define corpo/nome/cláusulas; a cópia GIMPLE também trata nome e cláusulas. |
| `gcc/gimplify.cc` | Gimplifica o corpo, preserva o estado `in_omp_construct`, examina/ajusta cláusulas com `ORT_WORKSHARE` e cria a instrução GIMPLE. |

`c_finish_omp_critical` atribui `void_type_node` e a localização à árvore e usa
`add_stmt`. O nome ausente é `NULL_TREE`. Não substitua o nome por uma referência
a uma variável do usuário: o símbolo de sincronização é criado mais tarde.

## Restrições, lowering e expansão

O arquivo central é `gcc/omp-low.cc`:

1. `diagnose_omp_structured_block_errors` trata transferências ilegais para
   dentro ou para fora de blocos estruturados; os casos de `critical` participam
   dos walkers dessa verificação.
2. `scan_omp_1_stmt` chama as verificações de restrições, cria um `omp_context`
   para `GIMPLE_OMP_CRITICAL` e percorre seu corpo.
3. `check_omp_nesting_restrictions` compara o nome da região com os nomes de
   regiões críticas externas e rejeita igualdade; `NULL_TREE == NULL_TREE`
   também cobre duas regiões sem nome. A verificação depende dos contextos
   visíveis nessa análise, não prova ausência de deadlock interprocedural.
   Outros casos dessa função incluem `critical` nas restrições de regiões
   worksharing, barreiras, ordered e outras construções aninhadas.
4. `lower_omp_1` encaminha a construção a `lower_omp_critical`.
5. `lower_omp_critical` gera aquisição, corpo e liberação; mantém os marcadores
   necessários para o restante do pipeline.

### Símbolo de região nomeada

`critical_name_mutexes` é um mapa gerenciado pelo GC que associa o identificador
ao `VAR_DECL` do lock. Para um nome ainda não encontrado, o lowering cria uma
variável de tipo `ptr_type_node` com o nome `.gomp_critical_user_<nome>` e marca
`TREE_PUBLIC`, `TREE_STATIC`, `DECL_COMMON`, `DECL_ARTIFICIAL` e
`DECL_IGNORED_P`. Finaliza a declaração no varpool e a reutiliza nas ocorrências
seguintes. O endereço dessa variável é passado às chamadas nomeadas.

Se a função atual for offloadable, ou existir região externa offloaded, o símbolo
também é marcado como offloadable. Isso permite sua participação no fluxo de
dispositivo; não significa que host e dispositivo compartilhem o mesmo mutex
físico. Consulte `gcc/omp-offload.cc` para o processamento posterior dos símbolos.

### Sequência gerada

Sem nome, o lowering seleciona `BUILT_IN_GOMP_CRITICAL_START` e
`BUILT_IN_GOMP_CRITICAL_END`, sem argumentos. Com nome, seleciona
`BUILT_IN_GOMP_CRITICAL_NAME_START` e `BUILT_IN_GOMP_CRITICAL_NAME_END`, passando
o endereço do símbolo comum.

Um `GIMPLE_BIND` envolve o marcador `GIMPLE_OMP_CRITICAL`, a aquisição, o corpo
transformado recursivamente, a liberação e `gimple_build_omp_return(true)`.
O corpo do marcador original é retirado depois de transferido para a sequência.
O retorno com `true` não deve ser confundido com uma cláusula `nowait`: não há
barreira coletiva de saída de `critical`.

`maybe_catch_exception` protege o corpo quando exceções estão habilitadas,
criando `GIMPLE_TRY_CATCH` com `GIMPLE_EH_MUST_NOT_THROW`; usa o hook de linguagem
ou trap. Não descreva isso como permissão para propagar exceções para fora da
região ou como simples liberação por um `finally`.

`gcc/passes.def` coloca diagnóstico e `omplower` antes do lowering de controle,
exceções e construção do CFG; `ompexp` vem depois. Em `gcc/omp-expand.cc`,
`expand_omp` despacha `GIMPLE_OMP_CRITICAL` para `expand_omp_synch`, que remove
os marcadores de entrada e saída e ajusta as arestas para fallthrough. As
chamadas de sincronização já existentes permanecem.

## Interface de execução e implementação dos locks

| Arquivo | Responsabilidade |
| --- | --- |
| `gcc/omp-builtins.def` | Quatro builtins critical; sem nome usam `BT_FN_VOID`, nomeados usam `BT_FN_VOID_PTRPTR`; atributos `ATTR_NOTHROW_LEAF_LIST`. |
| `gcc/builtin-types.def` | Tipos das assinaturas usadas pelos builtins. |
| `libgomp/libgomp_g.h` | Protótipos `GOMP_critical_start/end(void)` e `GOMP_critical_name_start/end(void **)`. |
| `libgomp/libgomp.map` | Exporta as quatro funções na interface versionada da biblioteca. |
| `libgomp/critical.c` | Lock sem nome e seleção/criação dos locks nomeados; inicialização condicional. |
| `libgomp/libgomp.h` | Tipos/interfaces internos, inclusão de `mutex.h` e suporte usado pelo runtime. |
| `libgomp/configure.tgt` | Escolhe caminhos de configuração do target que fornecem o mutex. |
| `libgomp/config/linux/mutex.h`, `libgomp/config/linux/mutex.c` | Mutex Linux: atomics no caminho rápido e futex no caminho lento. |
| `libgomp/config/posix/mutex.h`, `libgomp/config/posix/mutex.c` | Alternativa POSIX; verificar tamanho, alinhamento e inicialização próprios. |
| `libgomp/config/accel/mutex.h`, `libgomp/config/accel/mutex.c` | Implementação compartilhada de aceleradores, com espera ativa no header consultado. |
| `libgomp/config/rtems/mutex.h`, `libgomp/config/rtems/mutex.c` | Variante de mutex para RTEMS. |
| `libgomp/omp.h.in` | Constantes públicas `omp_sync_hint_*` usadas nos programas e testes. |

Em `libgomp/critical.c`, `GOMP_critical_start` executa uma fence
`MEMMODEL_RELEASE` e adquire `default_lock`; `GOMP_critical_end` libera esse
mutex. No caminho Linux, a aquisição usa semântica acquire e a liberação,
release. Analise fence e mutex conjuntamente ao discutir visibilidade de dados.

Para locks nomeados, `GOMP_critical_name_start` tem dois caminhos:

- Se `GOMP_MUTEX_INIT_0` for verdadeiro e tamanho/alinhamento do mutex couberem
  no armazenamento de um ponteiro, usa diretamente o espaço apontado por `pptr`.
- Caso contrário, lê o ponteiro armazenado; se estiver nulo, aloca e inicializa
  o mutex. Com `HAVE_SYNC_BUILTINS`, publica via compare-and-swap e destrói/libera
  uma alocação concorrente perdedora. Sem essa opção, protege a criação com
  `create_lock_lock`, executa a sincronização e publica o ponteiro.

Depois adquire o mutex escolhido. `GOMP_critical_name_end` repete a seleção da
forma de armazenamento e libera o mutex. Não afirme que toda região nomeada
sempre aloca memória ou que o símbolo sempre contém um ponteiro para heap.
Se inicialização por zero não for suficiente, `initialize_critical`, como
constructor, inicializa os locks estáticos pertinentes.

`libgomp/libgomp.texi`, seção “Implementing CRITICAL construct”, oferece contexto
histórico da interface, mas descreve a forma nomeada em termos de
`omp_set_lock`/`omp_unset_lock` e `omp_lock_t`. Para a interface efetivamente
gerada nesta árvore, prevalecem `lower_omp_critical`, os builtins e `critical.c`.

## Pré e pós-condições das primitivas de execução existentes

Contratos descritivos de `libgomp/critical.c`, sem criação de novas APIs ou
mudança da semântica. A inicialização do runtime/locks já deve estar concluída.

| Serviço | Pré-condições | Pós-condições no retorno normal |
| --- | --- | --- |
| GOMP_critical_start | Execução não possui o lock sem nome; evitar aquisição recursiva e ordem de locks incompatível. | Lock sem nome adquirido por essa execução; outras aquisições do mesmo lock são excluídas até end; a chamada pode bloquear. |
| GOMP_critical_end | A execução possui o lock obtido por start ainda não encerrado; exatamente um end correspondente. | Lock liberado, publicando os efeitos protegidos conforme a ordenação do mutex; não há barreira coletiva. |
| GOMP_critical_name_start(pptr) | pptr válido, gravável, alinhado e compartilhado para a identidade do nome; slot inicialmente zero ou previamente inicializado pelo runtime, vivo durante todo o uso; não possui o mesmo lock. | Lock nomeado criado/publicado se necessário e adquirido; mesma identidade compartilha exclusão. O slot não pode ser alterado pelo programa. |
| GOMP_critical_name_end(pptr) | Mesmo slot válido e lock possuído pela execução após name_start; nenhuma alteração/reutilização do slot. | Libera o lock correspondente; não destrói slot nem lock alocado e não constitui commit de transaction. |

As funções são void e não devolvem status recuperável; falta de memória no
caminho de criação segue o erro fatal da libgomp. Ponteiros inválidos, end sem
start e dupla liberação são violações; não garantir diagnóstico. Os mutexes
fornecem ordenação de memória, não rollback de efeitos. Esses contratos não
concedem suporte implícito a critical dentro de uma STM com repetição.

## Consumidores auxiliares da representação

Esses arquivos não são parsers da diretiva, mas precisam ser considerados ao
mudar a representação ou seu significado:

| Arquivo | Relação com `critical` |
| --- | --- |
| `gcc/tree-pretty-print.cc` | Imprime árvore, nome, cláusulas e corpo, inclusive hint. |
| `gcc/gimple-pretty-print.cc` | `dump_gimple_omp_critical` e despacho do dump GIMPLE. |
| `gcc/gimple-walk.cc` | Percorre nome/cláusulas e corpo da instrução OpenMP. |
| `gcc/tree-inline.cc` | Copia regiões GIMPLE e integra a construção aos tratamentos de inlining. |
| `gcc/tree-nested.cc` | Percorre o corpo na conversão de chamadas de funções aninhadas. |
| `gcc/gimple-low.cc` | Reconhece o marcador no lowering geral de controle. |
| `gcc/tree-ssa-alias.cc` | Classifica os quatro builtins como operações de sincronização relevantes à análise de memória. Isso não significa barreira coletiva OpenMP. |
| `gcc/cp/module.cc` | Inclui `OMP_CRITICAL` nos tratamentos de árvores do sistema de módulos C++. |
| `gcc/cp/constexpr.cc` | Considera a árvore OpenMP nos caminhos de avaliação/validade de expressões constantes. |
| `gcc/rust/backend/rust-constexpr.cc` | Possui caso para a árvore compartilhada; sua presença não demonstra um parser Rust de `#pragma omp critical`. |

Se a mudança alcançar dados que sobrevivem até LTO, audite os streamers
indicados em `OPENMP_AGENT.md`. Não presuma serialização especializada de uma
região que normalmente já foi expandida antes dessa etapa.

## Front-end Fortran: caminho adicional

Para uma mudança transversal de `critical`, verifique também Fortran; sua
entrada é `!$omp critical [(nome)] [hint(...)]` e `!$omp end critical`.

- `gcc/fortran/gfortran.h`: estados/códigos `ST_OMP_CRITICAL`,
  `ST_OMP_END_CRITICAL`, `EXEC_OMP_CRITICAL` e armazenamento de `critical_name`.
- `gcc/fortran/openmp.cc`: tabela de diretivas, `gfc_match_omp_critical`,
  `gfc_match_omp_end_critical` e resolução de cláusulas, incluindo diagnóstico de
  hint não zero sem nome.
- `gcc/fortran/parse.cc`: despacho e construção do bloco OpenMP, incluindo a
  correspondência da diretiva final.
- `gcc/fortran/trans-openmp.cc`: `gfc_trans_omp_critical` constrói
  `OMP_CRITICAL`, convertendo nome, corpo e cláusulas, e entra no pipeline comum.
- `gcc/fortran/dump-parse-tree.cc`: impressão da construção Fortran.
- `gcc/fortran/frontend-passes.cc`: reconhece `EXEC_OMP_CRITICAL` em passes do
  front-end.

Distinga essa construção do `CRITICAL` nativo de Fortran para coarrays, que usa
`EXEC_CRITICAL`/`ST_CRITICAL`. Resultados em `match.cc`, `decl.cc`, `resolve.cc`,
`trans.cc` e `st.cc` com esses códigos nativos não constituem automaticamente
parte do tratamento OpenMP.

## Testes existentes e investigação

| Testes | Evidência que oferecem |
| --- | --- |
| `gcc/testsuite/c-c++-common/gomp/critical-1.c` | Nome ausente/presente, hint e vírgula opcional. |
| `gcc/testsuite/c-c++-common/gomp/critical-2.c` | Rejeição de hints não constantes. |
| `gcc/testsuite/gcc.dg/gomp/critical-1.c` | Verifica as quatro chamadas nos dumps `ompexp`. |
| `gcc/testsuite/gcc.dg/gomp/critical-2.c`, `gcc/testsuite/gcc.dg/gomp/critical-3.c`, `gcc/testsuite/gcc.dg/gomp/critical-4.c` | Casos adicionais específicos de C; leia os diagnósticos e diretivas DejaGNU antes de reutilizar. |
| `gcc/testsuite/g++.dg/gomp/critical-1.C` | Chamadas nos dumps `omplower` e independência do nome em relação à função homônima. |
| `gcc/testsuite/g++.dg/gomp/critical-2.C` | Erros de sintaxe e recuperação. |
| `gcc/testsuite/g++.dg/gomp/critical-3.C` | Instanciação de templates e diagnósticos de hint, incluindo a particularidade descrita acima. |
| `libgomp/testsuite/libgomp.c/critical-1.c` | Exercita diretamente o par de funções sem nome sob concorrência. |
| `libgomp/testsuite/libgomp.c/critical-2.c` | Execução da diretiva em região paralela e preservação dos dados compartilhados. |
| `libgomp/testsuite/libgomp.c/target-critical-1.c` | Região nomeada em target e em função declare target; confira execução real no dispositivo, pois pode haver fallback para host. |
| `libgomp/testsuite/libgomp.c-c++-common/critical-hint-1.c` | Teste de compilação de hints e diagnósticos; registra validações ainda ausentes. |
| `libgomp/testsuite/libgomp.c-c++-common/critical-hint-2.c` | Teste de compilação que verifica valores de hints nos dumps da árvore original. |
| `libgomp/testsuite/libgomp.fortran/critical-hint-1.f90`, `libgomp/testsuite/libgomp.fortran/critical-hint-2.f90` | Casos de hint do front-end Fortran. |

Estar no diretório de testes da `libgomp` não significa que um teste executa o
programa: os dois testes C/C++ de hint acima têm `dg-do compile`.

Exemplos de seleção, a partir da raiz de um build configurado e construído com
as linguagens e runtimes necessários:

```sh
make -j4 check-gcc RUNTESTFLAGS='gomp.exp=critical-*.c'
make -j4 check-g++ RUNTESTFLAGS='gomp.exp=critical-*.C'
make -j4 check-g++ RUNTESTFLAGS='gomp.exp=critical-*.c'
make -j4 check-target-libgomp RUNTESTFLAGS='c.exp=critical-*.c'
make -j4 check-target-libgomp RUNTESTFLAGS='c++.exp=critical-*.c'
make -j4 check-target-libgomp RUNTESTFLAGS='c.exp=target-critical-1.c'
```

Use o compilador produzido por esta árvore com `-fopenmp` e
`-fdump-tree-original -fdump-tree-gimple -fdump-tree-omplower -fdump-tree-ompexp`.
Observe nome/hint na árvore e GIMPLE, chamadas inseridas em `omplower` e ausência
dos marcadores após `ompexp`. Os exemplos de comandos são orientações: nenhum
build ou teste do compilador foi executado para elaborar este documento.

Ao investigar uma mudança, cubra nomes iguais entre unidades de tradução,
regiões sem nome, nomes distintos, templates, recuperação de erros, blocos
estruturados, restrições de aninhamento e execução concorrente. Acrescente
testes de atributos, `_Pragma`, metadiretivas, LTO e dispositivo quando o alcance
da mudança exigir. Inspecione `.sum`/`.log`; não trate testes ausentes ou
`UNSUPPORTED` como sucesso.

## Roteiro de manutenção

1. Determine se o problema está no reconhecimento, hint, corpo, identidade do
   lock, restrições, código gerado ou mutex do target.
2. Compare C e C++, considerando o finalizador compartilhado e a instanciação
   adicional de templates. Avalie Fortran para mudanças na semântica comum.
3. Rastreie árvore, GIMPLE, lowering e chamadas; evite atribuir a `ompexp` a
   escolha de lock que ocorre em `omplower`.
4. Verifique símbolos comuns, ABI, efeitos de memória e configuração do runtime
   antes de propor uma mudança no lock ou nas chamadas.
5. Preserve walkers, cópia e dumps se alterar operandos ou cláusulas.
6. Apresente a proposta, evidências e testes efetivamente executados; mantenha
   explícitas as limitações já observadas no tratamento de hint e templates.

```sh
rg -n 'critical|CRITICAL' gcc/c-family gcc/c gcc/cp
rg -n 'OMP_CRITICAL|GIMPLE_OMP_CRITICAL' gcc --glob '*.{cc,h,def}'
rg -n 'lower_omp_critical|critical_name_mutexes' gcc/omp-low.cc
rg -n 'expand_omp_synch' gcc/omp-expand.cc
rg -n 'GOMP_critical|gomp_mutex' libgomp/critical.c libgomp/config
rg --files gcc/testsuite libgomp/testsuite | rg 'critical'
```

As buscas podem incluir o CRITICAL nativo de Fortran e outros significados de
“critical”; filtre pelo código da diretiva e pelo percurso descrito aqui.
