# Etapa 6.6 — fase 1: levantamento de cobertura e lacunas

Data: 2026-11-10. Levantamento estático dos contratos, scripts Python,
plugins e alvos Makefile presentes no workspace. Não foram executados builds
ou baterias de testes nesta fase. Os números do checkpoint são históricos,
não resultados novos nem comprovação de que o build corresponde aos fontes.
A 6.6 permanece aberta. Nenhum defeito funcional novo foi demonstrado.

## Base e escopo

Autoridades: [checkpoint](OPENMP_TRANSACTION_RETOMADA.md),
[estado consolidado](OPENMP_TRANSACTION_GRAPH_PROGRESS.md) e regras V01–V08
do [contrato](OPENMP_TRANSACTION_VERIFICATION.md#8-regras-de-aceitação-e-recusa).
Tarefas 1–5 e etapas até 6.5.3 estão concluídas nos domínios documentados.
Aceitação não amplia tipos, chamadas, exceções, frames ou plataformas.
Alternativa B e suporte a SSA arbitrariamente otimizado ficam fora do escopo.

Há compilador e libgomp em gcc-build; sua atualidade não foi verificada.
O workspace contém muitas alterações anteriores, inclusive arquivos novos;
os resultados futuros precisam identificar esse estado, além do commit HEAD.

## Matriz de cobertura existente

Os nomes check-* abaixo são alvos de transaction_test/Makefile. Cobertura
existente significa que o código de teste foi localizado, não aprovação atual.

| Regra | Evidência existente | Limite para a aceitação |
| --- | --- | --- |
| V01 — obrigações preservadas | check-obrigacoes, check-inspecao e check-transitivo; inventário O01–O07, derivação, reconciliação e invalidação. LTO tem negativos missing/map/roots. | Relacionar a retirada e a perda de obrigações à recusa do grafo atual, além do inventário histórico. |
| V02 — efeitos reais classificados | check-cobertura: read/write, aliases, índices, condições, argumentos, chamadas ocultas, retorno, volatile/asm e efeitos no prefixo do controlador; LTO raw. | A recusa de componente não substitui observação da recusa integrada; verificar também o oráculo de cada mutação. |
| V03 — identidade, faixa e guardas | check-guardas e check-memoria: limites/overflow, identidade, layout, extensão/kind, índice, early_address, aliases e binding estrangeiro; LTO layout/guard/ssa_use/phi. | Demonstrar os mesmos requisitos na fase efetivamente reconstruída e com checking desativado. |
| V04 — PRIVATE/REGISTERED/DYNAMIC | check-memoria: private_read/write, private_instance/clobber, choice/switch_choice e registro; check-canais e check-abi-v1. | Vincular prova do compilador à autorização real da ABI; não inferir correção do runtime só pela aprovação do grafo. |
| V05 — status e publicação | check-status-atual: old_status, overwrite, early/binding_early, private_dummy, causa, FAIL, publish_bypass, drop_final, unknown_return; LTO status/channel. | Conferir aprovação positiva, recusa negativa e ausência de publicação indevida no conjunto integrado. |
| V06 — chamadas e canais | check-chamadas, check-canais, check-transitivo e check-grafo-atual: incoming/result, vida útil, escape, assinatura, aresta, SCC, captura e produtor; LTO units/templates. | Casos atuais de mutação controller/producer usam checking=2; falta sua contraparte checking=0 no script do grafo atual. |
| V07 — propriedade e retry | check-proprietario-atual: remoção de BEGIN/corpo/STATUS/END/FAIL/limpeza, contexto/slot, causa, contador e retry; check-controlador e check-integracao injetam falhas por posição, conflitos repetidos e INVALID/desconhecido. | Consolidar prova do controlador atual e comportamento executado, incluindo descarte/limpeza antes de retry ou fatal. |
| V08 — geração e fase | check-grafo-atual: partial, notify, recheck, metadata, rebuild, failed, controller/producer e fronteira IPA; check-grafo-lto: reconstrução/revogação WPA/LTRANS/monolítica, incremental, formato e raízes inativas. | Negativos LTO e link LTO usam checking=2; falta provar recusa/revogação com checking=0 no link. |

O grafo atual observa APPROVED, dependências zero, revogação e nova geração.
Os testes de componentes frequentemente esperam DEPENDENT; isso é correto
para seu contrato, mas não equivale à aceitação agregada. Alguns testes de
mutação restauram o IR e compilam com sucesso após observar invalidação;
esse sucesso não significa que o programa corrompido foi aceito.

## Lacunas confirmadas nos testes e na rastreabilidade

1. **LTO com checking=0 incompleto — prioridade alta.**
   Em verificar_grafo_lto.py, compile_source aceita checking, mas o helper
   link fixa -fchecking=2. Os 17 modos negativos são compilados com o padrão
   checking=2. Positivos de compilação checking=0 não cobrem a reconstrução
   no link com checking=0. Acrescentar negativos e positivos nessa fronteira,
   preservando marcadores de mutação aplicada e diagnóstico esperado.
2. **Controlador/produtor no grafo atual — prioridade alta.**
   verificar_grafo_atual.py executa esses dois modos em C/C++ O0/O2 apenas
   com checking=2, embora a matriz principal use 0/2. Completar esse eixo.
3. **Aceitação sem relatório unificado — prioridade alta.**
   O Makefile oferece alvos individuais, sem alvo de aceitação 6.6.
   Os scripts de grafo/componentes atuais gravam logs e totais, mas não um
   resultado estruturado por requisito. Consolidar comando, configuração,
   regra, resultado esperado/obtido, duração e caminhos de evidência.
   Falha ou timeout deve impedir aprovação; zero casos não conta como sucesso.
4. **Regressão simplificada não substitui DejaGNU — prioridade alta.**
   verificar_regressao_compilador.py não interpreta dg-final e seleciona
   somente transaction*, sem critical*. Executar as suítes transaction e
   critical de compilador/runtime pelo DejaGNU, com biblioteca local e
   preservação de .sum/.log, ou registrar explicitamente a pendência.
5. **Asserções Python podem desaparecer — prioridade média.**
   Os scripts atuais de grafo, memória, canais, status, propriedade e cobertura
   usam assert sem proteção __debug__/PYTHONOPTIMIZE. O executor de aceitação
   deve recusar Python otimizado antes de iniciar os testes.
6. **Documentação de estado divergente — prioridade média.**
   README e trechos históricos descrevem etapas já concluídas como futuras.
   Atualizar o panorama ao consolidar a aceitação; manter identificados os
   limites do modo opcional -fopenmp-transaction-stm.

## Pontos que exigem investigação, sem defeito confirmado

- Diagnósticos: o contrato exige regra, operação, função, localização e causa;
  os scripts atuais verificam principalmente prefixos e marcadores. Auditar
  campos e encadeamento de chamadas nos negativos representativos. Ausência
  do texto literal V01–V08 não demonstra, sozinha, violação do contrato.
- Casos de mesa da seção 9 do contrato: conferir individualmente expressões
  com efeitos, chamada mais store, cópia de agregado e derivações. A afinidade
  com um fixture não basta para declarar a expressão específica coberta.
- Oráculos integrados: selecionar mutações de V01–V07 que alcancem a composição
  real e confirmar recusa sem aprovação residual; não duplicar toda a matriz
  local se os casos existentes já demonstrarem essa propriedade.
- Resultados históricos: não há medição nova de duração ou consumo de cota.
  Não estimar custo a partir da soma de verificações, que conta operações
  heterogêneas e não mede consumo do agente.

## Fases seguintes propostas

| Fase | Entrega e critério de saída |
| --- | --- |
| 2 — fechar lacunas de teste | Completar checking=0 em LTO e controller/producer; auditar oráculos/diagnósticos/casos de mesa; criar execução de aceitação com proteção das asserções e relatório por regra. Primeiro validar somente os casos alterados. |
| 3 — aceitação dos grafos | Executar componentes atuais e grafo atual; LTO em seções positive, negative, fixtures, units, format e templates. Preservar logs por execução: as seções compartilham diretório e alguns nomes de log. |
| 4 — runtime e regressões | Integração/controlador, ABI/chamadas, contratos internos relevantes e DejaGNU transaction/critical. Evitar repetição do controlador: check-integracao já depende dele. |
| 5 — consolidação | Resolver falhas, repetir somente conjuntos afetados e consolidar aprovação V01–V08, ambiente, comandos, ressalvas e documentação atualizada. |

Priorizar o domínio nativo Linux x86_64 já adotado: C/C++, O0/O2,
checking 0/2, GC forçado e DSO/archive, com exceções documentadas
(agregados compartilhados de integração em C, EH externo, preflight
conservador). Aceitação no domínio suportado não exige ampliar esse domínio.

O custo dominante incerto é a investigação de falhas. A fase 2 deve gerar
um checkpoint antes da bateria completa para permitir interrupção por cota
sem perder evidências. Esta fase 1 não concede aprovação da 6.6.

## Execução posterior — recusa com checking=0 (2026-11-10)

Concluída a validação dirigida das lacunas 1 e 2, sem mudanças no compilador
ou runtime. Scripts atualizados para parametrizar checking no link LTO,
incluir os negativos com 0/2 e testar controlador/produtor com 0/2 no modo
integral. Ambos recusam Python com asserções desativadas. Resultados novos:

- LTO negativo: **68 recusas esperadas**, 17 mutações × C/C++ × O0/O2;
  checking=0 tanto na compilação quanto no link. 137 verificações incluindo
  construção do plugin. Cada caso exige marcador de mutação serializada,
  diagnóstico de transação e ausência de ICE. Não são execuções de programas.
- LTO positivo: **53 verificações e 24 execuções**, C/C++ × O0/O2 ×
  observe/rebuild/revoke × DSO/archive, checking=0 na compilação e no link.
  Aprovação WPA/LTRANS e revogação nas fronteiras foram observadas.
- Controlador/produtor: **41 verificações e 16 execuções**, oito combinações
  C/C++ × O0/O2 × controller/producer com checking=0 e DSO/archive.
  O plugin insere NOP, observa invalidação, restaura IR e exige que a geração
  anterior continue inválida. Trata-se de teste de invalidação, não de uma
  compilação negativa de IR permanentemente corrompido.

Comandos executados da raiz:

```sh
python3 transaction_test/verificar_grafo_lto.py gcc-build negative 0
python3 transaction_test/verificar_grafo_lto.py gcc-build positive 0
python3 transaction_test/verificar_grafo_atual.py gcc-build controller-checking0
```

Logs por caso e relatório acceptance-checking0.json em
transaction_test/.build/lto-graph-checking0/. Logs de controlador/produtor em
transaction_test/.build/current-graph-checking0/. Resumos em
/tmp/stm-acceptance-checking0-lto-negative.log,
/tmp/stm-acceptance-checking0-lto-positive.log e
/tmp/stm-acceptance-checking0-current.log.

O build existente foi utilizado; não houve reconstrução nem nova comprovação
de correspondência integral entre binários e fontes. A execução não cobre
novamente toda a matriz LTO (monolítica, incremental, partições, formato etc.)
com checking=0, nem a regressão checking=2 dos scripts alterados. As demais
lacunas do levantamento e a aceitação integral da 6.6 continuam abertas.
