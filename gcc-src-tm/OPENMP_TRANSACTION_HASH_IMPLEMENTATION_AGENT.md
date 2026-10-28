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

# Agente de implementação e manutenção da hash de transaction

## Estado entregue — 2026-10-28

As nove primitivas de tabela estão operacionais: create/destroy,
acquire/lookup_acquire/release, entry_address/entry_size/entry_visit e count.
Há encadeamento de colisões, mutex por bucket, referências e metadados opacos.
A ponte usa handles reais e verifica falhas; o proprietário admite leases e
encerra por OPEN -> CLOSING -> CLOSED, com um coordenador e sem esperar usuários
no finalizador. A participação cobre todo o contexto, callbacks, limpeza e log
inicial. Não reimplementar essas entregas como se ainda fossem esqueletos.

Mistura fmix32/fmix64 por UINTPTR_MAX, módulo para capacidade arbitrária,
remoção na última referência, destrutor fora do lock e soma saturada já foram
adotados. Printf/fflush permanecem obrigatórios fora dos mutexes de buckets e
do proprietário. OOM é fatal depois de unlock; calloc parcial é desfeito pelo
helper sem diagnóstico. Consulte os algoritmos revisados nos cabeçalhos das
funções em libgomp/newtm-hash.c para o fluxo executado, distinguindo pré-condições
assumidas das validações efetivamente presentes.

A carga transaction_carga recebe threads, W:R e repetições por thread; make check
inclui os cenários 1:9, 1:1 e 9:1. Critical interno protege os valores enquanto
a STM não oferece isolamento; a carga testa registros/vida útil, não desempenho
ou correção de STM. Read/write e ciclo de vida da tentativa continuam esqueletos.

## Missão e autorização

Orientar manutenção da estrutura já implementada, revisão dos seus invariantes
e evolução expressamente solicitada. Preservar a separação entre hash, proprietário
da tabela e futura política transacional. Este guia não autoriza implementar
funcionalidades futuras de STM nem criar ABI pública por consequência.

Referências normativas do projeto:

- [Guia geral](OPENMP_AGENT.md).
- [Panorama e estado atual](OPENMP_TRANSACTION_PROJECT_AGENT.md).
- [Contratos da hash e da ponte](OPENMP_TRANSACTION_HASH_AGENT.md).
- [Inventário e histórico](OPENMP_TRANSACTION_IMPLEMENTATION.md).
- [Identidade e tipos](OPENMP_TRANSACTION_SCOPE_AGENT.md).
- [Acessos futuros](OPENMP_TRANSACTION_ACCESS_AGENT.md).
- [Abort e reinício futuros](OPENMP_TRANSACTION_ABORT.md).

Conferir os fontes antes de modificar. Fontes em gcc-src-tm/, build separado
em gcc-build/ e testes em transaction_test/, relativos à raiz do repositório
(/usr/src/gcc-tm no contêiner). Resultados datados são evidência das execuções
registradas, não substituem os testes pertinentes a uma nova alteração.

## Resultado entregue e limites

A tabela armazena entradas reais e compartilha a mesma geração entre
referências simultâneas à chave. Estrutura, referências e callbacks usam mutexes
por bucket; a ponte retém referências válidas até a saída externa, com flat
nesting. Preservar essas garantias e a drenagem das participações na manutenção.

Registro não concede reserva de leitura/escrita e não acessa o valor do objeto.
O corpo da diretiva continua acessando memória comum. Permanecem futuros:
instrumentação de acessos, reservas, cópias privadas, commit, abort, rollback,
retry e backoff. newtm_read/write e os seis serviços de ciclo de vida
permanecem esqueletos internos. Não ligá-los à ponte como operações funcionais. Não ampliar
gramática, tipos, tasking, suporte a dispositivos ou ABI pública nesta etapa.

## Decisões adotadas na implementação

A entrada é removida na última referência física, não no término do último
participante semântico. Metadados pertencem à geração e são destruídos fora do
lock. Acquire aloca sob mutex; antecipação da alocação permanece otimização
futura condicionada a medições, nova busca e descarte de candidatos. O proprietário
da tabela global controla admissão e drenagem; tabelas independentes continuam
sob quiescência externa. Essas decisões descrevem o código entregue, não propostas
pendentes. Mudanças devem explicitar impacto nos contratos e na validação.

## Decisões semânticas posteriores à entrega estrutural

O usuário exige agora registro explícito para todos os acessos preexistentes;
escalares/arrays/agregados restritos com filtros recursivos, excluindo volatile,
atômicos, VLA, unions, bit-fields, membros flexíveis e casos sem contrato; e
proibição de escritas const. Instâncias de classes, inclusive struct C++, ficam
para trabalho futuro. Também definiu identidade canônica demonstrável, índices
dinâmicos com limites em execução, rejeição de ponteiros desconhecidos sem
fallback e proibição de registros distintos sobrepostos. Campos/elementos usam
entrada/offset do agregado; ponteiro listado não registra a memória apontada.

O usuário também definiu versões transacionais internas com contexto e status
explícitos, resultado preenchido só em sucesso e assinatura pública preservada.
Falha segue aos epílogos pela cadeia preparada, sem valor fictício. Chamadas
indiretas/externas sem preparação são restritas; pure/const não prova acesso à
visão privada. A arquitetura foi escolhida, mas geração/linkage/ABI
continuam por implementar; não adicionar funções públicas da hash por consequência.
Exceções/cleanups do usuário foram rejeitados inicialmente, com suporte futuro
condicionado a protocolo de acesso dos destrutores em tentativa condenada.
A limpeza interna (inclusive destrutores de metadados/leases) permanece
obrigatória. Não converter exceção em CONFLICT, confirmar no desenrolamento ou
relaxar read/write condenados para permitir cleanup de usuário.

A representação privada também foi definida: escalares simples; agregados com
faixas válidas e modificadas, sem inicialização/acesso integral obrigatório para
operar sobre campos. Modificadas são subconjunto das válidas; commit publica
somente modificadas e descarte não publica. Buffers não escapam como endereços:
`&x` preserva o endereço lógico original. Isso não muda a unidade de conflito do
objeto nem a hash. Foram escolhidas listas de registros por tentativa e listas
de intervalos sobre buffer completo sob demanda; campos e implementação pendem.

São contratos de integração do compilador/contexto, não novas funções da hash
ou validações já executadas por ela. Não adicionar resolução dinâmica de
intervalos por consequência desta decisão nem tratar ausência no runtime como
leitura comum automática. Aplique os filtros no futuro caminho funcional e
preserve as nove primitivas genéricas. Detalhes e exemplo a += b no agente hash.

## Estrutura e invariantes

- Uma tabela global por instância da libgomp na ponte; capacidade inicial 1.024.
- Criação da primitiva aceita qualquer capacidade positiva representável.
- Colisões por encadeamento, capacidade imutável e sem redimensionamento.
- Chave por igualdade exata do endereço; tamanho positivo imutável por entrada.
- Converter endereço para uintptr_t e usar fmix32/fmix64 do MurmurHash3,
  selecionando por UINTPTR_MAX igual a UINT32_MAX/UINT64_MAX. Usar todos os
  bits, operações sem sinal na largura correta e módulo bucket_count sem
  truncamento prévio ou máscara; não exigir potência de dois. acquire e
  lookup usam a mesma função; release/visit usam o índice imutável da entrada.
  Outras larguras exigem diagnóstico na compilação da implementação operacional,
  até extensão explícita. Algoritmos/constantes e evidências exploratórias em
  [Decisão de mistura](OPENMP_TRANSACTION_HASH_AGENT.md#decisão-de-mistura-de-endereços).
- Uma referência física por aquisição bem-sucedida. Referência não é reserva
  transacional nem prolonga a vida do objeto do programa.
- Bucket protege lista, contadores, referências e acesso aos metadados.
- Nenhum mutex permanece adquirido após retorno normal. Nenhum mutex de bucket
  envolve o corpo da diretiva ou espera por outra transação.
- Apenas a contagem diagnóstica adquire todos os buckets, em ordem crescente,
  liberando-os em ordem inversa. Não há contador global com mutex por aquisição.
- Destruição exige impedir novas operações e concluir operações, visitas e
  destruidores em andamento, mantendo a exclusão até retornar. Referências
  ociosas podem permanecer: suas entradas causam BUSY, sem consumo ou alteração.
  count zero não impede novas operações, não exclui usuários da tabela nem
  destruidores de entradas já retiradas; não autoriza destroy.

## Primitivas implementadas e contratos de manutenção

Preservar assinaturas internas e os contratos completos do agente da hash.

| Primitiva | Resultado exigido |
| --- | --- |
| newtm_hash_create | Inicializa tabela e todos os mutexes antes de publicar *out; rejeita zero/nulos e overflow. |
| newtm_hash_destroy | Libera tabela vazia; BUSY preserva integralmente tabela não vazia. |
| newtm_hash_acquire | Insere ou retém a entrada existente sob lock; incompatibilidade de tamanho não altera referências. |
| newtm_hash_lookup_acquire | Retém uma entrada existente sem inserção; ausência retorna NOT_FOUND. |
| newtm_hash_release | Consome uma referência adquirida com sucesso; na última, desliga sob lock e conclui destrutor/free fora dele antes de retornar. Decremento positivo não garante sobrevivência após unlock sem outra referência independente. Tabela viva até retorno completo; count zero não basta para destroy. |
| newtm_hash_entry_address | Consulta chave imutável com referência retida. |
| newtm_hash_entry_size | Consulta tamanho imutável com referência retida. |
| newtm_hash_entry_visit | Executa callback curto uma vez sob lock, preservando a referência. |
| newtm_hash_count | Obtém fotografia consistente do número de entradas, saturada em SIZE_MAX. |

Em acquire, alocar a entrada ausente sob o mutex do bucket na implementação
inicial. Depois de a tabela estar operacional e validada, avaliar por medições
se compensa antecipar a alocação. Essa possível otimização exige nova busca sob
lock e descarte de candidatos não utilizados, preservando unicidade, tamanho,
overflow, referências e falhas sem alterações parciais. Não impor alocação
supérflua no caminho de aquisição de entrada existente. A otimização não é
requisito desta etapa; detalhes no
[contrato de alocação](OPENMP_TRANSACTION_HASH_AGENT.md#alocação-inicial-e-possível-otimização-de-acquire).

Nas operações com estado, falhas retornáveis usam NEWTM_HASH_INVALID,
NOT_FOUND, SIZE_MISMATCH, BUSY ou OVERFLOW conforme o contrato. Saída out válida
fica NULL na falha; não deixar inserções ou incrementos parciais. Verificar
overflow de alocações, referências e contadores antes da alteração.
Falta de memória continua fatal via gomp_fatal, mas somente após liberar os
locks. A tabela usa helper interno sem E/S baseado em malloc/calloc; acquire
usa malloc sob o bucket e desbloqueia antes do diagnóstico de falha. Não criar
estado recuperável fictício na API. Violações com ponteiros pendentes não têm
garantia de diagnóstico seguro.

policy_data pertence à entrada após instalação. visit deve retornar
normalmente, sem exceção/longjmp, e não pode reentrar na tabela, esperar por
outra transação, executar código do programa, inverter ordem de locks, consumir
a referência ou liberar o mutex. O endereço do slot não pode escapar; ponteiros
copiados não conservam proteção após retorno e exigem protocolo explícito de
vida útil/sincronização para uso posterior. Não há rollback do callback. Ao substituir
metadados, responde pelo destino dos anteriores. destroy_policy é obrigatório
quando houver metadados não nulos; executa exatamente uma vez por geração
retirada, sem lock de bucket, sem reentrada e sem alterar o objeto do programa.
Uma nova geração com a mesma chave pode existir durante essa destruição.
O callback retorna normalmente, sem exceção/longjmp; release exige tabela viva
até concluir callback e free. Retirar somente participações/reservas sustentadas
pela referência consumida; nenhuma pode sobreviver à última referência que a
sustenta. Cada sucesso de acquire/lookup exige uma release; cópias de ponteiro e
falhas não criam referências. Contador zero, tabela errada, dupla liberação e
ponteiro pendente são violações sem diagnóstico garantido. Não prometer que a
entrada continua viva após unlock/retorno sem outra referência independente.

## Integração implementada da ponte: garantias a preservar

1. Inicializar a tabela uma vez sob o lock existente, usando helper comum
   sem E/S ou diagnóstico fatal. Publicar newtm_trace_initialized apenas com
   tabela completamente válida; liberar o lock antes do log create ou de
   gomp_fatal. Na falha, o helper desfaz alocação parcial e não publica estado.
2. Em begin, preservar contexto por thread, flat nesting e proteção de overflow
   de profundidade. Continuam exigidas execução síncrona e ausência de suspensão
   ou troca de tarefa durante a região dinâmica.
3. Em register, verificar contexto ativo, endereço não nulo e tamanho positivo.
   Duplicata de mesmo tamanho permanece idempotente; tamanho diferente é fatal.
4. Para um registro novo, obter uma referência válida antes de publicar o nó na
   lista do contexto. Falha não pode deixar registro parcial. Definir e cumprir
   a limpeza do nó temporário nos caminhos de erro.
5. A ponte atual retorna void: erros estruturais detectados usam diagnóstico
   fatal claro, sem retry e sem aceitar handle NULL como sucesso. Não mudar ABI
   para introduzir propagação de conflito transacional nesta etapa.
6. Em end interno, reduzir apenas a profundidade. Em end externo, liberar cada
   referência uma vez, limpar registros e contexto e zerar o ponteiro da thread.
   Manter a limpeza dos caminhos excepcionais que chegam ao finally existente.
7. O proprietário admite participações somente em OPEN; CLOSING rejeita
   novos usuários e permite aos existentes concluir. Contexto externo retém
   lease até terminar toda a limpeza; consumidores diretos da tabela global
   também precisam de lease. Owner_close não aguarda drenagem: BUSY mantém
   CLOSING com usuários/coordenador/entradas residuais. Retry após término/join;
   somente um coordenador chama destroy fora do lock e publica CLOSED em OK.
   O finalizador apenas tenta close, sem fatal, espera ou remoção forçada;
   mantém controle/mutex para rejeitar chamadas tardias. Não garante dlclose.
   Contrato completo em
   [admissão](OPENMP_TRANSACTION_HASH_AGENT.md#admissão-e-participações-da-camada-proprietária).

O mutex global de inicialização não deve passar a proteger operações normais
da hash. O serviço não pode ser apresentado como sincronização dos valores do
programa, mesmo quando entradas de threads diferentes forem compartilhadas.

## Arquivos e documentação

Implementação principal: libgomp/newtm-hash.c e libgomp/newtm-hash.h, incluindo
a ponte provisória hoje localizada nesse fonte. Conferir mutexes dos targets e
as regras de inicialização/destruição. Alterar Makefile.am/Makefile.in somente
se a integração ou os testes exigirem, mantendo-os coerentes.

Preservar visibilidade interna de newtm_hash_* e newtm_read/write e os símbolos
GOMP_* existentes. Não exportar serviços internos para permitir testes.
Não modificar o compilador sem necessidade demonstrada pela integração.

Decisão do usuário: manter os printf/fflush para depuração na implementação.
Preservar os formatos atuais enquanto usados pelos verificadores. Não executar printf/fflush ou outra E/S de
diagnóstico sob mutexes de buckets, inclusive em visit e count. Se preservados,
capturar dados seguros sob lock e imprimir após desbloquear, sem desreferenciar
entradas liberadas ou metadados sem proteção. Ordem de impressão concorrente não
comprova ordem das operações. Logs na ponte seguem a mesma restrição.
Atualizar conjuntamente testes de saída e documentação quando mensagens mudarem;
transaction_test/verificar.py depende de contagens e posições dos logs hash.
Validar resultados, referências e callbacks além das mensagens, sem alterar
expectativas para esconder regressões. Logs dos stubs de acesso/ciclo de vida
permanecem um contrato separado. Detalhes em
[restrição de logs](OPENMP_TRANSACTION_HASH_AGENT.md#restrição-de-logs-na-implementação-operacional).

Atualizar panorama, agente da hash, inventário de implementação, manual e
transaction_test/README.md quando o comportamento mudar. Remover descrições
obsoletas de handles nulos e hash sem implementação, mantendo explícita a
ausência de isolamento transacional. Identificar o histórico como histórico.

## Validação para manutenção e novas alterações

Os testes privados existentes incluem o fonte real e headers/configuração
do build Linux x86_64 e ligam o archive local, sem ampliar a ABI. Manter esse
mecanismo reproduzível no Makefile ou justificar uma substituição compatível;
não depender exclusivamente de fontes em /tmp. Interposição/injeção de falhas
e contadores permanecem restritas aos testes, sem hooks de produção.

A partir da raiz, no ambiente compatível com o xgcc construído:

```sh
make -C gcc-build -j"$(nproc)"
make -C transaction_test -B check-hash-create-destroy check-hash-acquire \
  check-hash-entry check-hash-count check-hash-diagnostics check-hash-owner
make -C transaction_test -B check check-lifecycle
make -C transaction_test check-carga CARGA_THREADS=4 CARGA_REPETICOES=1000
```

Check já inclui check-carga (1:9, 1:1, 9:1); o último comando mostra execução
isolada. A carga também pode ser chamada como
transaction_test/.build/transaction_carga THREADS W:R REPETICOES. Uma repetição
é uma transação por thread; escrita é read-modify-write. São
floor(N*W/(W+R)) escritas por thread. Consultar ../transaction_test/README.md para
limites dos parâmetros, logs e timeouts. Todos os testes privados de hash usam
configuração Linux x86_64; isso não comprova suporte dos testes em outros targets.

| Grupo | Evidência exigida |
| --- | --- |
| Estrutura | Capacidades 1 e não potência de dois, colisões, chaves distintas, aquisição repetida, consulta ausente, identidade e tamanho. |
| Falhas | Nulos previstos, tamanho zero, SIZE_MISMATCH sem alteração, out limpo, BUSY preservando tabela e overflow antes de alterar estado. |
| Referências | Liberação parcial/final, consulta retendo entrada após outra release e remoção somente na última referência. |
| Metadados | visit com exclusão correta, substituição com propriedade definida e destrutor exatamente uma vez fora do lock. |
| Concorrência | Mesma chave, colisões, progresso em buckets distintos e contagem concorrente sem corrupção ou deadlock. |
| Ponte C/C++ | Registro antes do corpo, duplicatas, aninhamento lexical/por função, retenção até saída externa, threads distintas e limpeza excepcional suportada. |
| Integração | Templates, referências C++, cópias private/reduction externas, dumps e ligação com a libgomp reconstruída; não confundir registro aceito hoje com tipo STM admitido. |
| Subconjunto funcional futuro | Leitura externa sem controle, filtros recursivos, classes adiadas, vetor canônico com índice dinâmico e diagnóstico de limites, ponteiros sem identidade e registros distintos sobrepostos rejeitados, sem fallback de erro runtime; exceções/cleanups do usuário rejeitados com limpeza interna obrigatória. |

Validar a mistura nas larguras 32/64 com endereços consecutivos e alinhados,
capacidades 1 e não potências de dois e bases que exercitem bits superiores.
Separar simulação aritmética de execução nativa; a evidência exploratória não
substitui testes permanentes da implementação nem exige uniformidade perfeita.

Usar coordenação determinística em testes concorrentes, sem sleep como prova de
ordenação. Não executar count/reentrada a partir de visit para tentar provar
liberação do lock. Para overflow impraticável por volume, usar mecanismo interno
controlado e documentado; distinguir cobertura por teste de inspeção do código.
Se testar falta de memória, fazê-lo em processo separado com injeção controlada.

Reconstruir libgomp em gcc-build/ e executar os testes internos e os
subconjuntos DejaGNU C/C++ pertinentes a transaction. Reconstruir GCC apenas se
os seus fontes mudarem. Executar também os exemplos locais com recompilação
forçada. Conferir a biblioteca efetivamente carregada e os arquivos .sum/.log;
reportar PASS, FAIL, UNRESOLVED e UNSUPPORTED separadamente.

## Critérios de manutenção e tarefas remanescentes

A implementação estrutural e o protocolo do proprietário foram entregues.
Ao modificar, preservar os nove contratos, leases até a limpeza completa,
coordenador único e rejeição de chamadas tardias; executar a cobertura afetada
e atualizar os algoritmos em comentário e o inventário. Não declarar STM,
isolamento, commit, rollback ou retry implementados.

Pendências de validação/manutenção, distintas de reimplementar a hash:

- Reproduzir DejaGNU C/C++ com runtime local quando as ferramentas estiverem
  disponíveis; os resultados de 383 PASS da auditoria de 2026-10-21 são históricos.
- Validar nativamente 32 bits e outros backends de mutex/targets antes de
  anunciar cobertura além do ambiente Linux x86_64 já exercitado.
- Medir contenção antes de antecipar alocação ou alterar distribuição/capacidade;
  separar custo dos logs obrigatórios e do critical da carga dos custos da hash.
- Antes de instalar metadados de política na tabela global, configurar seu
  destrutor imutável na criação: owner já configura newtm_policy_destroy.
  Não alterar a configuração após publicação; criar metadados sob demanda.
- Ao integrar contexto STM, preservar leases durante todas as referências,
  tentativas/cleanups em andamento; não substituir quiescência por count zero.
  dlclose permanece sob exclusão externa de código, sem garantia automática.

As funcionalidades de STM pendentes estão na sequência do agente de projeto;
a carga parametrizada continuará usando critical até haver isolamento funcional.

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
