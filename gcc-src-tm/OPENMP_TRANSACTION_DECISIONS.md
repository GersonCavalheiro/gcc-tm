# Decisões vigentes e extensões futuras de transaction

Atualizado em 2026-10-29. Este é o registro central para os agentes do projeto.
Consolida as decisões da conversa e os contratos implementados; não autoriza
ativar integrações ou ampliar o subconjunto. Instruções posteriores do usuário
prevalecem. Ao mudar uma decisão, registrar motivo, substituição e impacto nos
agentes, ABI e testes, conservando o histórico identificado como tal.

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
