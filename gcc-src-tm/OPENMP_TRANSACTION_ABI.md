# ABI STM v1 — fronteira executável, integração do pragma desativada

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

## Especificação original e desenho dos futuros clones

A fronteira GOMP descrita abaixo foi concretizada na etapa 3 com os acréscimos
fail/discard, nest_begin/nest_end e outermost uint32_t. Afirmações de ausência de símbolos ou ABI
executável abaixo são históricas; o contrato vigente está no relatório da etapa 3.


Decisões vigentes, estado das propostas e condições para extensões futuras:
[registro central dos agentes](OPENMP_TRANSACTION_DECISIONS.md).


Esta especificação define o contrato entre futuras versões preparadas pelo
compilador e o runtime interno já implementado. Não acrescenta símbolos à DSO,
não modifica a ABI de rastreamento `GOMP_NEWTM_1.0` e não ativa execução STM do
`#pragma omp transaction`. Os planos de identidade desta entrega são metadados
sem avaliação; não são chamadas de acesso, clones executáveis ou verificações
dinâmicas de limites já emitidas.

## Seleção e ligação

Preparar automaticamente chamadas diretas com definição disponível na mesma
unidade de compilação, inclusive definições posteriores e instanciações de
templates. Validar transitivamente os corpos. Chamadas indiretas, virtuais,
variádicas e externas sem preparação conhecida são rejeitadas; atributos
pure/const/nothrow não fornecem preparação. Componentes recursivos precisam de
protótipos de todos os clones antes da geração dos corpos.

A assinatura e o símbolo públicos originais permanecem intactos. O clone tem
ligação interna ou visibilidade hidden, sufixo de assembler `.newtm.v1` e não
está sujeito à interposição da versão pública. Instanciações/COMDAT devem
preservar a associação ODR com a definição original; LTO deve preservar essa
associação sem transformar o clone em ABI pública. Os nomes são reservados à
geração futura, não símbolos entregues nesta etapa.

## Tipos de transporte

- Contexto: `struct newtm_context *`, opaco e emprestado ao clone.
- Deslocamentos, extensões e tamanhos: `size_t`, em bytes.
- Status na fronteira compilador/runtime: inteiro sem sinal de 32 bits, com
  constantes fixas 0=OK, 1=CONFLICT e 2=INVALID. Valores desconhecidos são INVALID.
  As traduções dos enums internos de hash/acesso/lifecycle são explícitas.
- Classificação do objeto canônico: inteiro sem sinal de 32 bits,
  0=SCALAR, 1=AGGREGATE. Referências classificam o referido; arrays classificam
  como AGGREGATE, mesmo com um só elemento. Tamanho/alinhamento não inferem kind.

Descritor lógico de um argumento ou resultado ponteiro/referência:

```c
struct newtm_binding_v1 {
  void *object;       /* endereço canônico original, nunca buffer privado */
  size_t object_size;
  size_t offset;
  uint32_t kind;
};
```

Esta estrutura especifica campos e unidades, não representação serializada ou
layout independente de target. Ambos os lados usam o mesmo ABI de target. O
descritor não possui referência à hash, não registra implicitamente o objeto e
não autoriza um acesso. O contexto é a autoridade para registro, tamanho, kind
e reserva; descritores incompatíveis produzem INVALID.

Na primeira implementação, o compilador deve provar um objeto canônico único.
Ponteiros de origem arbitrária, resultados de inteiros/endereço externo e
retornos sem identidade demonstrável são rejeitados. Retornos por referência
estão excluídos. A extensão para identidades alternativas/ponteiro nulo deve
definir explicitamente o respectivo descritor antes de admiti-los; não usar
object=NULL como fallback de memória comum.

## Assinatura das funções preparadas

Para uma função original `R f(A1 a1, ..., An an)`, o clone recebe:

```text
uint32_t f.newtm.v1(ctx, bindings, a1, ..., an, result [, result_binding])
```

`bindings` é um vetor somente leitura de descritores, na ordem dos parâmetros
ponteiro/referência da assinatura original. O protótipo do clone determina seu
comprimento; funções variádicas não são admitidas. Argumentos por valor seguem
as convenções originais de target e suas cópias são privados da chamada. Uma
referência continua recebendo seu endereço lógico conforme o ABI C++, acrescido
do descritor canônico correspondente. Não acessar a visão comum por esse endereço.

Para R diferente de void, `result` aponta para armazenamento tipado privado do
chamador. Para R=void, esse parâmetro é omitido. Se R é ponteiro admitido,
`result_binding` é obrigatório e transporta a identidade do valor retornado.
Outras categorias não recebem result_binding. Agregados C admitidos usam
armazenamento de resultado do tipo original; classes C++ permanecem excluídas.

Resultados e descritores de resultado só são preenchidos em OK. Em CONFLICT ou
INVALID ficam integralmente preservados; o chamador verifica status antes de
consumir qualquer resultado. O clone conserva resultados intermediários em
temporários e copia para as saídas apenas no epílogo de sucesso. Retornos por
referência e endereços de temporários privados que escapem são rejeitados.

Índices, argumentos e expressões de endereço são avaliados exatamente uma vez,
na ordem determinada pela linguagem. O descritor de resultado é produzido pela
chamada, não pela reavaliação de seus argumentos no chamador. Um deslocamento
dependente de parâmetro pertence ao corpo preparado que o calcula; o chamador
usa o descritor retornado. Receitas no plano atual não autorizam repetir índices
ou executar expressões fora de seu escopo.

## Identidade, limites e permissões

`s.campo` e `vetor[i]` usam a chave do objeto listado inteiro e offset em bytes.
Uma referência listada a subobjeto conserva a extensão e classificação do referido:
`int &r = a[1]` registra apenas esse escalar. O plano conserva nove campos:
declaração base física, offset da base registrada, extensão registrada, kind,
offset relativo do acesso, tamanho do acesso, modo (1 leitura, 2 escrita,
4 preparação de chamada), função e localização. Receitas não são operandos
executáveis dos walkers genéricos; cópias remapeiam as declarações explicitamente.
O objeto inteiro continua sendo a unidade de conflito. Ponteiro listado registra
o objeto ponteiro, não a memória apontada. `&x` mantém o endereço original.
Aliases provados são normalizados para a mesma chave. O plano retém declarações
resolvidas, não nomes a pesquisar novamente; cópia/outlining remapeiam declarações.

Antes de formar um endereço dinâmico, verificar índice/deslocamento e
`offset <= extent && size <= extent-offset`, sem somas/multiplicações que possam
transbordar. Índices inferiores à origem do array também são inválidos. Campos
de bit, uniões, membros flexíveis, VLA, volatile e atômicos são excluídos. Validar
arrays/agregados recursivamente; writes const são proibidos. O filtro inicial
rejeita agregados/arrays contendo ponteiros, porque ainda não acompanha a
proveniência desses valores individualmente. Leituras, inicializações e
atribuições de ponteiros escalares exigem identidade conhecida; publicação de
endereços de objetos privados e conversões ponteiro/inteiro são rejeitadas. Temporários privados
permitidos são recriados a cada tentativa e não escapam.

Registro deve ter sido alcançado antes de cada acesso a dados preexistentes,
inclusive leituras constantes que possam ser dobradas pelo front-end. Não há
registro implícito nem fallback. A análise usa interseção de registros em desvios
e não assume que um laço execute. Casos sem prova estática de registro são
rejeitados nesta etapa. Identidades de ponteiros modificados em laços são
invalidadas conservadoramente; vínculos ponteiro/referência recursivos e efeitos
de ponteiros em ciclos recursivos exigem identidade por ativação e são rejeitados.
Mutações de ponteiros encapsuladas em SAVE_EXPR também são conservadoramente
rejeitadas. Isso não impede recursão escalar sem esses vínculos. Futura integração só pode transferir essa verificação
para o runtime mantendo o mesmo contrato de falha permanente.

O runtime admite intervalos globalmente, antes da aquisição da hash. Chaves
iguais só compartilham uma geração com tamanho/kind compatíveis. Intervalos
distintos sobrepostos são INVALID, mesmo entre contextos ou sem reserva ativa.
Uma aquisição pendente já retém sua faixa. Remover a faixa somente após terminar
release/destrutor da hash, mantendo a lease durante toda a operação.

## Fronteira futura GOMP

Código de aplicação ligado à DSO não pode chamar símbolos hidden `newtm_*`.
A ativação exigirá outra família de símbolos exportados/versionados, proposta
`GOMP_transaction_stm_v1_*`, em versão própria `GOMP_NEWTM_2.0`. Não alterar
silenciosamente as assinaturas void da ponte de rastreamento existente.

Operações propostas, com status de 32 bits:

```text
begin(slot, outermost)
register(ctx, object, extent, kind)
read(ctx, object, offset, destination, size)
write(ctx, object, offset, source, size)
end(slot)
after_failure(slot, consecutive_conflicts)
```

Begin externo adquire/transfere a lease internamente; begin interno compartilha
o slot canônico. Novos slots proprietários copiados não são permitidos. O
compilador pode reutilizar o executor de corpos preparados ou gerar laço/epílogos
equivalentes, mas deve especificar e testar a adaptação de captura/outlining
antes de ativar uma dessas formas.

## Propagação e encerramento

Falhas seguem imediatamente por epílogos de funções preparadas até o controle
externo. Não consumir saída inválida nem continuar efeitos normais. INVALID
domina CONFLICT. Funções internas não confirmam nem descartam a tentativa
externa. Exceções, cleanups do usuário, E/S, syscalls, locks e alocação/liberação
do programa estão excluídos; o runtime mantém sua própria limpeza obrigatória.

Confirmação externa valida/prepara tudo antes de PUBLISHING; publica somente
modificações, retendo todas as reservas até terminar todas as cópias. OK exige
limpeza completa. Em falha anterior à publicação, preservar causa e recursos
até o descarte explícito, executado uma vez após os epílogos suportados.

Depois do descarte completo, CONFLICT aplica backoff e reinicia toda a região
externa, recriando registros e temporários. INVALID faz diagnóstico fatal após
limpeza e nunca repete. Não usar setjmp/longjmp através de frames C/C++, não
confirmar em finally e não converter exceções em conflitos.

## Estado da preparação para geração de código

A [avaliação de prontidão](OPENMP_TRANSACTION_INTEGRATION_READINESS.md) confirma
que esta especificação ainda não é uma ABI executável do compilador. A ativação
exige ligação entre ocorrências/temporários, descritores materializados, clones
e epílogos de status. Não reavaliar receitas do plano nem usar a ABI void antiga
como se propagasse falhas. A autorização condicionada do usuário não altera o
fato de que esses mecanismos ainda não foram implementados.

## Preparação de operandos concluída: etapa 1

A [vinculação de ocorrências](OPENMP_TRANSACTION_OCCURRENCES.md) oferece pontos de
consumo em High GIMPLE, com operandos efetivos e proveniência. O layout de seus
sites não é ABI runtime. A geração futura de descritores e acessos deve consumir
os operandos avaliados antes de folds/CFG/SSA; não executar o plano diagnóstico.
Source-only não tem resultado e exige reconciliação ou diagnóstico. Isso não
implementa as assinaturas executáveis, clones ou propagação de falhas acima.

## Descritores internos implementados: etapa 2

O [contrato de descritores](OPENMP_TRANSACTION_DESCRIPTORS.md) materializa os
campos de registro/acesso em High GIMPLE, sem alterar assinaturas GOMP. Kind vem
do tipo listado e a chave continua lógica. Incoming e result são canais
explícitos para a futura ligação de parâmetros/resultados; não constituem uma
ABI de chamada executável. Unknown/source-only/stale não fornecem resultado
válido. Clones devem reconstruir descritores após remapeamento e transportar
contexto/status antes de instrumentar acessos. A validade de faixa calculada
precisa ser consumida antes da operação; a ponte atual ainda não o faz.
