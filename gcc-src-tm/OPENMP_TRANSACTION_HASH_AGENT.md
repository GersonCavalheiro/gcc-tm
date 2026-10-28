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

# Agente orientador: biblioteca hash de transaction

## Missão, referências e estado — 2026-10-28

Manter a estrutura hash operacional e seus contratos. As nove primitivas em
libgomp/newtm-hash.c/.h estão implementadas, com referências reais e callbacks
protegidos por bucket; a ponte GOMP usa leases do proprietário e encerramento
OPEN/CLOSING/CLOSED. Não há hash estrutural ou protocolo de shutdown a implementar
do zero. Leia também OPENMP_AGENT.md, OPENMP_CRITICAL_AGENT.md,
OPENMP_TRANSACTION_HASH_IMPLEMENTATION_AGENT.md e OPENMP_TRANSACTION_IMPLEMENTATION.md.

O restante deste documento distingue a infraestrutura entregue da política STM
futura: registros e referências não concedem reservas nem copiam/publicam valores
do programa. Read/write, contexto de tentativa, commit, descarte e retry funcionais
continuam pendentes. Não executar os stubs como operações bem-sucedidas.

Os testes permanentes em ../transaction_test cobrem estrutura, referências,
metadados, contagem, diagnósticos e encerramento. A carga parametrizada
transaction_carga e verificar_carga.py executam W:R=1:9,1:1,9:1 via check-carga,
também incluído em check. Critical interno sincroniza os dados na etapa atual;
a carga não certifica isolamento ou desempenho STM. Os printf continuam ativos.

Fontes: gcc-src-tm/; build: gcc-build/; testes: transaction_test/ na raiz do
repositório (/usr/src/gcc-tm no contêiner). Localizar funções pelo nome e
confrontar o inventário antes de alterar; preservar relatos datados como histórico.

## Integração com os agentes de acesso e abort

Consultar [OPENMP_TRANSACTION_ACCESS_AGENT.md](OPENMP_TRANSACTION_ACCESS_AGENT.md)
para as assinaturas propostas de `newtm_read/write`, pré/pós-condições,
identidade objeto/deslocamento, pontos de invocação e sincronização dos acessos.
Consultar [OPENMP_TRANSACTION_ABORT.md](OPENMP_TRANSACTION_ABORT.md) para a
propagação da falha, cleanups, descarte e reinício da tentativa externa.

A divisão de responsabilidades é:

| Camada | Responsabilidade |
| --- | --- |
| Hash | Chaves, buckets, referências físicas e acesso protegido aos metadados opacos. |
| Primitivas de acesso e política | Reservas, visão privada, operação solicitada e sinalização da falha. |
| Controle de abort | Interromper continuação normal, conduzir cleanups, descartar a tentativa e decidir reinício ou erro permanente. |

Este arquivo mantém o contrato estrutural da hash e a política geral. O agente
de acesso detalha as operações; não criar aqui uma segunda definição divergente
das assinaturas ou dos estados `NEWTM_ACCESS_*`. As assinaturas já têm
esqueletos internos; os contratos funcionais continuam propostos, sem ABI
de ponte `GOMP_*` consolidada. Os estados de acesso
não são os estados `NEWTM_HASH_*`; traduzi-los explicitamente.

## Contrato estabelecido e decisões propostas

Estabelecido com o usuário:

- A tabela é global no runtime, compartilhada pelas threads que usam a mesma
  instância da biblioteca. Não é uma tabela de nós internos do compilador.
- A chave é o endereço de memória do objeto listado na diretiva.
- Somente acessos realizados em blocos transaction participam dos conflitos.
  Um acesso fora desses blocos não consulta a tabela, mesmo que outro thread
  tenha o objeto registrado naquele momento.
- A biblioteca deve permitir substituir a política de conflitos.
- Transações aninhadas usam flat nesting: compartilham o contexto e a
  identidade da transação externa, sem confirmação independente.
- A sincronização usa um mutex por bucket. Consultas e atualizações de buckets
  diferentes podem executar em paralelo; operações no mesmo bucket são
  serializadas. Não usar um mutex global no caminho normal dessas operações.
- A configuração inicial usa 1.024 buckets, capacidade configurável,
  colisões por encadeamento e nenhum redimensionamento. O número de buckets
  não limita o número de objetos registrados.

Propostas deste agente para uma primeira implementação, ainda não decisões
expressas do usuário: remover uma entrada quando sua última referência for
liberada. A hash usa igualdade exata da chave canônica; a granularidade por
objeto inteiro foi acordada, mas a hash não resolve sozinha a canonicalização.
Igualdade exata não detecta intervalos parcialmente sobrepostos, como uma
estrutura e um campo situado em endereço diferente. Registrar o tamanho não
resolve essa limitação automaticamente.

A biblioteca mantém registros e referências; a camada transacional aplica a
política inicial explicitada abaixo. Não embutir essa política nas operações
estruturais da hash: futuras políticas devem poder substituir seus metadados e
serviços sem reescrever o encadeamento e a gestão de referências.

## Política transacional inicial acordada

Estas decisões definem o comportamento desejado da camada transacional, não
transformam as primitivas da hash em uma implementação completa de transações.

### Visão integrada e estado da implementação

A política especificada combina detecção pessimista de conflitos antes dos
acessos, reservas compartilhadas/exclusivas mantidas até o término, escritas
adiadas em cópias privadas, abort da transação solicitante e flat nesting.
A lista identifica os objetos elegíveis; a reserva indica a permissão de acesso;
a cópia privada contém os valores pendentes. Esses três conceitos são distintos.

Esta é a política acordada para implementação. No código atual, os parsers preservam as declarações na árvore e a
gimplificação converte a lista em chamadas de rastreamento. O lowering elimina
o marcador preservando esse corpo instrumentado apenas para registro. Não
apresentar este documento como evidência de que o protocolo já está implementado
ou validado. Ao implementar, atualizar este parágrafo com o estado real.

A ponte proprietária atual cria a tabela com destroy_policy=NULL, válido
porque seus metadados ainda permanecem NULL. Antes de instalar metadados de
reservas/valores privados na tabela global, definir e configurar seu destrutor
na criação, antes da publicação. Não trocar o callback com usuários ativos nem
copiar para produção a alteração direta de campos usada por testes white-box.

### Elementos fundamentais e responsabilidades

| Elemento | Comportamento esperado |
| --- | --- |
| Contexto transacional | Identifica uma transação lógica e sua tentativa ativa; mantém profundidade, objetos registrados, reservas e buffers privados. |
| Lista de objetos | Registra objetos elegíveis no contexto, sem ler seus valores nem adquirir reservas antecipadamente. Listas internas ampliam o conjunto. |
| Hash global | Localiza registros por endereço canônico e mantém sua vida útil. Não decide conflito, commit ou abort. |
| Registro global da política | Mantém leitores identificados e eventual dono da escrita de cada objeto. Todas as alterações usam o mutex de seu bucket. |
| Registro privado por objeto/contexto | Mantém a participação, o modo de acesso, a indicação de escrita e os dados privados necessários à tentativa. |
| Reserva compartilhada | Permite leitura por várias transações; impede que outra obtenha escrita enquanto algum leitor a mantiver. |
| Reserva exclusiva | Permite escrita pela transação dona e seus próprios acessos posteriores; impede acessos de outras transações. |
| Mutex do bucket | Protege operações curtas na estrutura e nos metadados. Não é a reserva transacional e não fica adquirido durante o corpo. |
| Buffer privado | Recebe escritas ainda invisíveis às demais transações e atende leituras dos valores já modificados pela própria transação. |
| Controle de repetição | Encerra a tentativa conflitante, limpa seu estado, aguarda e recomeça a região externa inteira. |

O conjunto registrado, o conjunto de leitura e o conjunto de escrita não são
necessariamente iguais. Um objeto listado e nunca acessado não provoca conflito.
O conjunto de leitura contém objetos com reserva de leitura; o de escrita contém
objetos com reserva exclusiva e modificações pendentes. A política pode manter
histórico adicional de acessos, mas não precisa de contadores de versão para
validar leituras enquanto as reservas persistirem.

### Ciclo de vida de uma tentativa

1. **Entrada externa:** criar/ativar o contexto e iniciar uma tentativa com
   profundidade um. Registrar a lista externa sem acessar o valor dos objetos.
2. **Execução ativa:** cada acesso a objeto transacionado verifica e adquire ou
   confirma a reserva necessária antes de ler/escrever. Temporários privados da
   tentativa seguem as regras de restauração, sem registro obrigatório na hash.
3. **Entrada/saída interna:** incrementar/decrementar profundidade, compartilhar
   a identidade e acumular registros. A saída interna não publica nem libera
   reservas. A tentativa continua ativa no contexto externo.
4. **Confirmação externa:** se não houve abort, preparar a publicação, conservar
   todas as reservas e publicar todos os valores pendentes. Em seguida retirar
   reservas e participações, liberar referências e buffers e desativar o contexto.
5. **Conflito:** a primitiva impede o acesso solicitante, marca a tentativa
   para abort e retorna CONFLICT. Não executa a limpeza completa dentro do
   acesso. O chamador verifica o retorno e transfere para o controle de falha,
   sem consumir resultado inválido nem continuar normalmente. Não publicar nem
   abortar a transação que já detém a reserva incompatível.
6. **Abort e repetição:** executar os cleanups suportados com o estado ainda
   válido, descartar modificações privadas, retirar reservas/participações e
   liberar referências. Desfazer todos os níveis internos de uma só vez, aguardar
   sem reservas e iniciar uma tentativa nova na entrada externa.

Estados definidos em `enum newtm_attempt_state`, no cabeçalho interno de
transaction: ACTIVE, CONFLICTED, INVALID, PUBLISHING e CLOSED, com prefixo
NEWTM_ATTEMPT_. A máquina e o consumo único estão em
[ABORT](OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa).
Confirmação segue ACTIVE -> PUBLISHING -> CLOSED; conflito segue
ACTIVE -> CONFLICTED -> CLOSED por descarte externo. INVALID domina conflito
e só permite descarte, sem retry. Espera não é estado de uma tentativa; depois
do descarte concluído, esperar sem reservas e criar outra tentativa ACTIVE.
Entrar em PUBLISHING exige concluir toda preparação falível e é irrevogável.
Antes de liberar qualquer recurso, end/discard marca CLOSED e retira o contexto
do slot proprietário; somente esse consumidor conclui a limpeza. A hash não
executa essa máquina nem decide o descarte. A tentativa seguinte
não herda reservas, registros dinâmicos ou buffers da tentativa abortada: as
listas são processadas novamente conforme a execução. Contadores de backoff
podem persistir entre tentativas; dados do programa não podem vazar pelo reinício.

### Matriz de decisões de acesso

A comparação considera sempre outra transação lógica. Reservas da própria
transação, inclusive de regiões internas, não causam conflito.

| Acesso solicitado | Estado mantido por outras transações | Resultado |
| --- | --- | --- |
| Leitura | Nenhuma reserva ou apenas leitores | Conceder/manter reserva compartilhada e realizar leitura. |
| Leitura | Escritor | Abortar a solicitante antes da leitura. |
| Escrita | Nenhuma reserva | Conceder reserva exclusiva e escrever na cópia privada. |
| Escrita | Um ou mais leitores ou escritor | Abortar a solicitante antes da escrita. |
| Conversão da própria leitura para escrita | Nenhuma reserva de outra transação | Converter para exclusiva e escrever na cópia privada. |
| Conversão da própria leitura para escrita | Leitor ou escritor de outra transação | Abortar a solicitante. |
| Leitura/escrita com reserva exclusiva própria | Nenhuma reserva incompatível, por invariável | Usar o estado privado da própria transação. |

Não confundir conflito de hash com conflito transacional. Dois endereços que
caem no mesmo bucket apenas disputam brevemente seu mutex; podem pertencer a
objetos independentes. Um conflito transacional envolve o mesmo objeto canônico
e modos de acesso incompatíveis, mesmo sem colisões de hash.

### Garantias e limites da política

Para programas no subconjunto suportado e com todos os acessos relevantes
instrumentados, reservas mantidas até o término estabilizam as leituras entre
transações e impedem observação de publicação parcial. A confirmação lógica
acontece após publicar todos os objetos e antes de liberar a primeira reserva.
Não é uma fotografia global tomada na entrada: objetos diferentes podem ter
suas reservas adquiridas em instantes diferentes.

A política não garante isolamento em relação a acessos externos ao protocolo,
justiça entre transações nem limite de repetições. Abort por conflito não é erro
final do programa e não é exceção do usuário. Falha de recurso, uso não suportado
e exceção não devem ser convertidos automaticamente em repetição infinita.

A implementação inicial usa espera progressiva limitada por tentativa, com
número ilimitado de tentativas. A espera reduz colisões repetidas, mas não prova
progresso. Não há versão publicada antes do commit nem rollback da memória
original para objetos transacionados, pois as escritas permanecem privadas;
o abort ainda precisa limpar reservas, temporários e demais recursos da tentativa.

As restrições e questões abertas nas seções seguintes fazem parte do contrato.
Todos os acessos preexistentes exigem registro explícito; aliases não
identificáveis e registros distintos sobrepostos são rejeitados. Exceções e
efeitos externos são excluídos do primeiro subconjunto conforme o fechamento acima.

### Granularidade e conflitos

Cada objeto listado é uma unidade de conflito inteira. Acessar um campo ou
elemento conta como acesso ao objeto que o contém, com identificação por sua
chave canônica. A instrumentação não pode consultar apenas o endereço do campo
ou elemento e concluir que ele não está registrado. O mapeamento de subacessos
para o objeto listado é responsabilidade do compilador/runtime de acesso.
Sobreposições entre objetos registrados distintos não são suportadas no
protótipo; diagnosticar os casos identificáveis e documentar a restrição para
os demais. Hash por igualdade exata não detecta sobreposição de intervalos.

Verificar conflitos antes de cada acesso transacional. Entre transações
diferentes, leitura/leitura é compatível; leitura/escrita, escrita/leitura e
escrita/escrita conflitam. Uma transação não conflita consigo mesma, inclusive
no flat nesting. Abortar aquela que tenta realizar o acesso conflitante,
antes de realizar o acesso. Registro na lista não é uma leitura nem uma escrita:
a reserva é adquirida no primeiro acesso correspondente.

### Reservas, versões e validação

Manter leitores identificados e dono da reserva de escrita nos metadados da
política de cada entrada. Uma leitura obtém reserva compartilhada quando não
há escritor de outra transação. Uma escrita exige reserva exclusiva: não pode
haver escritor nem leitores de outras transações. Converter a própria reserva
de leitura para escrita somente se não houver outro leitor; se houver, abortar
a transação que tenta a conversão. Verificação e atualização da reserva ocorrem
juntas sob o mutex do bucket. Não manter esse mutex durante todo o bloco.

As reservas persistem até confirmação ou abort da transação externa, mesmo
quando o objeto só foi usado na região interna. Referências físicas e reservas
são coisas distintas. Uma referência na hash não dá permissão de acesso.

As escritas são adiadas: manter uma cópia privada dos novos valores por objeto
e contexto, sem publicar modificações antes do commit. Leituras da própria
transação enxergam seus valores pendentes. Escritas parciais devem preservar os
bytes não modificados do objeto, respeitando suas restrições de tipo; não
prescrever cópia bruta para toda classe C++ sem definir os tipos suportados.
Inicializar a cópia somente depois de obter a reserva necessária; `register`
não pode ler antecipadamente o valor original sem reserva. Uma escrita parcial
precisa preservar a parte não modificada sob reserva exclusiva. Uma leitura
não deve introduzir leitura tipada de campos não inicializados nem de padding.
A cópia é armazenamento interno: `&x` continua representando o endereço lógico
do objeto original, não pode expor o endereço do buffer privado. Desreferências
suportadas são redirecionadas pela instrumentação, preservando comparações de
ponteiros e o mapeamento objeto/deslocamento.

Com todas as reservas mantidas, os objetos lidos não podem ser modificados por
outra transação até o fim. Portanto esta política não exige contadores de versão
nem revalidação global do conjunto de leituras no commit. Isso depende de todos
os acessos transacionais relevantes estarem instrumentados e de as reservas
serem respeitadas. A implementação deve validar suas invariantes de propriedade
antes de publicar; referências vivas ou presença na hash não são validação.

### Confirmação e abort

Na saída externa bem-sucedida, manter todas as reservas, publicar todas as
escritas privadas e somente depois liberar qualquer reserva. Esse protocolo
impede que outra transação respeitando o protocolo observe publicação parcial,
mesmo quando os objetos pertencem a buckets diferentes. A publicação deve
obedecer à ordenação de memória estabelecida pelos locks de liberação e
posterior aquisição das reservas. Não liberar um objeto logo após copiá-lo
quando ainda houver outros objetos a publicar.
Preparar alocações, verificações e recursos que possam falhar antes de iniciar
a publicação. Depois da primeira escrita publicada, não permitir abort por
conflito nem executar código do usuário ou operação potencialmente lançadora;
a fase de publicação deve terminar antes da liberação de reservas.

Conflito antes de um acesso condena a transação lógica inteira. A primitiva
impede o acesso e sinaliza CONFLICT; o controle de abort conduz posteriormente
cleanups, descarte das escritas privadas, retirada de reservas/participações e
liberação das referências. Não executar essa limpeza completa dentro de
read/write. Reiniciar a partir da entrada externa, inclusive quando o conflito
ocorreu em uma região interna ou função chamada. INVALID identifica falha
permanente, não um conflito a repetir. Não executar o commit de uma tentativa
condenada nem converter erro permanente em retry. A implementação de reinício deve respeitar cleanups e vida
útil de objetos C/C++; não usar salto não local arbitrário atravessando destrutores.
Primeiro impedir a continuação normal e executar os cleanups suportados da
tentativa enquanto contexto e dados privados ainda são válidos; depois descartar
buffers e liberar reservas/participações/referências, e só então esperar.
Não permitir acesso a buffers já descartados durante a limpeza. Se os cleanups
não puderem cumprir esse protocolo, o caso fica fora do subconjunto suportado.

### Escalonamento e progresso

Após limpar a tentativa abortada, aguardar entre tentativas usando backoff com
espera progressiva e teto por espera; reiniciar sem limite de tentativas. Não
manter reservas nem mutexes durante a espera. Os parâmetros temporais e eventual
jitter são detalhes ajustáveis da implementação, a documentar e testar; usar
aritmética limitada, sem overflow em contadores de tentativas ou de espera.

Não há prioridade por idade nem fila transacional nesta política: quem tenta
um acesso conflitante aborta. Não esperar pelo dono do objeto mantendo reservas.
Isso evita ciclos de espera entre reservas transacionais, mas não prova ausência
de deadlock em locks do usuário ou no restante do runtime. Mutexes por bucket
tornam a tabela bloqueante. Backoff não garante ausência de starvation/livelock,
justiça ou conclusão em tempo limitado; nenhuma dessas garantias foi assumida.

### Código não transacional e efeitos externos

Acessos fora de transaction ficam fora do protocolo, sem isolamento forte contra
código não transacional e sem alteração das regras C/C++ sobre corridas de dados.
Uma escrita externa não será detectada por validação transacional inexistente.

Restringir operações cujo efeito não pode ser descartado ou repetido com
segurança: entrada/saída, arquivos, chamadas de sistema, chamadas externas sem
suporte e criação de tasks. Diagnosticar casos identificáveis e documentar a
pré-condição para chamadas indiretas ou outros casos não verificáveis. Não
anunciar diagnóstico completo por análise apenas local. E/S, chamadas de sistema,
locks do usuário, malloc/free do programa e exceções ficam fora inicialmente;
atômicos e volatile também são excluídos pelos critérios de tipos. Alocação
futura exigirá rastrear recursos e definir descarte ou liberação adiada.

### Tipos e identidade: decisões do usuário — 2026-10-28

O subconjunto inicial de objetos transacionados admite escalares comuns,
arrays e agregados com critérios restritos, completos e de tamanho conhecido.
Validar recursivamente elementos e membros; não basta sizeof conhecido.
Ficam inicialmente fora volatile, atômicos, VLA, unions, campos de bits,
membros flexíveis e qualquer caso sem contrato específico. Escritas em objetos
const são proibidas; preservar qualificadores, inclusive de componentes.

Suporte a instâncias de classes é trabalho futuro, não uma exceção por
trivialidade de cópia/destruição. Em C++, struct também define tipo de classe:
não admitir suas instâncias implicitamente apenas por serem agregados triviais.
Arrays de tipos excluídos e agregados com componentes excluídos também ficam
fora. Ponteiros permanecem escalares: controlar o valor de um ponteiro para
classe não significa oferecer suporte transacional à instância apontada.
Referências C++ a objetos escalares/arrays admitidos continuam designando o
objeto referido, sob as mesmas restrições.

Essas são regras do primeiro subconjunto STM funcional. A aceitação atual pelo
parser/rastreamento não comprova que os filtros semânticos já foram implementados.

Listar um ponteiro registra o objeto ponteiro e seu valor, não a memória apontada.
Para transacionar o objeto apontado, esse objeto precisa estar no conjunto listado
e ser identificado pela instrumentação. Referências C++ designam o objeto
referido, desde que ele atenda às mesmas restrições. Preservar tipos e
qualificadores durante a análise; emitir diagnóstico adequado ao caso rejeitado.

### Aliases e chamadas de funções

Suportar acessos diretos e aliases cuja identidade canônica o compilador consiga
estabelecer. O alias deve usar a mesma entrada e cópia privada, sem caminho
alternativo de acesso comum ao objeto registrado. Para vetor[i] com vetor
canônico conhecido, i pode ser dinâmico: calcular índice/endereço uma única vez,
sem overflow, e validar os limites em execução antes do acesso.

Rejeitar acessos por ponteiros cuja identidade não possa ser estabelecida.
Não fazer fallback silencioso à memória comum. Leituras e escritas em objetos
preexistentes exigem registro explícito; ponteiro desconhecido não autoriza
acesso externo comum. Resolução dinâmica de
identidade exige contrato adicional de intervalos/sobreposição e fica fora da
versão inicial; a hash por igualdade exata não fornece esse mecanismo.

Registros distintos sobrepostos são proibidos inicialmente, inclusive quando
usados por contextos diferentes. Campos/elementos de um agregado registrado usam
sua entrada e deslocamentos, com conflito no objeto inteiro. Repetir o mesmo
objeto canônico/mesma extensão não cria registro distinto: mantém deduplicação e
referências conforme a camada. Não confundir colisão de bucket com sobreposição.

A integração precisa impor a proibição e diagnosticar os casos fora do subconjunto;
o código atual da hash não verifica intervalos de entradas com chaves diferentes.
Não anunciar que essa validação está pronta nem introduzir busca dinâmica de
intervalos implicitamente. Se não for possível garantir a identidade e a ausência
de registros distintos sobrepostos no caso admitido, restringi-lo antes do acesso.

### Versões transacionais e propagação de falha

Decisão do usuário:

Gerar versões internas transacionais das funções chamadas, com contexto e status
explícitos. Preservar a assinatura pública original, inclusive seu retorno e
uso fora da transação. A transformação deve alcançar a cadeia de chamadas
preparadas, não somente o corpo léxico da diretiva ou funções inline.

```text
status = função_transacional(contexto, argumentos, resultado)
se status falhou:
    seguir para o epílogo de falha
usar resultado
```

A versão interna retorna status e só preenche o resultado no epílogo de sucesso.
Se falhar, o armazenamento de resultado permanece inalterado: não publicar
resultado parcial nem um valor fictício; o chamador não o lê nem
continua o caminho normal. Funções sem resultado também propagam status, sem
necessidade de fabricar um valor de retorno. OK/CONFLICT/INVALID são conceitos
semânticos: seu enum, ABI gerada e traduções entre estados de acesso/ciclo de
vida ainda precisam de definição técnica, sem pressupor igualdade numérica.

Passar o mesmo contexto da tentativa pela cadeia, preservando flat nesting.
Uma função chamada ou região interna não confirma, reinicia ou descarta a
tentativa externa por conta própria. Cada chamada e operação instrumentada deve
ter seu status verificado antes de consumir resultado ou executar efeitos
normais posteriores. Na falha, seguir ao epílogo da função, executar somente
cleanups suportados e propagar a causa até o controle externo, sem consumir o
contexto prematuramente; manter INVALID dominante conforme o contrato de falha.
O controle externo faz o descarte único depois dos cleanups; somente CONFLICT
permite repetir todo o bloco externo. Não usar setjmp/longjmp nem retornar
valores fictícios como canal implícito de erro.

Chamadas indiretas ou externas sem preparação ficam inicialmente restritas.
Pure/const não prova que uma função observa os valores privados de objetos
registrados; não autoriza chamar uma versão comum no lugar da transacional.
Inlining só é válido com verificação/instrumentação dos acessos resultantes,
não como prova isolada de segurança. Nas versões preparadas, todos os acessos
a objetos preexistentes também exigem registro explícito.

Identificação das versões, nomes/linkage, chamadas entre unidades de tradução,
representação de resultados por referência e detalhes da ABI gerada ainda
precisam ser definidos antes da integração. Não inventar um atributo já existente
ou exportar novos símbolos GOMP por esta decisão documental. Exceções e
cleanups do usuário foram rejeitados no subconjunto inicial; seu suporte futuro
exige protocolo próprio para tentativa condenada. Efeitos externos estão
excluídos do primeiro subconjunto conforme o fechamento acima. Nenhuma versão transacional ou transformação de chamadas foi
implementada por esta atualização dos agentes.

### Estado restaurado entre tentativas

Toda leitura ou escrita em objeto cuja vida começou antes da transação externa deve atingir
um objeto listado no contexto compartilhado, seja pela lista externa ou por uma
lista interna já processada. Rejeitar acessos identificáveis em objetos externos
não listados; não ampliar automaticamente o conjunto transacionado. Uma lista
interna não autoriza retroativamente um acesso anterior ao seu processamento.
Aplicar a regra também a aliases, funções chamadas e efeitos indiretos.

Variáveis automáticas criadas durante uma tentativa podem servir de estado
temporário. Devem ser recriadas no reinício, com inicializadores novamente
executados, e não podem escapar da tentativa. Não confundir uma variável local
`static` com um objeto novo a cada tentativa. Temporários não precisam ser
registrados quando são privados à tentativa e não compartilhados; se forem
explicitamente listados, obedecem às restrições de vida útil já estabelecidas.

O abort executa os cleanups suportados enquanto o contexto ainda está válido,
descarta as cópias privadas e limpa reservas/participações, nessa ordem, antes
da espera e da próxima tentativa. Não preservar incrementos, ponteiros escapados ou outro estado visível
produzido por uma tentativa abortada. O mecanismo de controle deve retornar à
entrada externa e reconstruir os escopos temporários, não apenas repetir a
instrução que encontrou conflito. Suporte a destrutores ou cleanups com efeitos
externos não decorre dessa exigência e permanece restrito.

### Registro explícito de leituras e escritas — decisão atual

Todos os acessos a objetos preexistentes exigem registro explícito já realizado,
inclusive leituras nas funções preparadas. A forma sem lista não infere objetos;
listas internas só autorizam acessos posteriores ao registro. Temporários privados
criados na tentativa seguem seu contrato de recriação e não escape.
Objeto ausente em newtm_read/write é INVALID, nunca acesso comum como fallback.
A permissão anterior para ler objetos externos não listados foi substituída.

### Representação dos valores privados — decisão do usuário

Escalares usam uma representação privada simples, com controle de validade e
modificação do valor. Agregados usam armazenamento privado acompanhado de
faixas válidas e faixas modificadas. Uma faixa válida contém bytes já obtidos
sob a reserva apropriada ou produzidos por uma escrita da tentativa; toda faixa
modificada deve ser válida. A organização escolhida é usar listas de intervalos válidos e modificados
com buffer de extensão completa, preenchido sob demanda; campos e algoritmos
de manutenção dos intervalos ainda precisam de especificação.

Ler um campo/elemento deve obter somente as partes solicitadas ainda ausentes
da visão privada, sob a reserva necessária, preservando escritas anteriores da
própria tentativa. Escrever deve disponibilizar e marcar como válidos e
modificados os bytes escritos, sem exigir leitura prévia desses bytes nem
inicialização/acesso integral do agregado. Uma leitura-modificação-escrita
continua exigindo uma leitura válida da parte usada no cálculo. Isso não torna
válida uma leitura de valor não inicializado no programa nem autoriza interpretar
padding, acessar fora do objeto ou contornar as restrições dos tipos.

O commit publica somente faixas modificadas, preservando as demais partes do
objeto; o descarte elimina os buffers e o controle de faixas sem publicá-los.
As faixas não criam identidades ou reservas independentes: o conflito continua
associado ao objeto canônico registrado. Alinhamento, vida útil e aliasing devem
ser respeitados na implementação dos temporários e transportes.

Buffers privados são internos ao runtime/compilador e nunca aparecem como
endereços do programa. `&x` continua sendo o endereço lógico original de `x`;
campos e elementos preservam seus endereços lógicos e deslocamentos. A
instrumentação redireciona os acessos admitidos à visão privada sem trocar essa
identidade nem habilitar acessos por ponteiros de identidade desconhecida.
Esses são contratos da futura STM funcional, ainda não implementados pelos stubs.

### Semânticas que ainda precisam de decisão

Tipos iniciais e aliases/sobreposição já estão definidos acima. Instâncias de
classes ficam explicitamente para trabalho futuro. A aplicação desses filtros,
o mapeamento canônico e os diagnósticos ainda precisam de implementação na
integração funcional; distinguir decisão estabelecida de validação executada.
A propagação entre funções foi definida por versões internas com contexto,
status explícito e resultado só em sucesso. A ABI gerada ainda precisa de
especificação técnica. Exceções/cleanups do usuário foram rejeitados
inicialmente; sua extensão é futura, com protocolo para tentativa condenada.
A representação privada foi definida acima: escalares simples e agregados com
faixas válidas/modificadas, preservando endereços lógicos. O layout concreto,
alinhamento ainda precisam de detalhamento técnico. Efeitos externos e o destino
de INVALID foram definidos no fechamento do subconjunto acima. Não pressupor
cópia integral universal nem descoberta de intervalos pela hash.

### Exceções e cleanups do usuário — decisão inicial

Rejeitar inicialmente exceções e cleanups do usuário no subconjunto STM
funcional, inclusive nas funções preparadas chamadas durante a tentativa.
Não admitir objetos/temporários que exijam destrutores do usuário, handlers de
cleanup ou chamadas sem garantia de compatibilidade com essa restrição.
A aceitação atual do parser/rastreamento e seus testes de desenrolamento não
significam que esses casos sejam suportados pela futura STM funcional.

A limpeza interna do runtime continua obrigatória: retirar reservas e
participações semânticas, liberar referências/buffers/contexto e devolver a
lease do proprietário somente depois dos recursos dependentes. Destrutores
internos de metadados de política continuam sujeitos a seus contratos; não são
cleanups de objetos do usuário e não ficam dispensados por esta restrição.

Uma tentativa condenada recusa novos acessos normais via read/write. Não usar
o caminho normal dessas primitivas em destrutores/cleanups nem relaxar seus
guards para aparentar suporte. Cleanup interno do protocolo trabalha sobre
seus próprios recursos e metadados, sem reabrir a tentativa ao código do usuário.
Exceções do usuário nunca viram CONFLICT, não autorizam retry e nunca provocam
commit durante desenrolamento. O finally de rastreamento não pode ser convertido
em confirmação incondicional.

Suporte a exceções/cleanups do usuário fica como trabalho futuro: definir a visão
de dados acessível aos destrutores com tentativa condenada, seu protocolo de
acesso separado, ordem de limpeza, vida útil dos buffers/contexto, falhas no
próprio cleanup e propagação da exceção original. Só então ampliar os casos
admitidos; não tratar essas decisões como autorização para implementá-los agora.
A aplicação dos diagnósticos ainda precisa de implementação; esta atualização
registra a semântica, sem alterar o compilador.

Tasks, suspensão e sincronizações coletivas dentro da transação exigem restrição
explícita: uma barreira dentro de código que pode ser repetido por apenas uma
thread pode impedir progresso. Rejeitar construções identificáveis não suportadas,
incluindo barreiras/worksharing/parallel internos, até definir seu protocolo.
Executar transaction dentro de uma região parallel externa continua sendo o
cenário pretendido; isso não autoriza herança automática do contexto por threads.

## Arquivos e organização

Manter a implementação C existente, seguindo as convenções da libgomp:

- `libgomp/newtm-hash.c`: implementação da tabela, cálculo de hash,
  encadeamento, alocação, sincronização e contagem de referências.
- `libgomp/newtm-hash.h`: interface interna, tipos opacos, contratos e código
  de estado. Não instalar esse cabeçalho como API pública OpenMP.
- `libgomp/newtm-transaction.c` e `libgomp/newtm-transaction.h`: seis esqueletos
  internos de ciclo de vida (ver inventário) e esqueletos
  internos de leitura/escrita que imprimem parâmetros e retornam INVALID.
  Contexto e política funcionais continuam futuros; nenhuma chamada a esses
  acessos é gerada pelo compilador nesta etapa.
- `OPENMP_TRANSACTION_HASH_AGENT.md`: manter esta especificação coerente com
  as decisões e limitações da implementação efetivamente entregue.

Usar prefixo `newtm_hash_` nas primitivas e tipos da tabela. Pesquisar colisões
antes de introduzir nomes. Os fontes já integram libgomp/Makefile.am; preservar
sua inclusão e tratar os arquivos gerados segundo a infraestrutura e versão de ferramentas do projeto,
sem regenerações amplas desnecessárias. Examinar as regras locais para incluir
os cabeçalhos na distribuição. Reutilizar mutexes e mecanismos de alocação e
falha da libgomp, sem introduzir dependência direta de pthread em código comum.

## Modelo de dados

Tipos opacos expostos pelo cabeçalho:

```c
struct newtm_hash_table;
struct newtm_hash_entry;
```

Cada tabela mantém um vetor de buckets e sua capacidade imutável. Cada bucket
mantém sua lista, seu número de entradas e seu próprio mutex. Cada entrada mantém:

- endereço do objeto, usado como chave e nunca desreferenciado pela tabela;
- tamanho em bytes informado pelo chamador;
- contagem de referências que mantêm a entrada viva;
- ponteiro opaco para metadados da política, inicialmente nulo;
- ligação interna da lista de colisões.

O encadeamento e os mutexes são privados. Não armazenar o valor do objeto nem nós
`tree`/GIMPLE nessa estrutura. Um indicador de escrita pertence ao registro
objeto/transação da política, não a um booleano global compartilhado sem
identidade de transação. Versões, leitores e reservas também pertencem à
política. Na política inicial, leitores e dono da escrita são obrigatórios;
versões numéricas não são necessárias.

Cada aquisição representa uma referência, não necessariamente uma transação
única. A tabela deve equilibrar cada aquisição com uma liberação. A camada
transacional elimina registros duplicados por endereço dentro do contexto
compartilhado: mantém uma referência de participação por objeto nesse contexto,
mesmo que ele seja listado várias vezes ou em diferentes níveis de aninhamento.
Uma aquisição temporária adicional, se realizada, deve ser liberada imediatamente
após identificar a duplicata. Tamanhos incompatíveis continuam sendo erro;
a deduplicação não pode ocultar essa verificação.

## Interface e primitivas

Assinaturas internas implementadas; preservar `size_t` e os qualificadores
do cabeçalho. O enum já define os estados `NEWTM_HASH_OK`, `NEWTM_HASH_INVALID`,
`NEWTM_HASH_NOT_FOUND`, `NEWTM_HASH_SIZE_MISMATCH`, `NEWTM_HASH_BUSY`,
`NEWTM_HASH_OVERFLOW` em `enum newtm_hash_status`.
Saídas por ponteiro ficam nulas nas falhas, quando aplicável. Falhas não deixam
inserções ou incrementos parciais. Falta de memória permanece fatal via
`gomp_fatal`, sem `NEWTM_HASH_NO_MEMORY`. Para não diagnosticar sob locks, a hash
usa malloc/calloc diretamente em um helper sem E/S para a tabela e malloc sob
o bucket para entradas; uma falha libera os locks antes de gomp_fatal. O helper
retorna NULL apenas internamente, sem criar estado recuperável na API. Verificar
overflows antes de alocar. Uma política recuperável de alocação exigirá revisão
explícita desse contrato, sem depender de um estado que nunca é retornado.
Funções que retornam estado rejeitam tabela nula quando recebem uma tabela,
saída nula quando exigem saída e endereço nulo quando recebem chave. Demais
primitivas exigem argumentos não nulos válidos como pré-condição.

### Criação e encerramento

```c
typedef void (*newtm_hash_policy_destroy_fn) (void *policy_data);
enum newtm_hash_status newtm_hash_create (
  size_t bucket_count, newtm_hash_policy_destroy_fn destroy_policy,
  struct newtm_hash_table **out);
enum newtm_hash_status newtm_hash_destroy (struct newtm_hash_table *table);
```

`newtm_hash_create` aloca uma tabela vazia e inicializa o mutex de cada bucket. O serviço
transacional global chama esta função com `NEWTM_HASH_DEFAULT_BUCKETS`, definido
como 1024. A primitiva rejeita capacidade zero e verifica overflow da alocação.
A redução aceita qualquer capacidade positiva com alocação representável.
A mistura escolhida é fmix32/fmix64, conforme a decisão detalhada abaixo.

`newtm_hash_destroy` libera buckets, tabela e todos os mutexes somente se não houver
entradas vivas; caso contrário retorna `NEWTM_HASH_BUSY`. Exige ausência de
operações concorrentes: os mutexes dos buckets não podem proteger acessos
que começam depois de ela ser destruída. A instância global dura até o término
do runtime; não destruir a tabela a cada saída de transaction.

Quiescência para `newtm_hash_destroy` é uma obrigação externa do chamador:
impedir o início de novas operações e aguardar a conclusão de todas as operações
em andamento, incluindo visitas e destruidores de metadados, mantendo essa
exclusão até o retorno. A primitiva não estabelece esse protocolo. Referências
retidas, mas ociosas durante a chamada, são permitidas: suas entradas causam
BUSY, sem consumo de referências ou destruição parcial. Após BUSY, o chamador
pode encerrar a exclusão e retomar o uso normal da tabela.

`newtm_hash_count() == 0` é somente uma fotografia de ausência de entradas
encadeadas. Não impede novas chamadas, não identifica usuários que ainda podem
acessar a tabela e não espera por uma última release que já retirou a entrada,
mas ainda executa seu destrutor fora do mutex. Não constitui autorização para
destruir a tabela. OK de destroy exige, separadamente, quiescência externa e
ausência de entradas.


### Admissão e participações da camada proprietária

A tabela global da ponte é protegida por um controle de vida útil separado das
nove primitivas: OPEN -> CLOSING -> CLOSED, irreversível. Estado, ponteiro,
contagem de participações e flag de coordenador são protegidos pelo mutex global.
Esse controle estático e seu mutex sobrevivem à tabela; não destruir o mutex no
finalizador enquanto chamadas tardias ainda podem consultá-lo.

Três serviços internos ocultos em newtm-hash.h, sem nova ABI GOMP:

- newtm_hash_owner_acquire recebe uma lease zerada/inativa. Em OPEN, inicializa
  a tabela se necessário e registra uma participação sob o mesmo mutex antes
  de expor o ponteiro. A participação já cobre o log create fora do lock.
  OK entrega tabela emprestada; BUSY rejeita CLOSING, INVALID rejeita CLOSED,
  NULL ou lease já ativa; OVERFLOW não modifica a contagem. Falhas em lease
  inativa deixam table NULL. OOM permanece fatal fora do lock.
- newtm_hash_owner_release consome exatamente uma participação ativa depois de
  terminar operações, referências, callbacks e limpeza. OK zera a lease;
  INVALID não consome participação em argumentos inativos/incompatíveis.
  Copiar a lease não cria participação; transferências e uso entre threads
  exigem sincronização externa e propriedade exclusiva. Não usar table depois.
- newtm_hash_owner_close fecha a admissão e tenta concluir sem aguardar drenagem.
  BUSY mantém CLOSING se há participações, coordenador já atuando ou entradas
  residuais; nunca remove à força, reabre ou libera parcialmente. O coordenador
  pode finalizar/join dos usuários e repetir a chamada. Se não há usuários,
  designa um único destruidor sob lock, chama destroy fora dele e só publica
  CLOSED/ponteiro NULL depois de OK. Sem tabela previamente criada, fecha sem
  alocar. CLOSED repetido retorna OK sem nova destruição.

Não é um algoritmo lock-free: pode aguardar brevemente o mutex de controle,
mas não espera participações, não faz polling interno nem segura recursos
esperando usuários. Não existe garantia de término se um usuário abandona sua
lease. BUSY após drenagem por entradas residuais indica consumidor fora do
protocolo ou referência perdida, não autorização para remoção forçada.

O contexto externo da ponte possui uma participação, não uma por objeto ou
nível aninhado. Begin externo solicita admissão; novo contexto recusado é fatal
fora do lock, pois a ABI void não oferece retorno de falha. Contextos já
admitidos podem aninhar/register/end em CLOSING. Register usa a tabela da lease;
end interno só reduz profundidade. End externo libera entradas e metadados,
limpa TLS, libera o contexto e só então devolve a participação transferida para
armazenamento local. Isso cobre a última release mesmo depois de retirar sua
entrada do bucket e enquanto executa o destrutor de política.

Consumidores diretos da tabela global devem manter uma lease durante TODO o
intervalo de uso, inclusive referências retidas entre chamadas; não podem
chamar destroy na tabela emprestada. O proprietário é seu único destruidor.
Tabelas independentes criadas pela primitiva create continuam sob quiescência
externa do próprio chamador. Nenhum dos mecanismos torna seguros ponteiros
pendentes ou cópias de lease tratadas como participações independentes.

O finalizador automático faz uma única tentativa não bloqueante de close.
Em BUSY, mantém tabela/controle vivos e não espera, não força limpeza nem chama
gomp_fatal recursivamente; o processo recupera a memória ao terminar. Se apenas
solicitar fechamento e continuar a execução, o coordenador explícito pode
repetir close depois de os usuários terminarem. O mutex de controle permanece
válido após CLOSED para rejeitar acessos tardios. Isso não protege código
removido por dlclose: descarregar a biblioteca exige impedir externamente toda
execução atual ou futura dela, inclusive callbacks.

Count zero continua sem provar quiescência. Fechamento exige ausência de
participações, e não observação da quantidade de entradas. Diagnósticos e logs
permanecem fora dos mutexes de controle e buckets; não esperar close segurando
stdio ou recursos de que os usuários precisam para concluir.

### Decisão de mistura de endereços

Para a etapa operacional, usar os finalizadores `fmix32` e `fmix64` do
[MurmurHash3 original](https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp).
Converter a chave para `uintptr_t` e selecionar por `UINTPTR_MAX`:

- `UINTPTR_MAX == UINT32_MAX`: `x ^= x >> 16; x *= 0x85ebca6b;`
  `x ^= x >> 13; x *= 0xc2b2ae35; x ^= x >> 16;`.
- `UINTPTR_MAX == UINT64_MAX`: `x ^= x >> 33; x *= 0xff51afd7ed558ccd;`
  `x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53; x ^= x >> 33;`.

Usar operações e constantes sem sinal na largura selecionada, com produtos
módulo 2^32 ou 2^64 e deslocamentos menores que a largura. Não selecionar por
`unsigned long`, não descartar bits inferiores por alinhamento e não truncar o
resultado antes da redução. O bucket é `x % bucket_count`, para qualquer
capacidade positiva com alocação representável; máscara não substitui o módulo.
`acquire` e `lookup_acquire` usam a mesma função interna, sem estado ou semente.
A comparação de chaves permanece por endereço exato. A entrada guarda o índice
imutável calculado, utilizado por `release` e `entry_visit`.

Larguras de `uintptr_t` diferentes de 32/64 ainda não têm algoritmo selecionado.
A implementação operacional deverá diagnosticá-las na compilação, sem truncar
silenciosamente. Suporte adicional exige extensão explícita; isso não altera
os corpos ainda não implementados. A mistura já é usada por acquire/lookup.

A escolha foi informada por simulação de 720 combinações: 32.768 endereços
numéricos distintos por sequência, larguras 32/64, três bases por largura,
incrementos 1/2/8/64/4096/65536 e capacidades 1/3/1009/1024/1536. Os finalizadores
ocuparam todos os buckets nos casos de capacidade maior que um examinados, com
maior carga inferior a duas vezes a média. Módulo direto e multiplicação simples
concentraram endereços alinhados para capacidades potência de dois. Esses dados
são evidência exploratória, não garantia universal, benchmark ou teste da
implementação C. A validação permanente deve cobrir esses padrões, bases que
exercitem bits superiores e identidade correta mesmo com colisões, sem exigir
uma distribuição perfeitamente uniforme. Não há promessa criptográfica.

### Inserção ou aquisição

```c
enum newtm_hash_status newtm_hash_acquire (
  struct newtm_hash_table *table, const void *address, size_t size,
  struct newtm_hash_entry **out);
```

Localiza a chave; se ausente, cria uma entrada com uma referência e metadados
nulos. Se presente e com o mesmo tamanho, incrementa a contagem e devolve a
mesma entrada. A busca e a alteração da lista e das referências ocorrem sob o mutex do bucket
calculado a partir da chave. Rejeita endereço nulo,
tamanho zero e overflow de contagem. Tamanhos diferentes para a mesma chave
retornam `NEWTM_HASH_SIZE_MISMATCH`; não substituir silenciosamente o tamanho.
Esse caso deverá ser revisto se a política futura admitir registros de subobjetos
com mesmo endereço inicial e extensões diferentes.

### Alocação inicial e possível otimização de acquire

A implementação inicial aloca a entrada ausente com o mutex do bucket adquirido.
Isso simplifica a inserção e mantém busca, validação e publicação em uma única
seção crítica. Usa malloc sem diagnóstico implícito: se falhar, desbloquear antes
de gomp_fatal, sem alterar lista/referências/contadores nem publicar out. OOM
continua fatal. Não é necessário antecipar a alocação nesta etapa.

Depois de a manipulação da tabela estar operacional e validada, medições de
contenção poderão justificar uma otimização que aloque fora do mutex. Essa
alternativa exigirá nova busca sob lock, pois outra thread pode inserir a chave
nesse intervalo, e descarte correto de candidatos não utilizados. Deverá
preservar unicidade da entrada encadeada por chave, verificação de tamanho,
limites dos contadores, propriedade das referências e ausência de alterações
parciais nas falhas. Uma aquisição existente não deve passar a depender de uma
alocação desnecessária que possa encerrar o processo. Trata-se de sugestão de
otimização futura, não de requisito para concluir a implementação inicial.

### Restrição de logs na implementação operacional

Decisão do usuário: manter os printf/fflush para depuração durante a
implementação das primitivas. Preservar os formatos usados pelos verificadores;
qualquer mudança necessária exige atualização conjunta dos testes. Nenhum printf/fflush ou outra E/S de diagnóstico pode ocorrer sob mutex de
bucket, inclusive em callbacks de visita e enquanto count mantém vários locks.
Se preservados, capturar somente dados necessários e seguros durante a seção
crítica e imprimir após desbloquear. Não desreferenciar entradas já liberadas
para imprimir nem acessar metadados mutáveis sem proteção. A ordem das mensagens
entre threads não representa necessariamente a ordem das operações na hash.
Logs da ponte também respeitam a restrição, inclusive o mutex global de
inicialização. A ponte aloca/inicializa/publica com helper sem E/S sob esse mutex
e imprime o mesmo registro create depois de desbloquear. Falhas de tabela ou
buckets não publicam estado parcial; seus diagnósticos fatais também ocorrem
após unlock. Em concorrência, um registro acquire de outra thread pode aparecer
antes do log create já publicado; a ordem textual não comprova ordem causal.

Alterações de mensagens exigem atualização conjunta de testes e documentação.
`transaction_test/verificar.py` conta mensagens newtm_hash_* e verifica sua
posição no exemplo aninhado; suas expectativas atuais não podem ser mantidas
sem revisão se o rastreamento mudar. A validação operacional deve verificar
resultados, referências e callbacks, não apenas mensagens. Os logs próprios dos
stubs de acesso/ciclo de vida têm contrato separado e não são removidos como
consequência desta decisão sobre a hash.

### Consulta com aquisição de referência

```c
enum newtm_hash_status newtm_hash_lookup_acquire (
  struct newtm_hash_table *table, const void *address,
  struct newtm_hash_entry **out);
```

Consulta sem criar entrada. Se encontrada, incrementa a contagem sob o mutex do bucket;
se ausente, retorna `NEWTM_HASH_NOT_FOUND`. A referência adquirida impede remoção
concorrente até a liberação. Não oferecer um `find` que devolva um ponteiro
sem proteção de vida útil. Uma referência temporária de consulta também precisa
ser liberada, mas não representa um participante semântico da transação.

### Liberação e remoção

```c
void newtm_hash_release (struct newtm_hash_table *table,
                         struct newtm_hash_entry *entry);
```

Decrementa a contagem sob o mutex do bucket e remove a entrada quando ela chega a
zero. A liberação da memória e dos metadados segue o contrato do destrutor
abaixo. O chamador deve possuir exatamente a referência que está liberando e não
pode usar o ponteiro depois da chamada sem outra referência válida. Entrada de
outra tabela, dupla liberação e uso depois de liberar são erros de programação;
verificações de depuração podem diagnosticá-los, sem prometer validar ponteiros
já liberados. Não expor remoção forçada de uma entrada ainda referenciada.

Cada sucesso de acquire ou lookup_acquire entrega uma referência que exige
exatamente uma release; copiar o ponteiro não incrementa o contador e falhas não
entregam referências a liberar. A entrada deve pertencer à tabela, continuar
encadeada e ter contador positivo no início válido da liberação.

O chamador deve retirar previamente as participações/reservas sustentadas pela
referência consumida, sem retirar as dos outros participantes com referências
independentes. Na última referência, nenhuma participação/reserva pode permanecer
dependente dessa entrada. A referência preserva a entrada, não o objeto do programa.

TABLE deve permanecer viva até o retorno completo, inclusive durante o callback
e free executados depois de desbloquear. Destroy precisa aguardar essa operação
inteira, e não apenas a retirada da lista. Ausência de entradas ou count zero
não demonstram que a limpeza terminou.

Um decremento positivo garante somente que esta chamada não destrói a entrada.
Outra thread pode consumir a última referência e liberá-la antes do retorno.
O chamador só pode acessar a entrada depois de release se possuir outra
referência retida independentemente. Se o decremento produzir zero, esta chamada
conclui a retirada, a destruição dos metadados não nulos e a liberação da entrada
antes de retornar, sem mutex de bucket adquirido.

Uma nova entrada para o mesmo endereço pode existir durante a destruição dos
metadados da geração retirada. O destrutor atua apenas nos recursos dessa
geração, retorna normalmente e não reentra na tabela, acessa a entrada retirada,
altera o objeto do programa ou interfere em outra geração. Exceção/longjmp
violariam a obrigação de concluir a liberação. Não existe rollback do callback.

NULL, tabela errada, contador zero, dupla liberação ou ponteiros pendentes são
violações de contrato, sem resultado recuperável ou diagnóstico garantido.


### Consulta aos dados imutáveis

```c
const void *newtm_hash_entry_address (const struct newtm_hash_entry *entry);
size_t newtm_hash_entry_size (const struct newtm_hash_entry *entry);
```

Devolvem chave e tamanho, imutáveis durante a vida da entrada. Exigem uma
referência válida mantida pelo chamador; não acessam o objeto do programa.

### Acesso aos metadados da política

```c
typedef void (*newtm_hash_visit_fn) (const void *address, size_t size,
                                     void **policy_data, void *argument);
void newtm_hash_entry_visit (struct newtm_hash_table *table,
                             struct newtm_hash_entry *entry,
                             newtm_hash_visit_fn visit, void *argument);
```

Executa uma operação curta sobre os metadados, sob o mutex do bucket da entrada, exigindo
uma referência válida. Permite à política consultar ou atualizar seu próprio
estado sem expor a estrutura interna. O callback não pode reentrar na tabela,
esperar por outra transação, executar código do usuário ou adquirir locks que
produzam inversão de ordem. Não executar o corpo transaction sob esse mutex.
Um ponteiro copiado para fora do callback não permanece protegido pelo mutex.

A propriedade de `policy_data` é da entrada após sua instalação pelo callback.
`newtm_hash_create` recebe um destrutor imutável `destroy_policy`, que pode ser
nulo apenas se os metadados permanecerem nulos. Na última liberação, retirar a
entrada do bucket sob seu mutex, decrementar o número de entradas desse
bucket, soltar o mutex, chamar o destrutor se os metadados
forem não nulos e finalmente liberar a entrada. O destrutor não pode acessar a
entrada retirada, reentrar na tabela nem depender de sua presença nela. Uma
nova entrada com a mesma chave pode existir durante essa limpeza; o destrutor
libera somente recursos da geração retirada, nunca altera o objeto registrado
ou metadados de outra geração.

Se `visit` substituir um ponteiro não nulo, o callback é responsável por liberar
ou transferir corretamente os metadados anteriores, respeitando as restrições de
execução sob o mutex. Preferir manter o mesmo objeto de metadados durante a vida
da entrada. Não permitir que o último participante semântico destrua metadados
ainda consultados por referências temporárias; a última referência física é que
dispara o destrutor.

### Estatística mínima

```c
size_t newtm_hash_count (struct newtm_hash_table *table);
```

Obtém o número total de entradas, não a soma de referências. Para fornecer um
retrato consistente, adquire todos os mutexes em ordem crescente de índice,
soma os contadores dos buckets e libera os mutexes em ordem inversa. Se a soma
exceder `SIZE_MAX`, devolve `SIZE_MAX` como contagem saturada; esse comportamento
deve estar documentado no cabeçalho. O incremento do contador de cada bucket
também deve rejeitar overflow antes da inserção. Não adicionar um contador global protegido por mutex no caminho
normal. A contagem é destinada a diagnóstico, pode bloquear todos os buckets
e não deve ser chamada em cada acesso transacional. Seu resultado pode mudar
assim que os locks forem liberados. Não acrescentar iteração pública ou redimensionamento sem
necessidade demonstrada.

## Pré e pós-condições consolidadas da hash

As nove primitivas já executam estes contratos, incluindo contagem consistente
com soma saturada. Isso não implementa reservas nem semântica transacional.
Toda pós-condição de preservação significa ausência de alteração causada pela
chamada; outras threads podem continuar alterando a tabela. Nenhum retorno
normal deixa mutex adquirido pelo chamador. Endereços de objetos são chaves:
a hash nunca lê/escreve seus valores nem adquire reservas transacionais.

Para funções com saída `out`, exigir armazenamento gravável e exclusivo durante
a chamada, sem sobreposição com tabela, entrada ou metadados. O conteúdo anterior
de *out não é liberado automaticamente; o chamador deve preservar referências
que já possuía. Uma falha com out válido sempre escreve NULL. Ponteiros não nulos
pendentes não são detectáveis com segurança: não prometer validá-los.

| Primitiva | Pré-condições para sucesso | Pós-condições em sucesso | Falha/violação |
| --- | --- | --- | --- |
| `newtm_hash_create` | out válido; capacidade positiva representável; destrutor válido ou NULL sob contrato de metadados sempre nulos. | OK; *out é tabela nova, vazia, com capacidade imutável e todos os mutexes inicializados; chamador assume a propriedade da tabela. | INVALID para out nulo/capacidade zero; OVERFLOW antes de alocar; *out=NULL quando possível; nenhuma tabela parcialmente publicada. Falta de memória é fatal. |
| `newtm_hash_destroy` | Tabela válida; novas operações impedidas e operações/visitas/destruidores em andamento concluídos, com exclusão mantida até retornar. Referências ociosas são permitidas e causam BUSY; count zero não estabelece quiescência. | OK somente sem entradas; todos os recursos estruturais liberados; table e referências ao seu armazenamento tornam-se inválidos. | INVALID para NULL; BUSY preserva tabela e referências, sem destruição parcial; uso pode ser retomado após encerrar a exclusão externa. Destruição concorrente é violação, não protegida pelos mutexes. |
| `newtm_hash_acquire` | Tabela válida; chave não nula; size positivo; out válido; nenhuma destruição concorrente. | OK; *out mantém exatamente uma nova referência do chamador; entrada nova tem chave/tamanho imutáveis e metadados NULL, ou a mesma geração existente é reutilizada. | INVALID, SIZE_MISMATCH ou OVERFLOW conforme o argumento/contador; sem inserção ou incremento parcial; referências anteriores preservadas. Alocação fatal não finge retorno recuperável. |
| `newtm_hash_lookup_acquire` | Tabela válida; chave não nula; out válido; nenhuma destruição concorrente. | OK; uma nova referência a entrada existente com a chave exata; não insere nem muda tamanho/metadados. | NOT_FOUND se ausente; INVALID para nulos; OVERFLOW sem incrementar. *out=NULL; nenhuma referência adquirida na falha. |
| `newtm_hash_release` | Tabela/entrada vivas e correspondentes; entrada encadeada, contador positivo e referência do chamador obtida por aquisição bem-sucedida; participações/reservas sustentadas por ela já retiradas. Tabela viva até retorno completo, inclusive callback/free. | Consome exatamente uma referência. Se zero, desliga sob lock e conclui destrutor/metadados/entrada fora dele antes de retornar. Se positiva, não destrói nesta chamada, mas outra thread pode destruir antes do retorno; acesso posterior exige outra referência independente. Nenhum mutex retido. | void; NULL, tabela errada, contador zero, dupla liberação, ponteiro pendente ou reserva ainda sustentada são violações sem resultado recuperável ou diagnóstico garantido. Count zero não autoriza destroy durante a limpeza. |
| `newtm_hash_entry_address` | Entrada válida e referência retida pelo chamador durante a consulta. | Retorna a chave imutável; não adquire/consome referência nem estende a vida do objeto do programa. | Sem valor sentinela de erro no contrato funcional; argumento inválido é violação. |
| `newtm_hash_entry_size` | Entrada válida e referência retida pelo chamador durante a consulta. | Retorna tamanho imutável positivo; não altera estado ou referências. | Sem retorno de erro; zero não é sentinela funcional prevista. |
| `newtm_hash_entry_visit` | Tabela/entrada correspondentes, referência retida, callback não nulo, argument válido para esse callback; fora de callback/destrutor da mesma tabela; cumprir restrições de espera, locks, retorno normal e propriedade descritas abaixo. | Callback chamado uma vez sob mutex do bucket; alterações dos metadados tornam-se visíveis pela sincronização; referência do chamador preservada e lock liberado antes de retornar. Slot não escapa; ponteiros copiados não mantêm proteção e exigem protocolo próprio de vida útil/sincronização para uso posterior. | Sem estado estrutural de erro; resultado da política via argument. Não há rollback automático das alterações do callback. Violação de pré-condição não é conflito transacional. |
| `newtm_hash_count` | Tabela válida, sem destruição concorrente; chamada fora de visit/destrutor e sem mutex de bucket já retido. | Número de entradas em uma fotografia consistente, saturado em SIZE_MAX; nenhum contador/referência alterado; todos os locks liberados. | Sem retorno de erro; NULL é violação. Resultado pode mudar após soltar os locks; zero não exclui novas operações, usuários da tabela ou destruidores de entradas já retiradas e não autoriza destroy. |

### Contratos dos callbacks operacionais

`newtm_hash_visit_fn`: pré-condições — chave/tamanho válidos, slot policy_data
pertencente a entrada retida, mutex do bucket já adquirido pela hash, argument
sob contrato do chamador. Pós-condições — slot continua NULL ou contém metadados
válidos de propriedade da entrada; metadados substituídos foram corretamente
liberados/transferidos; callback retorna normalmente sem liberar o mutex,
consumir referência ou guardar o endereço do slot para uso posterior. Não
reentrar na tabela, esperar por outra transação/reservas, adquirir locks com
inversão de ordem, lançar exceção, sair por longjmp ou executar código do
usuário. Ponteiros copiados não conservam a proteção do mutex após o retorno;
seu uso posterior exige protocolo explícito de vida útil e sincronização,
inclusive contra substituição dos metadados durante outra visita. Se sinalizar
CONFLICT/INVALID via argument, manter metadados consistentes;
o callback não executa descarte integral da tentativa.

`newtm_hash_policy_destroy_fn`: pré-condições — metadados não nulos da geração
retirada, última referência física consumida, participações/reservas já retiradas
e nenhum mutex de bucket retido pelo chamador. Pós-condições — recursos dessa
geração liberados exatamente uma vez, sem alterar o objeto do programa ou uma
nova entrada de mesma chave. Não reentrar na tabela, acessar entrada retirada,
lançar exceção, sair por longjmp nem depender de quiescência de entradas de
outra geração. Retornar normalmente; a tabela permanece viva até terminar
callback e free da release. Não há retorno recuperável nem rollback do destrutor.

## Limites de concorrência e de substituição de política

Contagem de referências garante vida útil, não indica quantas transações estão
ativas. `policy_data` deve manter separadamente as participações e identidades.
Na saída, a camada transacional retira sua participação sob `entry_visit` antes
de liberar sua referência. A última referência só pode ser liberada após retirar todas as participações e
reservas que ela sustenta. O destrutor dos metadados não substitui a liberação
do protocolo transacional. Uma consulta temporária pode manter a entrada na
tabela depois de terminar a última participação: presença na hash não significa
que há conflito ou transação ativa. Não expor a contagem física como contador
de participantes.

Busca, inserção, aquisição/liberação de referência e acesso aos metadados usam
somente o mutex do bucket correspondente. A contagem de referências não precisa
ser atômica, pois todas as suas alterações usam esse mesmo mutex. Endereço e
tamanho continuam consultáveis sem lock enquanto o chamador mantém uma referência.
Manter uma referência não mantém o mutex adquirido. Guardar na entrada um índice
de bucket imutável ou recalculá-lo da chave enquanto a referência está válida.

Inicializar todos os buckets antes de publicar a tabela. A destruição exige
quiescência de todos os usuários, incluindo callbacks e destruidores em andamento;
não basta observar zero entradas enquanto ainda existem operações concorrentes.
Na destruição, verificar primeiro todos os buckets: se algum estiver ocupado,
retornar BUSY sem destruir parcialmente a tabela. Fora da operação de contagem,
nenhuma primitiva deve adquirir vários mutexes de buckets. Callbacks não podem
chamar `newtm_hash_count`, pois ele tenta adquirir novamente o mutex já mantido.

As operações são atômicas por entrada, não por conjunto de objetos. Registrar
`a` e depois `b` não reserva ambos atomicamente. Políticas que precisem de
validação ou aquisição conjunta terão de estender a interface ou usar protocolo
próprio, com ordem de locks definida. Não prometer que qualquer política futura
caberá sem alterações nestas primitivas. A troca de política significa selecionar
outra implementação antes de começar a execução transacional, não trocar os
metadados ou destrutores enquanto há transações ativas.

A referência preserva o registro, não a vida útil do objeto do programa. O
chamador deve garantir que o objeto permanece vivo até terminar a participação.
Para o protótipo, não admitir liberação/reutilização do armazenamento enquanto
existirem referências ao registro daquela instância. Suporte a esse caso exigirá
identidade por geração e protocolo de recuperação adicional.

A exclusão dos acessos externos do protocolo não torna seguros acessos C/C++
concorrentes conflitantes sem sincronização. A tabela não modifica as regras de
corrida de dados da linguagem e não deve prometer resultados definidos nesses
programas. Testes de acesso externo devem evitar corridas de dados.

## Integração com o runtime e o compilador

O compilador gera chamadas; ele não chama `newtm_hash_acquire` para registrar
endereços durante a compilação. Somente o programa em execução conhece os
endereços efetivos de objetos locais e das cópias privadas OpenMP.

### Runtime

A instância global atual e seu controle proprietário estão em newtm-hash.c,
inicializados/publicados sob mutex. Uma eventual migração para
newtm-transaction.c deve preservar admissão, leases e coordenador de encerramento. Examinar os
mecanismos existentes em `libgomp/env.c`, `libgomp/libgomp.h` e `critical.c`;
respeitar targets onde `GOMP_MUTEX_INIT_0` é falso. A configuração da capacidade
fica na inicialização, sem alterar uma tabela já em uso.

Interface provisória atual, separada das primitivas hash:

```c
void GOMP_transaction_begin (void);
void GOMP_transaction_register (void *address, size_t size);
void GOMP_transaction_end (void);
```

O contexto é obtido internamente da thread. `begin` cria o contexto na entrada
externa e incrementa sua profundidade; `register` deduplica objetos; `end`
libera os registros apenas na saída externa. Esses serviços só fazem
rastreamento, sem commit, reservas ou armazenamento especulativo.

Uma interface futura com contexto explícito é apenas uma proposta; não alterar
as assinaturas atuais como consequência da implementação estrutural da hash.
A política funcional exige serviços adicionais de acesso, propagação de falha
e reinício, com ABI e tratamento de exceções definidos antes da integração.

Somente os serviços `GOMP_*` chamados pelo código gerado devem integrar a ABI,
com declarações em `libgomp/libgomp_g.h` e exportação versionada coerente em
`libgomp/libgomp.map`. As primitivas `newtm_hash_*` permanecem internas. Não
acrescentar símbolos a uma versão publicada arbitrariamente. A integração deve
preservar a tabela global única entre unidades de tradução que usam o mesmo
runtime; não gerar uma tabela estática em cada arquivo compilado.

### Pré e pós-condições da ponte atual de rastreamento

Os três serviços atuais retornam void. As condições abaixo descrevem seu uso
válido; não presumir que toda violação já seja detectada. Falhas explicitamente
detectadas usam gomp_fatal; não existe retorno booleano de sucesso/commit.

| Serviço | Pré-condições | Pós-condições no retorno normal |
| --- | --- | --- |
| `GOMP_transaction_begin()` | Runtime/thread utilizáveis; execução síncrona sem troca de tarefa; contexto anterior ausente ou válido; profundidade incrementável. | Criação global solicitada uma vez sob lock de inicialização; contexto da thread criado ou reutilizado; profundidade anterior +1, ou 1 se externo; registros anteriores preservados. Nenhum objeto lido/reservado. |
| `GOMP_transaction_register(address, size)` | Entrada begin ativa; address não nulo, size positivo e objeto vivo, tipo/endereço fornecidos corretamente pelo compilador; duplicatas com mesmo tamanho. | Objeto novo acrescentado ao contexto com uma chamada acquire; duplicata não acrescenta nem adquire de novo. Profundidade não muda; nenhum valor do objeto lido/escrito. |
| `GOMP_transaction_end()` | Entrada begin correspondente ainda não encerrada na mesma execução; contexto presente e profundidade positiva. | Profundidade reduzida em um. Se ainda positiva, registros preservados. Se zero, uma release por registro distinto, registros/contexto liberados e ponteiro da thread limpo. Memória do programa não alterada, nenhum commit/rollback realizado. |

A ponte exige tabela completamente inicializada e handle não nulo antes de
publicar o estado sob lock global, usando o mesmo helper não diagnóstico do
wrapper create. Seu log create e diagnósticos fatais ficam depois de unlock.
Na aquisição de entradas, exige OK e handle não nulo antes de publicar registro.
Register rejeita contexto ausente/inativo, endereço nulo e size zero. Falhas de
criação/aquisição, overflow de profundidade, tamanho incompatível em duplicata
e saída sem contexto são fatais, sem retry. Um nó temporário de registro é
liberado antes do diagnóstico de falha de aquisição. O estado não é publicado
parcialmente. As entradas são reais, mas não concedem reservas transacionais.

`newtm_trace_initialize`: chamado uma vez na inicialização da biblioteca, antes
do uso concorrente; ao retornar, o lock global está utilizável, inclusive em
targets que exigem inicialização explícita. Não cria a tabela nem contexto.
`newtm_trace_finalize`: solicita owner_close sem esperar drenagem. Fecha a
admissão; se há usuários/coordenador/entradas residuais, conserva tabela/controle
em CLOSING sem diagnóstico fatal ou remoção forçada. Sem usuários, um único
coordenador chama destroy fora do mutex e publica CLOSED em OK. O mutex estático
permanece vivo para chamadas tardias. Não substitui a exclusão de código exigida
por dlclose; ver o protocolo de admissão acima.

O registro atual exige contexto ativo/proprietário, endereço e tamanho
válidos e vida útil até a saída externa; a validação completa dos tipos
transacionais continua futura. Sucesso mantém uma referência real por objeto
distinto, sem adquirir reserva nem copiar seu valor. Falha não publica participação parcial nem muda profundidade; tamanho
incompatível é erro permanente, não conflito repetível. Definir sua sinalização
na futura ABI, sem inferir estado de sucesso do void atual.

### Política de aninhamento: flat nesting

Uma transação lógica possui identidade única, conjunto acumulado de objetos,
profundidade e estado da política. A primeira entrada estabelece profundidade
um; cada entrada interna retorna o mesmo contexto e incrementa a profundidade,
com verificação de overflow. Cada entrada bem-sucedida exige exatamente uma
saída correspondente, inclusive quando a região interna está em uma função
chamada pela região externa. Não inferir aninhamento somente pela estrutura
léxica do GIMPLE: o runtime precisa reconhecer o contexto ativo da execução.

Uma lista interna acrescenta objetos ao conjunto da transação externa. Sair da
região interna não confirma alterações, não encerra participações e não libera
os objetos acrescentados por ela. Esses objetos permanecem associados ao
contexto até a saída externa. Registros repetidos são idempotentes nesse contexto;
threads ou execuções independentes continuam tendo contextos distintos, ainda
que registrem o mesmo endereço. Não detectar conflito da transação consigo mesma.

Nesta política com abort e repetição, um conflito interno afeta a
transação lógica inteira; não há rollback nem repetição independente do bloco
interno. Essa semântica exige implementação explícita dos mecanismos de abort. A saída
externa é o único ponto elegível à confirmação; abort limpa antecipadamente
a tentativa inteira, sem executar saídas normais de níveis já desfeitos. Os caminhos
de exceção devem equilibrar níveis e garantir a limpeza externa sem interpretar
uma exceção automaticamente como confirmação.

A identidade ativa deve acompanhar a execução correta. Armazenamento local à
thread pode atender ao protótipo restrito a execução síncrona sem suspensão,
mas não resolve automaticamente tasks que suspendem ou migram. Não compartilhar
o contexto com outra thread por ela ter sido criada dentro da região. Antes de
habilitar tasking nesse contrato, definir a associação ao contexto de task e as
regras de herança; até lá, declarar essa integração não suportada e diagnosticar
os casos identificáveis, em vez de prometer suporte completo a tasks.

A retenção até a saída externa exige que todos os objetos registrados, inclusive
os introduzidos em regiões internas, permaneçam vivos até esse ponto. Para o
protótipo, objetos cuja vida termine antes do contexto externo não são suportados:
exemplo, uma variável de um sub-bloco listada por uma região interna quando o
sub-bloco termina antes da saída da região externa. Diagnosticar
casos verificáveis e documentar a pré-condição para os demais; não consultar
armazenamento expirado durante confirmação ou rollback. Uma política futura de
retirada antecipada exigirá especificação adicional, sem contrariar flat nesting.

### Front-ends e representação

1. Em `gcc/c/c-parser.cc:newtm_c_parser_transaction` e
   `gcc/cp/parser.cc:newtm_cp_parser_transaction`, resolver e preservar as
   declarações dos nomes no ponto da diretiva, antes de analisar seu corpo.
   Atualmente a lista já é resolvida e preservada na árvore. Definir as categorias
   de entidades admitidas em coerência com a política de tipos; variáveis e
   parâmetros são candidatos, funções e typedefs não são objetos transacionáveis.
   Casos especiais como membros e structured bindings exigem tratamento explícito.
2. Em `gcc/c-family/c-omp.cc:newtm_finish_transaction`, sua declaração em
   `c-common.h`, `gcc/tree.def` e `gcc/tree.h`, manter a representação
   persistente da lista e melhorar suas localizações quando necessário. Não guardar strings para resolver
   nomes posteriormente e não usar ponteiros para tokens temporários.
3. Em `gcc/cp/pt.cc`, separar o caso `OMP_TRANSACTION` quando necessário para
   substituir a lista em templates e validar os casos dependentes. Auditar
   módulos, walkers, cópia e dumps segundo o guia geral.
4. Em `gcc/gimplify.cc`, no caso `OMP_TRANSACTION`, planejar a identidade dos
   objetos necessária à instrumentação de acessos. Hoje a lista vira chamadas
   comuns durante a gimplificação. Avaliar `gcc/gimple.def`, `gimple.h`, `gimple.cc` e
   `newtm_build_transaction`: o layout atual `GSS_OMP` armazena apenas o corpo.
   Escolher layout que preserve os novos dados e atualizar consumidores.

### Lowering e chamadas geradas

O ponto de lowering é `gcc/omp-low.cc:newtm_lower_transaction`: ele faz
`lower_omp` do corpo usando `ctx->outer` e elimina o marcador. As chamadas de
registro já são geradas na gimplificação. A integração funcional deve preservar
essa ordem conceitual, revendo a ABI provisória conforme necessário:

```text
GOMP_transaction_begin()
para cada objeto listado:
    GOMP_transaction_register(endereço efetivo, tamanho em bytes)
corpo transformado
GOMP_transaction_end()
```

Essas três chamadas estabelecem somente registro e vida útil. Não constituem
uma ABI completa e independente da política para observar leituras/escritas.
Antes de prometer detecção de conflitos, definir serviços de acesso, identidade
da transação e ordenação com o acesso real à memória. Um simples aviso após
uma escrita pode ser insuficiente para a política escolhida.

O registro deve preceder o primeiro acesso transacional do corpo. Calcular o
endereço da instância correta: uma variável privada/reduction externa não pode
ser substituída pelo endereço de sua declaração original compartilhada.
Referências C++ devem identificar o objeto referido, não um armazenamento
interno de referência. Não interpretar automaticamente um ponteiro listado
como pedido para registrar o objeto apontado. Tipos incompletos, VLA e tamanhos
dependentes exigem tratamento explícito; não assumir `TYPE_SIZE_UNIT` constante.

Em `scan_omp_1_stmt`, tratar os novos operandos e remapeamentos preservando a
transparência atual do contexto externo. Examinar `lower_omp_critical` como
exemplo de geração de chamadas, sem copiar sua semântica de lock ou de exceções.
Os builtins dos serviços devem ser declarados em `gcc/omp-builtins.def`, com
assinaturas em `gcc/builtin-types.def` quando necessário. Não atribuir atributos
`pure`, `const` ou outros que permitam eliminar/reordenar sincronização.

A implementação da política acordada deve instrumentar leituras e escritas
antes da eliminação do marcador. Hoje a lista não está no marcador GIMPLE;
preservar explicitamente os dados necessários ou instrumentar antes. Em
`newtm_lower_transaction`, isso deve ocorrer antes de
`gsi_replace_with_seq`. Seguir o agente de acesso para preparar as chamadas
antes de `lower_omp(..., ctx->outer)` quando precisarem do remapeamento OpenMP,
e preservar os dados necessários antes da varredura quando afetarem captura.
Instrumentar cada acesso uma única vez, inclusive no aninhamento. Registro de
endereços por si só não detecta conflitos.
Aplicar os contratos de aliases e funções definidos acima; não afirmar cobertura
completa apenas pela inspeção de atribuições diretas. Auditar operações que
combinam leitura/escrita, builtins de cópia, acessos agregados e otimizações que
possam remover, mover ou desviar acessos do protocolo.

Garantir liberação nas saídas legais previstas pelo contrato da diretiva,
incluindo a interação com exceções e cleanups C++, sem inventar regras de
propagação. A atual proibição de saltos de blocos estruturados continua válida.
Não é necessário manter o marcador até `omp-expand.cc` se ele continuar sendo
eliminado integralmente em omplower. Auditar LTO e offload quando o escopo
atingir essas camadas; a primeira biblioteca de host não comprova suporte de
dispositivo ou compartilhamento entre host e dispositivo.

## Validação e entrega

Para a biblioteca, verificar colisões, chaves distintas, aquisição repetida,
consulta ausente, tamanho incompatível, liberação parcial/final, contagens,
capacidade inválida e overflow. Verificar o encerramento por falta de memória
conforme contrato, em processo separado se for usado um teste de falha injetada. Testar
concorrência entre threads adquirindo e liberando a mesma chave, incluindo uma
consulta que mantém a entrada viva após a saída de outro participante. Verificar
que operações em buckets distintos podem progredir em paralelo e que colisões
no mesmo bucket continuam corretas. Cobrir contagem concorrente e retorno BUSY
sem destruição parcial. Não
usar temporizações como prova de sincronização. Verificar limpeza de metadados
quando esse suporte existir.

Usar os diretórios e convenções existentes em `libgomp/testsuite`, evitando
exportar API interna apenas para viabilizar testes. Definir um mecanismo de
teste interno apropriado ao build ou testar por serviços públicos quando estes
existirem. Não prometer executar um teste interno que não consegue ser ligado.

Na integração, acrescentar testes C/C++ em
`gcc/testsuite/c-c++-common/gomp`, `gcc/testsuite/g++.dg/gomp` e testes de
execução na libgomp. Cobrir registro antes do corpo e liberação depois dele,
repetições, flat nesting, locais/globais/parâmetros, templates,
reutilização da mesma identidade nos níveis internos, retenção dos objetos
acrescentados internamente até a saída externa e limpeza exatamente uma vez.
Cobrir aninhamento por chamada de função, tamanhos incompatíveis em duplicatas,
limites de profundidade e restrições de vida útil e tasking. Verificar os tipos
aceitos e rejeitados, ponteiro versus objeto apontado, aliases resolvidos usando
a cópia privada, chamadas transacionais e chamadas sem suporte, rejeição de
escritas externas não listadas e recriação de temporários em cada tentativa. Testar leitura/leitura concorrente,
leitura/escrita e escrita/escrita conflitantes, conversão de reserva, descarte no
abort e ausência de publicação parcial. Verificar reservas adquiridas antes da
leitura inicial da cópia, identidade dos ponteiros e ordem dos cleanups. Testar
backoff com parâmetros controlados sem exigir tempo específico ou garantir
conclusão probabilística como asserção de correção.
Cobrir também
objetos privados externos e acesso fora da região sem instrumentação. Conferir
os dumps gimple/omplower e o runtime efetivamente utilizado.

Construir em `/usr/src/gcc-build`, incluindo o runtime modificado; `all-gcc`
sozinho não basta. Ler `.sum` e `.log` e relatar comandos e resultados reais.
As mensagens de depuração existentes nos parsers e em `omp-low.cc` já causam
falhas de saída adicional; distinguir essas falhas de regressões, sem modificar
expectativas para escondê-las. Não afirmar que os resultados históricos dos
outros documentos foram reproduzidos.

A entrega inicial da biblioteca estará completa quando suas primitivas,
propriedade de memória e sincronização estiverem implementadas e verificadas.
Isso não equivale a implementar a semântica transacional. Relatar claramente
quais pontos da integração acima continuam futuros e atualizar o manual se o
comportamento visível da diretiva mudar.

## Decisões ainda necessárias para integrar a diretiva

As políticas de tipos, aliases, funções chamadas e restauração estão definidas
nas seções anteriores. Antes de integrar o compilador, fechar os mecanismos de
reconhecimento/geração das versões transacionais escolhidas, sua ABI de
contexto/status/resultado e diagnóstico de chamadas não preparadas, além de
reinício C/C++, diagnósticos da rejeição inicial de exceções/cleanups e
representação privada. O protocolo para suporte futuro de cleanups em tentativa
condenada permanece fora do subconjunto inicial. Leituras externas, tipos iniciais e identidade/sobreposição já têm
regras definidas acima; falta implementar sua aplicação e seus diagnósticos. A forma sem lista existe hoje, mas não autoriza inferir
implicitamente todos os objetos acessados nem escrever objetos externos não
listados. Aplicar as limitações sobre sobreposição, tasks e efeitos externos e
diagnosticar os casos identificáveis. Esses mecanismos devem ser definidos antes
de implementar a ABI definitiva.

Escolher libgomp como local da biblioteca e remoção automática são propostas
técnicas deste documento. Não apresentá-las como escolhas já aprovadas
individualmente pelo usuário.

## Estado atual da biblioteca hash

`libgomp/newtm-hash.c` e `.h` definem tipos opacos, estruturas privadas e nove
primitivas internas. create/destroy/acquire/lookup_acquire/release estão
implementadas, incluindo mistura de endereços, colisões por encadeamento,
referências e retirada na última liberação. Address/size já retornam os campos
imutáveis sem lock com referência retida; visit executa o callback uma vez sob
mutex do bucket e libera-o ao retornar. Count já bloqueia todos os buckets em ordem crescente, soma com saturação
e desbloqueia em ordem inversa; zero não comprova quiescência. A ponte usa referências reais, verifica
criação/aquisição/destruição e preserva flat nesting. Todas mantêm printf/fflush
fora dos mutexes de buckets. Reserva, dados privados e STM continuam ausentes.

A ponte já verifica criação e aquisição antes de publicar estado ou registros,
não aceita handles nulos como sucesso, diagnostica falhas permanentes sem retry
e verifica destruição. Preservar esse contrato nas próximas etapas. lookup não
tem chamador de produção: um contexto que já possui referência pode reutilizá-la.

O fonte e o cabeçalho estão incluídos em Makefile.am/Makefile.in. Os símbolos
newtm_hash_* continuam internos; somente a ponte é exportada. Build, testes e limites de cada execução estão em
OPENMP_TRANSACTION_IMPLEMENTATION.md. Há validação funcional estrutural da hash;
não a confundir com testes de reservas/STM ou com a auditoria histórica.

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

## Relação com os esqueletos de ciclo de vida

newtm_begin permanece esqueleto com tabela emprestada e slot de contexto; ela deve
sobreviver aos contextos. newtm_register reterá uma referência por objeto distinto.
newtm_end/discard retirarão reservas antes de liberar referências. Essa propriedade
é contrato futuro: os seis stubs não chamam a hash nem possuem contexto funcional.
A ponte GOMP permanece em newtm-hash.c com tabela e referências reais,
sem reservas ou acesso a valores do programa.
NEWTM_LIFECYCLE_* não é NEWTM_HASH_*; falha estrutural não vira conflito por cast.
