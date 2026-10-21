# Inventário consolidado das primitivas de transaction

## Escopo e leitura

Inventário conferido nos fontes em 2026-10-21. Fontes atuais:
`/usr/src/gcc-omp-transaction`; build separado: `/usr/src/gcc-build`.
A consolidação inicial foi documental; a etapa seguinte acrescentou seis
esqueletos internos de ciclo de vida, descritos abaixo. A sequência de trabalho
continua priorizando esqueletos antes de implementar operações reais. Os agentes especializados detalham os contratos;
este arquivo concentra assinaturas, propriedade, estados, efeitos e chamadas.

Há três situações distintas: **existente**, **esqueleto existente** e
**conceitual, sem assinatura C definida**. A coluna de efeitos futuros não
representa comportamento disponível. Os nomes `stm_*` são pseudocódigo e não
devem ser copiados para cabeçalhos ou tratados como ABI aprovada.

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

**Hoje:** todos são esqueletos que imprimem argumentos e fazem `fflush(stdout)`.
As quatro funções de estado retornam `NEWTM_HASH_INVALID`; create/acquire/lookup
zeram `*out` quando não nulo; address retorna NULL, size/count retornam zero.
release/visit não liberam recursos nem invocam callbacks. Não há locks de bucket
ou acesso ao objeto nessas operações atuais.

Na tabela seguinte, estados e efeitos são o **contrato funcional futuro**.

| Operação | Proprietário e recursos | Estados/resultado e efeitos | Ponto de chamada |
| --- | --- | --- | --- |
| `newtm_hash_create` | Chamador assume tabela e mutexes somente em OK; ponte será dona da tabela global. | OK cria tabela vazia; INVALID para capacidade zero/out nulo; OVERFLOW antes de alocar. Falha não publica tabela parcial. | Hoje primeiro begin, sob lock global de inicialização. |
| `newtm_hash_destroy` | Consome tabela somente em OK; exige quiescência de usuários, callbacks e destruidores. | OK destrói tabela vazia/mutexes; INVALID para NULL; BUSY preserva integralmente tabela ocupada. | Hoje finalize da biblioteca após uso; futuro exige quiescência real. |
| `newtm_hash_acquire` | Entrega exatamente uma referência ao chamador; contexto a retém até saída externa. | OK insere ou retém chave exata sob mutex; INVALID, SIZE_MISMATCH ou OVERFLOW sem alteração parcial. Não lê objeto nem concede reserva. | Hoje register para objeto distinto; futuro registro funcional. |
| `newtm_hash_lookup_acquire` | Entrega referência que o chamador deve liberar. | OK retém existente; NOT_FOUND não insere; INVALID/OVERFLOW não adquirem referência. | Sem chamador de produção hoje; consulta interna futura quando necessária, sem registro implícito. |
| `newtm_hash_release` | Consome uma referência pertencente ao chamador; reservas já retiradas. | void; última referência desliga entrada sob mutex e destrói metadados/entrada fora dele. NULL, dupla liberação ou tabela errada são violações. | Hoje end externo; futuro commit/descarte após retirar participações. |
| `newtm_hash_entry_address` | Referência permanece com chamador; endereço é emprestado, imutável. | Retorna chave sem ler objeto ou adquirir reserva; argumento inválido é violação, não retorno NULL funcional. | Sem chamador hoje; consultas futuras de registro/acesso/publicação quando necessárias. |
| `newtm_hash_entry_size` | Referência permanece com chamador. | Retorna extensão positiva imutável, sem reserva; zero não é erro funcional previsto. | Sem chamador hoje; futura validação de limites. |
| `newtm_hash_entry_visit` | Preserva referência; slot de metadados pertence à entrada, argument ao chamador. | void; callback uma vez sob mutex do bucket; resultado da política via argument. Não há rollback automático do callback. | Sem chamador hoje; futura concessão/conversão/retirada de reservas. |
| `newtm_hash_count` | Não transfere recursos; tabela emprestada. | Fotografia consistente saturada em SIZE_MAX; locks em ordem crescente e liberação inversa. Não prova quiescência. | Sem chamador hoje; diagnóstico/testes fora de callbacks e locks de bucket. |

### Callbacks declarados, ainda não executados pelos stubs

```c
typedef void (*newtm_hash_visit_fn)
  (const void *address, size_t size, void **policy_data, void *argument);
typedef void (*newtm_hash_policy_destroy_fn) (void *policy_data);
```

| Operação | Proprietário e recursos | Estados e efeitos futuros | Ponto de chamada futuro |
| --- | --- | --- | --- |
| `newtm_hash_visit_fn` | Entrada possui metadados instalados; callback responde pelo destino dos substituídos. Slot não pode escapar; argument é emprestado. | void; pode atualizar slot e argument sob lock. Não consome referência nem libera mutex; sem reentrada, espera, exceção ou código do usuário. Falha da política mantém metadados consistentes. | Dentro de entry_visit; hash controla lock. |
| `newtm_hash_policy_destroy_fn` | Recebe propriedade dos metadados da geração retirada. | void; libera exatamente uma vez fora do lock, sem reentrada ou acesso à entrada retirada/objeto do programa. | Última release após retirada de reservas; não depende de outra geração da mesma chave. |

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
| `GOMP_transaction_begin` | Runtime possui lock e ponteiro global de tabela, ainda NULL, e contexto trace por thread; contexto possui lista de registros. | void; solicita create uma vez sob lock, cria/reutiliza contexto e incrementa profundidade. Falta de thread/overflow detectados são fatais. Ignora INVALID da hash. | Código emitido na gimplificação antes de cada região dinâmica. |
| `GOMP_transaction_register` | Contexto possui nó novo; objeto/endereço são emprestados pelo programa. Entry permanece NULL no stub. | void; deduplica endereço, exige tamanho igual em duplicata ou falha fatal; chama acquire para novo registro. Ausência de contexto é fatal; não valida hoje NULL/size zero. Não lê valor. | Gimplificação emite após begin, na ordem da lista, antes do corpo. |
| `GOMP_transaction_end` | Contexto libera seus nós e a si mesmo só na saída externa; ponteiro da thread é limpo. | void; reduz profundidade; se zero, chama release por registro. Saída sem contexto/profundidade é fatal. Nenhum commit, rollback ou retry. | Cleanup TRY_FINALLY emitido na gimplificação, inclusive desenrolamento que o alcance. |
| `newtm_trace_initialize` | Biblioteca possui lock global trace. | void; inicializa lock quando target exige; não cria tabela/contexto. | Constructor da biblioteca. |
| `newtm_trace_finalize` | Biblioteca possui lock e ponteiro global, ainda NULL; nenhuma tabela real existe para liberar. | void; destroy se houve uso, ignorando estado somente na etapa stub. Não limpa contextos ativos. | Destructor; exige fim de usuários e contextos. |

Ao tornar a hash funcional, revisar a ponte na mesma entrega: conferir estados
antes de publicar inicialização/registros, reter handles válidos e conferir
destruição. A ponte void usa erro fatal para falhas estruturais detectadas nessa
etapa. Não inferir sucesso transacional de retorno void. O contexto trace não
guarda reservas, buffers ou causa de falha. Ver
[HASH_IMPLEMENTATION_AGENT](OPENMP_TRANSACTION_HASH_IMPLEMENTATION_AGENT.md).

## Ciclo de vida: esqueletos internos existentes

As seis assinaturas abaixo estão em `libgomp/newtm-transaction.h`, com corpos
em `.c` e visibilidade oculta. O contexto permanece opaco, sem definição ou
alocação funcional. Não são aliases dos símbolos GOMP de rastreamento. Nenhum
chamador de produção usa estes serviços nesta etapa; os pontos abaixo são
previstos para integração futura. `stm_*` continua sendo notação conceitual.

```c
enum newtm_lifecycle_status newtm_begin
  (struct newtm_hash_table *table, struct newtm_context **slot, bool *outermost);
enum newtm_lifecycle_status newtm_register
  (struct newtm_context *ctx, const void *object, size_t size);
enum newtm_lifecycle_status newtm_end (struct newtm_context **slot);
enum newtm_lifecycle_status newtm_fail
  (struct newtm_context *ctx, enum newtm_access_status cause);
enum newtm_lifecycle_status newtm_discard (struct newtm_context **slot);
enum newtm_lifecycle_status newtm_backoff (size_t attempt);
```

**Contrato atual dos stubs:** uma linha com todos os parâmetros, seguida de
`fflush(stdout)`; retorno sempre `NEWTM_LIFECYCLE_INVALID`. Nenhum lock,
alocação, registro, reserva, acesso à hash, publicação, liberação, marcação de
falha ou espera. Nenhum ponteiro de contexto/tabela/objeto/slot é desreferenciado.
Somente `newtm_begin` escreve false em `outermost` não nulo, que deve ser gravável
e não sobrepor os demais argumentos. O slot é entrada/saída de propriedade:
preservá-lo na falha evita perder uma tentativa externa. Parâmetros nulos são
admitidos nos testes dos stubs; isso não valida seu uso funcional. INVALID aqui
significa indisponibilidade; não autoriza retry nem certifica descarte.

Na tabela, estados/efeitos descrevem os **contratos funcionais futuros**.
Os nomes OK/CONFLICT/INVALID usam o prefixo NEWTM_LIFECYCLE_.

| Operação e categoria | Proprietário dos recursos | Estados e efeitos | Ponto de chamada previsto |
| --- | --- | --- | --- |
| `newtm_begin`, primitiva de entrada | Chamador possui slot; contexto criado passa a ser possuído por esse handle. Tabela é emprestada e vive além dos contextos. | OK cria tentativa externa com profundidade 1 e outermost=true, ou entra no mesmo contexto/tabela com profundidade +1 e outermost=false. Não registra objetos. Falha preserva slot/profundidade e inicializa outermost=false; CONFLICT só para tentativa já condenada por conflito; INVALID para violação detectada ou causa permanente já registrada, que prevalece. | Entrada de cada região dinâmica; ponte futura obterá a tabela e o slot da execução. |
| `newtm_register`, primitiva de registro | Contexto retém referência por objeto distinto; objeto é emprestado pelo programa e vive até encerramento externo. | OK deduplica por endereço/tamanho ou registra; sem reserva/leitura. Tamanho incompatível/argumento inválido é INVALID e condena contexto válido. Tentativa já conflitante retorna CONFLICT. Falha não adiciona registro parcial nem muda profundidade. | Após begin bem-sucedido, em cada lista já alcançada. |
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

Antes de criar cada esqueleto faltante, definir assinatura interna e estados,
propriedade em sucesso/falha, efeitos permitidos no stub e ponto de chamada.
Não obrigar commit, backoff ou retry a terem símbolo próprio se forem fases ou
fluxo de controle. Não ligar stubs que retornam falha ao código gerado como se
fossem operações bem-sucedidas. A futura implementação da hash fica depois dessa
consolidação e da etapa de esqueletos; seu roteiro técnico permanece válido.

Validação histórica da consolidação inicial: comparação documental com cabeçalhos, definições
e chamadores locais; verificação de links relativos e cobertura dos símbolos.
A consolidação inicial não exigiu rebuild; validação da etapa de esqueletos
registrada em [IMPLEMENTATION](OPENMP_TRANSACTION_IMPLEMENTATION.md).

### Revisão do inventário

A revisão conferiu assinaturas completas (retorno e parâmetros), callbacks,
estados e chamadores nos fontes. Corrigiu a omissão do chamador de cópia/remapeamento
em tree-inline.cc e a descrição da tabela trace, cujo ponteiro permanece NULL.
Explicitou a liberação de recursos após confirmação e a distinção entre reservas
persistentes e mutexes liberados a cada acesso. Links para os fontes permitem
repetir essa conferência; nomes stm_* continuam conceituais. Os esqueletos
newtm_* de ciclo de vida foram acrescentados na etapa seguinte descrita acima.
