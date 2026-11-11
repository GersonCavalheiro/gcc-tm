# Ponto de retomada — após o bloco 8 da 6.5.3

**Estado vigente em 2026-11-11: passo 7 concluído; etapa 6.6 APROVADA E
CONCLUÍDA no domínio experimental documentado.** Ver
[parecer final](OPENMP_TRANSACTION_ACCEPTANCE_FINAL.md) e
.build/diagnostic-step7/report.json. As pendências obrigatórias dos
diagnósticos foram corrigidas e os passos 5–6 passaram no build conferido.
Não foram executadas novas baterias no passo 7. Não há ampliação de domínio,
commit, publicação ou nova tarefa implícita. As entradas seguintes são
históricas e não substituem esse estado vigente.

Passo 6 concluído em 2026-11-11: regressões afetadas passaram no mesmo build
do passo 5. DejaGNU transaction/critical: seis seleções, 629 PASS e zero
resultados inesperados. Regressão transitiva: 317 verificações e 32 execuções,
sem ICE. Ver OPENMP_TRANSACTION_DIAGNOSTIC_STEP6.md e
.build/diagnostic-step6/report.json. Sem alterações de produção/testes nesta
rodada. Próximo: passo 7, parecer final de aceitação; a 6.6 continua aberta.

Passo 5 concluído em 2026-11-11: build incremental e matriz do passo 4
aprovados, 93 negativos e 45 controles executados. Ver
OPENMP_TRANSACTION_DIAGNOSTIC_STEP5.md e .build/diagnostic-step5/report.json.
Sem alteração de produção nesta rodada. Próximo: passo 6, regressões de
mensagens/seleções afetadas; depois parecer final do passo 7. A 6.6 segue aberta.

Passo 4 concluído em 2026-11-11: negativos preparados, com regras, causas,
controles e limites em OPENMP_TRANSACTION_DIAGNOSTIC_STEP4.md e
transaction_test/diagnostic_negative_plan.json. Acrescentado caso agregado
de dependência declarada não preparada; smoke C/O0/checking0 passou (uma
recusa e um controle executado). Matriz completa ainda não executada neste
passo. Próximo: passo 5. A 6.6 continua aberta.

Passo 3 concluído: ver OPENMP_TRANSACTION_DIAGNOSTIC_STEP3.md. Causa/regra
da análise de armazenamento agora são transportadas explicitamente; PRIVATE
sem alocação da ativação, tag inválida e ramo incorreto usam V04, com V03
preservada para identidade/faixa e para a primeira falha anterior.
Build aprovado; 56 casos de componente e 36 integrados passaram; regressões
de memória 281/32 e status 449/100 aprovadas. A 6.6 não foi encerrada.

Passo 2 concluído: ver OPENMP_TRANSACTION_DIAGNOSTIC_STEP2.md. Os 36 IDs foram
uniformizados em 38 emissores, com regras V06/V07 do controlador separadas,
fallback de localização e contexto/cadeia de chamadas atual. all-gcc passou;
validação dirigida: 49 recusas e 25 execuções positivas. Próximo: passo 3,
classificação correta de causas PRIVATE/V04 em resolver::ranges. A 6.6 segue aberta.

Passo 1 da correção de diagnósticos concluído: ver
OPENMP_TRANSACTION_DIAGNOSTIC_INVENTORY.md e
transaction_test/diagnostic_inventory.json. Inventário estático de 36 pontos
(35 antigos e um parcial), sem alteração do compilador ou novas baterias.
Próxima ação autorizável: passo 2, uniformização, observando a separação
das condições V07/V06 do controlador registrada no inventário.

**Estado vigente após revisão documental/estática de 2026-11-10:**
6.6 NÃO CONCLUÍDA. Regressões e contraprovas executadas aprovadas, mas
persistem recusas atuais sem contexto completo e classificação V03 incorreta
para uma causa PRIVATE/V04. Ver OPENMP_TRANSACTION_FINAL_REVIEW.md.
Próxima ação delimitada: corrigir esses caminhos de diagnóstico e validar
negativos afetados, sem repetir indiscriminadamente todas as baterias.
A revisão não executou novos testes nem iniciou as correções.
Os checkpoints seguintes são históricos da sequência de trabalho.

Atualização de 2026-11-10: a 6.6 foi iniciada por solicitação do usuário.
Levantamento e testes dirigidos checking=0 concluídos; ver
[evidências consolidadas](OPENMP_TRANSACTION_ACCEPTANCE_EVIDENCE.md).
Aceitação parcial: 231 verificações e 40 execuções recentes, incluindo
68 recusas LTO. A 6.6 permanece aberta; as pendências estão no relatório.
O estado abaixo é o checkpoint histórico anterior ao início da 6.6.

Build confirmado posteriormente em 2026-11-10: make integral incremental
concluído, após corrigir caminhos antigos em quatro arquivos gerados da
libgomp; oito compilações/links/execuções C/C++ com/sem LTO e DSO/archive
aprovados. Ver relatório de evidências e build-confirmation/report.json.

Auditoria posterior concluída com lacunas: ver
[auditoria V01–V08](OPENMP_TRANSACTION_ACCEPTANCE_AUDIT.md).
Recusas dirigidas e positivos passaram no build confirmado, mas diagnósticos
e contraprovas integradas específicas ainda impedem fechar a 6.6.

Contraprovas dirigidas posteriores: PRIVATE/V04, consumo antecipado/V05,
store de agregado retirado e certificado após fechamento da janela foram
recusados na composição nativa. Passaram 28 recusas e oito controles,
C/C++ O0/O2 checking=0/2, agregado somente C. Ver seção final da auditoria.
Retirada da janela atual não equivale a injetar um descritor expirado.
A 6.6 continua aberta por diagnósticos e validações restantes.

Diagnósticos reproduzidos corrigidos em seguida: fallback de localização,
regra/operação/função, raiz/controlador e contexto LTO. all-gcc e testes
dirigidos passaram: 45 recusas com contexto e 13 execuções positivas.
Ver OPENMP_TRANSACTION_DIAGNOSTICS.md e .build/diagnostics/report.json.
Não se reivindica conversão de todas as mensagens antigas ou reconstrução
de toda cadeia intermediária; a 6.6 continua aberta por validações restantes.

Contraprovas LTO ampliadas passaram: WPA, LTRANS balanced/max/1to1,
monolítica, incremental e fat. Foram 126 casos novos; incluindo regressões
auxiliares, 167 recusas esperadas e 62 execuções positivas nesta rodada.
Ver OPENMP_TRANSACTION_LTO_ACCEPTANCE.md e .build/lto-phase-acceptance/report.json.
Pareamentos O0/checking0/DSO e O2/checking2/archive, agregado somente C.
Próxima pendência principal: regressões DejaGNU transaction/critical,
seguidas de revisão final de rastreabilidade; manter as limitações documentadas.

Pendência DejaGNU concluída posteriormente: seis suítes transaction/critical,
C/C++ e libgomp, total 629 PASS e zero resultados inesperados nas execuções
aceitas. DejaGNU/Expect usados por extração local sem sudo. Foram corrigidos
somente cinco testes e a seleção local de libgomp.spec no site.exp gerado.
Ver OPENMP_TRANSACTION_DEJAGNU.md e .build/dejagnu/accepted/report.json.
Próximo trabalho: revisão final de rastreabilidade V01–V08 e escopo de aceitação.

Atualizado em 2026-11-07. Bloco 8 concluído e validado no domínio conservador
documentado. A 6.5.3 está concluída nesse domínio; o próximo trabalho é a
etapa 6.6, aceitação integral. Não iniciá-la sem nova solicitação do usuário.

## Estado atual

- Tarefas 1–5 implementadas no domínio funcional opcional documentado.
- 6.1–6.4, 6.5.1–6.5.2 e blocos 1–8 da 6.5.3 concluídos nos seus domínios.
- O frontend e cada fase LTO constroem gerações próprias de prova atual.
  Nenhum certificado, resultado consolidado ou aprovação é serializado.
- Contratos declarativos, tipos, constantes e identidades escalares são
  transportados. Não há transporte de GIMPLE, SSA_NAME ou mapas operacionais
  por instrução; `newtm_region_sources` fica fora do envelope.
- IDs locais têm namespace por unidade. As declarações dos operandos reais
  prevalecem sobre cópias vindas de outros streams de contratos.
- A correspondência SSA exige versões alcançáveis e argumentos corretos de
  PHI em suas próprias arestas. A visão temporária restaura integralmente o
  SSA carregado, incluindo PHIs e SSA de memória, antes da publicação.
- As raízes vêm das chamadas atuais e das regiões obrigatórias de um
  controlador vivo. BEGIN/invocações/END são confrontados: remover a chamada
  não faz uma região corrompida desaparecer da seleção.
- Chamadores das funções preparadas do modo funcional precisam pertencer à
  unidade de prova. O modo de origem é registrado e validado. O controle
  externo manual da ABI, fora desse modo, conserva seu funcionamento sem
  publicar raízes ou receber aprovação de grafo.
- Auxiliares preparados preservados sem controlador ativo levam contratos,
  mas não recebem raízes nem certificados fictícios.
- `newtm_current_get` concede APPROVED atomicamente, required/proved 31 e
  dependências zero, apenas na janela atual. Componentes continuam DEPENDENT;
  o getter histórico continua com current_approved=false.
- WPA, LTRANS, LTO monolítico e incremental reexecutam as provas. Alterações
  revogam raízes dependentes; restaurar IR não reanima uma geração. Uma
  reconstrução completa cria outro identificador. A autoridade é retirada
  antes das transformações seguintes.
- Os descritores continuam retirados na entrada de omplower. A reconstrução
  usa fatos congelados na janela correta e operandos atuais, sem consulta
  tardia a descritores ou replay de expressões de origem.
- ABI/runtime/layouts e canais incoming/result preservados. Alternativa B
  continua otimização futura, fora de novos requisitos deste bloco.

## Domínio e custo

O formato desta árvore é LTO 16.1. Recompilar objetos 16.0: eles são recusados
pelo controle de versão, antes de interpretar os corpos. A ABI v1 do runtime
e seu versionamento de símbolos permanecem iguais.

A unidade de prova usa temporariamente O0/noipa até a reconstrução final;
as opções, atributos e condição de inlining originais são restaurados depois.
Objetos fat restauram também o caminho nativo após o streaming. Links
incrementais conservam os contratos até a fase final. Os membros protegidos
ficam juntos em LTRANS, preservando as políticas de partição do código comum.
Há custo de otimizações prévias, memória e streaming, detalhado no relatório.
Não se reivindica reconstrução de qualquer SSA arbitrariamente otimizado.

Continuam as restrições do preflight: não são gerados clones remotos para
funções sem definição admitida na unidade de preparação. Testes entre unidades
usam chamadas comuns a funções com controladores próprios. Agregados
compartilhados do fixture de integração continuam restritos a C.

## Referências e implementação

Ler [progresso consolidado](OPENMP_TRANSACTION_GRAPH_PROGRESS.md) e
[reconstrução em LTO](OPENMP_TRANSACTION_LTO_GRAPH.md). O
[certificado do frontend](OPENMP_TRANSACTION_CURRENT_GRAPH.md) e os contratos
de CURRENT_COVERAGE, CURRENT_OWNER, CURRENT_STATUS, CHANNELS e MEMORY continuam
válidos nos seus escopos. Estados vigentes prevalecem sobre relatos históricos.

Implementação principal: gcc/newtm-lto.{cc,h}, newtm-current.{cc,h},
newtm-clone.cc, newtm-memory.cc, newtm-access.cc, newtm-abi.{cc,h},
newtm-guard.cc, tree-into-ssa.cc, cgraphunit.cc, lto-streamer-in/out.cc,
lto-streamer.h, lto/lto.cc, lto/lto-partition.cc e Makefile.in.

Testes: check-grafo-lto, verificar_grafo_lto.py, lto_graph_probe_plugin.cc,
lto_graph.c, lto_graph_other.c, lto_graph_unused.c e lto_graph_template.C.
O teste do bloco 7 também passou a observar aprovação LTO reconstruída,
seguida de revogação, em vez da antiga ausência de implementação LTO.

## Validação concluída

- Build all-gcc concluído sem avisos novos; self-tests internos desabilitados.
- Grafo LTO: **426 verificações e 122 execuções** na matriz integral final.
- Grafo atual: 543 verificações e 212 execuções; histórico: 317 verificações.
- Cobertura atual: 695/156; canais: 241/44; memória: 281/32.
- Propriedade atual: 553/88; status atual: 449/100.
- Guardas: 102 verificações, 24 execuções e 1036 cenários sintéticos.
- Controlador: 225/106; integração: 1078/930.
- ABI/versionamento: 88 verificações; chamadas: 75/18.
- Higiene do diff, arquivos novos e sintaxe Python: aprovadas.
- C/C++, O0/O2, checking 0/2, GC, DSO/archive, GNU, EH externo, parallel,
  transações aninhadas, recursão, COMDAT, unidades múltiplas e partições
  balanced/max/1to1/monolítica, link incremental e objetos fat.
- Negativos incluem versão/modo/envelope/mapa/raiz/componente inválidos,
  identidade/layout, acesso residual, guarda, SSA antigo, PHI, contexto,
  status, proprietário, resultado e supressão da invocação do auxiliar.
- Remover a chamada revelou uma fuga durante a revisão; o seletor foi
  corrigido e o caso integrou a bateria final. A checagem de chamadores foi
  delimitada ao modo funcional para preservar o driver manual da ABI.
- Resultados limitados ao ambiente nativo Linux x86_64. A 6.6 permanece
  necessária para a aceitação integral do projeto.

Logs finais: /tmp/stm-lto-graph-build.log, /tmp/stm-lto-graph-tests.log,
/tmp/stm-lto-graph-regression.log, /tmp/stm-lto-graph-components.log,
/tmp/stm-lto-graph-protocols.log, /tmp/stm-lto-graph-execution-final.log e
/tmp/stm-lto-graph-calls-final.log. Contraprovas da revisão em
/tmp/stm-lto-entry-before-fix.log, /tmp/stm-lto-entry-after-fix.log e
/tmp/stm-lto-caller-closure.log. Artefatos em transaction_test/.build/lto-graph/.
Os logs intermediários de desenvolvimento não substituem esses resultados.

As alterações estão no workspace; este checkpoint não é commit Git nem backup
do ambiente. Preservar alterações preexistentes, inclusive Dockerfile.gcc-tm
e os muitos arquivos novos de etapas anteriores.

## Preferência de interação

O usuário prefere menos confirmações. Dentro de uma tarefa autorizada, executar
leituras, alterações reversíveis no projeto, builds e testes necessários sem
pedir confirmação conversacional a cada comando. Pedir decisão apenas quando
necessária e ainda não resolvida. Respeitar as aprovações impostas pelo cliente;
esta preferência não altera o sandbox nem concede acesso irrestrito ao host.

O bwrap falha ao criar namespaces neste ambiente. Os comandos necessários
foram executados por escalonamento com revisão automática. A política never
não corrige essa falha. Permissões são configuradas na invocação do Codex;
este arquivo não as modifica.

## Retomada

Nome sugerido: gcc-tm-bloco-8-concluido (renomear na CLI). Ao voltar, conferir
este checkpoint antes de repetir builds/testes. Próxima etapa: 6.6, aceitação
integral. O checkpoint não autoriza iniciá-la automaticamente.
