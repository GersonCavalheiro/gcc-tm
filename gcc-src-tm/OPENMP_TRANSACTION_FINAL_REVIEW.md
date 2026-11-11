# Etapa 6.6 — revisão final documental e estática

**Revisão histórica de 2026-11-10, superada pelo
[parecer final de 2026-11-11](OPENMP_TRANSACTION_ACCEPTANCE_FINAL.md).**
Os achados abaixo foram tratados nos passos 2–3 e validados nos passos 5–6.
A conclusão vigente é aceitação da 6.6 no domínio experimental documentado.

Data: 2026-11-10. **Conclusão: não aprovar ainda o encerramento integral da
6.6. A implementação funcional tem evidência consistente no domínio testado,
mas permanece uma pendência normativa de diagnósticos.**

Esta revisão não executou builds, programas de teste ou baterias DejaGNU.
Foram lidos contratos, código, runners e relatórios, e conferidos arquivos e
hashes das evidências. Não se demonstrou nova falha de isolamento, publicação,
limpeza ou retry; a revisão estática também não prova sua ausência geral.

## Referência normativa e domínio

A autoridade é a seção 8 e os casos da seção 9 de
[OPENMP_TRANSACTION_VERIFICATION.md](OPENMP_TRANSACTION_VERIFICATION.md).
O domínio continua opcional por -fopenmp -fopenmp-transaction-stm, C/C++ no
host Linux x86_64, com as restrições documentadas de tipos, chamadas, frames,
agregados e otimização. A alternativa B não foi incluída nesta aceitação.
O modo padrão de rastreamento não concede STM automaticamente.

Os parágrafos antigos de status em documentos históricos não anulam os
checkpoints e relatórios posteriores. Nenhum limite de suporte foi ampliado
ou requisito normativo dispensado para produzir esta conclusão.

## Integridade e validade das evidências

Foram conferidos **540 hashes de artefatos preservados**, sem divergências:
103 do conjunto diagnostics, 408 de lto-phase-acceptance e 29 de
dejagnu/accepted. Os hashes dos fontes listados pelo relatório diagnostics
também correspondem aos arquivos atuais. As mudanças posteriores nessa
sequência foram nos testes/documentação, sem alteração funcional de produção.

O build-confirmation registra make integral incremental com saída zero,
oito execuções positivas e hashes. A mudança posterior nos diagnósticos teve
all-gcc próprio aprovado e novas validações. Não houve build limpo/bootstrap;
a correspondência usada é a das dependências do make e dos manifestos, sem
pretensão de reprodução independente a partir de uma árvore vazia.

Relatórios históricos de ABI, integração, guardas, status, falhas, propriedade
e composição foram localizados e declaram sucesso. São evidência histórica
identificada, sem atribuir-lhes nova execução ou somar seus totais aos atuais.

| Evidência recente | Resultado e interpretação |
| --- | --- |
| Diagnósticos e contraprovas dirigidas | 45 recusas e 13 execuções positivas; cobre as mensagens e casos selecionados, não todas as recusas possíveis. |
| Fases/configurações LTO | 126 casos novos; incluindo regressões auxiliares, 167 recusas esperadas e 62 execuções positivas nessa rodada. |
| DejaGNU transaction/critical | Seis suítes, 629 PASS, zero resultados inesperados nas execuções aceitas; inclui dg-final. |

Não há um total único de testes independentes: as rodadas reutilizam casos,
e PASS, verificações, links e execuções medem coisas diferentes. Os snapshots
locais .build não são versionados nem substituem backup ou commit Git.

## Rastreabilidade funcional V01–V08

| Regra | Caminho estático examinado | Evidência e juízo |
| --- | --- | --- |
| V01 | Inventário/derivações em newtm-obligation; correspondência de operações em memory_verify_cfg; fechamento em current_body/newtm_current_verify. | Store derivado retirado é recusado na composição nativa e nas fases LTO; inventário conserva pai/filho e invalidação. Requisito funcional sustentado no domínio. |
| V02 | coverage_snapshot/coverage e classificação das operações atuais; cobertura de corpo/controlador exigida pelo agregado. | raw sem ocorrência recusado no link; matriz residual histórica e contraprovas atuais presentes. Requisito funcional sustentado. |
| V03 | check_lowered_registrations/check_lowered_memory, proveniência aritmética e resolver::ranges. | Limites, overflow, identidade, SSA/PHI e mutações específicas têm testes; guard LTO isoladamente não é contado como prova de faixa, pois é interceptado por canais. Requisito funcional sustentado por evidências combinadas. |
| V04 | resolver::ranges exige alocação da ativação para PRIVATE constante, valida tag/faixas/ramo; ABI read/write delega ao runtime real. | PRIVATE com identidade trocada recusado no agregado nativo/LTO; ABI e memória têm cobertura própria. A falsificação numérica literal da tag não foi reexecutada integrada; sua obrigação está sustentada pelo código e pelos testes de classificação/armazenamento. |
| V05 | status_configure/status_verify e disponibilidade de canais antes de consumo/publicação. | Consumo antecipado recusado explicitamente por V05 em nativo e LTO; status de outra chamada/publicação têm matrizes de componente. A mutação status que aciona V07 não foi usada como substituto de V05. |
| V06 | Assinaturas, chamadas, capturas, contexto emprestado e canais verificados transitivamente por newtm_clone_verify_current. | context/channel/missing, outlining, recursão, unidades múltiplas e templates têm evidências. Requisito funcional sustentado no domínio conservador, sem clones remotos novos. |
| V07 | Componentes de status/propriedade e controlador exigidos antes da publicação; runtime mantém a responsabilidade por reservas/limpeza. | owner/entry/causa, commit e retry têm contraprovas e integração histórica aprovada. Não se deduz correção do algoritmo do runtime somente do certificado do compilador. |
| V08 | Janela, geração, snapshot, epoch e publicação atômica em newtm-current; reconstrução própria em newtm-lto. | Retirada recusa recomposição; envelope/SSA/versão são confrontados; WPA/LTRANS/monolítica/incremental e revogação foram exercitados. Requisito funcional sustentado nas configurações documentadas. |

O agregado não publica parcialmente: exige todas as classes, dependências
resolvidas, controlador presente, IR atual e epoch estável. Falhas invalidam
os membros tocados e produzem REFUSED. Um componente DEPENDENT ou certificado
histórico não basta. Esse fechamento está visível no código, sem depender
somente dos marcadores dos plugins de teste.

## Casos normativos e alcance dos oráculos

Os positivos executam incremento com efeito colateral e chamada privada,
chamada seguida de store e cópia de agregado em C. Há recusa integrada de
leitura residual, WRITE derivado retirado, PRIVATE redirecionado, consumo
antecipado e evidência fora da janela. Cópia de mesma origem e consulta após
retirada de descritores também têm contraprovas sintéticas de API.

Retirar o certificado atual não é o mesmo que injetar um ponteiro de descritor
expirado. Essas evidências permanecem separadas. Nenhuma delas, sozinha,
autoriza reaproveitar metadados operacionais após omplower. O caso de stub
ativo recusado é distinto do funcionamento manual da ABI fora do modo STM.

O contrato exige comportamentos e provas, não uma técnica única de mutação
ou toda combinação possível de flags. Por isso, matriz LTO dirigida e ausência
de injeção pré-reconstrução em LTRANS isolado são **limitações da evidência**,
não um defeito funcional demonstrado ou um bloqueio novo por si sós.

## Pendência obrigatória: diagnósticos não uniformes

A seção 8 exige regra, espécie de operação, localização, função e causa;
cadeia de chamada e ancestral de derivação quando necessários. Os casos
reproduzidos foram corrigidos, mas a implementação atual não cobre todos os
caminhos dessa exigência. Duas constatações estáticas impedem o fechamento:

1. **Recusas do caminho agregado ainda contornam o contexto novo.**
   newtm-clone.cc:527 emite apenas "transaction prepared signature lost its
   parameter correspondence"; a linha 949 informa falta de guarda de overflow
   sem a nota estruturada. current_body chama esses verificadores antes dos
   componentes contextualizados. A recusa de membro não preparado na linha
   2746 também não fornece regra/operação. Em newtm-current.cc:382, o contexto
   de fallback só é emitido se não houver erro anterior: ele não completa essas
   mensagens. Portanto não basta chamar essas recusas de mensagens antigas;
   elas continuam alcançáveis no caminho de aceitação atual.
2. **Uma causa PRIVATE pode ser rotulada V03 em vez de V04.**
   newtm-memory.cc:893 recusa PRIVATE constante sem alocação da ativação;
   o retorno de ranges na linha 1713 chama fail sem regra explícita. O padrão
   de fail nas linhas 996–997 é V03, e a causa não começa com V04 para permitir
   a especialização do helper. A análise estática demonstra a classificação
   incorreta nesse caminho; não foi executada nova contraprova nesta revisão.

Além disso, o contexto atual mostra raiz/controlador/membro, mas não toda
cadeia intermediária. newtm_obligation_report contém ancestral de obrigação
quando disponível, sem resolver todos os casos reconstruídos. Não há base
para declarar conformidade integral desses detalhes condicionais.

Não foi demonstrado que essas lacunas permitam aceitar uma transação inválida.
São defeitos de apresentação/rastreabilidade exigida pelo contrato. O sucesso
dos testes existentes não elimina um requisito obrigatório não atendido.

## Parecer e próximo trabalho delimitado

**6.6 NÃO CONCLUÍDA.** Há sustentação consistente para o comportamento
funcional no domínio documentado e regressões selecionadas aprovadas.
Não aprovo a afirmação "diagnósticos completos" nem aceitação integral.

Para voltar à decisão de fechamento: contextualizar as recusas ainda usadas
por current_body/controlador; transportar a regra correta de falhas PRIVATE;
validar os campos com negativos desses caminhos e o contexto de derivação/
chamada quando necessário. Depois atualizar os relatórios e repetir somente
as verificações afetadas. Não é necessário repetir indiscriminadamente toda
a matriz nem ampliar o domínio de suporte para tratar essa pendência.

Este parecer encerra a revisão solicitada; não inicia essas correções ou
novas baterias. Os relatórios de fases anteriores continuam como históricos,
e esta conclusão prevalece sobre suas listas de próximas etapas.

Registro estruturado da revisão:
[report.json](../transaction_test/.build/final-review/report.json), com
manifestos conferidos, estado de aceitação e hashes dos fontes examinados.

Passo 1 da correção executado posteriormente: o
[inventário de diagnósticos](OPENMP_TRANSACTION_DIAGNOSTIC_INVENTORY.md)
identifica 36 pontos de emissão no caminho atual, com associação de regra,
categoria e escopo. Nenhum código de produção foi alterado nesse passo.

Atualização posterior: [passo 2 concluído](OPENMP_TRANSACTION_DIAGNOSTIC_STEP2.md),
com uniformização dos pontos inventariados, build e negativos dirigidos aprovados.
O achado sobre a classificação PRIVATE/V04 permanece aberto para o passo 3;
esta atualização não concede aceitação integral da 6.6.

Atualização posterior: [passo 3 concluído](OPENMP_TRANSACTION_DIAGNOSTIC_STEP3.md).
A causa PRIVATE é propagada como V04, preservando V03 para identidade/faixa;
os testes dirigidos e as regressões de memória/status passaram. O parecer
acima permanece o registro da revisão anterior às correções: não descreve
esse defeito como ainda presente após o passo 3, nem substitui a validação
final dos passos seguintes por uma aprovação automática.
