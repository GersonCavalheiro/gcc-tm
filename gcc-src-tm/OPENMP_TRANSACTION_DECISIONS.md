# Decisões vigentes e extensões futuras de transaction

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

## Etapa 5.4 — ambiente léxico como extensão futura — 2026-10-31

Por orientação do usuário, registrar a alternativa B como possibilidade de
otimização futura: transportar explicitamente o ambiente léxico necessário às
chamadas de funções GNU aninhadas a partir de auxiliares preparados, preservando
identidade, classificação de armazenamento e vida útil das capturas.

Essa alternativa amplia a cobertura; não há ganho de desempenho demonstrado.
Sua implementação fica fora do escopo atual da 5.4. Por ora, conservar a cadeia
estática nativa nos casos compatíveis e a recusa explícita quando o frame léxico
necessário não estiver disponível. Este registro não implementa a extensão.

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


Atualizado em 2026-10-30. Este é o registro central para os agentes do projeto.
Consolida as decisões da conversa e os contratos implementados; não autoriza
ativar integrações ou ampliar o subconjunto. Instruções posteriores do usuário
prevalecem. Ao mudar uma decisão, registrar motivo, substituição e impacto nos
agentes, ABI e testes, conservando o histórico identificado como tal.

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

Escolha técnica vigente para F01: usar controle externo gerado por slot/status,
sem adaptação ao callback do executor. O laço e os epílogos serão gerados na
instrumentação. Nomes GOMP_transaction_stm_v1_* e GOMP_NEWTM_2.0 agora existem;
clones .newtm.v1 e canais incoming/result continuam para a tarefa seguinte.

## Como interpretar os estados

- **Adotada**: decisão explícita de projeto ou contrato já implementado a preservar.
- **Especificada, não ativada**: desenho da ABI para implementação futura; não
  significa que existam clones, símbolos exportados ou geração funcional.
- **Restrição conservadora atual**: limite da prova/implementação, não proibição
  definitiva da linguagem nem decisão do usuário de excluir toda extensão futura.
- **Adiada/em aberto**: extensão sem suporte; os pré-requisitos abaixo orientam
  trabalho futuro, sem escolher silenciosamente uma solução ainda não aprovada.

O [relatório de revisão](OPENMP_TRANSACTION_SUBSET_REVIEW.md) registra evidências
e limites da última entrega. A [ABI interna](OPENMP_TRANSACTION_ABI.md) detalha
transporte e chamadas futuras. Se o código divergir de um contrato adotado,
registrar a divergência e tratá-la; não reinterpretar a divergência como decisão.

## Decisões adotadas

| ID | Decisão e consequência |
| --- | --- |
| D01 | Desenvolver e verificar a correção do runtime antes da integração funcional compilador/GOMP. A entrega atual contém diagnósticos, identidade/sobreposição e ABI documentada. O pragma continua com rastreamento e memória comum. |
| D02 | Toda leitura ou escrita de objeto cuja vida começou antes da tentativa externa exige registro explícito já alcançado. Inclui constantes e acessos transitivos de funções preparadas. Sem registro implícito ou acesso alternativo à memória comum. |
| D03 | Chamadas diretas com definição na mesma unidade são selecionadas automaticamente e validadas transitivamente, inclusive definições posteriores e templates instanciados. Indiretas e externas sem preparação conhecida são rejeitadas. A análise/plano existe; clones executáveis ficam para a integração. |
| D04 | Retornos por referência e ponteiros sem identidade demonstrável são rejeitados no estágio atual. Um ponteiro listado registra o próprio objeto ponteiro, não o apontado. Endereços privados não podem escapar. |
| D05 | Campo/elemento usa objeto físico canônico e deslocamento em bytes. A extensão efetivamente registrada é preservada: referência a elemento não registra o array inteiro. Identidade igual exige tamanho/kind compatíveis; registros distintos sobrepostos são inválidos. |
| D06 | O contexto externo possui lease e lista de registros estáveis por tentativa. Aninhamentos compartilham o contexto. O slot proprietário tem endereço estável e não pode ser consumido por cópia ou outra execução. |
| D07 | Registrar só valida/deduplica e obtém referência à hash: não lê valores, cria visão privada ou obtém reservas. Falhas retornadas não deixam registro nem admissão de intervalo parcial. |
| D08 | Metadados compartilhados surgem sob demanda. Permitir leitores compartilhados, escritor exclusivo e promoção somente sem outros leitores. Reservas são conservadas até confirmação ou descarte. |
| D09 | Escalares usam buffer sob demanda; agregados usam buffer privado e listas de intervalos válidos/modificados. Acesso a campo não exige ler o agregado inteiro. Leituras observam as escritas da tentativa; erros preservam saídas, dados anteriores e memória original. |
| D10 | Antes de PUBLISHING, concluir toda validação/preparação recuperável. Publicar somente valores/intervalos modificados, mantendo todas as reservas até terminar todas as cópias. Depois da primeira publicação não há falha recuperável. Sucesso exige limpeza completa. |
| D11 | INVALID é permanente e domina conflito: propagar, limpar uma vez e só então diagnosticar fatalmente, sem retry. CONFLICT reinicia todo o bloco externo após descarte completo e espera progressiva. O executor atual recebe corpo explicitamente preparado. |
| D12 | A lease é liberada por último, após reservas, referências, callbacks e recursos privados. Não executar alocação, E/S ou callbacks sob mutex de bucket/índice. Destrutor de entrada libera só metadados compartilhados e exige ausência de reservas. |
| D13 | E/S, syscalls, locks do programa, alocação/liberação do programa, exceções e chamadas não preparadas ficam fora do subconjunto inicial. Logs e sincronização internos do runtime não autorizam esses efeitos no programa. |

A rejeição de sobreposição é global entre tentativas, inclusive durante aquisição
pendente na hash e antes de qualquer reserva. O índice de intervalos pertence à
política STM; a hash genérica, isoladamente, não prova disjunção. Retirar a faixa
somente depois de terminar release/destrutor da hash, mantendo a lease ativa.

## Desenho especificado para integração futura

A ABI interna v1 especifica contexto emprestado, tamanhos/offsets em bytes,
status de 32 bits (0 OK, 1 CONFLICT, 2 INVALID), kind de 32 bits (0 SCALAR,
1 AGGREGATE), bindings canônicos e resultado por saída tipada. Saídas permanecem
intactas em falha; verificar status antes de consumi-las. Argumentos, índices e
endereços devem ser avaliados uma única vez segundo as regras da linguagem.

A assinatura pública original é preservada. O desenho usa clones internos/hidden
com associação à definição original, bindings para parâmetros ponteiro/referência
e binding adicional para resultado ponteiro. Recursão exigirá preparação dos
protótipos do componente antes dos corpos. Contexto/bindings não transferem
propriedade nem registram implicitamente objetos.

Os nomes `.newtm.v1`, `GOMP_transaction_stm_v1_*` e `GOMP_NEWTM_2.0` são propostas
da especificação, não símbolos entregues nem ABI pública já estabilizada. A
escolha entre adaptar `newtm_execute` e gerar laço/epílogos externos continua
aberta. Não alterar silenciosamente a ponte void de `GOMP_NEWTM_1.0`.

## Restrições conservadoras atuais

O filtro rejeita volatile, atômicos, uniões, bitfields, membros flexíveis, VLA,
tipos incompletos/de tamanho variável, classes C++, acesso parcial de escalar
complexo preexistente e arrays/agregados contendo ponteiros. O último limite
impede copiar endereços privados sem acompanhar sua proveniência por campo.

A análise invalida vínculos de ponteiros modificados em laços e diante de
chamadas relevantes. Rejeita vínculos ponteiro/referência recursivos, efeitos de
ponteiros em ciclos recursivos e mutações encapsuladas em SAVE_EXPR quando não
prova identidade por ativação/avaliação única. Recursão escalar simples é admitida.
Registros condicionais que não dominam o acesso são recusados. Esses limites só
podem ser relaxados com prova e regressões que preservem D02–D05 e D09–D11.

## Extensões futuras e condições para retomada

| ID | Extensão e estado | Pré-requisitos e critério de aceitação | Agentes responsáveis |
| --- | --- | --- | --- |
| F01 | Integração funcional compilador/GOMP — avaliada sob autorização condicionada; ativação com bloqueios técnicos | Definir adaptação de captura/outlining e controle externo; emitir clones/acessos preparados e propagação imediata de status; validar limites antes de formar endereços. Testar conflito em função interna reiniciando o prefixo externo, INVALID sem repetição e ausência de publicação parcial. Etapas 1 e 2 implementadas: vínculos e descritores internos; faltam ABI executável, instrumentação, clones e epílogos. O relatório de prontidão e o contrato de ocorrências registram a evolução. | Projeto, escopo, acesso, OpenMP geral |
| F02 | Chamadas preparadas entre unidades, indiretas/virtuais e variádicas — sem suporte | Definir como comprovar preparação, transportar bindings e preservar associação de símbolos em ligação/LTO/interposição. Atributos pure/const/nothrow não bastam. O mecanismo de reconhecimento continua em aberto. | Projeto, escopo |
| F03 | Retornos por referência; identidades alternativas e ponteiro nulo — adiados | Especificar descritor, vida útil e propagação de identidade antes de aceitar. Provar ausência de escape de privados; nunca usar NULL/identidade desconhecida como acesso à memória comum. Sem representação escolhida para esses casos. | Escopo, acesso |
| F04 | Ponteiros em agregados, recursão com ponteiros e prova mais precisa em laços — extensão da análise | Acompanhar proveniência por campo/elemento e por ativação, efeitos transitivos e avaliação única de SAVE_EXPR. Cobrir cópia de agregados, mutação por callee, ciclos recursivos, desvios e escape. Não basta remover diagnósticos. | Escopo, acesso |
| F05 | Classes C++, cleanups e exceções — fora do subconjunto inicial | Definir construção/destruição, propagação de falha e limpeza em todos os caminhos, sem efeitos especulativos irreversíveis. Não converter exceções em conflitos nem confirmar em finally. Política de suporte ainda aberta. | Projeto, escopo, acesso |
| F06 | Alocação/liberação e outros efeitos externos do programa — fora do subconjunto inicial | Alocação exige rastrear recursos por tentativa e definir descarte/liberação adiada. Outros efeitos precisam de protocolo explícito que preserve D10–D11; não presumir que possam ser desfeitos ou repetidos. | Projeto, acesso, hash |
| F07 | Outros tipos, subacessos escalares e granularidade de conflito — sem ampliação aprovada | Definir representação, aliases, limites e publicação para cada categoria. A unidade de conflito atual é o objeto registrado, não cada intervalo modificado; mudar granularidade exige rever reservas, sobreposição e commit. | Escopo, acesso, hash |
| F08 | Índice de intervalos mais eficiente e política de espera — otimização futura | O índice linear e a espera ativa limitada são a base atual. Preservar admissão global atômica, candidatos pendentes e retirada após callback. Escolha de árvore/estrutura, justiça, escalonamento ou limite de tentativas não está decidida; medir antes de substituir. | Hash, implementação da hash, acesso |
| F09 | LTO ampliado, módulos C++, outros targets e Fortran — validação/escopo futuros | Os testes atuais incluem casos simples de LTO; não certificam módulos ou outros targets. Verificar preservação/remapeamento do plano e ABI do target. Fortran requer front-end e sintaxe próprios e não está incluído nesta entrega. | Projeto, escopo, OpenMP geral |

## Manutenção e evidências exigidas dos agentes

Atualizar este registro, o guia do agente afetado e a ABI quando uma nova tarefa
mudar um contrato. Identificar separadamente decisão do usuário, escolha técnica
e restrição temporária. Novas ideias entram como propostas; sua presença aqui
não as transforma em funcionalidade implementada ou autorização para integração.

Para cada extensão, registrar testes pertinentes, resultado e limite da evidência.
A última entrega passou 201 verificações do subconjunto e 37 casos existentes
por execução direta, além das regressões internas e sanitizadores descritos no
relatório. Isso não equivale à suíte inteira do GCC: DejaGNU estava indisponível;
a execução direta não interpreta dg-final. Não transportar resultados antigos
para código, configuração ou extensão que ainda não foram verificados.

## Exemplos executáveis do runtime interno — 2026-10-29

A pedido do usuário, foram acrescentados dois exemplos em transaction_test:
contador escalar compartilhado em região parallel e carga equivalente a
transaction_carga.c com leituras/escritas ponderadas e par valor/complemento.
A implementação usa corpos preparados explicitamente e a API interna, com
propagação de status, retry externo e commit reais; permanece dentro de D01.
Não usa critical para proteger os dados e não ativa a integração futura F01.
A ligação separada usa libgomp.a local para OpenMP e STM, sem duplicar runtime
com uma DSO do sistema. O descritor do callback é controle estável; resultados
especulativos permanecem privados. Novos exemplos devem preservar esse contrato.

Evidência específica: check-internos passou com -O2 -Wall -Wextra -Werror,
contador em 1/2/4 threads, carga em 1/4 threads com mil operações por thread,
proporções 1:9, 1:1, 9:1, 0:1 e 1:0 e argumentos inválidos. Esses exemplos são
testes funcionais, não benchmarks; os logs internos permanecem ativos.

## Avaliação para integração com o compilador — 2026-10-29

O usuário autorizou avaliar e implementar se viável, substituindo o adiamento
por escopo por uma avaliação de prontidão técnica. A [verificação específica](OPENMP_TRANSACTION_INTEGRATION_READINESS.md)
identificou bloqueios na associação ocorrência/temporário, transporte executável
de kind/bindings, geração de clones e controle de status/falha. A sonda confirma
acessos comuns e chamada à função original depois de omplower. A integração
funcional não foi ativada; não há evidência de atendimento ao critério solicitado.
O impedimento atual é técnico, não falta de autorização para avaliar a integração.

Preservar D02–D13 enquanto se prepara F01. O plano diagnóstico conserva receitas
com efeitos e não pode ser reavaliado como código. A sequência e a porta de
aceitação do relatório orientam a preparação; não constituem funções entregues.

## Etapa 1 autorizada e implementada — 2026-10-29

O usuário autorizou implementar a vinculação entre ocorrências e temporários
GIMPLE, sem ativar a integração funcional. O [contrato da etapa](OPENMP_TRANSACTION_OCCURRENCES.md)
registra a escolha técnica: chave por instrução, sites de origem identificados
sem heurística textual, grupos de avaliação e consulta a operandos/resultados
atuais. A origem GENERIC é proveniência, nunca receita a executar. Cópias mantêm
a origem e recebem ID operacional novo. A janela é High GIMPLE e termina antes
de CFG/SSA; não presumir persistência em otimizações, módulos ou streaming LTO.

Testemunhos source-only são explicitamente não avaliados e sem resultado. Sua
reconciliação/verificação de registro pertence à futura instrumentação; não
inventar valores ou permitir acesso comum como substituição. F01 permanece
parcial: a próxima etapa materializa descritores a partir dos operandos avaliados.
Esta decisão técnica não amplia o subconjunto nem a ABI executável.

## Etapa 2: transporte de registros e identidade

Implementados [descritores internos](OPENMP_TRANSACTION_DESCRIPTORS.md) com chave
lógica, extensão, offset e kind capturado do tipo listado. Cálculos checados
preservam índices avaliados, offsets assinados e limites por dimensão. Estado
incoming/result representa binding pendente através de chamadas; não simular
campos completos nem OK. Unknown/source-only devem ser resolvidos ou rejeitados
pela instrumentação. Não ampliar o subconjunto nem ativar GOMP nesta entrega.

Decisão para extensões: a janela dos descritores termina antes de omplower;
cópias ficam stale, exigindo reconstrução com os operandos/objetos remapeados.
Uma futura extensão para CFG/SSA deve definir propagação/mesclagem de identidades,
invalidar provas após stores/chamadas e preservar avaliação única. Tokens de
origem substituem raízes para helpers GENERIC liberados explicitamente. Testes
nativos 32 bits e backends alternativos permanecem pendentes antes de prometer
cobertura nesses alvos. Os contratos D02–D05 não autorizam fallback em unknown.
