## Estado atual — diagnósticos e identidade, 2026-10-29

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


Decisões vigentes, estado das propostas e condições para extensões futuras:
[registro central dos agentes](OPENMP_TRANSACTION_DECISIONS.md).


Esta atualização prevalece sobre os estados históricos abaixo. Diagnósticos e
plano canônico estão implementados em `gcc/newtm-analysis.cc`; o runtime rejeita
registros distintos sobrepostos globalmente, inclusive aquisições pendentes.
O registro continua sem leitura de valores ou aquisição de reservas. Chamadas
diretas com definição na unidade são validadas transitivamente e recebem plano
de preparação; clones executáveis ainda não são gerados. Retornos por referência
e ponteiros sem identidade demonstrável são rejeitados.

A [ABI interna](OPENMP_TRANSACTION_ABI.md) está especificada, sem ativação da
ponte funcional GOMP. O pragma mantém o rastreamento; STM, commit e retry funcionam
pelas interfaces internas com corpos preparados explicitamente. Consulte o
[relatório desta entrega](OPENMP_TRANSACTION_SUBSET_REVIEW.md) para limites e
validação. As seções seguintes registram etapas anteriores, não pendências atuais.

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

# Inventário consolidado das primitivas de transaction

## Escopo e leitura

Inventário atualizado em 2026-10-28. Fontes em gcc-src-tm/, build em gcc-build/
e testes em transaction_test/, relativos à raiz (/usr/src/gcc-tm no contêiner).
A hash (nove primitivas), ponte e três serviços owner já são operacionais;
read/write e seis serviços de ciclo de vida continuam esqueletos. Não listar
hash/encerramento do proprietário como trabalho por implementar. Os agentes
detalham os contratos; este arquivo concentra assinaturas, propriedade,
estados, efeitos e chamadas. Históricos de 2026-10-21 permanecem datados.

Há três situações distintas: **existente**, **esqueleto existente** e
**conceitual, sem assinatura C definida**. A coluna de efeitos futuros não
representa comportamento disponível. Os nomes `stm_*` são pseudocódigo e não
devem ser copiados para cabeçalhos ou tratados como ABI aprovada.

## Decisões semânticas do usuário — 2026-10-28

Estas decisões especificam o futuro subconjunto funcional; não anunciam filtros
já implementados pelo parser nem acesso STM nos stubs. A hash continua genérica,
sem inspeção do tipo do objeto do usuário.

| Tema | Regra definida | Trabalho de implementação |
| --- | --- | --- |
| Acessos preexistentes | Leituras e escritas exigem registro explícito já alcançado; a permissão anterior de leitura externa comum foi substituída. | Diagnosticar acesso sem registro; INVALID em contexto válido segue limpeza segura e fatal, nunca fallback ou retry. |
| Tipos | Escalares, arrays e agregados restritos com validação recursiva e tamanho conhecido. Excluir volatile, atômicos, VLA, unions, bit-fields, membros flexíveis e casos sem contrato; proibir escritas const. | Aplicar filtros e repetir verificações após instanciação. Suporte a instâncias de classes, inclusive struct C++, é futuro; trivialidade não habilita automaticamente. |
| Identidade/sobreposição | Vetor canônico conhecido admite índice dinâmico e limites em execução. Rejeitar ponteiros sem identidade e registros distintos sobrepostos. Campos/elementos usam entrada e offset do agregado registrado; ponteiro listado não inclui a memória apontada. | Implementar mapeamento/diagnóstico e avaliação única. Hash exata não descobre intervalos; não anunciar detecção de sobreposição pronta ou aceitar indireção desconhecida como leitura externa comum. |

Chamadas e propagação: gerar versões transacionais internas com contexto e
status explícitos, preservar a assinatura pública e preencher resultado somente
em sucesso. Falha segue ao epílogo de cada função até o controle externo; nenhum
valor fictício ou continuação normal. Chamadas indiretas/externas sem preparação
são restritas; pure/const não prova acesso à visão privada. São contratos da
geração futura, não novas funções/ABI implementadas. Geração, nomes/linkage,
representação de retornos e traduções de status precisam de especificação técnica.
Exceções/cleanups do usuário: rejeitar inicialmente. A limpeza interna do
runtime continua obrigatória. Read/write recusam acessos normais em tentativa
condenada; não usá-los em destrutores para aparentar suporte. Exceção do usuário
nunca vira CONFLICT nem provoca commit durante desenrolamento. Suporte futuro
precisa definir visão/acesso de dados de destrutores, ordem/falhas de cleanup e
vida útil. Ramos de desenrolamento explicitamente suportado nos contratos gerais
abaixo são dessa extensão futura, não do primeiro subconjunto. Isso não dispensa
destrutores internos de metadados em release. A restrição e seus diagnósticos
não foram implementados por esta atualização documental.
Representação privada definida: escalares simples e agregados com faixas válidas
/modificadas (modificadas são subconjunto das válidas), sem exigir acesso ou
inicialização integral para operar sobre um campo. Commit publica somente faixas
modificadas; descarte não publica. Buffers são internos: `&x` mantém o endereço
lógico original. A unidade de conflito permanece o objeto canônico. Layout e
implementação ainda pendem; efeitos externos estão excluídos conforme o fechamento acima.
Todos os acessos preexistentes exigem registro explícito; continuam aplicáveis
as regras C/C++ de concorrência com código fora do protocolo. Ver o [contrato da hash](OPENMP_TRANSACTION_HASH_AGENT.md).

## Tipos, estados e propriedade comuns

- `struct newtm_hash_table` e `struct newtm_hash_entry`: tipos opacos declarados
  em `libgomp/newtm-hash.h`, estruturas privadas em `newtm-hash.c`.
- `struct newtm_context`: somente declaração opaca em `newtm-transaction.h`.
  Não existe definição funcional. Não equivale a `newtm_trace_context`.
- `enum newtm_hash_status`: `NEWTM_HASH_OK`, `NEWTM_HASH_INVALID`,
  `NEWTM_HASH_NOT_FOUND`, `NEWTM_HASH_SIZE_MISMATCH`, `NEWTM_HASH_BUSY`,
  `NEWTM_HASH_OVERFLOW`.
- `enum newtm_access_status`: `NEWTM_ACCESS_OK`, `NEWTM_ACCESS_CONFLICT`,
  `NEWTM_ACCESS_INVALID`. Não converter erros estruturais em
  conflito por igualdade de valores numéricos ou cast.
- `enum newtm_lifecycle_status`: `NEWTM_LIFECYCLE_OK`,
  `NEWTM_LIFECYCLE_CONFLICT`, `NEWTM_LIFECYCLE_INVALID`. Resultados de operações,
  não estados do contexto. Traduzir explicitamente de/para os enums hash/acesso.
- `enum newtm_attempt_state`: `NEWTM_ATTEMPT_ACTIVE`,
  `NEWTM_ATTEMPT_CONFLICTED`, `NEWTM_ATTEMPT_INVALID`,
  `NEWTM_ATTEMPT_PUBLISHING`, `NEWTM_ATTEMPT_CLOSED`. Declarado no cabeçalho
  interno; transições e consumo único especificados em
  [ABORT — máquina de estados](OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa).
  Não há layout de contexto nem transições executadas pelos stubs.
- Referência física mantém uma entrada viva; reserva autoriza acesso;
  buffer privado guarda dados especulativos. São recursos distintos.
- Objetos do programa pertencem ao programa e precisam permanecer vivos até
  o encerramento externo. Registro/referência não prolonga sua vida útil.
- No desenho funcional, contexto/tentativa pertencem à execução atual na mesma
  tarefa/thread, sem pontos de escalonamento transitivos. Contexto retém
  referências, reservas e buffers até confirmação ou descarte externo.
- Bytes usam `size_t`; `offset` é relativo ao objeto canônico inteiro.
  Para acesso funcional: `size > 0`, `offset <= object_size` e
  `size <= object_size - offset`, sem overflow. Buffers privados válidos e
  sem sobreposição indevida; tipos e qualificadores validados pelo compilador.
- Alocação funcional da hash segue libgomp: falta de memória fatal, não um
  estado recuperável inventado. Ponteiros pendentes não têm validação garantida.

## Hash: decisão de mistura para a implementação funcional

`acquire` e `lookup_acquire` converterão a chave para `uintptr_t` e usarão
MurmurHash3 fmix32/fmix64, selecionado por UINTPTR_MAX de 32/64 bits, com
aritmética sem sinal na largura correta e todos os bits do endereço. Reduzir
por `% bucket_count`, sem truncamento prévio nem máscara; capacidade positiva
com alocação representável não precisa ser potência de dois. Igualdade continua
por endereço exato; o índice imutável fica na entrada para release/visit.
Outras larguras deverão ser diagnosticadas na compilação da implementação
operacional até extensão explícita, sem truncamento silencioso. Constantes,
algoritmos e limites da evidência exploratória estão em
[Decisão de mistura](OPENMP_TRANSACTION_HASH_AGENT.md#decisão-de-mistura-de-endereços).
A mistura está implementada em acquire/lookup_acquire; create/destroy e
release também são operacionais. Address/size consultam campos imutáveis com
referência retida; visit protege o callback com mutex do bucket. Count já
obtém fotografia consistente sob todos os mutexes, com soma saturada em SIZE_MAX.

A implementação inicial de acquire alocará entradas ausentes sob o mutex do
bucket. Após validação operacional, medições poderão motivar alocação fora do
lock, com nova busca e descarte de candidatos não utilizados, preservando todos
os contratos e evitando alocação supérflua na aquisição de entrada existente.
É sugestão futura, não requisito inicial; ver
[contrato de alocação](OPENMP_TRANSACTION_HASH_AGENT.md#alocação-inicial-e-possível-otimização-de-acquire).

Decisão do usuário: manter os printf/fflush dos logs hash para depuração.
A inicialização da ponte usa helper sem diagnóstico para alocar/publicar sob
lock global e imprime create depois de unlock. Acquire mantém malloc sob lock,
mas libera o mutex antes de diagnosticar falha; não retorna estado OOM.
O helper de tabela usa malloc/calloc e desfaz alocação parcial antes de retornar
NULL internamente; create e begin convertem essa falha em fatal fora de locks.
A ordem de logs concorrentes não equivale à ordem de publicação.
Printf/fflush e demais E/S de diagnóstico são proibidos sob mutexes de buckets,
inclusive em visit/count. Mensagens preservadas devem usar dados seguros e ser
emitidas fora dos locks, sem acessar entradas liberadas. Atualizar testes de
saída (incluindo transaction_test/verificar.py) e documentação junto com qualquer
mudança de mensagens; logs isolados não comprovam comportamento operacional.
Ver [restrição de logs](OPENMP_TRANSACTION_HASH_AGENT.md#restrição-de-logs-na-implementação-operacional).

## Hash: assinaturas internas existentes

Todas as nove primitivas têm visibilidade oculta; cabeçalho não instalado como
API pública. `out` válido é zerado na falha, sem liberar automaticamente seu
conteúdo anterior. No contrato funcional, nenhuma saída retém mutex.

```c
enum newtm_hash_status newtm_hash_create
  (size_t bucket_count, newtm_hash_policy_destroy_fn destroy_policy,
   struct newtm_hash_table **out);
enum newtm_hash_status newtm_hash_destroy (struct newtm_hash_table *table);
enum newtm_hash_status newtm_hash_acquire
  (struct newtm_hash_table *table, const void *address, size_t size,
   struct newtm_hash_entry **out);
enum newtm_hash_status newtm_hash_lookup_acquire
  (struct newtm_hash_table *table, const void *address,
   struct newtm_hash_entry **out);
void newtm_hash_release
  (struct newtm_hash_table *table, struct newtm_hash_entry *entry);
const void *newtm_hash_entry_address (const struct newtm_hash_entry *entry);
size_t newtm_hash_entry_size (const struct newtm_hash_entry *entry);
void newtm_hash_entry_visit
  (struct newtm_hash_table *table, struct newtm_hash_entry *entry,
   newtm_hash_visit_fn visit, void *argument);
size_t newtm_hash_count (struct newtm_hash_table *table);
```

**Hoje:** create/destroy são operacionais: create valida argumentos/overflow,
aloca e inicializa buckets/mutexes e publica somente a tabela completa; destroy
exige quiescência externa, retorna BUSY sem alterações se ocupada ou libera a
tabela vazia. Todos os printf/fflush de depuração foram preservados, fora de locks.
acquire insere ou retém entradas com tamanho compatível sob mutex e
lookup_acquire retém apenas as existentes. Ambos validam argumentos e overflow.
release consome referências, remove na última e destrói metadados fora do lock.
A ponte verifica retornos e retém entradas reais até a saída externa.
Address/size retornam chave e tamanho imutáveis sem lock, exigindo referência
retida; visit executa o callback uma vez sob mutex, preserva a referência e
libera o lock ao retornar. Count conta entradas (não referências) sob todos os
mutexes em ordem crescente, satura em SIZE_MAX e libera em ordem inversa.
Zero não comprova quiescência nem término dos destruidores.
As nove operações da tabela seguinte têm implementação. Não há reservas nem STM.

| Operação | Proprietário e recursos | Estados/resultado e efeitos | Ponto de chamada |
| --- | --- | --- | --- |
| `newtm_hash_create` | Chamador assume tabela e mutexes somente em OK; owner é dono da tabela global. | OK cria tabela vazia; INVALID para capacidade zero/out nulo; OVERFLOW antes de alocar. Falha não publica tabela parcial. | Wrapper interno/testes; begin usa o helper comum sem E/S sob lock e emite o mesmo log depois de unlock. |
| `newtm_hash_destroy` | Consome tabela somente em OK; exige impedir novas operações e concluir operações, visitas e destruidores em andamento, mantendo exclusão até retornar. Referências ociosas podem permanecer e causar BUSY; count zero não basta. | OK destrói tabela vazia/mutexes; INVALID para NULL; BUSY preserva integralmente tabela ocupada e referências; permite retomar uso após encerrar a exclusão externa. | Hoje owner_close após drenar leases; tabelas independentes sob quiescência do chamador. |
| `newtm_hash_acquire` | Entrega exatamente uma referência ao chamador; contexto a retém até saída externa. | OK insere ou retém chave exata sob mutex; INVALID, SIZE_MISMATCH ou OVERFLOW sem alteração parcial. Não lê objeto nem concede reserva. | Hoje register para objeto distinto; futuro registro funcional. |
| `newtm_hash_lookup_acquire` | Entrega referência que o chamador deve liberar. | OK retém existente; NOT_FOUND não insere; INVALID/OVERFLOW não adquirem referência. | Sem chamador de produção hoje; consulta interna futura quando necessária, sem registro implícito. |
| `newtm_hash_release` | Consome uma referência obtida com sucesso em acquire/lookup; copiar ponteiro não adquire referência. Entrada encadeada, contador positivo e tabela correspondente viva até retorno, inclusive callback/free. Retirar só participações/reservas sustentadas pela referência consumida. | void; na última, desliga sob mutex e conclui destrutor/free fora dele antes de retornar. Decremento positivo não garante sobrevivência após unlock; acesso posterior exige outra referência independente. NULL, contador zero, dupla liberação, tabela errada e ponteiros pendentes são violações sem diagnóstico garantido. | Hoje end externo; futuro commit/descarte após retirar participações. Destroy deve aguardar a chamada inteira, não apenas count zero. |
| `newtm_hash_entry_address` | Referência permanece com chamador; endereço é emprestado, imutável. | Retorna chave sem ler objeto ou adquirir reserva; argumento inválido é violação, não retorno NULL funcional. | Sem chamador hoje; consultas futuras de registro/acesso/publicação quando necessárias. |
| `newtm_hash_entry_size` | Referência permanece com chamador. | Retorna extensão positiva imutável, sem reserva; zero não é erro funcional previsto. | Sem chamador hoje; futura validação de limites. |
| `newtm_hash_entry_visit` | Preserva referência; slot de metadados pertence à entrada, argument ao chamador. | void; callback uma vez sob mutex do bucket; resultado da política via argument. Não há rollback automático do callback. Proibidos espera por transações, inversão de locks e escape do slot; ponteiros copiados exigem proteção própria após retorno. | Sem chamador hoje; futura concessão/conversão/retirada de reservas. |
| `newtm_hash_count` | Não transfere recursos; tabela emprestada. | Fotografia consistente saturada em SIZE_MAX; locks em ordem crescente e liberação inversa. Zero não impede novas operações nem exclui usuários da tabela ou destruidores de entradas já retiradas; não autoriza destroy. | Sem chamador de produção; diagnóstico/testes fora de callbacks e locks de bucket. |

### Callbacks de visita e destruição operacionais

```c
typedef void (*newtm_hash_visit_fn)
  (const void *address, size_t size, void **policy_data, void *argument);
typedef void (*newtm_hash_policy_destroy_fn) (void *policy_data);
```

| Operação | Proprietário e recursos | Estados e efeitos | Ponto de chamada |
| --- | --- | --- | --- |
| `newtm_hash_visit_fn` | Entrada possui metadados instalados; callback responde pelo destino dos substituídos. Slot não pode escapar; argument é emprestado. | void; pode atualizar slot e argument sob lock. Não consome referência nem libera mutex; sem reentrada, espera por transação, inversão de locks, exceção/longjmp ou código do usuário. Ponteiros copiados não ficam protegidos após retorno; uso posterior exige protocolo de vida útil e sincronização. Falha da política mantém metadados consistentes. | Dentro de entry_visit operacional; hash controla lock. |
| `newtm_hash_policy_destroy_fn` | Recebe propriedade dos metadados da geração retirada. | void; libera recursos apenas da geração retirada uma vez fora do lock, sem reentrada ou acesso à entrada retirada/objeto do programa/nova geração. Retorna normalmente, sem exceção/longjmp; tabela viva até callback e free terminarem. | Última release após retirada de reservas; não depende de outra geração da mesma chave. |

Remoção na última referência é o desenho proposto pelos contratos atuais, não
uma aprovação histórica individual do usuário. Ver detalhes e limites em
[HASH_AGENT](OPENMP_TRANSACTION_HASH_AGENT.md).

## Acesso: assinaturas internas existentes

```c
enum newtm_access_status newtm_read
  (struct newtm_context *ctx, const void *object, size_t offset,
   void *destination, size_t size);
enum newtm_access_status newtm_write
  (struct newtm_context *ctx, const void *object, size_t offset,
   const void *source, size_t size);
```

**Hoje:** esqueletos ocultos em `libgomp/newtm-transaction.c/.h`, sem chamadores
na ponte ou no compilador. Imprimem os cinco parâmetros, fazem fflush e retornam
INVALID sem validar/desreferenciar argumentos ou alterar contexto/buffers.
Nessa etapa, INVALID significa ausência de implementação.

| Operação | Proprietário e recursos | Estados e efeitos funcionais futuros | Ponto de chamada futuro |
| --- | --- | --- | --- |
| `newtm_read` | Contexto possui referências/reservas/visão privada; destino privado pertence ao código instrumentado. | OK preenche destino por completo sob reserva compatível, observando escritas próprias. CONFLICT impede leitura; INVALID é permanente. Falha preserva destino. Sem publicação ou limpeza global. | Em cada leitura suportada, antes de usar o resultado; chamada por ponte GOMP de acesso ainda sem assinatura definida. |
| `newtm_write` | Origem privada pertence ao código instrumentado; contexto possui reserva exclusiva e dados privados. | OK altera somente visão privada. CONFLICT/INVALID não aplicam escrita parcial; origem e memória original preservadas em todo retorno normal. Sem commit/descarte. | Em cada escrita suportada, após avaliação única da origem; ponte de acesso ainda indefinida. |

Para contexto válido e proprietário correto, falha registra causa e condena a
tentativa sem liberar globalmente recursos. Contexto inválido não é acessado
para tentar condená-lo. Todo retorno deve ser verificado imediatamente;
CONFLICT leva a descarte/retry externo, INVALID não autoriza retry. Hash não
implementa esses estados. No contrato funcional, todo retorno normal libera
os mutexes adquiridos pela chamada e preserva profundidade/identidade. A reserva
é concedida antes de ler memória original e permanece até o encerramento externo;
retê-la não significa manter um mutex adquirido. Ver [ACCESS_AGENT](OPENMP_TRANSACTION_ACCESS_AGENT.md).

## Controle de admissão e participações implementado

Camada proprietária da tabela global, separada das nove primitivas, com serviços
ocultos e sem alteração da ABI GOMP:

```c
struct newtm_hash_owner_lease { struct newtm_hash_table *table; bool active; };
enum newtm_hash_status newtm_hash_owner_acquire
  (struct newtm_hash_owner_lease *lease);
enum newtm_hash_status newtm_hash_owner_release
  (struct newtm_hash_owner_lease *lease);
enum newtm_hash_status newtm_hash_owner_close (void);
```

| Serviço | Pré-condições/propriedade | Efeitos/retornos |
| --- | --- | --- |
| owner_acquire | Lease zerada/inativa, armazenamento exclusivo; não copiar como novo recurso. | OPEN entrega uma participação e tabela emprestada em OK, antes do log de criação. BUSY em CLOSING, INVALID em CLOSED/NULL/ativa e OVERFLOW sem incrementar. OOM fatal fora do lock. |
| owner_release | Lease ativa e correspondente; terminar todas as referências, operações, callbacks e limpeza antes. | OK consome uma participação e zera a lease; INVALID não consome argumento inativo/incompatível. Nunca usar o ponteiro depois. Não destrói tabela. |
| owner_close | Coordenador não bloqueante; nunca destruir diretamente a tabela emprestada. | OPEN -> CLOSING -> CLOSED. BUSY com usuários, outro destruidor ou entradas residuais; permanece CLOSING. Usuários existentes podem concluir. Um coordenador destrói fora do lock após drenagem e publica CLOSED só em OK. Repetição CLOSED é OK. |

O controle estático sobrevive à tabela, inclusive seu mutex. Contexto externo
retém uma participação até concluir releases/metadados/TLS/free; aninhamentos
reutilizam essa participação. Consumidores diretos da tabela global precisam de
lease pelo intervalo completo, inclusive referências entre chamadas. Não afeta
quiescência de tabelas independentes criadas pelo chamador. Copiar/abandonar lease
ou liberar antes de terminar seu uso continua violação. Não garante dlclose
seguro ou término de usuários que não concluem. Detalhes em
[protocolo do proprietário](OPENMP_TRANSACTION_HASH_AGENT.md#admissão-e-participações-da-camada-proprietária).

## Ponte e suporte de rastreamento existentes

Assinaturas da ponte em `libgomp/libgomp_g.h`; símbolos GOMP exportados e
versionados em `libgomp.map`. Os helpers trace são static em `newtm-hash.c`.

```c
void GOMP_transaction_begin (void);
void GOMP_transaction_register (void *address, size_t size);
void GOMP_transaction_end (void);
static void newtm_trace_initialize (void); /* constructor */
static void newtm_trace_finalize (void);   /* destructor */
```

| Operação | Proprietário e recursos atuais | Estados e efeitos atuais | Ponto de chamada atual |
| --- | --- | --- | --- |
| `GOMP_transaction_begin` | Runtime possui lock e tabela global inicializada no primeiro uso, e contexto trace por thread; contexto possui lista de registros. | void; inicializa via helper comum sem E/S sob lock, publica só tabela completa e libera lock antes do log create ou diagnóstico fatal. Begin externo retém uma lease; aninhamentos reutilizam a existente mesmo em CLOSING. Admissão recusada, falhas estruturais e overflow são fatais fora do lock. | Código emitido na gimplificação antes de cada região dinâmica. |
| `GOMP_transaction_register` | Contexto possui nó novo; objeto/endereço são emprestados pelo programa. Nó retém uma referência real à entrada. | void; deduplica endereço, exige tamanho igual em duplicata ou falha fatal; chama acquire para novo registro. Contexto inativo, NULL/size zero e falha de aquisição são fatais; libera nó temporário antes de falhar. Não lê valor. | Gimplificação emite após begin, na ordem da lista, antes do corpo. |
| `GOMP_transaction_end` | Contexto libera seus nós e a si mesmo só na saída externa; ponteiro da thread é limpo. | void; reduz profundidade; na saída externa libera registros/metadados, limpa TLS, libera contexto e só então devolve a participação. Saída inválida é fatal; nenhum commit, rollback ou retry. | Cleanup TRY_FINALLY emitido na gimplificação, inclusive desenrolamento que o alcance. |
| `newtm_trace_initialize` | Biblioteca possui lock global trace. | void; inicializa lock quando target exige; não cria tabela/contexto. | Constructor da biblioteca. |
| `newtm_trace_finalize` | Biblioteca possui lock e tabela global real após uso, emprestada aos contextos. | void; uma tentativa de owner_close sem aguardar usuários. BUSY conserva tabela/controle em CLOSING, sem fatal/limpeza forçada. Não destrói mutex de admissão; CLOSED seguro para rejeição tardia. | Destructor; pode solicitar fechamento com usuários ativos; dlclose ainda exige exclusão externa de código. |

A ponte já confere estados antes de publicar inicialização/registros, retém
handles válidos e confere destruição; preservar essas garantias nas próximas
etapas. A ponte void usa erro fatal para falhas estruturais detectadas nessa
etapa. Não inferir sucesso transacional de retorno void. O contexto trace não
guarda reservas, buffers ou causa de falha. Ver
[HASH_IMPLEMENTATION_AGENT](OPENMP_TRANSACTION_HASH_IMPLEMENTATION_AGENT.md).

## Ciclo de vida: esqueletos internos existentes

As seis assinaturas abaixo estão em `libgomp/newtm-transaction.h`, com corpos
em `.c` e visibilidade oculta. O contexto é opaco na interface e definido em
newtm-context.h. A ponte chama begin, end interno e discard; registro STM e
publicação continuam pendentes. Não são aliases dos símbolos GOMP. `stm_*` continua sendo notação conceitual.

```c
enum newtm_lifecycle_status newtm_begin
  (struct newtm_hash_owner_lease *lease, struct newtm_context **slot,
   bool *outermost);
enum newtm_lifecycle_status newtm_register
  (struct newtm_context *ctx, const void *object, size_t size,
   enum newtm_object_kind kind);
enum newtm_lifecycle_status newtm_end (struct newtm_context **slot);
enum newtm_lifecycle_status newtm_fail
  (struct newtm_context *ctx, enum newtm_access_status cause);
enum newtm_lifecycle_status newtm_discard (struct newtm_context **slot);
enum newtm_lifecycle_status newtm_backoff (size_t attempt);
```

**Estado atual:** begin cria/aninha, fail condena e discard limpa; end conclui
somente saída interna. End externo marca INVALID e preserva recursos para
descarte; não há publicação. register/backoff/read/write ainda são stubs com
printf/fflush e INVALID. Os logs são mantidos fora dos mutexes.

Na tabela, estados/efeitos descrevem os **contratos funcionais futuros**.
Os nomes OK/CONFLICT/INVALID usam o prefixo NEWTM_LIFECYCLE_.

| Operação e categoria | Proprietário dos recursos | Estados e efeitos | Ponto de chamada previsto |
| --- | --- | --- | --- |
| `newtm_begin`, primitiva de entrada | Chamador possui slot; contexto criado passa a ser possuído por esse handle. Lease é transferida no begin externo e permanece até toda a limpeza. | OK cria tentativa externa com profundidade 1 e outermost=true, ou entra no mesmo contexto/tabela com profundidade +1 e outermost=false. Não registra objetos. Falha preserva slot/profundidade e inicializa outermost=false; CONFLICT só para tentativa já condenada por conflito; INVALID para violação detectada ou causa permanente já registrada, que prevalece. | Entrada de cada região dinâmica; ponte futura obterá a tabela e o slot da execução. |
| `newtm_register`, primitiva de registro | Contexto retém referência por objeto distinto; objeto é emprestado pelo programa e vive até encerramento externo. | OK deduplica por endereço/tamanho/kind ou registra; sem reserva/leitura. Tamanho ou kind incompatível/tag desconhecida/argumento inválido é INVALID e condena contexto válido. Tentativa já conflitante retorna CONFLICT. Falha não adiciona registro parcial nem muda profundidade. | Após begin bem-sucedido, em cada lista já alcançada. |
| `newtm_end`, primitiva de encerramento normal | Slot/contexto continuam do chamador na saída interna e em falha; saída externa OK consome contexto e zera slot. | Profundidade seleciona saída interna ou confirmação externa. OK interno só reduz profundidade. OK externo publica tudo sob reservas, retira participações, libera referências/buffers/contexto. CONFLICT/INVALID antes de publicar condenam contexto válido com a causa efetiva, preservando INVALID dominante e slot/profundidade/recursos para descarte explícito; nunca descartam implicitamente. | Término normal de cada região; não chamar como commit indiscriminado no finally. |
| `newtm_fail`, helper interno compartilhado | Contexto mantém todos os recursos; chamador preserva causa efetiva antes do futuro descarte. | Recebe NEWTM_ACCESS_CONFLICT/INVALID e devolve causa efetiva no domínio lifecycle; INVALID prevalece. ACCESS_OK/causa desconhecida é uso inválido. Nunca OK. Não muda profundidade, libera recursos ou transfere controle; contexto inválido/alheio não é alterado. | Futuras primitivas de acesso/controle ao detectar falha; conversão dos estados é explícita. Propagação entre funções continua pendente. |
| `newtm_discard`, primitiva de descarte | Controle externo chama após cleanups suportados, conservando causa/possível exceção fora do contexto. | OK descarta tentativa não publicada, todos os níveis, reservas/referências/buffers e contexto; zera slot. Significa limpeza concluída, não commit ou autorização para retry. INVALID detectável preserva slot; falha estrutural irrecuperável é fatal. Nunca CONFLICT. Duplo descarte é violação. | Após falha de acesso/end propagada ao controle externo, ou desenrolamento explicitamente suportado. |
| `newtm_backoff`, helper de controle de retry | Chamador guarda contador consecutivo de conflitos; nenhuma reserva/mutex ou uso de buffer descartado. | attempt é contagem positiva, primeira espera=1, saturada em SIZE_MAX pelo chamador. OK após espera progressiva com teto; INVALID para entrada inválida; nunca CONFLICT. Não cria contexto. Fórmula temporal/jitter ainda pendentes. | Somente após descarte OK motivado por CONFLICT, antes de nova tentativa externa. |

Slots e saídas devem ser válidos, exclusivos durante a chamada e não sobrepor
contexto, tabela ou objetos. Toda operação funcional exige a execução proprietária
sem escalonamento transitivo; nenhum mutex escapa no retorno. Alocação segue a
falha fatal da libgomp. Não prometer validação de ponteiros pendentes.

### Fases sem símbolos adicionais

- **Saída interna:** fase de `newtm_end`, preserva registros e visão privada.
- **Confirmação externa:** fase de `newtm_end`, prepara recursos antes de publicar;
  conserva todas as reservas até publicar tudo. Não pode retornar falha recuperável
  após a primeira publicação. A liberação final pertence ao mesmo encerramento.
- **Reinício externo:** fluxo gerado pelo compilador, após descarte e backoff
  bem-sucedidos; recria temporários, listas e chamadas. Não há primitiva retry,
  commit isolado, saída interna isolada, criador/destruidor separado de contexto
  ou setjmp/longjmp. Helpers de implementação só serão acrescentados se necessários.

O desenho escolhido usa **descarte explícito pelo controle externo em toda falha**.
Substitui o modelo ilustrativo anterior de end-false com descarte implícito.
Uma falha de end mantém recursos válidos para cleanups; o chamador não continua o
corpo, não repete end nem usa slot preservado como prova de tentativa saudável.
Guardar o indicador outermost de uma entrada OK para decidir onde o controle
externo reside; end usa a profundidade real, sem confiar nesse bool como parâmetro.
INVALID permanente não entra em retry; exceção do usuário preserva sua natureza.

### Estados da tentativa e consumo único

O enum `newtm_attempt_state` distingue ativa, condenada por conflito, inválida,
em publicação e encerrada. A máquina normativa está em
[ABORT](OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa): INVALID
prevalece sobre conflito; só ACTIVE pode entrar em PUBLISHING, após toda
preparação falível. PUBLISHING é irrevogável; CLOSED nunca reabre. Espera ocorre
fora do contexto e retry cria nova tentativa. Estado não equivale a resultado.

Há um slot proprietário canônico por tentativa; entradas internas compartilham
esse slot, nunca copiam propriedade. end externo consome após publicação total;
discard consome no controle externo após cleanups. Ambos marcam CLOSED, zeram
profundidade e retiram o contexto do slot **antes da primeira liberação**.
Só o consumidor retém ponteiro privado para terminar a limpeza, sem reentrada;
OK só vem após liberação completa. Segundo end/discard via slot vazio é INVALID
sem limpeza; não inspecionar ponteiro já liberado. CLOSED/slot NULL não provam
commit nem limpeza terminada. Erro de validação anterior ao consumo preserva
slot; falha estrutural durante liberação é fatal, sem segunda tentativa de limpar.

O enum é o único novo tipo desta etapa; nenhuma função adicional ou layout de
contexto foi criado. Os stubs continuam retornando INVALID e preservando slot,
sem anunciar confirmação ou descarte. Guards contra escalonamento e a ponte
exportada de acesso continuam sem assinatura; propagação e cleanups ainda
dependem de implementação específica.

O owner atual inicializa destroy_policy=NULL e não instala metadados de
política na ponte. Antes de read/write/reservas funcionais, configurar o destrutor
imutável na criação e definir sua propriedade; não publicar metadados não nulos
numa tabela sem callback nem alterar esse callback com usuários ativos.

## Cobertura permanente e carga parametrizada

Além dos testes DejaGNU, transaction_test possui alvos check-hash-create-destroy,
check-hash-acquire, check-hash-entry, check-hash-count, check-hash-diagnostics,
check-hash-owner e check-lifecycle. A ponte é verificada pelos exemplos de check.
Check inclui também check-carga, que executa transaction_carga com três argumentos:
THREADS W:R REPETICOES (por thread), nos casos 1:9, 1:1 e 9:1. Defaults:
CARGA_THREADS=4 e CARGA_REPETICOES=1000. Escritas são read-modify-write, com
floor(N*W/(W+R)) escritas por thread; registros/liberações permanecem concorrentes,
mas critical interno protege os valores enquanto não há STM. O verificador
confere contagens e resultado, não apenas mensagens. Não interpretar isso como
validação de reservas, commit, rollback ou desempenho STM.

Consulte [testes](../transaction_test/README.md) para parâmetros, timeouts e logs.
Os quatro testes de estrutura, além de diagnósticos e owner, usam configuração
Linux x86_64; as evidências de sanitizadores e limitações por etapa estão em
[IMPLEMENTATION](OPENMP_TRANSACTION_IMPLEMENTATION.md). Pendências funcionais e
de portabilidade estão no agente de projeto, sem confundir históricos com novas
execuções exigidas por uma alteração.

## Helpers existentes do compilador

Estes serviços executam na compilação. Árvores pertencem à infraestrutura GC do
GCC; parsers, tokens e iteradores são emprestados e não podem escapar como
metadados runtime. Falhas seguem diagnósticos/recuperação da linguagem, não os
enums NEWTM. A tabela registra efeitos atuais; contratos completos em
[IMPLEMENTATION](OPENMP_TRANSACTION_IMPLEMENTATION.md).

| Assinatura | Proprietário e efeitos | Estados/resultado | Ponto de chamada |
| --- | --- | --- | --- |
| `static tree newtm_c_parser_transaction(location_t loc, c_parser *parser, bool *if_p)` | Consome tokens; resolve lista antes do corpo e constrói árvore rastreada por GC. Parser permanece do front-end. | Retorno tree sob convenções do parser; diagnostica e recupera erros sem certificar lista parcial. | Despacho de construção OpenMP C. |
| `static tree newtm_cp_parser_transaction(cp_parser *parser, cp_token *pragma_tok, bool *if_p)` | Mesmo fluxo com lookup/acesso C++ e referências externas; não retém token. | tree sob convenções C++; erros do front-end, sem estado runtime. | Despacho de construção OpenMP C++. |
| `tree newtm_finish_transaction(location_t loc, tree body, tree objects)` | Cria OMP_TRANSACTION com corpo/lista e o acrescenta ao fluxo. Nós sob GC. | Retorna nó adicionado; não resolve nomes nem emite acesso runtime. | Parsers e substituição de templates C++. |
| `gimple *newtm_build_transaction(gimple_seq body)` | Associa sequência ao novo marcador GSS_OMP; corpo não é copiado e lista não é transportada. | Novo GIMPLE_OMP_TRANSACTION; sem estado transacional. | Gimplificação de OMP_TRANSACTION em gcc/gimplify.cc e cópia/remapeamento por remap_gimple_stmt em gcc/tree-inline.cc. |
| `static void newtm_lower_transaction(gimple_stmt_iterator *gsi_p)` | Lowering do corpo no contexto externo e substituição do marcador pela sequência; marcador removido não pode ser reutilizado. | void; exige contexto omp existente. Ainda sem instrumentação. | Dispatcher lower_omp em omp-low.cc. |

A gimplificação também gera begin/register/finally(end), sem helper newtm próprio.
A instrumentação futura precisa preservar/remapear identidade antes de eliminar
o marcador, incluindo private/reduction e templates, sem reinstrumentar regiões
internas. A ponte GOMP de acesso e seus builtins continuam indefinidos.

## Localização para conferir o inventário

| Grupo | Declaração e implementação | Chamadas atuais |
| --- | --- | --- |
| Hash e callbacks | [newtm-hash.h](libgomp/newtm-hash.h), [newtm-hash.c](libgomp/newtm-hash.c) | create/acquire/release/destroy pela ponte no mesmo fonte; demais serviços sem chamadores de produção. |
| Acessos e ciclo de vida | [newtm-transaction.h](libgomp/newtm-transaction.h), [newtm-transaction.c](libgomp/newtm-transaction.c) | Sem chamadas geradas ou pela ponte. |
| Ponte trace | [libgomp_g.h](libgomp/libgomp_g.h), [newtm-hash.c](libgomp/newtm-hash.c), [libgomp.map](libgomp/libgomp.map) | [omp-builtins.def](gcc/omp-builtins.def) e [gimplify.cc](gcc/gimplify.cc); initialize/finalize pelo ciclo da biblioteca. |
| Parsers e finalizador | [c-parser.cc](gcc/c/c-parser.cc), [parser.cc](gcc/cp/parser.cc), [c-common.h](gcc/c-family/c-common.h), [c-omp.cc](gcc/c-family/c-omp.cc) | Dispatchers nos parsers; finalizador também em [pt.cc](gcc/cp/pt.cc). |
| Construtor e lowering GIMPLE | [gimple.h](gcc/gimple.h), [gimple.cc](gcc/gimple.cc), [omp-low.cc](gcc/omp-low.cc) | Construtor em gimplify.cc e [tree-inline.cc](gcc/tree-inline.cc); lowering pelo dispatcher em omp-low.cc. |

## Referências reutilizadas e critério para a próxima etapa

`GOMP_critical_start/end(void)` e `GOMP_critical_name_start/end(void **)` são
serviços existentes de locks, descritos em [CRITICAL_AGENT](OPENMP_CRITICAL_AGENT.md).
Não integram as novas primitivas STM e não fazem rollback/commit. Alocadores,
mutexes, gomp_fatal e funções genéricas do GCC também são infraestrutura existente.

Para novos serviços exigidos por uma tarefa, definir assinatura interna e estados,
propriedade em sucesso/falha, efeitos permitidos no stub e ponto de chamada.
Não obrigar commit, backoff ou retry a terem símbolo próprio se forem fases ou
fluxo de controle. Não ligar stubs que retornam falha ao código gerado como se
fossem operações bem-sucedidas. A hash antes prevista para depois da
consolidação já foi entregue, assim como owner e os testes privados. A próxima
integração funcional é contexto/política/acessos/controle de falha, conforme o
[roteiro de projeto](OPENMP_TRANSACTION_PROJECT_AGENT.md#orientação-para-as-próximas-etapas).

Validação histórica da consolidação inicial: comparação documental com cabeçalhos, definições
e chamadores locais; verificação de links relativos e cobertura dos símbolos.
A consolidação inicial não exigiu rebuild; validação da etapa de esqueletos
registrada em [IMPLEMENTATION](OPENMP_TRANSACTION_IMPLEMENTATION.md).

### Revisão histórica da consolidação inicial

A revisão conferiu assinaturas completas (retorno e parâmetros), callbacks,
estados e chamadores nos fontes. Corrigiu a omissão do chamador de cópia/remapeamento
em tree-inline.cc e a descrição da tabela trace, cujo ponteiro naquela etapa
permanecia NULL. Hoje a tabela e as referências são reais, com leases owner.
Explicitou a liberação de recursos após confirmação e a distinção entre reservas
persistentes e mutexes liberados a cada acesso. Links para os fontes permitem
repetir essa conferência; nomes stm_* continuam conceituais. Os esqueletos
newtm_* de ciclo de vida foram acrescentados na etapa seguinte descrita acima.
