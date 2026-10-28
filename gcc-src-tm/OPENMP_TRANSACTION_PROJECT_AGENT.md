## Checkpoint de regressão da base atual

Alvo `make -C transaction_test check-checkpoint-contexto`, implementado em
[checkpoint_contexto_pronto_para_registro.py](../transaction_test/checkpoint_contexto_pronto_para_registro.py).
Reúne oito alvos existentes com recompilação dos testes, logs individuais,
relatório JSON, timeout e parada na primeira falha. Nome do marco:
contexto_pronto_para_registro. Não acrescenta cobertura STM nem substitui testes
pendentes de OOM do contexto, descarte simultâneo ou matriz completa de estados.
Reconstruir a libgomp antes de executar; sanitizadores não fazem parte do alvo.

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

Estrutura inicial aceita: [estruturas e invariantes do contexto](OPENMP_TRANSACTION_CONTEXT_PROPOSAL.md).
Implementar esse desenho, sem substituir pelas alternativas comparadas no documento.

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

# Agente de visão geral do projeto transaction

## Missão e uso deste documento

Orientar a evolução da extensão experimental `#pragma omp transaction` no GCC
16.2.0, apresentando a intenção do projeto, a política de memória esperada,
as responsabilidades das camadas, os arquivos envolvidos e o estado atual.
Usar este documento como ponto de entrada; os contratos detalhados permanecem
nos agentes especializados relacionados abaixo.

Este agente descreve e organiza o projeto. Sua criação não autoriza implementar
etapas futuras, mudar a política ou consolidar uma ABI ainda pendente. Instruções
do usuário prevalecem. Ao executar uma tarefa, conferir os fontes e atualizar o
estado somente com o que foi efetivamente implementado e verificado.

Os caminhos dos fontes são relativos a gcc-src-tm/; transaction_test/ fica
na raiz do repositório, ao lado de gcc-build/. No contêiner atual a raiz é
/usr/src/gcc-tm. Build sempre separado dos fontes. Localizar funções pelo nome, pois
números de linha mudam. Não confundir resultados históricos com testes executados
na tarefa em andamento.

## Intenção do projeto

Oferecer memória transacional em software (STM) por meio de uma construção
lexical integrada aos front-ends C e C++, à representação intermediária do GCC
e ao runtime libgomp. O programador deverá escrever acessos C/C++ comuns dentro
do bloco; o compilador deverá instrumentar os acessos suportados automaticamente.
A lista da diretiva identifica os objetos elegíveis para a transação.

```c
#pragma omp transaction (a, b)
{
  a++;
  b += a;
}
```

O objetivo funcional é manter escritas especulativas privadas, impedir acessos
incompatíveis entre transações participantes, publicar as escritas na confirmação
externa e repetir a tentativa externa em caso de conflito. Isso ainda não é a
semântica oferecida pelo executável atual: hoje o corpo acessa memória comum.

A diretiva é uma extensão própria, não uma diretiva do padrão OpenMP. Reutiliza
a infraestrutura OpenMP e libgomp, mas não é um alias de critical e não usa a
implementação GNU TM (`__transaction_atomic`, `GIMPLE_TRANSACTION`, libitm) como
mecanismo implícito. O marcador próprio é `GIMPLE_OMP_TRANSACTION`.

O escopo atual é C/C++ no host. Não anunciar suporte Fortran, dispositivos de
offload ou identidade transacional entre host/dispositivo sem trabalho e testes
específicos. O objetivo de concorrência inclui transações executadas por threads
de uma região parallel externa; transaction não cria uma equipe por si só.

## Documentos e responsabilidades

| Documento | Conteúdo e responsabilidade |
| --- | --- |
| [OPENMP_AGENT.md](OPENMP_AGENT.md) | Percurso geral de diretivas, integração ao GCC/libgomp, contratos e validação. |
| [OPENMP_CRITICAL_AGENT.md](OPENMP_CRITICAL_AGENT.md) | Construção análoga para estudar parsing, lowering e mutexes; não define STM. |
| [OPENMP_TRANSACTION_SCOPE_AGENT.md](OPENMP_TRANSACTION_SCOPE_AGENT.md) | Resolução de nomes, categorias, tipos, identidade e preservação/remapeamento de dados. |
| [OPENMP_TRANSACTION_HASH_AGENT.md](OPENMP_TRANSACTION_HASH_AGENT.md) | Política geral, estrutura da hash, referências, callbacks e ponte atual de rastreamento. |
| [OPENMP_TRANSACTION_HASH_IMPLEMENTATION_AGENT.md](OPENMP_TRANSACTION_HASH_IMPLEMENTATION_AGENT.md) | Manutenção da hash já implementada, decisões adotadas, protocolo do proprietário, testes e pendências de validação; não reimplementar como tarefa futura. |
| [OPENMP_TRANSACTION_ACCESS_AGENT.md](OPENMP_TRANSACTION_ACCESS_AGENT.md) | Contratos de newtm_read/write, objeto/deslocamento, sincronização e instrumentação. |
| [OPENMP_TRANSACTION_ABORT.md](OPENMP_TRANSACTION_ABORT.md) | Ciclo de vida, estados de falha, cleanups, descarte, confirmação e reinício externo. |
| [OPENMP_TRANSACTION_PRIMITIVES.md](OPENMP_TRANSACTION_PRIMITIVES.md) | Inventário central: assinaturas, propriedade, estados, efeitos e chamadas; separa stubs e operações conceituais. |
| [OPENMP_TRANSACTION_IMPLEMENTATION.md](OPENMP_TRANSACTION_IMPLEMENTATION.md) | Estado de implementação, contratos locais dos helpers, evidências e histórico identificado como tal. |
| [OPENMP_TRANSACTION_AUDIT.md](OPENMP_TRANSACTION_AUDIT.md) | Auditoria de 2026-10-21: verificação dos fontes/contratos, resultados atuais, correção do verificador e limites. |

A hash mantém registros e referências físicas. A camada de acesso/política
mantém reservas e valores privados. O controle de abort conduz propagação de
falha, cleanups, descarte e repetição. Não deslocar a política para a hash nem
executar limpeza integral dentro de uma operação de leitura/escrita.

Em divergência documental, conferir o código para descrever o estado atual e
explicitar o conflito para regras futuras; não escolher silenciosamente uma
semântica nova. Assinaturas stm_* conceituais não constituem ABI aprovada.

## Política esperada de manipulação da memória

Esta seção resume a política especificada para implementação futura. Não é uma
lista de garantias atualmente fornecidas pelo runtime.

### Objetos, registro e identidade

- A lista usa identificadores simples, resolvidos no escopo da diretiva antes
  de analisar seu corpo. Preservar a declaração, o tipo e os qualificadores.
- O registro dinâmico associa o endereço canônico e o tamanho do objeto ao
  contexto; não lê seu valor nem adquire antecipadamente reserva de acesso.
- A unidade de conflito é o objeto inteiro listado. Campo ou elemento usa a
  chave do objeto que o contém e um deslocamento em bytes, sem criar uma
  unidade de conflito independente por acidente.
- Listar ponteiro registra o objeto ponteiro, não toda a memória apontada.
  Referências C++ designam o objeto referido. Preservar o endereço lógico:
  `&x` não pode passar a expor o buffer privado.
- Aliases suportados devem usar a mesma identidade e visão privada. Igualdade
  exata de chaves na hash não resolve aliases arbitrários nem sobreposições
  parciais. Restringir casos sem mecanismo de identificação válido.
- Objetos registrados, inclusive os acrescentados em regiões internas, precisam
  permanecer vivos até o encerramento externo. Referência à hash não prolonga
  a vida útil do objeto do programa.

### Contexto e aninhamento

A primeira entrada encontrada dinamicamente estabelece a transação externa.
Entradas subsequentes, inclusive em funções chamadas preparadas, compartilham
contexto, tentativa e identidade: flat nesting. Listas internas acrescentam
objetos; duplicatas são idempotentes no contexto, mas tamanhos incompatíveis
não podem ser ocultados pela deduplicação.

Saída interna apenas reduz a profundidade. Não confirma, descarta ou libera
reservas/objetos independentemente. Um acesso antes do processamento de uma
lista interna não se torna transacional retroativamente.

Contexto local à thread só é válido sob a restrição de executar integralmente
na mesma tarefa, sem pontos de escalonamento durante toda a tentativa, inclusive
em chamadas transitivas. Ser uma tarefa tied, isoladamente, não assegura isso.

A máquina especificada em
[ABORT](OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa) distingue
ACTIVE, CONFLICTED, INVALID, PUBLISHING e CLOSED, declarados como
`enum newtm_attempt_state` no cabeçalho interno. INVALID prevalece sobre
conflito; PUBLISHING só começa após preparação falível e é irrevogável.
CLOSED é terminal, não um resultado de commit. Espera ocorre sem contexto;
retry cria uma nova tentativa. Cada tentativa possui um slot canônico: end
externo após publicação ou discard pelo controle externo marca CLOSED e retira
o slot antes de liberar recursos. Esse consumidor é o único dono da limpeza.
O enum e os contratos estão definidos; contexto, transições e guardas funcionais
continuam por implementar. Os stubs não anunciam confirmação bem-sucedida.

### Hash global e sincronização

A hash prevista é global à instância do runtime, compartilhada entre threads,
com 1.024 buckets na configuração inicial, capacidade configurável na criação,
encadeamento para colisões e sem redimensionamento inicial. Capacidade não
limita a quantidade total de objetos registrados.

Um mutex por bucket protege estrutura e operações curtas nos metadados da
política. Verificar conflito e conceder/converter reserva na mesma seção
indivisível. Não manter mutex de bucket durante o corpo da região nem usar um
mutex global para serializar o caminho normal dos acessos.

Referência física mantém a entrada viva; reserva concede permissão de acesso.
Não tratar esses conceitos como equivalentes. O desenho da hash especifica
remoção na última referência; a política de remoção foi registrada no agente
da hash como proposta, não como decisão individual já consolidada pelo usuário.

### Leituras, escritas e conflitos

| Operação solicitada | Reservas de outras transações | Resultado esperado |
| --- | --- | --- |
| Leitura | Nenhuma ou somente leitores | Conceder/manter compartilhada e ler a visão da transação. |
| Leitura | Escritor | Impedir leitura e sinalizar conflito. |
| Escrita | Nenhuma | Conceder exclusiva e escrever somente na cópia privada. |
| Escrita | Leitor ou escritor | Impedir escrita e sinalizar conflito. |
| Promoção de leitura própria para escrita | Nenhum outro leitor/escritor | Converter para exclusiva. |
| Promoção de leitura própria para escrita | Reserva incompatível de outra transação | Sinalizar conflito na solicitante. |

Reservas da própria transação, inclusive de níveis internos, não conflitam
consigo mesmas. Conflito aborta a solicitante, não o detentor da reserva. Não
esperar pelo detentor mantendo reservas próprias; a espera entre tentativas
ocorre somente após descarte completo.

Obter a reserva antes de ler a memória original, inclusive para inicializar
cópia privada. Manter reservas até confirmação ou descarte externo. Leituras
observam escritas anteriores da própria tentativa. Escritas parciais preservam
as partes não alteradas, respeitando regras de representação e tipos C/C++.
Não implementar cópia tipada indiscriminada de campos não inicializados ou
padding, nem assumir que memcpy serve para toda classe C++.

As primitivas internas são `newtm_read(ctx, object, offset, destination, size)`
e `newtm_write(ctx, object, offset, source, size)`. O contrato funcional exige
faixas válidas sem overflow, temporários privados e verificação de cada retorno
antes de usar o resultado ou continuar com efeitos do programa. Os estados
NEWTM_ACCESS_* não são estados estruturais NEWTM_HASH_*.

### Confirmação, descarte e reinício

Apenas a saída externa saudável pode confirmar. Preparar recursos antes da
publicação, manter todas as reservas enquanto publica todas as escritas e só
então liberar qualquer reserva. Não permitir abort por conflito depois da
primeira escrita publicada. A ordenação de memória deve ligar publicação,
retirada das reservas e aquisição posterior pelo protocolo.

CONFLICT condena a tentativa externa inteira e impede a continuação normal.
A primitiva sinaliza a causa sem executar limpeza global. O controle de abort
propaga a falha por caminho seguro, executa cleanups suportados com recursos
válidos, descarta valores privados, retira reservas/participações e libera
referências/buffers exatamente uma vez. Depois espera sem reservas/locks e
reinicia todo o bloco externo, incluindo funções e listas internas.

INVALID é falha permanente no contrato funcional: nunca autoriza retry e, com
contexto válido, exige limpeza segura seguida de diagnóstico fatal.
Nos stubs atuais tem outro significado explicitamente temporário: ausência de
implementação, sem validação ou alteração de estado.

Backoff progressivo tem teto por espera, com tentativas sem limite conforme a
política prevista. Não promete justiça, ausência de starvation ou conclusão.
Não usar setjmp/longjmp no retry normal nem atravessar destrutores por saltos
arbitrários. Reconstruir temporários privados de cada tentativa.

### Regras definidas e decisões ainda pendentes

- Acessos externos não instrumentados ficam fora do protocolo; não há isolamento
  forte em relação a eles nem alteração das regras C/C++ de corrida de dados.
- Escritas em objetos cuja vida começou antes da transação exigem registro
  prévio. A forma sem lista não autoriza incluir implicitamente todos os objetos.
- Decisão atual: todos os acessos preexistentes exigem registro explícito,
  inclusive leituras; substitui a permissão anterior de leituras externas comuns.
  Sem fallback; INVALID em contexto válido exige limpeza segura e fatal, sem retry.
- Decisão: escalares, arrays e agregados restritos, completos e de tamanho
  conhecido, com validação recursiva. Excluir volatile, atômicos, VLA, unions,
  bit-fields, membros flexíveis e casos sem contrato; proibir escritas const.
  Instâncias de classes ficam para trabalho futuro, incluindo struct C++.
- Decisão: vetor[i] admite índice dinâmico com vetor canônico conhecido e limites
  em execução; ponteiros sem identidade conhecida são rejeitados. Campos e
  elementos usam a entrada do agregado registrado e offsets. Proibir registros
  distintos sobrepostos; repetição do mesmo objeto/mesma extensão é deduplicação.
  A hash não verifica intervalos: aplicação e diagnósticos ainda são trabalho
  de integração, não garantias executadas pelo código atual.
- Decisão: escalares privados simples; agregados com faixas válidas/modificadas,
  sem exigir inicialização/acesso integral para operar sobre um campo. Commit
  publica apenas faixas modificadas; descarte não publica. Buffers são internos:
  &x preserva o endereço lógico original. A unidade de conflito é o objeto.
- Restringir E/S, efeitos externos não descartáveis, chamadas desconhecidas,
  criação/suspensão de tasks e sincronizações internas não suportadas. Não
  prometer rollback de efeitos externos ou execução exatamente uma vez.
- Decisão: versões internas transacionais com contexto/status explícitos,
  resultado só em sucesso e assinatura pública preservada. Falha segue aos
  epílogos até o controle externo; chamadas indiretas/externas sem preparação
  são restritas e pure/const não substitui preparação.
- Decisão: rejeitar inicialmente exceções e cleanups do usuário. A limpeza
  interna é obrigatória; tentativa condenada recusa read/write normais. Suporte
  futuro precisa de protocolo para acesso de destrutores aos dados condenados.
  Exceção nunca vira CONFLICT nem provoca commit durante desenrolamento.
- Geração/identificação/linkage/ABI das versões, diagnósticos dessas restrições
  e ABI de ponte de acesso ainda precisam de especificação e implementação.
  O finally atual de rastreamento não pode virar commit automático.

## Percurso técnico e ponto de instrumentação

```text
#pragma omp transaction (identificadores)
  -> registro e dispatcher do front-end
  -> resolução dos nomes e parsing do bloco C/C++
  -> OMP_TRANSACTION: corpo + declarações
  -> gimplificação: begin + register + corpo + finally(end)
  -> GIMPLE_OMP_TRANSACTION: marcador com corpo
  -> newtm_lower_transaction: lower_omp no contexto externo
  -> remoção do marcador e passes comuns
  -> programa chama a ponte GOMP_transaction_* de rastreamento
```

Esse é o percurso atual. A instrumentação funcional deverá introduzir operações
de acesso enquanto a identidade do objeto e o contexto estão disponíveis,
antes de eliminar o marcador em `newtm_lower_transaction`. Preparar chamadas
antes do lower_omp quando precisarem de seu remapeamento; disponibilizar metadados
antes da varredura se afetarem captura. A lista hoje não é operando do marcador
GIMPLE, portanto sua preservação/mapeamento é trabalho preparatório necessário.
Não reinstrumentar acessos de regiões internas nem temporários gerados.

Uma futura ponte exportada GOMP_* poderá obter o contexto da execução e chamar
as primitivas internas. Isso exige decisão de ABI; os símbolos newtm_read/write
não devem ser exportados apenas para suprir a instrumentação ou testes.

## Arquivos envolvidos e caracterização

### Registro, front-ends e opções

| Arquivo | Conteúdo relacionado ao projeto |
| --- | --- |
| [gcc/c-family/c-pragma.h](gcc/c-family/c-pragma.h) | Identificador PRAGMA_OMP_TRANSACTION dentro da enumeração de pragmas. |
| [gcc/c-family/c-pragma.cc](gcc/c-family/c-pragma.cc) | Registro de transaction no namespace omp habilitado por -fopenmp. |
| [gcc/c/c-parser.cc](gcc/c/c-parser.cc) | newtm_c_parser_transaction, lista simples, busca C, diagnósticos, corpo e despacho. |
| [gcc/cp/parser.cc](gcc/cp/parser.cc) | Parser/despacho C++, busca com acesso/ambiguidade e processamento de variáveis automáticas externas. |
| [gcc/c-family/c-common.h](gcc/c-family/c-common.h) | Declaração compartilhada de newtm_finish_transaction. |
| [gcc/c-family/c-omp.cc](gcc/c-family/c-omp.cc) | Finalizador que cria OMP_TRANSACTION e classificação compartilhada da diretiva. |
| [gcc/common.opt](gcc/common.opt) | Opção -ftransaction-debug e variável flag_transaction_debug. |
| [gcc/cp/pt.cc](gcc/cp/pt.cc) | Substituição de corpo e lista de objetos na instanciação de templates. |
| [gcc/cp/module.cc](gcc/cp/module.cc) | Classificação da árvore para infraestrutura de módulos C++. |
| [gcc/cp/constexpr.cc](gcc/cp/constexpr.cc) | Reconhecimento da construção nos caminhos de validade/avaliação constexpr. |

### Representações, gimplificação e lowering

| Arquivo | Conteúdo relacionado ao projeto |
| --- | --- |
| [gcc/tree.def](gcc/tree.def) | Código OMP_TRANSACTION com dois operandos: corpo e objetos resolvidos. |
| [gcc/tree.h](gcc/tree.h) | Acessores OMP_TRANSACTION_BODY e OMP_TRANSACTION_OBJECTS. |
| [gcc/tree-pretty-print.cc](gcc/tree-pretty-print.cc) | Impressão da construção GENERIC, incluindo lista e corpo. |
| [gcc/gimple.def](gcc/gimple.def) | Código GIMPLE_OMP_TRANSACTION com layout GSS_OMP. |
| [gcc/gimple.h](gcc/gimple.h) | Declaração do construtor e integração às classificações genéricas OpenMP. |
| [gcc/gimple.cc](gcc/gimple.cc) | newtm_build_transaction e tratamento do corpo na infraestrutura GIMPLE. |
| [gcc/gimplify.cc](gcc/gimplify.cc) | Conversão da árvore, validação de tamanho, geração de begin/register/end e TRY_FINALLY; referências C++ registram o referido. |
| [gcc/omp-low.cc](gcc/omp-low.cc) | Varredura, verificações de bloco estruturado e newtm_lower_transaction; ponto previsto de instrumentação futura. |
| [gcc/gimple-walk.cc](gcc/gimple-walk.cc) | Percurso do corpo pelos walkers. |
| [gcc/gimple-pretty-print.cc](gcc/gimple-pretty-print.cc) | Dumps legíveis/brutos do marcador. |
| [gcc/tree-inline.cc](gcc/tree-inline.cc) | Cópia do marcador/corpo e integração aos percursos de inlining. |
| [gcc/tree-nested.cc](gcc/tree-nested.cc) | Tratamento do corpo na transformação de funções aninhadas. |
| [gcc/gimple-low.cc](gcc/gimple-low.cc) | Invariantes do lowering geral; o marcador deve ter sido eliminado antes dessa fase. |
| [gcc/omp-builtins.def](gcc/omp-builtins.def) | Builtins atuais da ponte GOMP_transaction_begin/register/end; nenhum builtin de acesso read/write. |

### Runtime, primitivas e construção

| Arquivo | Conteúdo relacionado ao projeto |
| --- | --- |
| [libgomp/newtm-hash.h](libgomp/newtm-hash.h) | Tipos opacos, estados, callbacks e nove primitivas da hash; lease e três serviços internos de admissão/encerramento, sem exportação GOMP nova. |
| [libgomp/newtm-hash.c](libgomp/newtm-hash.c) | Nove primitivas operacionais, mistura, buckets/referências/metadados, três serviços owner e ponte GOMP com lease por contexto externo; logs fora dos locks. |
| [libgomp/newtm-transaction.h](libgomp/newtm-transaction.h) | Contexto opaco, enums de resultado de acesso/ciclo de vida, enum de estados da tentativa e oito declarações ocultas (read/write e seis operações de ciclo de vida), com contratos futuros. |
| [libgomp/newtm-transaction.c](libgomp/newtm-transaction.c) | Criação, aninhamento, condenação e limpeza; end interno funcional, end externo indisponível; register/read/write/backoff ainda stubs. |
| [libgomp/libgomp.h](libgomp/libgomp.h) | Campo newtm_transaction_context no estado da thread e infraestrutura interna utilizada. |
| [libgomp/team.c](libgomp/team.c) | Inicialização do ponteiro de contexto no ciclo de vida da thread. |
| [libgomp/libgomp_g.h](libgomp/libgomp_g.h) | Protótipos da ponte GOMP_transaction_* atual. |
| [libgomp/libgomp.map](libgomp/libgomp.map) | Exportação versionada da ponte; as primitivas newtm_* permanecem internas. |
| [libgomp/Makefile.am](libgomp/Makefile.am) | Inclusão dos fontes/cabeçalhos hash e transaction na construção/distribuição. |
| [libgomp/Makefile.in](libgomp/Makefile.in) | Regras geradas correspondentes, objetos e dependências; manter coerência com Makefile.am. |

### Infraestrutura reutilizada e pontos de auditoria

Estes arquivos dão suporte ou servem de referência; sua presença nesta lista não
significa que tenham recebido implementação específica de transaction:

| Arquivo ou conjunto | Papel na evolução |
| --- | --- |
| [libcpp/directives.cc](libcpp/directives.cc) | Pragmas adiados e entrega dos tokens aos front-ends. |
| [gcc/passes.def](gcc/passes.def) | Ordem dos passes e posição das verificações/lowering OpenMP. |
| [gcc/omp-expand.cc](gcc/omp-expand.cc) | Expansão OpenMP posterior; hoje não recebe o marcador transaction, removido em omplower. |
| [gcc/builtin-types.def](gcc/builtin-types.def) | Tipos das assinaturas de builtins; auditar quando a futura ponte de acesso for definida. |
| [gcc/tree-cfg.cc](gcc/tree-cfg.cc) | Verificação/infraestrutura que também consome classificações genéricas GIMPLE. |
| [libgomp/critical.c](libgomp/critical.c) | Referência de uso de mutexes e inicialização por target; não é implementação STM. |
| libgomp/config/*/mutex.h e mutex.c | Mutexes portáveis, ordenação de memória e inicialização conforme target. |
| gcc/tree-streamer-* e gcc/gimple-streamer-* | Auditoria de serialização se novos dados sobreviverem até LTO; o marcador atual é eliminado antes da serialização normal. |

### Documentação, testes e exemplos

| Arquivo ou conjunto | Conteúdo e cobertura |
| --- | --- |
| [gcc/doc/extend.texi](gcc/doc/extend.texi) | Sintaxe da extensão, efeito de rastreamento e limitações atuais. |
| [gcc/doc/invoke.texi](gcc/doc/invoke.texi) | Opção -ftransaction-debug e sua distinção dos logs do runtime. |
| gcc/testsuite/c-c++-common/gomp/transaction-1.c a transaction-10.c | Sintaxe, execução comum, representações/dumps, contextos, saltos, aninhamento, macros e erros de listas. |
| gcc/testsuite/c-c++-common/gomp/transaction-log-*.c | Chamadas geradas e rejeição de entidades que não são variáveis/parâmetros. |
| [gcc/testsuite/c-c++-common/gomp/transaction-lookup-1.c](gcc/testsuite/c-c++-common/gomp/transaction-lookup-1.c) | Escopo, ocultação, nomes ausentes e recuperação. |
| [gcc/testsuite/gcc.dg/gomp/transaction-1.c](gcc/testsuite/gcc.dg/gomp/transaction-1.c) | Função aninhada C e referências externas. |
| [gcc/testsuite/g++.dg/gomp/transaction-1.C](gcc/testsuite/g++.dg/gomp/transaction-1.C) | Templates e vida útil de objeto local no corpo comum. |
| gcc/testsuite/g++.dg/gomp/transaction-log-*.C | Referências/templates e captura em lambda. |
| [gcc/testsuite/g++.dg/gomp/transaction-lookup-1.C](gcc/testsuite/g++.dg/gomp/transaction-lookup-1.C) | Acesso privado e ambiguidade C++. |
| [libgomp/testsuite/libgomp.c-c++-common/transaction-log-1.c](libgomp/testsuite/libgomp.c-c++-common/transaction-log-1.c) | Execução da ponte, duplicatas e liberação somente na saída externa. |
| [transaction_test/transaction_exemplo.c](../transaction_test/transaction_exemplo.c) | Máximo paralelo com referência sequencial e empate; usa critical externo para sincronização real. |
| [transaction_test/transaction_aninhamento.c](../transaction_test/transaction_aninhamento.c) | Aninhamento lexical/por função, lista interna e duplicatas, sem concorrência. |
| [transaction_test/verificar.py](../transaction_test/verificar.py) | Asserções sobre resultados, contagens e ordem dos logs da ponte. |
| [transaction_test/lifecycle_stubs.c](../transaction_test/lifecycle_stubs.c) | Contratos dos seis stubs de ciclo de vida, com slots, canários e limites. |
| [transaction_test/verificar_lifecycle.py](../transaction_test/verificar_lifecycle.py) | Logs e símbolos internos, ligado ao archive do build pelo alvo check-lifecycle. |
| [transaction_test/hash_create_destroy.c](../transaction_test/hash_create_destroy.c) | Estrutura, mutexes e preservação em BUSY. |
| [transaction_test/hash_acquire.c](../transaction_test/hash_acquire.c) | Inserção/consulta, referências, overflow, colisões e concorrência. |
| [transaction_test/hash_entry.c](../transaction_test/hash_entry.c) | Consultas imutáveis, callbacks, substituição de metadados e exclusão por bucket. |
| [transaction_test/hash_count.c](../transaction_test/hash_count.c) | Contagem distinta de referências, saturação e zero com destrutor ainda pendente. |
| [transaction_test/hash_diagnostics.c](../transaction_test/hash_diagnostics.c) e [verificador](../transaction_test/verificar_hash_diagnostics.py) | Quatro processos de regressão para inversão de locks com stdout/stderr e OOM injetado. |
| [transaction_test/hash_owner.c](../transaction_test/hash_owner.c) e [verificador](../transaction_test/verificar_hash_owner.py) | Onze cenários de admissão/encerramento, coordenador único, callbacks e logs pendentes, chamadas tardias e saída com contexto ativo. |
| [transaction_test/transaction_carga.c](../transaction_test/transaction_carga.c) | THREADS W:R REPETICOES; uma operação lógica por transação e repetição por thread; critical interno enquanto não há isolamento. |
| [transaction_test/verificar_carga.py](../transaction_test/verificar_carga.py) | 1:9/1:1/9:1, contagens/resultados e uma acquire/release por transação, sem depender de assert Python. |
| [transaction_test/Makefile](../transaction_test/Makefile) | Build local; check inclui check-carga; alvos privados hash, diagnósticos, owner e lifecycle separados. |
| [transaction_test/README.md](../transaction_test/README.md) | Comandos, cobertura e limites dos exemplos. |
| [transaction_test/.gitignore](../transaction_test/.gitignore) | Exclusão dos executáveis/logs locais de .build. |

transaction_test/a.out é artefato antigo, não fonte nem evidência de execução da
versão atual. Os agentes listados no início também integram a documentação do
projeto. Arquivos /tmp referidos em relatórios são evidências locais de tarefas,
não testes permanentes ou dependências da distribuição.

## Estado atual

### Implementado e verificável nos fontes

- Reconhecimento em C/C++ com -fopenmp de lista opcional não vazia de
  identificadores simples e corpo estruturado, sem cláusulas. A forma sem lista
  é aceita. -fopenmp-simd isoladamente não registra esta extensão.
- Resolução antes do corpo; aceitação de VAR_DECL/PARM_DECL; diagnósticos para
  ausência/categoria inválida e busca C++ com acesso/ambiguidade. Isso não equivale
  à validação completa do subconjunto de tipos de uma STM funcional.
- Lista preservada na árvore GENERIC e substituída em templates. Na gimplificação,
  exige-se tipo completo de tamanho constante e emitem-se endereços/tamanhos para
  registro. Arrays não são automaticamente tratados como ponteiros listados.
- Marcador GIMPLE próprio, consumidores auxiliares e remoção em omplower,
  preservando ambiente de dados e restrições aplicáveis da região OpenMP externa.
- Chamadas GOMP_transaction_begin/register/end e ponte de rastreamento com
  profundidade por thread, deduplicação por endereço e retenção até a saída externa.
- Inicialização global de rastreamento e chamadas create/acquire/release/destroy.
  A ponte usa tabela e entradas reais, verifica os retornos antes de publicar
  estado ou registros. O proprietário agora controla OPEN/CLOSING/CLOSED e
  participações: cada contexto externo retém uma lease até concluir a limpeza.
  Fechamento recusa novos usuários e só destrói depois da drenagem, com um
  coordenador. O finalizador faz tentativa sem esperar; BUSY conserva recursos
  para o processo, sem fatal ou remoção forçada. Controle/mutex sobrevivem à tabela.
  A inicialização usa helper sem E/S sob lock global e emite create depois de
  unlock. Falhas de alocação são fatais somente após liberar o lock pertinente.
- Nove primitivas hash declaradas e operacionais.
  create/destroy gerenciam tabela/mutexes; acquire/lookup_acquire usam fmix32/64,
  busca sob mutex e referências, com validação de tamanho/overflow; release
  retira na última referência e destrói metadados fora do lock. Address/size
  consultam campos imutáveis sem lock com referência retida; visit executa
  callback sob mutex do bucket sem consumir a referência. Count adquire todos
  os mutexes em ordem crescente, soma entradas com saturação e desbloqueia em
  ordem inversa; zero não comprova quiescência. Printf/fflush fora dos mutexes.
- Esqueletos newtm_read/write internos, incluídos no build da libgomp. Imprimem os
  cinco parâmetros, fazem fflush(stdout) e retornam NEWTM_ACCESS_INVALID. Não
  desreferenciam parâmetros, modificam contexto/buffers ou validam acessos.
- Seis esqueletos internos de ciclo de vida: begin/register/end/discard e os
  helpers fail/backoff, no mesmo fonte/cabeçalho de read/write. Retornam
  NEWTM_LIFECYCLE_INVALID; apenas begin inicializa outermost=false. Slots e dados
  permanecem preservados. Saída interna/commit são fases futuras de end.
- Contrato futuro de falha: end preserva recursos; controle externo descarta
  explicitamente depois dos cleanups, com causa salva antes da liberação.
- -ftransaction-debug controla mensagens do compilador em stderr, desativadas por
  padrão. Os printf do runtime continuam em stdout e não são controlados por ela.

### Ainda não implementado ou não consolidado

Há criação/destruição, aquisição/consulta com retenção e liberação de entradas,
consultas address/size, visita sincronizada de metadados e contagem consistente
saturada. Não há protocolo real de reservas, visão privada dos
dados, instrumentação automática de leitura/escrita, commit, abort funcional,
rollback ou reinício de tentativas. O compilador não gera acessos STM. A ponte usa begin/end interno/discard, mas
não chama newtm_read/write ou registro STM. Não existe ponte exportada de acesso nem sua ABI final.

O contexto privado já implementa criação e limpeza; a ponte de rastreamento em
newtm-hash.c o utiliza sem armazenar valores especulativos. O corpo continua lendo/escrevendo memória
comum. Portanto transaction ainda não protege atualizações concorrentes.

O TRY_FINALLY atual equilibra encerramento/limpeza de rastreamento, inclusive em
saídas excepcionais que o alcancem; não confirma nem desfaz valores do programa.
A matriz futura de restrições de tipos, efeitos externos, aliases, tasks e
exceções ainda não é integralmente validada pelo código atual.

### Validação registrada e seus limites

A etapa de hash/owner executou build PIC/não PIC e testes privados, regressões,
ASan/UBSan e ThreadSanitizer conforme o relatório de implementação. Na etapa da
carga, make check passou com -O2 -Wall -Wextra -Werror e 4 threads/1000 repetições:
1:9 produziu 400 escritas/3600 leituras; 1:1, 2000/2000; 9:1, 3600/400. Passaram
também contagens não múltiplas do período, relações 0:1/1:0, limites de pesos,
19 entradas inválidas e detecção de equipe limitada pelo ambiente. Esses testes
usam critical e não validam isolamento STM. Os detalhes e comandos estão em
[IMPLEMENTATION](OPENMP_TRANSACTION_IMPLEMENTATION.md) e
[README dos testes](../transaction_test/README.md).

Os parágrafos seguintes registram etapas anteriores, com seus próprios limites:

O relatório de implementação registra builds e subconjuntos DejaGNU anteriores,
além de testes diretos. A etapa dos esqueletos read/write reconstruiu libgomp com
-Wall -Werror, verificou impressão de parâmetros, INVALID, preservação de buffers
e visibilidade interna. Os exemplos locais passaram para a etapa de rastreamento.

A etapa de ciclo de vida reconstruiu a libgomp com -Wall -Werror, executou
22 chamadas de contrato pelo alvo check-lifecycle (incluindo visibilidade) e
recompilou/executou os exemplos. Detalhes no relatório de implementação.
Esses resultados não validam isolamento/STM; DejaGNU não foi executado nessa
etapa. Reproduzir os testes pertinentes na próxima alteração.

Na etapa seguinte, a definição da máquina acrescentou o enum de cinco estados
e o protocolo de consumo único em ABORT, sem implementar contexto ou transições.
A libgomp foi reconstruída; check-lifecycle passou com 26 chamadas, incluindo
descarte repetido e saída posterior sem sucesso fictício. Os exemplos passaram
com 1/2/4 threads e aninhamento; não houve nova execução DejaGNU nessa etapa.

## Orientação para as próximas etapas

### Entregas concluídas, não repetir como pendência

A hash estrutural (nove primitivas), integração da ponte com referências reais,
correção dos dois caminhos de diagnóstico e admissão/participações do proprietário
já estão implementadas. OPEN/CLOSING/CLOSED governa a vida útil da tabela, não a
máquina de estados da tentativa STM. Count zero continua sem provar quiescência.
A carga parametrizada e seus três cenários também já integram make check.

### Trabalho funcional ainda pendente e dependências

A sequência abaixo organiza dependências para a próxima tarefa autorizada;
não declara funcionalidades entregues nem cria uma nova ABI por si só.

| Ordem | Tarefa | Dependências e critério de integração |
| --- | --- | --- |
| 1 | Integrar registro/deduplicação à base de contexto já implementada; detalhar a ABI das versões geradas. | Leituras externas, tipos, identidade, chamadas com contexto/status e rejeição inicial de exceções/cleanups já foram decididos. Classes e suporte a cleanups do usuário ficam para futuro. |
| 2 | Estender a base funcional do contexto com registro STM e metadados sob demanda. | Distinguir lease do proprietário, referência física, participação semântica, reserva e buffer privado. Preservar a lease enquanto qualquer tentativa/cleanup depender da tabela; definir transições/falhas e o destrutor de política antes da publicação (o owner já usa newtm_policy_destroy) antes de ligar os stubs. |
| 3 | Implementar metadados da política, reservas e operações read/write com escalares simples, faixas válidas/modificadas para agregados e limites de subacessos. | Depende de contexto e propagação segura. Usar entry_visit com callback curto e sem reentrada/espera; não conservar mutex entre acessos. Não tratar INVALID dos stubs como sucesso ou conflito. |
| 4 | Preservar identidade e instrumentar cada acesso C/C++ no ponto adequado do pipeline. | Especificar ABI e gerar versões internas com contexto/status explícitos, resultado só em OK e epílogos encadeados, preservando assinatura pública; aplicar filtros recursivos, rejeição de ponteiros desconhecidos e registros distintos sobrepostos, e limites dinâmicos. Exigir registro explícito para todos os acessos preexistentes, sem fallback. Preservar private/reduction, referências, templates e avaliação única; GIMPLE atual guarda corpo. |
| 5 | Completar saída externa, publicação, descarte, propagação da falha, espera e retry. | Integração coordenada com etapas anteriores: end falho preserva recursos, discard explícito após cleanups e retry somente de conflito externo. Publicar somente faixas modificadas; liberar a representação privada no descarte. Não retornar OK de commit/descarte sem efeitos concluídos. |
| 6 | Validar STM funcional no subconjunto admitido. | Testes de conflitos, valores privados, publicação/aborto, aninhamento, progresso e decisões de tipos/identidade. Reavaliar o critical da carga somente depois do isolamento implementado; logs atuais não provam STM. |
| Futuro | Suporte transacional a instâncias de classes. | Definir critérios de linguagem, construção/cópia/destruição, vida útil e exceções antes de ampliar o conjunto; não admitir por mera trivialidade ou sizeof conhecido. |
| Futuro | Exceções e cleanups do usuário. | Definir visão/acesso de destrutores com tentativa condenada, ordem e falhas de cleanup, vida útil e propagação da exceção; não usar read/write normais, converter em CONFLICT ou confirmar durante desenrolamento. |

As etapas têm dependências mútuas: não ativar acessos ou publicação parcialmente
sem um caminho seguro de falha e limpeza. Atualizar algoritmos em comentário,
contratos especializados, inventário e documentação quando o comportamento mudar.

### Validação e otimizações ainda abertas

- Reproduzir os subconjuntos DejaGNU com a libgomp correta quando o ambiente
  tiver as ferramentas; os 383 PASS de 2026-10-21 não validam o código posterior.
- Executar nativamente 32 bits e targets/backends alternativos antes de anunciar
  cobertura que não foi obtida no Linux x86_64 atual.
- Medir contenção e separar custos dos printf e do critical da carga antes de
  otimizar alocação sob lock ou distribuição. Manter os logs exigidos pelo usuário.
- Não prometer unload seguro: dlclose requer exclusão externa de código atual e
  futuro. O finalizador já evita esperar/forçar destruição com leases ativas.

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

## Verificação geral de 2026-10-21

A [auditoria](OPENMP_TRANSACTION_AUDIT.md) reconstruiu incrementalmente GCC e
libgomp e obteve 383 PASS nos subconjuntos transaction/critical de C, C++ e
libgomp, sem FAIL/UNRESOLVED/UNSUPPORTED finais. Passaram também exemplos,
esqueletos e verificações diretas de LTO, módulos, exceções e identidade de dados.
Foi corrigido um falso positivo do verificador lifecycle sob Python -O/-OO.
A auditoria confirmou a etapa de rastreamento/esqueletos; não valida STM funcional
ou a suíte completa. Instruções de runtime local e evidências constam no relatório.
