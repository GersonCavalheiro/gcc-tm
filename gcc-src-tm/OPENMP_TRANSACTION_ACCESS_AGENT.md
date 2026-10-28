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

Procedimentos implementados na base do contexto: owner configura destrutor
interno e begin transfere a lease. Registro e acessos STM continuam pendentes.

## Interface de classificação e estrutura aceita — 2026-10-28

O usuário aceitou a [especificação das estruturas do contexto](OPENMP_TRANSACTION_CONTEXT_PROPOSAL.md):
lista simples de registros estáveis por tentativa, escalares com buffer sob demanda
e flags, agregados com buffer completo sob demanda e duas listas de intervalos,
participação intrusiva de leitores e metadados compartilhados por entrada.
As alternativas documentadas não substituem essa escolha inicial.

A interface interna newtm_register(ctx, object, size, kind) recebe agora
`enum newtm_object_kind`: NEWTM_OBJECT_SCALAR ou NEWTM_OBJECT_AGGREGATE.
O compilador deve classificar o objeto canônico inteiro após validar seu tipo;
arrays são AGGREGATE, ponteiros listados são SCALAR sem incluir o apontado.
Não inferir classificação por tamanho/alinhamento. O registro privado conserva
kind imutável; read/write consultam esse registro. Repetição exige tamanho e
kind compatíveis; tag desconhecida ou incompatível é INVALID no contrato
funcional, com limpeza segura e fatal para contexto válido, nunca retry.
Não altera as nove primitivas da hash nem a ABI pública GOMP existente.

A assinatura, o enum e o log numérico de kind já estão no código; newtm_register
continua stub: imprime, faz fflush e retorna INVALID sem examinar contexto ou
objeto. A estrutura aceita está em newtm-context.h, com criação e limpeza
implementadas. Integrar classificação e registro STM no compilador/ponte.

## Organização escolhida para o contexto

Cada tentativa externa mantém uma lista de registros, compartilhada pelos níveis
aninhados do mesmo contexto. Cada registro corresponde a um objeto canônico
registrado e permanece em endereço estável até sua remoção na limpeza. Repetir o
registro do mesmo objeto reutiliza o registro existente; a busca inicial é linear.
Não introduzir agora vetor de registros nem índice privado adicional.

O contexto contém controle da tentativa, profundidade, causa e lease do
proprietário. Seus registros mantêm referências à hash, participação/reserva e
representação privada. Metadados compartilhados de leitores/escritor permanecem
associados à entrada da hash e sincronizados pelo mutex do bucket; não confundir
esses metadados com o estado privado da tentativa.

Escalares usam representação simples. Para agregados, escolher buffer da extensão
completa do objeto, preenchido sob demanda, e listas de intervalos válidos e
modificados. Alocar a extensão não implica ler ou inicializar todo o objeto.
Intervalos modificados devem estar contidos nos válidos; leituras completam
somente trechos solicitados ausentes e commit publica somente modificados.
A unidade de conflito e os endereços lógicos originais permanecem preservados.

Esta escolha define a organização inicial. Campos C, armazenamento dos escalares,
representação dos limites/união de intervalos e ordem detalhada de liberação
precisam ser especificados antes da implementação. Avaliar otimizações de busca
e alocação após obter funcionamento correto. Nenhuma estrutura executável foi
introduzida por esta atualização documental.

## Fechamento do subconjunto inicial — decisão do usuário

Todos os acessos, de leitura ou escrita, a objetos cuja vida começou antes da
tentativa externa exigem registro explícito já alcançado no contexto. Não há
registro implícito nem fallback à memória comum. Esta decisão substitui a
permissão anterior de leituras externas não listadas: em `transaction(a) { a += b; }`,
se b é preexistente, seu acesso exige registro explícito também. Temporários
automáticos privados são permitidos nos tipos admitidos, recriados a cada retry
e sem escapar da tentativa; não autorizam exceções ou cleanups do usuário.

INVALID detectado com contexto válido é falha permanente: impedir novos efeitos
normais, preservar a causa, propagá-la pelos epílogos preparados até o controle
externo, concluir a limpeza interna segura e somente então emitir diagnóstico
fatal. Não publicar valores nem executar retry. O descarte libera reservas,
referências, buffers, contexto e participação do proprietário conforme a
propriedade de cada recurso. Não introduzir cláusula nem resultado novo na
linguagem; os status são canais internos. Contextos inválidos/alheios não podem
ser desreferenciados ou limpos especulativamente. Falta de memória permanece
fatal; não há promessa de recuperação de OOM nem sua conversão em CONFLICT.
Essa decisão não muda os retornos das nove primitivas genéricas da hash.

Ficam fora do primeiro subconjunto E/S, chamadas de sistema, locks do usuário e
malloc/free executados pelo programa dentro da tentativa, inclusive por funções
chamadas. Admitir alocação exigirá rastreamento de recursos e contrato de descarte
ou liberação adiada. Alocação e sincronização internas do runtime continuam
necessárias; seus logs de depuração não concedem suporte a E/S do programa.

O critical do teste de carga é apoio ao rastreamento atual. Reavaliá-lo quando o
teste verificar isolamento STM real; o teste atual não comprova esse isolamento
nem constitui uma exceção às restrições do futuro subconjunto funcional.

As decisões semânticas, incluindo cópias privadas, estão fechadas para definir
o layout do contexto. Layout/ABI, filtros, diagnósticos e execução desse protocolo
ainda precisam de implementação; os stubs atuais não passam a implementá-los.

# Agente orientador: primitivas de leitura e escrita de transaction

## Missão, escopo e estado

Especificar a construção das primitivas internas de acesso à memória da extensão
`#pragma omp transaction` nesta árvore GCC 16.2.0. Implementar leituras e escritas
sob reservas, com escritas adiadas, identidade por objeto inteiro e flat nesting.
Este documento orienta trabalho futuro; sua criação não implementa nem autoriza,
por si só, alterações no compilador, na hash ou no runtime.

Leia também:

- [Guia geral OpenMP](OPENMP_AGENT.md).
- [Escopo e tipos](OPENMP_TRANSACTION_SCOPE_AGENT.md).
- [Hash e política transacional](OPENMP_TRANSACTION_HASH_AGENT.md).
- [Abort e reinício](OPENMP_TRANSACTION_ABORT.md).
- [Estado da implementação](OPENMP_TRANSACTION_IMPLEMENTATION.md).

As assinaturas abaixo já estão declaradas no esqueleto interno; seus contratos
funcionais continuam propostos. Não representam ABI exportada ou aprovação de
mecanismos ainda pendentes. Instruções do usuário prevalecem sobre este guia.
Não ampliar o subconjunto suportado para contornar uma pendência semântica.

Fontes em gcc-src-tm/, build em gcc-build/ e testes em transaction_test/,
relativos à raiz (/usr/src/gcc-tm no contêiner). Localize funções pelo nome;
números de linha mudam.

Estado conferido nos fontes:

- Os parsers preservam declarações em `OMP_TRANSACTION_OBJECTS`.
- `gcc/gimplify.cc` converte a lista em chamadas de início/registro/encerramento;
  o encerramento está em `GIMPLE_TRY_FINALLY`.
- `GIMPLE_OMP_TRANSACTION`, construído por `newtm_build_transaction`, usa
  `GSS_OMP` e guarda somente o corpo, sem lista própria de objetos.
- `gcc/omp-low.cc:newtm_lower_transaction` ainda apenas transforma o corpo no
  contexto OpenMP externo e elimina o marcador.
- libgomp/newtm-hash.c contém a hash operacional (nove primitivas), ponte com
  referências reais e três serviços owner para admissão/participações/close.
  Mutexes de buckets protegem registros e metadados; não há reservas ou valores
  privados transacionais. Uma referência ou lease não é autorização de acesso.
- O owner configura newtm_policy_destroy antes de publicar a tabela. Contexto e
  limpeza já existem, mas concessão de reservas e metadados sob demanda pendem.
- `libgomp/newtm-transaction.c` e `.h` contêm os esqueletos internos de
  newtm_read/write: apenas printf dos parâmetros, fflush e retorno INVALID.
  Não há chamada gerada pelo compilador nem implementação funcional de acesso.

Não usar os retornos INVALID/handles nulos dos stubs como implementação de
sucesso ou de conflito. A hash e a ponte com referências já são funcionais.
A próxima integração depende de contexto de tentativa, reservas/valores privados,
propagação de falha e instrumentação segura, preservando o owner já implementado.

Os mesmos arquivos abrigam a criação/limpeza real e as interfaces de ciclo de vida;
ver [PRIMITIVES](OPENMP_TRANSACTION_PRIMITIVES.md). Read/write ainda não chamam
newtm_fail. Na implementação futura, esse helper recebe causa de acesso e
retorna causa efetiva lifecycle; traduzir explicitamente, preservando INVALID.
Falha de acesso/end conserva recursos para cleanups e descarte explícito pelo
controle externo; nenhum end com falha já descartou a tentativa. Não converter
OK de discard em sucesso de acesso ou autorização de retry.

A máquina da tentativa está definida em
[ABORT](OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa) e no enum
`newtm_attempt_state` do cabeçalho interno. Acesso normal só ocorre em ACTIVE;
CONFLICTED devolve CONFLICT sem acessar dados, INVALID devolve INVALID e
prevalece sobre conflito. Falha permanente detectada em contexto válido agrava
CONFLICTED para INVALID. PUBLISHING é fase exclusiva e irrevogável de end;
CLOSED não admite acesso nem novo consumo. ctx passado a read/write/fail é
emprestado: essas operações nunca liberam o contexto ou consomem seu slot.
End externo bem-sucedido ou discard externo marca CLOSED e retira o slot antes
da limpeza física, invalidando os empréstimos. O enum não implementa transições;
os stubs mantêm retorno INVALID sem marcar estado ou anunciar sucesso.

## Interface interna e contrato funcional proposto

A camada já tem um esqueleto em `libgomp/newtm-transaction.h` e
`libgomp/newtm-transaction.c`. Manter tipos opacos,
`size_t`, convenções e alocadores da libgomp, sem dependência direta de pthread
no código comum. Separar política transacional da estrutura física da hash.

```c
struct newtm_context;

enum newtm_access_status
{
  NEWTM_ACCESS_OK,
  NEWTM_ACCESS_CONFLICT,
  NEWTM_ACCESS_INVALID
};

enum newtm_access_status
newtm_read (struct newtm_context *ctx,
            const void *object,
            size_t offset,
            void *destination,
            size_t size);

enum newtm_access_status
newtm_write (struct newtm_context *ctx,
             const void *object,
             size_t offset,
             const void *source,
             size_t size);
```

Usar o prefixo `newtm_` e verificar colisões antes de criar símbolos. As
primitivas permanecem internas, com visibilidade oculta. Seus estados não são
os estados estruturais `NEWTM_HASH_*`; fazer a tradução explicitamente.
CONFLICT significa incompatibilidade com reserva de outra transação lógica;
colisão de bucket não é conflito transacional. INVALID indica falha permanente
de contrato detectável, nunca uma solicitação de retry.

O parâmetro `object` em write identifica a chave; seu `const void *` não concede
nem retira permissão de escrita no programa. A permissão depende do tipo do
objeto original e deve ser verificada pelo compilador. Não aceitar escrita em
objeto const porque o buffer privado é mutável.

### Identidade e unidades

| Parâmetro | Significado |
| --- | --- |
| `ctx` | Contexto opaco da tentativa ativa, pertencente à execução atual. |
| `object` | Endereço canônico do objeto já registrado nesse contexto. |
| `offset` | Deslocamento em bytes a partir do início desse objeto. |
| `destination` | Buffer temporário privado que receberá os bytes da leitura. |
| `source` | Buffer temporário privado com os bytes da nova escrita. |
| `size` | Número de bytes efetivamente acessados nesta operação. |

A chave representa o objeto inteiro listado, não o subobjeto acessado. Para
`vetor[i]`, passar a chave de vetor e o deslocamento do elemento. Para `s.campo`,
passar a chave de s e o deslocamento do campo suportado. Esses acessos disputam
reservas sobre todo o objeto, mesmo quando atingem elementos distintos.

Listar um ponteiro registra o objeto ponteiro; não registra automaticamente a
memória apontada. Referências C++ identificam o objeto referido. `&x` continua
produzindo o endereço lógico original: nunca expor o endereço da cópia privada.
Aliases suportados devem resultar na mesma chave e deslocamento.

A hash implementa igualdade exata de chaves, sem descoberta de intervalos.
Decisão do usuário: rejeitar ponteiros sem identidade conhecida e registros
distintos sobrepostos, inclusive entre contextos. Campos/elementos de agregado
registrado usam sua entrada e deslocamentos. Vetor canônico conhecido admite
índice dinâmico, com avaliação única e verificação de limites em execução antes
do acesso. Resolução dinâmica exige contrato adicional e não integra a primeira
versão. Esses diagnósticos ainda precisam de implementação; a hash não os fornece.

## Contrato específico dos esqueletos atuais

Pré-condições desta etapa: parâmetros podem ser nulos; ponteiros são apenas
impressos, sem desreferência. offset/size são impressos como size_t, inclusive
zero e valores extremos. O chamador deve fornecer valores de ponteiro que possam
ser passados à função; não é necessário um contexto transacional real para um
teste isolado do esqueleto. Não criar acesso inválido para preparar argumentos.

Pós-condições: uma linha em stdout identifica operação e todos os parâmetros,
seguida de fflush; retorno sempre NEWTM_ACCESS_INVALID. Nenhum objeto, destino,
origem ou contexto é modificado; nenhuma validação, alocação, reserva, hash,
publicação ou limpeza transacional ocorre. printf tem os efeitos normais de E/S;
não se promete ordem entre threads ou sucesso de E/S pelo retorno INVALID.

Este comportamento é exceção explícita aos contratos funcionais abaixo. INVALID
é um sentinela de ausência de implementação, não diagnóstico real de argumento
ou transação condenada. Não inserir esses stubs no caminho de execução do
programa como se produzissem uma leitura ou escrita válida. Os símbolos são
internos/ocultos e o cabeçalho não é instalado como API pública.

## Pré-condições comuns

1. `ctx` é válido, pertence à thread/tarefa executante e identifica uma tentativa
   ativa. Todas as entradas internas usam o mesmo contexto. Não há suspensão,
   migração nem ponto de escalonamento de tarefas durante sua execução dinâmica.
2. `object` é não nulo e foi registrado antes do acesso, na lista externa ou em
   lista interna já executada. O registro não leu seu valor nem adquiriu reserva.
3. O contexto mantém uma referência física válida à entrada hash. O objeto do
   programa permanece vivo até o fim da participação externa, inclusive quando
   acrescentado por uma região interna.
4. O tipo do acesso pertence ao subconjunto suportado. O compilador preservou
   qualificadores, identidade, alinhamento e as regras de vida útil/aliasing.
5. `size > 0`. Com `object_size` obtido do registro, verificar
   `offset <= object_size` e `size <= object_size - offset` antes de somar ou
   formar um endereço de acesso. Rejeitar faixa fora dos limites, sem overflow.
6. `destination`/`source` é não nulo, tem ao menos size bytes válidos e pertence
   a armazenamento privado da tentativa, sem sobreposição com o objeto original,
   com buffers internos da política ou com outro objeto transacional.
7. A origem da escrita está inicializada conforme as regras da linguagem. Ela
   já foi avaliada com a instrumentação necessária; preparar source não pode
   esconder uma leitura comum de objeto transacional.

Validar o que for verificável, mas não prometer detectar ponteiro pendente,
buffer pequeno ou proprietário errado por inspeção de um endereço arbitrário.
Esses casos são violações de pré-condição, não conflitos recuperáveis. Testes de
argumentos inválidos devem usar entradas cuja validação seja segura.

Para ctx válido e proprietário correto, uma falha permanente detectada deve
impedir commit e registrar motivo permanente no contexto. Para ctx nulo,
retornar INVALID sem desreferenciá-lo. Se houver estado inválido que não possa
ser manipulado com segurança, seguir o mecanismo explícito de erro fatal da
libgomp; nunca tentar limpar ou alterar contexto de outra execução.

Objeto não registrado retorna INVALID nesta interface. Não inserir implicitamente
na hash nem executar acesso comum como fallback. Todo acesso preexistente exige
registro explícito, inclusive leitura. O compilador deve diagnosticar os casos
identificáveis; INVALID detectado com contexto válido segue limpeza e fatal.
Um ponteiro sem identidade conhecida deve ser rejeitado, não presumido externo.

## Contrato de newtm_read

### Sequência obrigatória

1. Validar contexto, participação, argumentos e limites antes do acesso ao objeto.
2. Consultar/alterar os metadados da política sob o mutex do bucket da entrada.
3. Se houver escritor de outra transação, marcar a tentativa para abort e
   retornar CONFLICT sem ler o objeto nem modificar destination.
4. Na ausência de escritor incompatível, adquirir/manter reserva compartilhada.
   Se o contexto já possui a exclusiva, reutilizá-la, sem rebaixamento.
5. Soltar o mutex após estabelecer a reserva e a consistência dos metadados.
6. Copiar a faixa solicitada da visão lógica da própria transação: bytes já
   escritos vêm da cópia privada; os demais vêm de uma origem estabilizada pela
   reserva. Uma cópia privada inicializada integralmente também pode atender.
7. Retornar OK somente depois de preencher destination completamente.

A reserva deve preceder a primeira leitura da memória original, inclusive a
inicialização de qualquer cópia. Nunca adquirir a reserva depois de copiar.

### Pós-condições

- OK: destination contém os bytes da faixa na visão da transação, incluindo
  suas escritas anteriores; o objeto original permanece inalterado.
- CONFLICT/INVALID: destination permanece inalterado; o chamador não pode
  consumi-lo como valor produzido pela operação.
- Reserva obtida com sucesso permanece até o término externo; retorno não
  libera reserva ou referência. O contexto continua ativo em OK.

Não realizar operação falível recuperável depois de começar a modificar
destination. Preparar recursos antes ou usar temporário intermediário. Falha
fatal de alocação encerra o processo conforme o contrato dos alocadores atuais;
não inventar retorno NO_MEMORY que nunca é produzido.

## Contrato de newtm_write

### Sequência obrigatória

1. Validar contexto, participação, argumentos, tipo e limites aplicáveis.
2. Preparar recursos privados necessários antes de aplicar a nova alteração,
   sem ler o objeto original antes de obter a reserva apropriada.
3. Sob o mutex do bucket, conceder/manter reserva exclusiva somente se não
   houver escritor nem leitores de outras transações. A própria participação
   não conta como conflito.
4. Converter a reserva própria de leitura para exclusiva apenas se não houver
   outro leitor. Caso contrário, condenar a tentativa e retornar CONFLICT.
5. Soltar o mutex depois de estabelecer a exclusiva e os metadados consistentes.
6. Inicializar/preservar as partes necessárias da representação privada sob
   reserva; copiar source para a faixa privada e registrar os bytes alterados.
7. Retornar OK. Não copiar a nova escrita para a memória original.

### Pós-condições

- OK: a visão privada da faixa contém source; leituras posteriores da própria
  transação a observam. As demais partes mantêm seus valores lógicos anteriores.
- CONFLICT/INVALID: a alteração solicitada não foi aplicada, nem parcialmente.
  Escritas especulativas anteriores não são publicadas.
- Em todo retorno normal, a memória original permanece inalterada.
- A reserva exclusiva permanece até confirmação ou descarte externo.

É aceitável manter referências, reservas e buffers adquiridos anteriormente em
uma tentativa condenada até o protocolo de abort limpá-los. Não confundir essa
retenção com sucesso parcial da operação. Não liberar tudo dentro de write:
cleanups suportados podem precisar do contexto ainda válido.

## Condições comuns de retorno das primitivas de acesso

Na implementação funcional, toda saída normal libera qualquer mutex adquirido
pela chamada, preserva a profundidade e a identidade do contexto e não executa
commit, rollback, backoff ou nova entrada/saída de região. Referências e reservas
bem-sucedidas pertencem ao contexto até seu encerramento; não transferir sua
propriedade ao buffer de saída. A origem de write nunca é modificada.

Em falha recuperável, somente o motivo e o estado condenado do contexto válido
podem mudar, além de recursos intermediários que precisam ficar consistentes
para posterior limpeza. A operação solicitada não tem sucesso parcial. INVALID
por ctx NULL não altera contexto algum. Violação que não possa ser validada com
segurança não garante retorno recuperável. Nenhuma falha autoriza leitura do
destino não preenchido, acesso comum alternativo ou reinício apenas interno.

Os mesmos contratos se aplicam às futuras pontes de acesso GOMP_* em relação aos
dados e resultados; aquisição do contexto e sinalização ABI ainda devem ser
especificadas quando seus protótipos forem definidos.

Exceções do usuário não são CONFLICT e não autorizam retry; desenrolamento
não pode provocar commit. O rastreamento atual equilibra finalmente/encerramento,
mas isso não comprova suporte a exceções ou destrutores no subconjunto STM.
Destrutores internos de metadados continuam obrigatórios conforme release e
não se confundem com cleanups do usuário rejeitados nesta decisão.

## Representação privada e restrições de tipos

O transporte em bytes evita uma família inicial de funções por tipo, mas não
resolve sozinho as regras C/C++. Começar pelo subconjunto definido no agente da
hash: escalares comuns e agregados admitidos, completos, de tamanho conhecido.
Validar recursivamente os componentes. Ficam fora inicialmente volatile,
atômicos, VLA, unions, bit-fields, membros flexíveis e casos sem contrato;
escritas em const permanecem proibidas. Instâncias de classes ficam para trabalho
futuro, incluindo struct C++: trivialidade não as habilita implicitamente.
Isso define o subconjunto funcional, não descreve filtros já ativos no parser.

Não implementar a leitura de um campo como uma leitura tipada de todos os
outros campos. Evitar introduzir avaliação de valores não inicializados,
acessos fora do objeto ou interpretação de padding.

Decisão: escalares usam representação privada simples; agregados usam buffers
privados com faixas válidas e modificadas. Toda faixa modificada é válida.
Leituras completam somente partes solicitadas ainda ausentes, sob reserva,
preservando os bytes já escritos pela tentativa. Escritas marcam seus bytes
como válidos/modificados, sem exigir leitura prévia ou inicialização integral do
agregado; uma operação read-modify-write exige a leitura válida da parte usada.
Commit publica somente faixas modificadas; descarte libera buffers e faixas sem
publicação. Não ler bytes ausentes nem interpretar padding como valores.
A unidade de conflito permanece o objeto canônico, não cada faixa.

As faixas serão listas de intervalos sobre buffer de extensão completa,
preenchido sob demanda. Detalhar sua manutenção e respeitar alinhamento, vida útil e
aliasing; não anunciar memcpy universal como solução para todos os tipos C++.
Buffers nunca são endereços observáveis do programa: `&x` preserva o endereço
lógico original; campos/elementos preservam seus offsets. A instrumentação
redireciona acessos sem expor buffers nem permitir ponteiros desconhecidos.
Esta representação é contrato futuro, não implementação dos stubs atuais.

source/destination pertencem ao código instrumentado e não podem executar
construtores, conversões, destrutores ou callbacks do usuário dentro das
primitivas. Essas operações exigem tratamento próprio no compilador. Não expor
ponteiros aos buffers privados como resultado destas APIs.

## Sincronização e ordenação de memória

Usar os mutexes portáveis da libgomp. A sincronização dos metadados ocorre pelo
mutex do bucket, em operações curtas realizadas por `newtm_hash_entry_visit`
ou serviço equivalente cujo contrato seja explicitamente definido.

| Operação | Proteção exigida |
| --- | --- |
| Obter/manter referência física | Primitivas hash, sob o mutex do bucket. |
| Examinar leitores/escritor e conceder/converter reserva | Uma seção indivisível sob esse mesmo mutex. |
| Consultar chave e tamanho imutáveis | Referência física válida; não exige mutex adicional. |
| Manipular buffers privados | Propriedade exclusiva do contexto; sem compartilhamento concorrente. |
| Ler memória original | Reserva compatível adquirida antes do acesso. |
| Publicar memória original | Exclusiva própria mantida, no commit externo. |

Não separar a verificação de conflito da aquisição da reserva por um unlock.
Não usar referência física ou presença na hash como permissão de acesso.
Não manter o mutex do bucket durante o corpo da região, callbacks do usuário,
backoff, chamadas transacionais recursivas ou espera pelo dono de reserva.

Após conceder a reserva, as cópias de dados podem ocorrer fora do mutex do
bucket porque a reserva continua válida e todos os participantes respeitam o
protocolo. O contexto não pode ser limpo concorrentemente enquanto essas cópias
ocorrem. Não oferecer essa garantia para acessos externos não instrumentados.

Cada acesso trata uma entrada; não adquirir vários mutexes de buckets na mesma
operação. Não usar um mutex global no caminho normal de leitura/escrita.
Callbacks de visit não podem reentrar na hash, chamar count ou esperar por
outra transação. Preferir preparar alocações fora dessas seções curtas.

A publicação dos dados no commit deve preceder a retirada da reserva sob o
mutex; sua liberação e a aquisição posterior precisam estabelecer a ordenação
de memória necessária. Verificar o contrato dos mutexes em cada target usado;
volatile não substitui sincronização e fences isoladas não substituem reservas.

No commit, conservar todas as reservas enquanto publica todas as escritas.
Liberar qualquer reserva somente depois da publicação completa. Preparar antes
da primeira publicação tudo que possa falhar; depois dela não permitir abort
por conflito, código do usuário ou operação lançadora. As primitivas de acesso
não executam commit nem publicam dados por conta própria.

## Pontos de invocação no compilador

### Quem chama e quando

O compilador gera chamadas para execução do programa; não consulta endereços
reais dos objetos durante a compilação. Não chamar read/write no parser, em
`newtm_finish_transaction` ou como parte do simples registro das listas.

Emitir read imediatamente antes do uso do valor de um acesso transacional e
write no lugar da escrita correspondente. Avaliação de índices, endereços e
operandos deve preservar a ordem e a quantidade de avaliações exigidas pela
linguagem. Expressões usadas no cálculo do endereço também podem exigir leituras
transacionais antes de formar o acesso final.

Para `int x`, a transformação conceitual de `x++` é:

```c
int temporary;
status = newtm_read (ctx, &x, 0, &temporary, sizeof temporary);
if (status != NEWTM_ACCESS_OK)
  /* transferir para tratamento de falha; nao usar temporary */;
++temporary;
status = newtm_write (ctx, &x, 0, &temporary, sizeof temporary);
if (status != NEWTM_ACCESS_OK)
  /* transferir para tratamento de falha */;
```

Os comentários representam transferência obrigatória, não um bloco vazio que
permita continuar. Preservar resultados de pre/pos-incremento, conversões e
regras de overflow da operação original. Uma promoção de reserva que conflita
pode abortar x++; não é preciso fundir as duas chamadas para correção inicial.

Para `b = a`, ler a para um temporário privado e escrever esse temporário em b.
Não materializar source por cópia comum de a. Temporários gerados pelo compilador
não devem ser reinstrumentados como objetos do programa.

### Ponto de integração nesta árvore

O ponto previsto para transformar os acessos é
`gcc/omp-low.cc:newtm_lower_transaction`, enquanto
`GIMPLE_OMP_TRANSACTION` e seu corpo ainda existem, antes de
`gsi_replace_with_seq`. Preparar a classificação e emitir as chamadas antes do
`lower_omp(..., ctx->outer)` atual quando essas chamadas precisarem passar pelo
remapeamento OpenMP existente. Não simplesmente inserir chamadas depois do
lowering usando declarações originais de variáveis private/reduction.

Esse ponto exige trabalho preparatório: hoje a lista não está no marcador
GIMPLE. Preservar metadados de identidade desde `OMP_TRANSACTION_OBJECTS`, ou
introduzir representação intermediária equivalente durante a gimplificação.
Não reconstruir escopos por strings nem presumir que reconhecer chamadas
GOMP_transaction_register seja análise suficiente de todos os acessos.

Auditar `gcc/gimplify.cc`, `gcc/gimple.def`, `gcc/gimple.h`, `gcc/gimple.cc`,
`gcc/omp-low.cc:scan_omp_1_stmt` e os consumidores de cópia/walk/dump da
representação escolhida. Se dados adicionais afetarem captura, preparar sua
visibilidade antes da varredura OpenMP. A varredura atual atravessa transaction
no contexto externo; preservar a identidade das instâncias efetivas privadas,
reductions e variáveis capturadas por outlining.

Planejar regiões aninhadas antes da transformação recursiva. Instrumentar cada
acesso exatamente uma vez, compartilhar o contexto e não revisitar chamadas ou
temporários já instrumentados quando o marcador interno tiver sido removido.
A lista interna amplia o conjunto apenas a partir de seu registro dinâmico.

A transformação deve abranger acessos em funções chamadas preparadas para
execução transacional. Percorrer somente o corpo lexical do pragma não cobre
uma função separadamente compilada. Antes de admiti-la, definir identificação
ou geração da versão instrumentada escolhida, contexto/status explícitos e retorno
seguro da falha. Inline não é prova de instrumentação correta.

Auditar acessos em atribuições, condições, retornos, argumentos, índices,
MEM_REF/COMPONENT_REF/ARRAY_REF, cópias agregadas e builtins de memória. Não
permitir que memcpy/memmove/memset ou uma chamada desconhecida contornem o
protocolo. Casos sem suporte devem ser restringidos, não silenciosamente
executados sobre a memória original.

O compilador deve produzir chamadas comuns com efeitos de memória e arestas
explícitas de sucesso/falha. Não atribuir pure, const ou propriedades que
permitam remover ou mover reservas, leituras ou escritas indevidamente.
Propriedades nothrow/leaf dependem do mecanismo de propagação escolhido; não
copiá-las automaticamente da ponte provisória. Verificar O0, O2 e LTO.

### Ponte do compilador para o runtime

Estas primitivas internas não devem ser exportadas apenas porque o código
instrumentado precisa chamá-las. Definir ponte `GOMP_transaction_*` própria,
com declarações em `libgomp/libgomp_g.h`, builtins em `gcc/omp-builtins.def`,
tipos em `gcc/builtin-types.def` quando necessários e versionamento adequado
em `libgomp/libgomp.map`.

A ponte pode obter o contexto da execução atual; a interface interna mantém
ctx explícito. Não mudar as assinaturas atuais de begin/register/end apenas
pela criação destas duas primitivas. Fixar a ABI da ponte somente junto do
contrato de falha e das dependências de commit/abort.

## Falha, aninhamento e pós-condições globais

CONFLICT condena a tentativa externa inteira. Não reiniciar somente a operação,
o bloco interno ou a função chamada. Se o contexto já está condenado, recusar
novos acessos normais sem ler/escrever dados; preservar o motivo permanente
quando a causa for INVALID. Não transformar erro permanente em CONFLICT.

O chamador deve verificar todo retorno antes de usar resultados ou continuar
com efeitos do programa. Não fornecer valor fictício e esperar que a falha de
commit posterior torne segura a execução intermediária.

Não fazer limpeza total dentro de read/write. Executar cleanups suportados com
o contexto ainda válido; depois descartar buffers, retirar reservas e
participações, liberar referências e reconstruir a tentativa externa. Acesso
por cleanup exige protocolo próprio: a recusa normal de contexto condenado
não implementa, por si só, esse caminho. Restringir cleanups ainda sem suporte.

A propagação foi definida pelo usuário: gerar versões internas com contexto
e status explícitos, preservando assinatura pública e preenchendo resultado só
em sucesso. Verificar status de cada acesso e chamada preparada antes de usar
seu resultado ou executar efeitos normais posteriores. Falha leva ao epílogo
da função e é propagada até o controle externo, sem resultados fictícios ou
consumo antecipado do contexto. Manter a lease até concluir cleanups/recursos.

Chamadas indiretas ou externas sem preparação ficam restritas; pure/const não
substitui a versão transacional. A geração dessas versões e sua ABI ainda são trabalho de integração.
Exceções/cleanups do usuário são rejeitados inicialmente; seu suporte futuro
exige definir como destrutores acessam dados de uma tentativa condenada.
Read/write recusam novos acessos normais nesse estado; não relaxar os guards
nem usar esse caminho em cleanups para simular suporte. A limpeza interna do
runtime (reservas/referências/buffers/contexto/lease) continua obrigatória. Não usar setjmp/longjmp nem
atravessar destrutores por salto arbitrário. Exceção do usuário não equivale a
conflito nem autoriza retry infinito.

O encerramento atual em TRY_FINALLY serve ao rastreamento. A futura integração
deve distinguir commit no término normal, descarte no desenrolamento e retry.
Não confirmar automaticamente no finally e não executar encerramento normal de
níveis internos que já tenham sido desfeitos pelo abort externo.

## Validação e critérios de entrega

Entregar testes determinísticos das primitivas e da instrumentação. Não usar
apenas resultados aritméticos ou printf como evidência de isolamento.

| Grupo | Casos mínimos |
| --- | --- |
| Contratos | Contexto nulo/inativo, objeto não registrado, tamanho zero, limites exatos, offset e tamanho extremos sem overflow. |
| Leitura | Valor inicial, faixa parcial, duas leituras, leitura após própria escrita, destination preservado na falha. |
| Escrita | Nenhuma publicação antecipada, faixas não alteradas preservadas, duas escritas sobrepostas no mesmo objeto, source inalterado, falha sem aplicação parcial. |
| Reservas | Leitura/leitura compatível, leitura/escrita e escrita/escrita conflitantes; promoção própria com e sem outro leitor. |
| Aninhamento | Mesmo contexto, ausência de conflito consigo mesmo, registro interno retido e nenhum commit/liberação independente. |
| Commit/abort | Publicação conjunta observável só pelo protocolo, descarte integral, nenhuma reserva/referência restante, retry externo completo. |
| Identidade | Escalares, campo/elemento, ponteiro versus apontado, referência C++, alias suportado e rejeição dos demais. |
| GCC | Valor de pre/pos-incremento, avaliação única de índices, curto-circuito, private/reduction externos, templates, chamadas preparadas e ausência de dupla instrumentação. |
| Erros | Estado condenado impede uso de resultado; INVALID não provoca retry infinito; destrutores/exceções e efeitos não suportados são tratados ou rejeitados. |

Forçar as ordens relevantes de conflito por mecanismos de teste controlados,
sem inserir barreiras ou pontos de escalonamento de tarefas em transações do
programa. Hooks internos de teste não devem virar API pública nem conceder
operações antes das reservas. Cobrir buckets distintos e colisões sem confundir
contenda estrutural com conflito de objetos.

Testar as primitivas internas por mecanismo que respeite sua visibilidade;
não exportar símbolos somente para permitir ligação de um teste. Verificar os
retornos e invariantes dos metadados, não só o número de mensagens.

Usar DejaGNU nos diretórios apropriados do GCC e da libgomp. Construir o
compilador e o runtime em `/usr/src/gcc-build`; confirmar nos comandos dos logs
que os executáveis ligam a libgomp reconstruída. Conferir .sum/.log, distinguir
PASS/FAIL/UNRESOLVED/UNSUPPORTED e registrar comandos efetivamente executados.

A entrega das duas funções isoladas não significa STM pronta. A integração só
pode anunciar acesso transacional quando houver hash/contexto funcionais,
reservas corretas, instrumentação completa no subconjunto admitido, propagação
segura da falha, publicação/descarte e testes correspondentes. Relatar
explicitamente tudo que continuar pendente e atualizar os agentes afetados.

## Inventário consolidado e sequência de trabalho

Consultar [OPENMP_TRANSACTION_PRIMITIVES.md](OPENMP_TRANSACTION_PRIMITIVES.md)
para assinaturas existentes, propriedade dos recursos, estados, efeitos atuais
e futuros e pontos de chamada. Os contratos especializados deste agente
permanecem detalhados aqui; mudanças de interface devem atualizar também o
inventário. Os nomes stm_* continuam conceituais, sem assinatura C ou ABI definida.

Estado consolidado em 2026-10-28: as nove primitivas da hash, a ponte com
referências reais e o controle de admissão/participações OPEN/CLOSING/CLOSED
estão implementados. Os testes permanentes incluem estrutura, concorrência,
metadados, contagem, diagnósticos, encerramento e carga parametrizada 1:9/1:1/9:1.
Não listar novamente a implementação da hash ou seu shutdown como tarefas futuras.
Read/write e os seis serviços de ciclo de vida da tentativa continuam esqueletos;
reservas, dados privados, instrumentação de acessos, propagação segura, commit,
descarte e retry funcionais continuam pendentes. A ordem de integração está em
[próximas etapas](OPENMP_TRANSACTION_PROJECT_AGENT.md#orientação-para-as-próximas-etapas).
Não confundir participação do proprietário com reserva/estado de tentativa STM.
Fontes: `gcc-src-tm/`; build: `gcc-build/`; testes: `transaction_test/`, na raiz do
repositório. No contêiner atual, a raiz é `/usr/src/gcc-tm`. Caminhos antigos em
relatos datados são históricos; adaptar comandos à cópia em uso.
