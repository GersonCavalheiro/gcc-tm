## Contexto: criação e limpeza implementadas — 2026-10-28

Estruturas concretas em [newtm-context.h](libgomp/newtm-context.h), privadas à
libgomp. newtm_begin recebe lease*, slot e outermost: transfere exclusivamente
a lease da ponte ao contexto externo e preserva-a no aninhamento. O endereço do
slot canônico deve permanecer estável; contexto guarda slot e execução dona para
rejeitar consumo por cópia ou outra thread. Isso não valida ponteiros pendentes.

newtm_fail registra a causa, com INVALID dominante, sem limpar recursos.
newtm_discard desliga o slot e retira reservas sob o bucket; libera buffers,
listas de intervalos, referências e registros fora dele, depois contexto e lease.
O owner configura newtm_policy_destroy antes de publicar a tabela; o callback
exige leitores/escritor vazios e libera somente metadados compartilhados.
Todos os printf/fflush e diagnósticos permanecem fora dos mutexes.

A ponte GOMP usa esse contexto para entrada e limpeza do rastreamento. Sua lista
legada de objetos ainda é separada dos registros privados STM e é drenada antes
do descarte. newtm_end só implementa saída interna; saída externa marca INVALID
sem publicar, para descarte explícito. A ponte externa usa descarte de trace,
não commit. Registro STM, criação sob demanda de metadados, reservas, valores
privados, read/write, publicação e retry continuam pendentes. As estruturas de
registros já existem para permitir limpeza, mas os testes as preparam por acesso
privado: isso não significa que registro/acesso STM estejam implementados.

check-lifecycle agora testa contextos reais, incluindo lease, aninhamento,
slot copiado, outra thread, overflow, falhas dominantes, leitores compartilhados,
limpeza de buffers/intervalos sem publicação e caminhos fatais. Testes owner,
diagnósticos e exemplos/carga passaram. ASan/UBSan passaram no teste de contexto
com fontes de hash/contexto instrumentadas; não há nova execução DejaGNU.

## Lease e destrutor da política — decisões aceitas

Transferir exclusivamente a lease da ponte para o contexto externo, invalidando
o token de origem sem incrementar participações. Aninhamentos preservam a mesma
lease. Liberá-la somente após toda a limpeza, inclusive releases e callbacks;
na falha anterior à transferência, a ponte continua responsável pelo token.
Não publicar contexto parcialmente inicializado nem duplicar admissão/liberação.

Configurar o destrutor interno da política na criação da tabela global, antes
de publicá-la, substituindo o NULL atual; manter o callback imutável durante sua
vida útil. Criar metadados sob demanda: preparar candidato fora do bucket,
instalar um só em entry_visit e liberar candidato excedente fora do mutex.

Na limpeza, retirar leitores/escritor sob o bucket antes de liberar referências.
Na última referência, release executa o destrutor fora do bucket. O destrutor
verifica que leitores/escritor estão vazios e libera somente os metadados
compartilhados; buffers privados pertencem aos registros. Participações residuais
são violação interna fatal, nunca removidas à força. Não chamar código do usuário,
adquirir reservas, acessar valores dos objetos ou liberar contextos nesse callback.
A lease permanece ativa durante toda a execução do destrutor.

Base implementada: owner configura destrutor, begin transfere lease e discard
conclui limpeza. Criação de metadados e acesso STM ainda são futuros.

# Especificação aceita das estruturas do contexto transacional

Estrutura inicial aceita pelo usuário — 2026-10-28. Definida em newtm-context.h;
criação, transferência da lease e limpeza implementadas. Valores, registro STM,
reservas e publicação ainda não são funcionais.
As alternativas abaixo são comparações, não substituições aprovadas. Parte das decisões em
[HASH_AGENT](OPENMP_TRANSACTION_HASH_AGENT.md) e
[ACCESS_AGENT](OPENMP_TRANSACTION_ACCESS_AGENT.md).

## Organização e propriedade

Uma tentativa externa possui um contexto e uma lista simplesmente encadeada de
registros estáveis, um por objeto canônico distinto. Aninhamentos compartilham
contexto e lista; saída interna não libera registros nem reservas. A busca é
linear inicialmente. Ponteiros emprestados não possuem o contexto: somente o
slot proprietário pode consumi-lo no end externo ou discard.

O contexto possui a lease do proprietário da tabela, os registros e seus dados
privados. Cada registro possui uma referência física à entrada da hash. A entrada
possui os metadados compartilhados da política; leitores/escritor nesses
metadados são referências emprestadas aos registros, não referências contadas.
Um registro não pode ser liberado enquanto estiver ligado nesses metadados.

## Estruturas C aceitas para a implementação inicial

O enum newtm_object_kind já está declarado em newtm-transaction.h; sua reprodução
abaixo é documental, não deve gerar segunda definição. Os tipos concretos estão
em newtm-context.h, reutilizando estado, lease e entrada existentes.
Não exportar essas estruturas na ABI pública. As estruturas concretas podem
ficar em cabeçalho privado compartilhado pela implementação, mantendo
newtm_context opaco na interface interna de chamadas.

```c
enum newtm_object_kind {
  NEWTM_OBJECT_SCALAR,
  NEWTM_OBJECT_AGGREGATE
};

enum newtm_reservation {
  NEWTM_RESERVATION_NONE,
  NEWTM_RESERVATION_READ,
  NEWTM_RESERVATION_WRITE
};

enum newtm_failure_reason {
  NEWTM_REASON_NONE,
  NEWTM_REASON_CONFLICT,
  NEWTM_REASON_UNREGISTERED,
  NEWTM_REASON_BOUNDS,
  NEWTM_REASON_INCOMPATIBLE_REGISTRATION,
  NEWTM_REASON_INVALID_OPERATION
};

struct newtm_interval {
  size_t begin;                 /* Inclusivo. */
  size_t end;                   /* Exclusivo. */
  struct newtm_interval *next;
};

struct newtm_scalar_value {
  unsigned char *bytes;          /* Alocação sob demanda, tamanho do objeto. */
  bool valid;
  bool modified;
};

struct newtm_aggregate_value {
  unsigned char *bytes;          /* Extensão completa, preenchida sob demanda. */
  struct newtm_interval *valid;
  struct newtm_interval *modified;
};

struct newtm_record {
  struct newtm_record *next;     /* Lista privada do contexto. */
  struct newtm_context *context;
  struct newtm_hash_entry *entry;
  const void *object;           /* Endereço lógico original. */
  size_t size;                  /* Extensão imutável. */
  enum newtm_object_kind kind;
  enum newtm_reservation reservation;
  struct newtm_record *reader_next; /* Lista compartilhada, sob bucket. */
  union {
    struct newtm_scalar_value scalar;
    struct newtm_aggregate_value aggregate;
  } value;
};

struct newtm_object_policy {
  struct newtm_record *readers;
  struct newtm_record *writer;
};

struct newtm_failure_info {
  enum newtm_failure_reason reason;
  const void *object;           /* Somente identificação; não desreferenciar. */
  size_t offset;
  size_t size;
};

struct newtm_context {
  enum newtm_attempt_state state;
  size_t depth;
  struct newtm_hash_owner_lease lease;
  struct newtm_record *records;
  struct newtm_failure_info failure;
  struct newtm_context **owning_slot;
  struct gomp_thread *owner;
};
```

Não adicionar contador de registros ou de leitores inicialmente: seriam estados
redundantes a manter sem necessidade demonstrada. O contexto é usado somente
pela execução proprietária, sem migração de tasks ou uso simultâneo. Não precisa
de mutex próprio nem de campos atômicos nesta política. Isso não permite que
outras execuções leiam seu estado sem sincronização.

## Invariantes e operações

1. O contexto publicado no slot já possui lease ativa e depth=1. Antes disso,
   falhas não publicam contexto parcial. A lease da ponte é transferida
   exclusivamente ao contexto externo, invalidando o token de origem sem nova
   admissão. Aninhamentos preservam a mesma lease até toda a limpeza terminar.
   Antes da transferência, a ponte retém responsabilidade pelo token. A assinatura
   de begin agora recebe lease*, slot e outermost; slot deve permanecer no mesmo
   endereço até o descarte. Campos owning_slot/owner rejeitam cópia e outra thread,
   sem prometer validar endereços arbitrários ou já liberados.
2. Registro não lê o valor nem obtém reserva. Registro repetido compatível é
   idempotente; tamanho ou classificação incompatível condena a tentativa.
   Preparar nó privado antes de vinculá-lo; devolver qualquer referência física
   adquirida se a operação não publicar o registro.
3. NONE não aparece em leitores/escritor. READ aparece uma vez na lista de
   leitores e não como escritor. WRITE é o único escritor e tem lista de leitores
   vazia. Não existe writer acompanhado de leitores, nem mesmo do próprio dono.
4. Consulta, concessão, promoção READ->WRITE e retirada de reserva ocorrem em
   entry_visit sob o mutex do bucket. Promoção só ocorre quando o solicitante é
   o único leitor: retirá-lo da lista e torná-lo escritor na mesma visita. Se não
   puder promover, preservar sua reserva READ até o descarte e condenar somente
   a tentativa solicitante; não esperar mantendo reservas.
5. A lista privada e os buffers são manipulados somente pelo proprietário.
   reader_next, reservation e leitores/escritor são inspecionados/alterados pelo
   protocolo sob o bucket. Outras execuções não percorrem context->records nem
   dereferenciam buffers/contexto alheios. Conflito não exige condenar o dono
   remoto, evitando sincronização adicional do estado do contexto.
6. Antes de qualquer leitura da memória original, obter reserva compatível.
   Antes de escrever na visão privada, obter reserva exclusiva. Depois de sair
   da visita, a reserva continua impedindo operações incompatíveis até sua
   retirada; o buffer pode ser preenchido fora do mutex.
7. Intervalos são não vazios, ordenados, disjuntos e com adjacentes unidos:
   0 <= begin < end <= record->size. A união dos modificados está contida na
   união dos válidos; as listas são independentes e não compartilham nós.
   Verificar size <= object_size - offset antes de calcular offset + size.
8. Leituras de agregados completam somente os trechos pedidos ausentes de valid;
   nunca sobrescrevem modificados. Escritas marcam seu trecho em ambas as listas,
   sem ler o valor anterior. Preparar todos os nós necessários antes de começar
   a aplicar a operação: em falha retornável, destination/escrita solicitada
   ficam inalterados. OOM continua fatal; não introduzir status recuperável.
9. Para escalares, acessos cobrem o valor inteiro: offset=0 e size=record->size.
   A instrumentação de tipos admitidos garante essa propriedade. Escrita torna
   valid e modified verdadeiros; leitura carrega uma vez quando necessário.
   modified implica valid. O buffer é transporte de representação em bytes;
   não formar ponteiro tipado para ele nem depender de alinhamento de tipo.
   Temporários tipados e avaliação correta pertencem ao compilador.
10. Após todas as validações/preparações, o end externo entra em PUBLISHING e
    publica escalares modificados e intervalos modificados dos agregados.
    Nenhuma operação recuperavelmente falível pode seguir a primeira publicação.
    Reter TODAS as reservas até concluir TODAS as publicações, antes de retirar
    a primeira reserva. Nunca publicar bytes privados inválidos ou padding não
    modificado. Descarte não copia bytes para objetos originais.
11. Na limpeza, marcar CLOSED e desligar o slot conforme o contrato existente;
    retirar cada registro dos metadados sob o bucket antes de liberar sua
    referência, nó ou contexto. Depois liberar buffers/intervalos e referências
    físicas fora do bucket. Manter a lease até terminar todos os releases e
    callbacks; transferi-la exclusivamente para token local se o contexto for
    liberado antes de owner_release. Nunca usar a tabela depois desse release.
12. failure é diagnóstico interno sem alocação obrigatória. INVALID domina
    CONFLICT; conservar a primeira causa permanente. No controle externo,
    copiar os dados necessários do diagnóstico antes de consumir o contexto,
    limpar com segurança e emitir fatal fora dos mutexes. OOM permanece fatal
    conforme os alocadores, sem promessa de recuperação. Nenhum retry por INVALID.

## Integrações que o layout não resolve sozinho

- Endereço/tamanho não distinguem escalar de agregado. A interface interna já
  foi ajustada para newtm_register(ctx, object, size, kind), com o enum
  newtm_object_kind. O compilador fornecerá a classificação validada do objeto
  canônico: arrays/agregados admitidos usam AGGREGATE; ponteiro listado usa
  SCALAR, sem registrar o apontado. Não inferir tipo por sizeof/alinhamento.
  Tag desconhecida ou repetição com kind incompatível é INVALID no contrato
  funcional. O stub só imprime kind e retorna INVALID, sem validar argumentos.
  Ainda implementar o transporte desse dado pelo compilador e pela ponte;
  não foi alterada ou exportada ABI pública GOMP nesta etapa.
- A admissão global deve configurar o destrutor de política antes de publicar
  a tabela. O owner agora configura newtm_policy_destroy imutável. A instalação
  sob demanda de policy_data continua pendente da política de acesso. Preparar candidato fora do bucket, instalar
  um só durante visita e liberar candidato excedente fora dele. Na última
  referência, release executa o destrutor fora do bucket com a lease ainda ativa.
  O destrutor verifica leitores/escritor vazios e libera somente metadados
  compartilhados; não libera buffers privados/contextos, acessa valores, adquire
  reservas ou chama código do usuário. Participações residuais exigem diagnóstico
  fatal interno, sem remoção forçada. O callback permanece imutável.
- Validar aliases e rejeitar registros distintos sobrepostos exige integração
  própria. A hash exata e esta lista não fornecem detecção global de intervalos
  entre contextos. Não declarar essa restrição implementada por este layout.
- Logs permanecem fora dos mutexes. E/S, locks e alocações internas do runtime
  não autorizam esses efeitos no programa. Temporários automáticos privados
  recriados no retry não são registros de objetos preexistentes da lista.

## Alternativas compatíveis com as escolhas já feitas

| Ponto | Proposta inicial | Alternativa | Consequência |
| --- | --- | --- | --- |
| Lista da tentativa | Simples, inserção na cabeça | Dupla ou simples com cauda | Facilita remoção arbitrária/ordem de registro, mas exige campos adicionais; sem necessidade atual. |
| Escalares | Buffer de bytes por tamanho, sob demanda | Pequeno armazenamento embutido com fallback | Reduz malloc para tipos comuns; exige limite explícito e tratamento de tamanhos maiores. Não usar union de alguns tipos como se cobrisse todos os escalares. |
| Participação de leitores | Elo intrusivo no registro estável | Nós de participação separados | Desacopla registros privados de metadados compartilhados, com mais alocações e regras de propriedade. |
| Intervalos | Duas listas independentes | Uma lista segmentada com marcas valid/modified | Evita duplicação de trechos modificados; aumenta a complexidade de divisão e junção de segmentos. Ambas são representações por intervalos. |
| Metadados da política | Criados no primeiro uso da entrada | Criados no primeiro registro | Menos alocações para objetos nunca acessados versus caminho de acesso mais simples. Registro continua sem reserva/leitura nas duas opções. |
| Diagnóstico | Motivo e faixa no contexto | Estrutura de falha passada explicitamente pelos epílogos | Menor contexto, mas mais argumentos e mais cuidado para preservar INVALID dominante. |

Vetor/índice privado em substituição à lista, bitmap em substituição às listas de
intervalos e buffer agregado esparso são desenhos possíveis em geral, mas
reabririam escolhas já feitas. Não são propostos para a primeira implementação.

## Critérios para implementação e validação

A classificação já tem canal na interface interna; integrar sua geração no
compilador. Antes de ativar o contexto funcional, implementar a transferência da
lease e o destrutor conforme as decisões aceitas (já implementados na base).
Implementar em etapas: contexto e limpeza; registro/deduplicação; operações de
intervalos e armazenamento; reservas; read/write; publicação; integração do
compilador. Testar promoção com leitores concorrentes, falha antes de efeitos,
remoção de leitores antes de free, preenchimento parcial preservando escritas,
publicação somente modificada e limpeza após INVALID. Reavaliar o critical da
carga somente quando o teste verificar isolamento STM real.
