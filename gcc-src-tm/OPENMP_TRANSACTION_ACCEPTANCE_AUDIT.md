# Etapa 6.6 — auditoria de cobertura, diagnósticos e recusa integrada

Data: 2026-11-10. **Auditoria concluída com lacunas; aceitação integral não
concedida.** Foram examinados contrato, implementação e oráculos dos testes.
As execuções dirigidas abaixo usam o build confirmado, checking=0.
Nenhum fonte do compilador ou runtime foi alterado nesta auditoria.

## Recusa integrada observada

Foram recompilados 17 representantes C/O0 das mutações do plugin LTO. Para
cada um foram exigidos: compilação da mutação com sucesso, marcador de sua
aplicação, link recusado, diagnóstico do detector específico esperado,
ausência de ICE, ausência de certificado positivo emitido pelo observador
do plugin e ausência do executável de saída. O erro ocorre no caminho normal
de reconstrução/composição do link, sem chamada direta do plugin a um
verificador de componente. Isso é mais forte que apenas localizar um erro
genérico ou o marcador de mutação.

O mesmo fixture sem mutação foi aprovado em WPA/LTRANS (três raízes) e
executado. Não se trata de uma entrada que já falha antes da mutação.
As 68 recusas anteriores cobrem C/C++ O0/O2; esta rodada dirigida executa
um representante de cada modo após a confirmação do build, sem repetir a
matriz inteira. Não estende a evidência a LTRANS corrompido separadamente:
os negativos são introduzidos antes do streaming e recusados no link LTO.

| Mutação | Detector efetivamente observado | Conclusão permitida |
| --- | --- | --- |
| version/mode/envelope/map | Versão, modo ou seção/mapa de contratos inválidos | V08: envelope inválido não concede aprovação. |
| roots/component/missing | Raiz ausente, contrato de memória ausente ou chamador desprotegido | V01/V06/V08: perdas de contratos não eliminam silenciosamente a exigência. |
| raw | Cobertura: uncovered direct read | V02: leitura residual sem origem é recusada. |
| layout | Assinatura/layout ABI incompatível | Parte estrutural de V03/V06. |
| ssa_use/phi | Correspondência SSA não reconstruível | V03/V08: versões/arestas atuais são confrontadas. |
| guard | Canais: resultado perdeu reaching producer | Recusa integrada confirmada, mas não demonstra isoladamente a prova de guardas V03. |
| context/channel | Contexto emprestado ou canais alterados | V06, com relação a propriedade e transporte. |
| status | Status: V07, epílogo substitui causa original | Não equivale ao teste V05 de consumo antecipado de valor/binding. |
| owner/entry | Slot proprietário diferente ou invocação preparada ausente | V07 e fechamento do controlador. |

V01–V08 são conjunções de requisitos; uma mutação representativa não fecha
automaticamente todos os requisitos da regra associada.

## Casos normativos da seção 9

| Caso do contrato | Evidência auditada | Estado |
| --- | --- | --- |
| a[i++] += f(&local_privado) | acceptance_contract.c usa a expressão com touch; verifica i=1, alteração do elemento correto, local=3 e valor final. Grafo aprovado e programa executado em C/C++ O0/O2 LTO. | Positivo executável novo; não prova sozinho comportamento sob retry. |
| *destino = callee(origem) | Mesmo fixture, com ponteiro destino e retorno usado no store; valores finais conferidos. | Positivo executável novo C/C++ O0/O2 LTO. |
| *destino = *origem agregado | Mesmo fixture em C; origem e destino conferidos. obligation_probe_plugin.cc deriva store e invalida filho/pai. | Positivo novo; obrigação de escrita perdida tem teste sintético, ainda sem contraprova integrada específica. |
| Cópia/remapeamento com mesma origem | obligation_probe_plugin.cc exige cópia PENDING/INVALIDATED sem tratamento herdado; inspection_probe_plugin.cc recusa sequência copiada. Ambos reexecutados em C/O0/checking=0. | Prova sintética de identidade; não é mutação integrada específica de cópia de mesma origem. |
| Carga sem ocorrência | raw insere leitura global no SSA sem origem, e o link recusa por cobertura. | Recusa integrada nova. |
| PRIVATE forjado | memory_model_probe_plugin.cc, private_read/write e outras variantes, invoca newtm_memory_verify_cfg diretamente. | Componente coberto; falta contraprova integrada dirigida V04. |
| Guarda de status removida ou de chamada errada | current_status_probe_plugin.cc, old_status/early/binding_early, invoca newtm_status_verify_cfg/entry diretamente. O guard LTO é interceptado pela análise de canais. | Falta contraprova integrada dirigida V05. |
| Stub de falha sem operações residuais | calls_refused.c com modo funcional ativo é recusado por source-only; execução nova C/O0/checking=0. newtm_clone_prepare retira corpo e gera FAIL; grafo atual recusa membro não preparado. | Recusa ativa confirmada no preflight; teste manual anterior de stub não equivale a aprovação de grafo. |
| Consulta após retirada dos descritores | inspection_probe_plugin.cc exige outside descriptor window; obligation_probe verifica RETIRED/PENDING sem anchor. Reexecução sintética aprovada. | Teste de API presente; falta contraprova integrada específica de evidência retirada reutilizada. |

O fixture novo passou em quatro compilações/links/execuções LTO, observando
aprovação agregada de três raízes em C e duas em C++, e revogação na fronteira.
Agregados permanecem restritos a C conforme o domínio documentado.

## Diagnósticos: lacuna confirmada

Dos 17 representantes negativos, **16 indicam arquivo/linha/coluna e apenas
dois explicitam V01–V08 (ambos V07)**. Não basta exigir o texto literal da
regra: memória/canais/cobertura e invariantes de envelope dão pistas da
categoria e da causa. Contudo, esses diagnósticos não identificam de forma
uniforme a função, a operação e a regra violada exigidas pela seção 8.

Contraprova concreta: raw produz apenas
`lto1: error: transaction transformed coverage proof failed: uncovered direct read`,
sem localização nem função. A instrução injetada não tem localização;
coverage_fail em newtm-memory.cc escolhe gimple_location quando há instrução,
mas não usa a localização da função como fallback se ela for desconhecida.
Memória/canais/status/propriedade usam padrão semelhante. Portanto há
uma lacuna de diagnóstico reproduzível, mesmo com recusa funcional correta.

newtm_obligation_report inclui função, espécie, obrigação, escopo, geração e
nota de ancestral, mas essa apresentação não é uniformemente utilizada nas
provas reconstruídas. Não foi demonstrada emissão de cadeia de chamada ou
derivação necessária para todos os erros transitivos. Não concluir conformidade
integral de diagnóstico apenas porque a compilação falha.

## Lacunas para fechamento por regra

- **V01:** acrescentar contraprova integrada de store derivado retirado;
  a invalidação do pai está demonstrada sinteticamente.
- **V02:** recusa de raw está demonstrada; completar diagnóstico com escopo
  e auditar a matriz residual completa na regressão final.
- **V03:** separar uma mutação real de faixa/overflow/identidade que alcance
  o detector pretendido. Não usar o nome guard como evidência suficiente.
- **V04:** levar PRIVATE forjado e os ramos DYNAMIC/REGISTERED relevantes até
  a reconstrução/composição real; preservar os testes próprios da ABI/runtime.
- **V05:** testar consumo/publicação antecipada ou status de outra chamada
  no grafo integrado e exigir detector compatível, não a substituição da causa V07.
- **V06:** recusa de context/channel/missing está demonstrada; completar
  diagnósticos e preservar os positivos de capturas/chamadas suportadas.
- **V07:** owner/entry/status mostram recusa; limpeza/commit/retry continuam
  dependendo também da regressão de integração/runtime, fora desta rodada.
- **V08:** envelope/SSA e fronteiras têm evidência; completar cópia/retirada
  integrada e as configurações LTO restantes, sem herdar certificados históricos.

## Artefatos e reprodução

- [report.json](../transaction_test/.build/acceptance-audit/report.json):
  códigos de saída, comandos, hashes de logs, diagnósticos dos negativos e positivos.
- [supplement.json](../transaction_test/.build/acceptance-audit/supplement.json):
  comandos de inventário/inspeção sintéticos e recusa de membro transitivo.
- Diretório transaction_test/.build/acceptance-audit: logs de cada operação.
- Runner principal: `python3 transaction_test/auditar_aceitacao.py gcc-build`.

O campo audit_execution_passed indica que os oráculos executados passaram;
não significa cobertura completa nem aprovação da 6.6. O runner principal
não reexecuta os comandos complementares registrados em supplement.json.
Os artefatos locais em .build continuam ignorados pelo Git.


## Contraprovas integradas posteriores — quatro casos dirigidos

Em 2026-11-10 passaram **36 casos: 28 recusas e oito controles executados**.
Matriz nativa C/C++, O0/O2, checking=0/2; agregado somente em C. Os controles
usam o mesmo fixture sem mutação, observando APPROVED com todas as classes
provadas e dependências zero, antes de executar e conferir seus resultados.

| Contraprova | Casos | Detector observado |
| --- | --- | --- |
| PRIVATE/V04 | 8 | Acesso no braço PRIVATE redirecionado a outro incoming, conservando a guarda; memory operation lost its current instance. |
| Consumo antecipado/V05 | 8 | Buffer READ copiado antes da guarda de sucesso; V05: result or binding consumed before its own call succeeded. |
| Store derivado retirado | 4 | WRITE de cópia de agregado substituído por status zero, preservando READ e continuação; memory operation changed its role. |
| Evidência retirada | 8 | Janela real de prova encerrada após aprovação; IR confirmado idêntico; recomposição recusada, geração antiga INVALID e nova REFUSED com proved=0. |

As três mutações de corpo são aplicadas após ompexp e deixadas até a composição
normal da unidade. O plugin não chama verificadores de componentes. Exige-se
falha com detector específico, sem ICE, objeto gerado ou aprovação residual.
Na retirada, usa-se a API agregada de produção para tentar recompor o grafo
após encerrar a janela; não é uma consulta isolada a componente.

Estas contraprovas fecham os quatro casos dirigidos no domínio nativo testado.
PRIVATE altera identidade no braço privado, sem falsificação separada da tag
numérica. Retirada testa reutilização do certificado após fechar a janela
atual, sem injeção de ponteiro de descritor expirado. O teste deste último
continua sintético. Não se reivindica nova cobertura LTO nem de todos os
ramos DYNAMIC/REGISTERED. V05 ainda emite diagnóstico sem função/localização.
Nenhum código de produção foi alterado; a 6.6 permanece aberta.

Reprodução: `python3 transaction_test/verificar_contraprovas_integradas.py gcc-build`.
Evidências: [report.json](../transaction_test/.build/integrated-counterproof/report.json),
com comandos, códigos de saída, diagnósticos e hashes de logs. As lacunas nas
seções anteriores descrevem o estado anterior a estas contraprovas.
