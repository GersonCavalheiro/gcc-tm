# Etapa 6.6 — parecer final de aceitação

Data: 2026-11-11. **APROVADA E CONCLUÍDA NO DOMÍNIO EXPERIMENTAL DOCUMENTADO.**
O passo 7 encerra a sequência de revisão e correção dos diagnósticos.
Não foi identificada pendência obrigatória remanescente nesse escopo.
Esta é uma decisão de aceitação de engenharia baseada em revisão estática
e evidências executáveis; não é prova universal de correção do compilador.

## Domínio aceito

- Host nativo Linux x86_64, C/C++ e opção explícita
  `-fopenmp -fopenmp-transaction-stm`.
- Tipos, chamadas, capturas, frames, exceções e agregados limitados aos
  subconjuntos conservadores já definidos. Agregados compartilhados dos
  fixtures pertinentes continuam restritos a C.
- Certificação atual e reconstrução LTO nas janelas e configurações
  documentadas; não se admite SSA arbitrariamente otimizado.
- O modo padrão com apenas -fopenmp permanece de rastreamento. A existência
  de clone ou certificado histórico não torna uma chamada comum transacional.

Não houve ampliação de suporte ou dispensa dos requisitos V01–V08 para obter
esta conclusão. Alternativa B, novas plataformas e qualificação geral para
produção não integram esta aceitação.

## Fechamento dos achados anteriores

| Achado da revisão anterior | Correção e evidência | Decisão |
| --- | --- | --- |
| Recusas alcançáveis no agregado sem contexto uniforme | 36 IDs migrados para 38 emissores; localização com fallback, regra, operação, função, raiz/controlador e cadeia reconstruída do CFG quando disponível; condições V06/V07 e componentes ausentes separados. | Fechado no caminho inventariado. |
| PRIVATE sem alocação da ativação podia receber V03 | Regra tipada junto da primeira causa no resolver; V04 para armazenamento, V03 para identidade/faixa; detalhe do solver preservado sem substituir a regra do caso. | Fechado; literal PRIVATE, tag, ramo e primeira causa validados. |
| Caso de membro não preparado sem contraprova específica do gate | Dependência declarada acrescentada após baseline aprovado; recomposição recusa D34/V06 e proved=0; controle sem mutação executado. | Fechado no escopo declarado pelo teste. |

As mensagens de parser e de preparação adjacentes, fora dos caminhos
inventariados de verificação da transformação, não foram todas reformatadas.
Isso não é uma alegação de que todo erro possível do GCC tenha o mesmo
formato. O diagnóstico não fabrica ancestrais ou chamadas ausentes, nem
consulta descritores expirados para explicar uma recusa.

## Decisão por requisito

| Regra | Evidência determinante | Parecer no domínio |
| --- | --- | --- |
| V01 | Inventário/derivação persistem; retirada do WRITE derivado e perda de contratos provocam recusa; pai/filho e retirada também têm provas de API. | Atendida. |
| V02 | Inspeção do IR real e cobertura residual de corpo/controlador; carga sem ocorrência recusada no link; classificação não depende só da origem. | Atendida. |
| V03 | Identidade, faixa, overflow, SSA/PHI, registros e frames conferidos; negativos de overflow/identidade e controles equivalentes aprovados; V03 preservada para falhas próprias. | Atendida. |
| V04 | Prova de alocação PRIVATE, domínio de storage e escolha de ramo; causa V04 transportada corretamente; testes do componente real e contraprovas integradas de identidade; ABI/runtime mantêm autorização REGISTERED. | Atendida. |
| V05 | Disponibilidade de valor/binding por instância de status e publicação sob sucesso; consumo antecipado recusado especificamente por V05; regressão de status aprovada. | Atendida. |
| V06 | Assinaturas, canais, contexto emprestado e fechamento transitivo; D34 recusado; chamadas/capturas/recursão/unidades e regressões DejaGNU aprovadas. | Atendida. |
| V07 | Controlador, propriedade, causa, limpeza, commit/descarte e retry têm provas e testes próprios; V07 do proprietário não é confundida com V06 da assinatura. | Atendida. |
| V08 | Gerações, snapshots, dependências, janela e publicação atômica; retirada impede recomposição; reconstrução própria nas fases LTO e revogação validadas. | Atendida. |

A inspeção estática do agregado exige todas as classes de prova, dependências
resolvidas, controlador, snapshots atuais e epoch estável antes de APPROVED.
Falhas não publicam uma aprovação parcial. Os contratos da ABI/runtime não
são substituídos por essa decisão do compilador.

## Evidências utilizadas

| Conjunto | Resultado | Papel nesta decisão |
| --- | --- | --- |
| Passo 2 | 49 recusas/25 execuções positivas; migração 36/38 conferida estaticamente. | Correção dos caminhos inventariados e contexto transitivo. |
| Passo 3 | 56 casos de componente e 36 integrados; memória 281/32 e status 449/100. | Classificação PRIVATE e regressão dos consumidores. |
| Passo 5 | 93 negativos/45 controles executados, com build e fontes conferidos. | Matriz final preparada, incluindo membro não preparado e regressão LTO. |
| Passo 6 | DejaGNU: seis seleções, 629 PASS, zero resultados inesperados; grafo transitivo: 317 verificações/32 execuções. | Compatibilidade das mensagens, dg-final e regressões afetadas no mesmo build. |
| Matriz anterior de fases LTO | 126 casos novos; rodada completa com auxiliares: 167 recusas/62 execuções. | WPA/LTRANS, monolítica, incremental, partições, fat, unidades e formato. |

Os resultados de cada rodada conservam suas datas e escopos. Não somar tudo
como se fossem testes independentes, nem declarar a matriz antiga reexecutada
no último passo. As mudanças posteriores de produção ficaram limitadas a
diagnósticos/metadados locais de falha, com regressões dirigidas LTO e gerais
posteriores aprovadas. O runtime e a ABI foram conferidos contra os hashes
do build confirmado, sem alteração nesse intervalo.

Neste passo 7 foram verificadas **2.200 entradas de manifesto**, incluindo
cópias repetidas entre snapshots, sem divergências. Foram conferidos também
os fontes atuais, a migração, os binários do passo 6 e nove hashes pertinentes
de runtime/ABI/bibliotecas do build confirmado. Não houve novo build ou nova
bateria neste passo: são verificações de integridade e revisão estática.

## Limites da evidência que permanecem explícitos

- A matriz nova de fases LTO usa pareamentos dirigidos, não todo o produto
  cartesiano de otimização, checking e biblioteca.
- PRIVATE literal é testado no componente real com mapas isolados; a mutação
  integrada de identidade PRIVATE é um teste diferente. Não se afirma que
  cada mutação foi injetada em cada fase.
- Parte das contraprovas LTO altera o SSA após reconstrução aprovada e exige
  recomposição. Não houve injeção pré-reconstrução em todo LTRANS isolado.
- Nem todos os 38 emissores defensivos foram executados individualmente;
  todos tiveram revisão estática e os caminhos representativos têm negativos.
- O build é incremental, sem bootstrap e com self-tests desabilitados.
  DejaGNU cobre as seleções transaction/critical, não toda a suíte GCC.

Essas limitações não foram convertidas em garantia maior. O contrato não
exige um método único de mutação nem toda combinação possível de flags;
a combinação de implementação revisada, controles e contraprovas é suficiente
para esta aceitação no domínio conservador. Não há falha obrigatória conhecida
nesses requisitos sendo dispensada por limitação de cota.

## Registro e efeito do parecer

[report.json](../transaction_test/.build/diagnostic-step7/report.json) registra
o parecer, a matriz V01–V08, os manifestos conferidos, hashes dos fontes e
binários e as limitações. Os artefatos .build são locais e ignorados pelo Git;
a aceitação não cria commit, backup, versão publicada ou instalação.

Este parecer substitui a conclusão de não aprovação da revisão de 2026-11-10,
porque seus achados obrigatórios foram corrigidos e validados nos passos
posteriores. Os relatórios antigos permanecem históricos. **Não há tarefa 7
nova implícita nem trabalho de ampliação de suporte autorizado por este fechamento.**
