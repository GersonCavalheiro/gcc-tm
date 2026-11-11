# Etapa 3: fronteira executável STM v1

Estado vigente da [etapa 5.5](OPENMP_TRANSACTION_CONTROLLER.md):
`-fopenmp -fopenmp-transaction-stm` ativa o controlador funcional. O padrão
conserva rastreamento. O serviço void/noreturn `GOMP_transaction_stm_v1_fatal`
foi acrescentado em GOMP_NEWTM_2.1 para erro terminal sem contexto, sem alterar
layouts ou versões anteriores. As descrições de ativação pendente abaixo são
históricas; validação ampliada (5.6) e cobertura integral (6) continuam pendentes.

Estado posterior: a [etapa 4](OPENMP_TRANSACTION_CLONES.md) implementa clones,
resolve incoming/result e acrescenta status(ctx) à família GOMP v1, mantendo
GOMP_NEWTM_2.0 e os layouts anteriores. O transporte específico dos clones
encapsula a identidade original com classificação de armazenamento. A ativação
do pragma e os acessos STM automáticos ainda dependem das tarefas 5–6.


Implementada em 2026-10-30. O pragma continua de rastreamento. Os serviços
internos de STM já existentes são reutilizados sem alterar suas políticas.

## Interfaces e layout

`include/newtm-abi.h` é o contrato compartilhado compilador/runtime. Define
`newtm_status_v1` como uint32_t, `newtm_slot_v1` como ponteiro para contexto opaco
e `newtm_binding_v1` com campos, nesta ordem: `void *object`,
`size_t object_size`, `size_t offset`, `uint32_t kind`. O macro de campos é
consumido também pelo construtor GCC: alinhamento/padding são os do target,
sem packing nem formato serializado. Não supor offsets LP64 em outros targets.

Status fixos: 0 OK, 1 CONFLICT, 2 INVALID. Kind: 0 SCALAR, 1 AGGREGATE.
Enums internos são traduzidos por switches; resultados desconhecidos viram
INVALID. Kind e causas desconhecidas condenam a tentativa válida com INVALID;
a causa OK também não é uma solicitação válida de fail. INVALID domina conflito.

Contextos são emprestados aos acessos/futuros clones. Bindings transportam
identidade lógica, não referências físicas, registro implícito ou autorização.
Produção/consumo de bindings incoming/result pertence à tarefa 4. A assinatura
pública original das funções do programa permanece disponível.

## Serviços exportados

`libgomp/newtm-abi.c` implementa dez adaptadores, declarados no contrato e
incluídos em `libgomp_g.h`. Todos retornam uint32_t:

| Sufixo de GOMP_transaction_stm_v1_ | Argumentos | Efeito |
| --- | --- | --- |
| begin | slot*, uint32_t *outermost | Entrada externa admite lease; interna incrementa profundidade no mesmo slot. |
| register | ctx, object, extent, uint32_t kind | Valida/registra objeto; não copia dados nem obtém reserva. |
| read | ctx, object, offset, destination, size | Lê visão privada; preserva destino em falha. |
| write | ctx, object, offset, source, size | Atualiza somente visão privada; preserva origem e memória original. |
| nest_begin | ctx | Entrada aninhada por contexto emprestado, sem expor o slot. |
| nest_end | ctx | Saída apenas aninhada; profundidade externa é INVALID, sem commit. |
| end | slot* | Saída interna reduz profundidade; externa confirma e limpa. |
| fail | ctx, uint32_t cause | Condena sem consumir recursos; serve também a validações geradas pelo compilador. |
| discard | slot* | Consome todos os níveis sem publicar. |
| after_failure | slot*, size_t consecutive_conflicts | Descarta e espera; só retorna CONFLICT. INVALID limpa e termina fatalmente. |

Símbolos pertencem a `GOMP_NEWTM_2.0`, que herda `GOMP_NEWTM_1.0`.
Os símbolos void antigos mantêm ABI e semântica de rastreamento; `newtm_*`
continuam hidden. Não misturar a ponte TLS antiga com slots da nova fronteira.
O header é interno da árvore de desenvolvimento, sem nova API de usuário em omp.h.

## Propriedade, entrada e controle externo escolhido

O chamador inicializa um slot exclusivo com NULL e mantém seu endereço estável
até confirmação/descarte. Nunca cria cópia proprietária. Toda região aninhada
reutiliza o mesmo slot; funções preparadas recebem apenas contexto emprestado.
Essas funções usam nest_begin/nest_end: o runtime acessa o slot canônico e
verifica execução/propriedade, conservando-o opaco ao clone. Nest_end recusa
profundidade <= 1, condenando a tentativa com INVALID em vez de publicar.
Essa adaptação fecha a lacuna entre o controle por slot e a assinatura dos clones.
Não há mutex retido durante o corpo nem agendamento de tarefas admitido.

Outermost usa uint32_t 0/1, inicializado em zero também em falha retornada;
só pode ser consumido após OK. O armazenamento deve ser válido, separado do slot
e dos demais objetos/runtime. O adaptador begin adquire uma lease externa,
transfere-a somente no sucesso interno e libera-a em toda falha retornada após
admissão. Falha de admissão não publica contexto. Begin falho sem contexto não
permite chamar after_failure: o futuro controle externo precisa diagnosticar
esse erro permanente diretamente. Com tentativa existente condenada, a falha
segue até o epílogo externo. OOM continua fatal conforme contrato interno.

A futura emissão seguirá slot/status, sem adaptar callbacks a newtm_execute:
entrada -> registros -> corpo preparado -> end apenas em sucesso. Falhas de
validade geradas chamam fail(INVALID), e falhas de operações propagam status até
o controle externo. Esse controle chama after_failure uma única vez com contador
positivo saturado de conflitos. CONFLICT retornado autoriza recomeçar toda a
região externa com slot vazio e temporários/registros novos. INVALID nunca repete.
O laço e seus epílogos ainda não são gerados nesta etapa.

End falho conserva slot e recursos, sem publicação parcial. Não confirmar em
finally. End externo OK só retorna após publicação e limpeza completas; discard
OK significa limpeza, não confirmação. A lease é liberada por último. Objetos,
buffers e slots devem satisfazer vida útil, extensão, disjunção e permissões dos
contratos internos; a ABI não valida ponteiros arbitrários/pendentes.

## Integração do compilador e janela dos descritores

`gcc/builtin-types.def` e `gcc/omp-builtins.def` definem assinaturas com retorno
BT_UINT32 e tamanhos BT_SIZE. Contexto/slot e ponteiros de transporte usam
ponteiros opacos no builtin, com a representação nativa do target. Os tipos de
contexto/slot são construídos por `newtm_abi_context_type` (ponteiro para record
incompleto) e `newtm_abi_slot_type` (ponteiro para o valor de contexto). Alocar
slots do tipo de contexto evita tratar um objeto void* como context* por alias
incompatível. O tipo de binding é construído por `newtm_abi_binding_type`; todos
os tipos opacos/de transporte retidos têm raízes GC.

`newtm_abi_build_call` recebe builtin, destino uint32_t e operandos GIMPLE já
avaliados; verifica operação, aridade e forma dos operandos. Retorna gcall com
status explícito. Não faz busca de descritores nem avalia receitas GENERIC.
O chamador futuro deve inserir desvios de status/validade e tratar saídas somente
em OK, resolver incoming/result e rejeitar unknown/source-only/stale.

Consumir os descritores enquanto ainda existem antes de omplower, emitindo
operandos/chamadas comuns que o remapeamento possa tratar, ou reconstruir os
descritores a partir do corpo remapeado. A cópia stale não autoriza reutilizar
campos antigos. Não foi estendida a vida das tabelas para outlining/CFG/SSA.

## Validação e limites

Build GCC (`make -j4 all-gcc`) e libgomp (`make -j4` no build da biblioteca),
com objetos PIC/não PIC e -Wall -Werror para o runtime, aprovados. O comando
conjunto inicial acionou libatomic com caminhos históricos inexistentes; o build
local da libgomp também conservava esses caminhos em Makefile/libtool/config.status.
Os arquivos gerados desta biblioteca foram corrigidos para a cópia atual, e os
builds relevantes foram concluídos separadamente. Não foi alterada libatomic.

`make -C transaction_test check-abi-v1`: 88 verificações aprovadas. Matriz C/C++,
O0/O2, DSO/archive, declarações do header/construtor GIMPLE; objdump/readelf/nm verificam versão, dependência dinâmica,
exports novos, conservação dos antigos e ocultação de newtm_*. Plugin exclusivo
de teste antes de omplower reemite chamadas usando o construtor do compilador,
exige todas as dez operações, força GC e compara layout de bindings com o
front-end. Não é o passe de instrumentação do pragma. Os corpos de teste são
explicitamente preparados e verificam cada status.

Execução cobre confirmação escalar/agregada, faixas, registro duplicado,
aninhamento, slot copiado recusado, conflito real entre threads, destino de read
preservado em conflito/INVALID, descarte sem publicação, end falho sem publicação parcial de dois objetos, INVALID dominante,
kind/causas desconhecidos e retorno CONFLICT somente após limpeza/backoff. Aninhamento por contexto emprestado
recusa outra thread e saída do nível externo; conflito interno preserva níveis
até o epílogo externo consumir tudo.
Interposição apenas no executável estático força falha interna e tag desconhecida
após admissão, confirma liberação da lease e fechamento sem vazamentos; outro
processo verifica diagnóstico fatal somente depois de slot consumido, referências
e lease liberadas, tabela drenada e memória original preservada.

Regressões: 63/63 ocorrências, 36/36 descritores, 201 verificações do subconjunto;
check-register, check-access (inclui values), check-commit, check-retry (inclui
failure), check-overlap e check (inclui carga e exemplos internos) aprovados.
Não foi executado DejaGNU. Cobertura nativa Linux x86_64 LP64; sem nova alegação
sobre 32 bits, outros targets, LTO ampliado ou offload.

A ativação do pragma exige tarefas 4–6: clones, bindings reais, instrumentação,
epílogos de status e verificador de cobertura. A sonda confirma que o pragma
continua emitindo somente as chamadas antigas de rastreamento.
