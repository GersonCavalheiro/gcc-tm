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

Os caminhos são relativos a `/usr/src/gcc-omp-transaction`. O build existente está em
`/usr/src/gcc-build`, separado dos fontes. Localizar funções pelo nome, pois
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
| [OPENMP_TRANSACTION_HASH_IMPLEMENTATION_AGENT.md](OPENMP_TRANSACTION_HASH_IMPLEMENTATION_AGENT.md) | Etapa de hash operacional posterior aos esqueletos: adaptação da ponte, testes e critérios de entrega; não altera o estado implementado. |
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

INVALID é falha permanente no contrato funcional e não autoriza retry infinito.
Nos stubs atuais tem outro significado explicitamente temporário: ausência de
implementação, sem validação ou alteração de estado.

Backoff progressivo tem teto por espera, com tentativas sem limite conforme a
política prevista. Não promete justiça, ausência de starvation ou conclusão.
Não usar setjmp/longjmp no retry normal nem atravessar destrutores por saltos
arbitrários. Reconstruir temporários privados de cada tentativa.

### Limites e decisões ainda pendentes

- Acessos externos não instrumentados ficam fora do protocolo; não há isolamento
  forte em relação a eles nem alteração das regras C/C++ de corrida de dados.
- Escritas em objetos cuja vida começou antes da transação exigem registro
  prévio. A forma sem lista não autoriza incluir implicitamente todos os objetos.
- A política para leituras externas não listadas continua pendente. Não assumir
  consistência dessas leituras ou registro automático.
- O subconjunto previsto começa por escalares e agregados admitidos de tamanho
  conhecido; exige decisões verificáveis para tipos C++ e casos especiais.
  VLA, volatile, atômicos e operações não triviais não têm suporte implícito.
- Restringir E/S, efeitos externos não descartáveis, chamadas desconhecidas,
  criação/suspensão de tasks e sincronizações internas não suportadas. Não
  prometer rollback de efeitos externos ou execução exatamente uma vez.
- Propagação segura de falha entre funções, identificação de versões preparadas,
  exceções, acessos por cleanups e ABI de ponte de acesso precisam ser definidos.
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
| [libgomp/newtm-hash.h](libgomp/newtm-hash.h) | Tipos opacos, enum de estados, capacidade padrão, callbacks e nove primitivas internas da hash. |
| [libgomp/newtm-hash.c](libgomp/newtm-hash.c) | Estruturas de tabela/bucket/entrada e corpos de depuração; também hospeda provisoriamente a ponte GOMP_transaction_* e seu contexto de rastreamento. |
| [libgomp/newtm-transaction.h](libgomp/newtm-transaction.h) | Contexto opaco, enums de resultado de acesso/ciclo de vida, enum de estados da tentativa e oito declarações ocultas (read/write e seis operações de ciclo de vida), com contratos futuros. |
| [libgomp/newtm-transaction.c](libgomp/newtm-transaction.c) | Oito esqueletos que imprimem parâmetros, fazem fflush e retornam INVALID; begin também inicializa outermost=false. |
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
| [transaction_test/transaction_exemplo.c](transaction_test/transaction_exemplo.c) | Máximo paralelo com referência sequencial e empate; usa critical externo para sincronização real. |
| [transaction_test/transaction_aninhamento.c](transaction_test/transaction_aninhamento.c) | Aninhamento lexical/por função, lista interna e duplicatas, sem concorrência. |
| [transaction_test/verificar.py](transaction_test/verificar.py) | Asserções sobre resultados, contagens e ordem dos logs da ponte. |
| [transaction_test/lifecycle_stubs.c](transaction_test/lifecycle_stubs.c) | Contratos dos seis stubs de ciclo de vida, com slots, canários e limites. |
| [transaction_test/verificar_lifecycle.py](transaction_test/verificar_lifecycle.py) | Logs e símbolos internos, ligado ao archive do build pelo alvo check-lifecycle. |
| [transaction_test/Makefile](transaction_test/Makefile) | Compilação com GCC e libgomp do build local; alvo check. |
| [transaction_test/README.md](transaction_test/README.md) | Comandos, cobertura e limites dos exemplos. |
| [transaction_test/.gitignore](transaction_test/.gitignore) | Exclusão dos executáveis/logs locais de .build. |

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
  Os handles continuam nulos porque as operações hash ainda são stubs; a ponte
  ignora suas falhas deliberadamente apenas nesta etapa.
- Nove primitivas hash definidas como esqueletos. Funções de estado imprimem
  argumentos e retornam INVALID; saídas ficam nulas e consultas retornam null/zero.
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

Não há tabela hash operacional, protocolo real de reservas, visão privada dos
dados, instrumentação automática de leitura/escrita, commit, abort funcional,
rollback ou reinício de tentativas. O compilador e a ponte de rastreamento não
chamam newtm_read/write nem os seis stubs de ciclo de vida. Não existe ponte exportada de acesso nem sua ABI final.

O contexto opaco das primitivas de acesso ainda não é uma implementação do
contexto funcional. O contexto de rastreamento em newtm-hash.c é provisório e
não armazena valores especulativos. O corpo continua lendo/escrevendo memória
comum. Portanto transaction ainda não protege atualizações concorrentes.

O TRY_FINALLY atual equilibra encerramento/limpeza de rastreamento, inclusive em
saídas excepcionais que o alcancem; não confirma nem desfaz valores do programa.
A matriz futura de restrições de tipos, efeitos externos, aliases, tasks e
exceções ainda não é integralmente validada pelo código atual.

### Validação registrada e seus limites

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
A libgomp foi reconstruída; check-lifecycle passou com 24 chamadas, incluindo
descarte repetido e saída posterior sem sucesso fictício. Os exemplos passaram
com 1/2/4 threads e aninhamento; não houve nova execução DejaGNU nessa etapa.

## Orientação para as próximas etapas

Antes de passar de esqueletos a operações reais, delimitar a tarefa autorizada
e suas dependências. Manter o contrato explícito de erro e atualizar os chamadores
quando a hash começar a produzir entradas válidas; não carregar para o runtime
funcional a aceitação provisória de handles nulos.

A integração de acessos exige preservar identidade, instrumentar exatamente uma
vez e manter semântica de avaliação C/C++. A integração de abort exige caminho
seguro de falha entre funções e cleanups. A entrega funcional depende também de
publicação/descarte e testes determinísticos de concorrência, não apenas de
printfs ou de uma execução que produza o resultado esperado.

Atualizar este panorama, os contratos especializados e a documentação do usuário
quando o comportamento mudar. Conservar a distinção entre intenção, contrato
proposto, implementação efetiva e evidência de validação.

## Inventário consolidado e sequência de trabalho

Consultar [OPENMP_TRANSACTION_PRIMITIVES.md](OPENMP_TRANSACTION_PRIMITIVES.md)
para assinaturas existentes, propriedade dos recursos, estados, efeitos atuais
e futuros e pontos de chamada. Os contratos especializados deste agente
permanecem detalhados aqui; mudanças de interface devem atualizar também o
inventário. Os nomes stm_* continuam conceituais, sem assinatura C ou ABI definida.

A sequência solicitada prioriza contratos e esqueletos antes das operações reais.
Os seis esqueletos internos de ciclo de vida já foram acrescentados; contratos,
classificação e evidências estão no inventário e no relatório de implementação.
A hash operacional e o ciclo de vida funcional continuam etapas posteriores.
Fontes atuais: `/usr/src/gcc-omp-transaction`; referências a
`/usr/src/gcc-16.2.0` neste documento correspondem ao caminho antigo.

## Verificação geral de 2026-10-21

A [auditoria](OPENMP_TRANSACTION_AUDIT.md) reconstruiu incrementalmente GCC e
libgomp e obteve 383 PASS nos subconjuntos transaction/critical de C, C++ e
libgomp, sem FAIL/UNRESOLVED/UNSUPPORTED finais. Passaram também exemplos,
esqueletos e verificações diretas de LTO, módulos, exceções e identidade de dados.
Foi corrigido um falso positivo do verificador lifecycle sob Python -O/-OO.
A auditoria confirmou a etapa de rastreamento/esqueletos; não valida STM funcional
ou a suíte completa. Instruções de runtime local e evidências constam no relatório.
