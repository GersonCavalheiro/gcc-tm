## Checkpoint contexto_pronto_para_registro — 2026-10-28

Criado teste agregador com cabeçalho de cobertura e limites, alvo
check-checkpoint-contexto no Makefile e documentação de execução. Reúne oito
alvos existentes, recompila testes contra o build selecionado, guarda logs e
relatório JSON e interrompe em falha/timeout, encerrando o grupo de processos.
Não implementa novas operações STM ou testes para as lacunas já identificadas.
Validação: os oito alvos passaram no contêiner gcc-tm com -O2 -Wall -Werror;
falha simulada confirmou parada imediata e relatório reprovado.

## Base do contexto implementada — 2026-10-28

Criado newtm-context.h com contexto, registros, intervalos e política aceitos;
acrescentados owning_slot e owner para consumo canônico e execução proprietária.
Begin agora recebe lease*, inicializa privadamente e transfere antes de publicar;
nesting preserva a lease. Fail registra INVALID dominante; discard desliga slot,
retira reservas sob bucket e libera buffers/intervalos/referências/contexto fora
dele, com owner_release por último. End só conclui saída interna; saída externa
condena com INVALID, pois publicação ainda não existe. Algoritmos documentados
nos cabeçalhos dos corpos e nos contratos. Mantidos printf/fflush fora dos locks.

Owner configura newtm_policy_destroy antes da publicação. Ponte trace mantém
sua lista legada e usa contexto real para entrada/limpeza, sem inferir kind ou
anunciar isolamento STM. Erros permanentes detectados pela ponte com contexto
válido drenam seus recursos antes de fatal. Registro/read/write/backoff e
publicação STM continuam pendentes; nenhuma ABI pública GOMP alterada.

Validação no contêiner: libgomp -Wall -Werror; check-lifecycle com
-O2 -Wall -Wextra -Werror; owner (11 processos), diagnósticos (4), exemplos
1/2/4 threads, aninhamento e carga 1:9/1:1/9:1 passaram. ASan/UBSan passaram com
fontes de contexto/hash instrumentadas e archive estático restante. Testes
privados com -Wextra -Werror encontraram avisos preexistentes em cabeçalhos GCC;
rodaram com -Wall -Werror, como a libgomp. Sem nova execução DejaGNU.

## Lease e destrutor: procedimentos aceitos — 2026-10-28

Aceita a transferência exclusiva da lease da ponte ao contexto externo,
preservada no aninhamento e liberada após toda a limpeza. Aceito o destrutor
interno configurado antes da publicação da tabela, metadados sob demanda com
candidato preparado fora do bucket e destruição na última referência fora do
mutex. Exigir leitores/escritor vazios; participação residual é erro interno
fatal, sem remoção forçada. O destrutor libera somente metadados compartilhados.

Contratos e especificação sincronizados; implementação ainda pendente. Nenhum
código executável alterado nesta atualização documental.

## Classificação no registro e aceitação do contexto — 2026-10-28

Aceita a estrutura inicial de OPENMP_TRANSACTION_CONTEXT_PROPOSAL.md, ainda por
implementar. Adicionado enum newtm_object_kind e quarto argumento kind em
newtm_register; comentários definem classificação canônica, arrays/agregados,
ponteiros, compatibilidade de repetição e INVALID para tags desconhecidas no
contrato funcional. Mantidos printf/fflush, com kind numérico no log. O stub
continua sem inspecionar argumentos/contexto, sempre INVALID. Nenhuma nova
cláusula, ABI pública GOMP ou chamada gerada pelo compilador foi introduzida.

Adaptados teste e verificador de lifecycle para ambas as classes e tag inválida.
No contêiner gcc-tm: libgomp recompilada com -Wall -Werror; check-lifecycle passou
com -O2 -Wall -Wextra -Werror, 26 chamadas, canários preservados e símbolos
internos presentes no archive e ausentes da API dinâmica.

## Organização do contexto escolhida — 2026-10-28

Escolhida lista de registros por tentativa, com registros estáveis e busca linear
inicial; aninhamentos usam o mesmo contexto. Mantidas as recomendações de
escalares simples, buffer de extensão completa sob demanda para agregados e
listas de intervalos válidos/modificados. Alocação completa não exige leitura ou
inicialização integral. Separar registros privados dos metadados compartilhados
da entrada hash. Campos concretos e algoritmos de intervalos ainda pendem.
Somente documentação; nenhuma implementação ou teste executável alterado.

## Fechamento semântico antes do layout do contexto — 2026-10-28

Registradas as decisões atuais: INVALID em contexto válido exige limpeza segura
seguida de diagnóstico fatal, sem retry, nova cláusula ou resultado na linguagem;
OOM permanece fatal. Excluir E/S, chamadas de sistema, locks do usuário e
malloc/free do programa; alocação futura exigirá rastreamento e descarte ou
liberação adiada. Temporários automáticos privados são recriados no retry e não
escapam. Registro explícito para todos os acessos preexistentes substitui a
permissão anterior de leituras externas comuns. O critical da carga é apoio ao
rastreamento e será reavaliado para testes de isolamento STM real.

Conjunto semântico fechado para definir layout do contexto. Atualização somente
documental; filtros, diagnósticos e protocolo funcional não foram implementados.
As notas anteriores preservam o histórico das decisões então vigentes.

## Decisão semântica: representação privada — 2026-10-28

Escalares usam representação simples. Agregados usam armazenamento privado com
faixas válidas/modificadas; modificadas são válidas. Acesso a um campo não exige
inicializar ou acessar todo o objeto. Leituras completam partes ausentes sob
reserva e preservam escritas da tentativa; commit publica somente modificadas,
descarte não publica. A unidade de conflito continua o objeto canônico.
Buffers permanecem internos: &x preserva o endereço lógico original.

Contratos e roteiro sincronizados. Layout das faixas e implementação funcional
continuam pendentes. Somente documentação; nenhum código executável alterado.

## Decisão semântica: exceções e cleanups do usuário — 2026-10-28

O usuário definiu rejeição inicial de exceções e cleanups do usuário, inclusive
nas funções preparadas. A limpeza interna do runtime permanece obrigatória,
incluindo destrutores internos de metadados, referências, buffers/contexto e
leases. Tentativas condenadas recusam novos acessos normais por read/write;
essas primitivas não podem ser relaxadas para executar destrutores do usuário.
Exceções nunca são CONFLICT, não autorizam retry e não provocam commit durante
desenrolamento. Finally de rastreamento não constitui confirmação STM.

Suporte futuro deverá definir a visão e o protocolo de acesso de destrutores
com tentativa condenada, ordem/vida útil de recursos, falhas no cleanup e
propagação da exceção original. Referências genéricas a desenrolamento suportado
nos agentes passam a designar essa extensão futura. Testes C++ anteriores de
rastreamento continuam históricos e não comprovam suporte STM a esses casos.

Sincronizados contratos e roteiros em HASH/ACCESS/SCOPE/ABORT, panorama,
inventário e guia de implementação. Somente documentação: nenhum diagnóstico
novo foi implementado no compilador, nenhum código executável mudou e não
houve build/testes nesta rodada.

## Decisão semântica: chamadas preparadas e status explícito — 2026-10-28

O usuário escolheu gerar versões transacionais internas das funções, recebendo
contexto e retornando status explícito, com resultado preenchido somente em
sucesso. O chamador verifica status antes de consumir resultado ou continuar;
falha segue ao epílogo e é propagada pela cadeia até o controle externo, sem
valores fictícios. A assinatura pública original permanece intacta. Contexto,
flat nesting e lease são preservados enquanto houver referências/cleanups;
funções internas não confirmam nem descartam a tentativa externa por conta própria.

Chamadas indiretas ou externas sem preparação ficam inicialmente restritas.
Pure/const não prova que a visão privada é observada e não autoriza usar versão
comum. Exceções/cleanups têm decisões próprias, sem conversão automática em
CONFLICT. A ABI das versões geradas, enum/traduções de status, nomes/linkage,
retornos por referência e sua geração ainda precisam de especificação técnica
e implementação. O pseudocódigo nos agentes é conceitual; nenhum símbolo GOMP,
clone, builtin ou transformação de chamadas foi criado nesta rodada documental.

Sincronizados HASH/ACCESS/SCOPE/ABORT, panorama, roteiro e inventário. Mantidas
as decisões anteriores de leituras externas comuns, tipos iniciais e identidade.
Não houve alteração de código executável, build ou testes nesta atualização.

## Decisões semânticas: leituras externas, tipos e identidade — 2026-10-28

Registradas decisões explícitas do usuário nos agentes HASH/ACCESS/SCOPE/ABORT,
no panorama, inventário e roteiro de implementação:

- Somente os objetos listados e já registrados participam do protocolo. Em
  transaction(a) { a += b; }, a é transacional e b é lido normalmente, sem
  registro, reserva, validação ou conflito STM. Retry pode observar outro b;
  regras normais de corrida de dados continuam aplicáveis. Escritas externas
  não listadas mantêm sua restrição separada.
- Escalares, arrays e agregados restritos de tamanho conhecido, com validação
  recursiva. Excluir volatile, atômicos, VLA, unions, bit-fields, membros flexíveis
  e casos sem contrato; proibir escritas const. Instâncias de classes ficam
  para futuro; em C++, struct também é tipo de classe, sem admissão automática
  por trivialidade.
- Identidade canônica conhecida permite vetor[i] dinâmico com limites em
  execução. Rejeitar ponteiros sem identidade, sem fallback comum, e registros
  distintos sobrepostos (inclusive entre contextos). Campos/elementos de agregado
  registrado usam sua entrada/offset; repetir mesmo objeto/mesma extensão não
  cria registro distinto. Ponteiro listado não registra a memória apontada.

O compilador deve classificar leituras comuns antes de selecionar newtm_read;
INVALID nessa primitiva não autoriza fallback. A hash atual continua exata e
não detecta intervalos/sobreposição. Aplicação dos filtros, mapeamento e seus
diagnósticos são tarefas futuras de integração, não mudanças executáveis desta
atualização. Decisões de chamadas/falhas, exceções/cleanups, representação privada
e efeitos externos permanecem abertas. Esta rodada alterou apenas documentação,
sem build ou testes de execução; não declara novas garantias no compilador.

## Carga parametrizada e atualização dos roteiros — 2026-10-28

Adicionados transaction_test/transaction_carga.c e verificar_carga.py. O programa
recebe THREADS ESCRITAS:LEITURAS REPETICOES; cada thread executa N regiões
transaction e floor(N*W/(W+R)) escritas lógicas, intercaladas por acumulador sem
overflow. Uma escrita é read-modify-write. Verifica valor/complemento, contagens
e equipe efetivamente lançada; rejeita pesos/somas fora de uint32_t, entradas
inválidas e total fora de unsigned long long. Critical interno protege os dados,
mas registro/liberação dos objetos podem concorrer. Não comprova STM funcional.

Makefile inclui check-carga em check, com defaults 4 threads/1000 repetições e
cenários 1:9,1:1,9:1; CARGA_THREADS/CARGA_REPETICOES são configuráveis. O verificador
usa checagens explícitas Python, timeout de 60 segundos por cenário e confere
uma acquire/release por transação e create/destroy únicos. Logs permanentes de
execução ficam em transaction_test/.build/transaction_carga-*.log (ignorados pelo
Git); arquivos .c/.py e regras do Makefile são os testes reproduzíveis.

Na entrega do teste passaram make check com -O2 -Wall -Wextra -Werror, valores
400/3600,2000/2000,3600/400 de escritas/leituras, oito combinações adicionais
(incluindo 0:1/1:0, pesos extremos e períodos incompletos), 19 entradas inválidas
e recusa explícita de equipe reduzida por OMP_THREAD_LIMIT. Evidências locais:
/tmp/newtm-carga-check.log, /tmp/newtm-carga-boundaries.log no contêiner, e
transaction_test/.build/carga-audit/check.py para os casos exploratórios.

Esta revisão dos agentes é documental: não reexecutou compilação/testes.
Roteiros agora separam hash/owner/carga entregues de contexto, reservas, acessos,
propagação de falha, publicação/descarte e retry pendentes. Auditoria DejaGNU
2026-10-21 e resultados das demais etapas abaixo permanecem históricos.

## Controle de admissão e encerramento da camada proprietária — 2026-10-28

Acrescentados três serviços internos ocultos em newtm-hash.c/.h, sem nova ABI
GOMP: owner_acquire, owner_release e owner_close, usando newtm_hash_owner_lease.
São serviços do proprietário, não novas primitivas da tabela. Estado
OPEN/CLOSING/CLOSED, contagem de participações, ponteiro e coordenador ficam sob
mutex global estático. O mutex permanece vivo depois da tabela para rejeitar
admissões tardias. Não há reinicialização automática após CLOSED.

Admissão e registro da participação são indivisíveis em relação ao fechamento.
A participação cobre inclusive o log create fora do mutex. GOMP begin externo
admite uma lease antes de expor tabela/contexto; aninhamentos reutilizam-na em
CLOSING. Register/end usam o ponteiro da lease. A saída externa libera todas as
entradas/metadados, limpa TLS, transfere a participação para armazenamento local,
libera contexto e só então devolve a lease. Consumidores diretos da tabela global
precisam de participação durante todo uso/referências; nunca destruir tabela
emprestada. Tabelas independentes preservam quiescência externa própria.

Owner_close fecha admissão e não espera usuários: retorna BUSY se ainda há
participações, coordenador em andamento ou entradas residuais, preservando
CLOSING/tabela. Um único coordenador chama destroy fora do mutex, somente após
zero participações, e publica CLOSED/ponteiro NULL após OK. Repetição CLOSED é
OK sem dupla liberação. BUSY não força remoção nem reabre admissão; coordenador
pode concluir/join de usuários e repetir. Não é lock-free: pode aguardar o mutex
curto de controle, mas não espera drenagem ou faz polling interno.

O finalizador automático apenas tenta owner_close: com usuários, não espera,
não chama fatal recursivamente nem destrói tabela/mutex em uso. Mantém recursos
para recuperação pelo processo; explicit close pode concluir após término dos
usuários se a execução continuar. O protocolo não protege código removido por
dlclose; exige impedir externamente qualquer execução atual/futura da biblioteca.
Os nove serviços de tabela mantêm seus printf fora dos locks e contratos locais.

Teste permanente: make -C transaction_test -B check-hash-owner. Onze casos em
processos separados, timeout de 15 segundos, configuração privada Linux x86_64:
fechamento sem uso; aninhamento e drenagem; coordenadores concorrentes com stdout
bloqueado fora do mutex; destrutor pendente; log inicial pendente já coberto por
participação; entradas residuais por injeção quiescente; corrida de admissão com
oito threads; erros e overflow restaurado; begin tardio fatal; saída real com
contexto ativo sem destruição; finalizador solicitado com contexto ativo que
posteriormente conclui a limpeza. Nenhum hook de produção ou exportação nova.

No contêiner gcc-tm passaram rebuild PIC/não PIC com -Wall -Werror, onze casos
owner, diagnósticos, quatro testes da hash, exemplos com 1/2/4 threads/aninhamento
e 24 chamadas lifecycle. Owner e diagnósticos passaram com ASan/UBSan; onze casos
owner passaram com ThreadSanitizer incluindo os caminhos lentos dos mutexes.
Readelf confirmou os três serviços owner como GLOBAL HIDDEN; nm confirmou a
ABI GOMP_NEWTM_1.0 sem exportar novos serviços. A sonda C++ com template,
referência, exceção e outra região após catch passou, incluindo destroy final
após devolver as participações; fonte/log locais em transaction_test/.build.
DejaGNU permanece indisponível; não foi executada suíte completa nem dlclose.
Relatos anteriores sobre finalização e protocolo pendente são históricos.

## Diagnósticos fora dos locks — 2026-10-28

Corrigidos dois ciclos reproduzidos na revisão: inicialização com lock global
esperando stdout enquanto outra thread dona do stream espera begin; e OOM de
entrada com bucket retido esperando stderr enquanto a dona do stream adquire
o mesmo bucket. Nenhum printf de depuração foi removido ou mudou de formato.

Create compartilha helper não diagnóstico baseado em malloc/calloc. O helper
valida por contrato capacidade positiva representável, limpa alocação parcial
quando calloc falha, inicializa mutexes e retorna tabela completa ou NULL interno.
O wrapper público preserva suas validações/status e converte NULL em fatal sem
lock. Begin usa o helper sob lock global, publica só tabela completa, desbloqueia
e só então imprime create ou diagnostica OOM. Acquire continua alocando sob o
bucket, mas usa malloc e desbloqueia antes de gomp_fatal na falha, sem modificar
lista, contadores ou out. Nenhum novo estado OOM recuperável ou ABI foi criado.
Logs concorrentes podem aparecer em ordem diferente da publicação efetiva.

Teste permanente: make -C transaction_test -B check-hash-diagnostics.
Interposição de malloc/calloc/printf/gomp_fatal somente no teste white-box,
com headers reais já incluídos antes das macros, sem hooks de produção. Cada
caso executa em processo separado com timeout de 10 segundos. Cobre stdout
bloqueado durante inicialização e stderr bloqueado durante OOM de entrada,
tabela e buckets; verifica progresso de outra operação, estado não publicado
parcialmente, logs e saída fatal 1 em OOM. Falta de memória é simulada somente
na thread escolhida; não consome memória real excessiva.

Validação no contêiner gcc-tm: build PIC/não PIC e todos os testes locais da
hash, exemplos com 1/2/4 threads/aninhamento e check-lifecycle passaram com
-O2 -Wall -Werror. Os quatro novos cenários passaram com AddressSanitizer e
UndefinedBehaviorSanitizer (códigos próprios de falha para impedir falso sucesso
em casos OOM). Aquisição, visitas e count passaram com ThreadSanitizer incluindo
instrumentação dos caminhos lentos reais dos mutexes. Logs locais em
/tmp/newtm-diagnostics-*.log e transaction_test/.build/hash_diagnostics-*.log.
DejaGNU permanece indisponível; não foi executada a suíte completa.

A finalização automática e sua exigência de quiescência externa não foram
alteradas; este ajuste não resolve a questão de shutdown identificada na revisão.
As seções anteriores sobre alocadores e sequência de logs são históricas.

## Contagem consistente e saturada — 2026-10-28

Implementada newtm_hash_count: printf/fflush de depuração antes dos locks,
aquisição de todos os mutexes em ordem crescente, soma dos entry_count com
verificação antes da adição e saturação em SIZE_MAX, desbloqueio de todos em
ordem inversa, inclusive quando a soma satura antecipadamente. Conta entradas,
não referências; nenhum estado é alterado. Requer tabela válida, sem destruição
concorrente, fora de callbacks/destruidores e sem mutex de bucket já retido.
Zero não estabelece quiescência e não autoriza destroy. As nove primitivas da
hash agora são operacionais; isso não implementa reservas ou STM.

Teste permanente: make -C transaction_test -B check-hash-count. Inclui o fonte
real/configuração privada Linux x86_64, sem hooks/ABI novos. Verifica vazio,
contagem distinta de referências, retenção/liberação, capacidades 1/3/1024 e
colisões; injeta contadores somente no teste para atingir SIZE_MAX exato e
saturação, restaura-os e verifica locks utilizáveis depois. Exercita quatro
threads inserindo/retendo/liberando enquanto contagens concorrentes progridem.
Um destrutor coordenado por condição de teste demonstra zero com a última
release ainda pendente, sem violar quiescência chamando destroy nesse intervalo.
Nenhum callback de visita espera, reentra na hash ou chama count.

No contêiner gcc-tm passaram rebuild PIC/não PIC com -Wall -Werror,
check-hash-count/check-hash-entry/check-hash-acquire/check-hash-create-destroy,
check (1/2/4 threads e aninhamento) e check-lifecycle (24 chamadas). Count passou
também com AddressSanitizer/UndefinedBehaviorSanitizer. DejaGNU indisponível;
suíte completa não executada. Relatos anteriores abaixo são históricos.

## Consultas imutáveis e visita de metadados — 2026-10-28

Implementadas newtm_hash_entry_address, newtm_hash_entry_size e
newtm_hash_entry_visit em libgomp/newtm-hash.c. Address/size exigem referência
retida e retornam campos imutáveis sem mutex, sem acessar o objeto ou alterar
referências. Visit usa o índice imutável da entrada, bloqueia somente seu bucket,
chama o callback uma vez com chave/tamanho/slot/argumento e desbloqueia antes de
retornar. O callback preserva referência, propriedade e consistência dos
metadados; não espera, reentra, executa E/S ou sai por exceção/longjmp.
Printf/fflush das primitivas foram preservados antes de adquirir qualquer lock.
Somente count permanece esqueleto; não há reservas nem STM funcional.

Teste permanente: make -C transaction_test -B check-hash-entry. Inclui o fonte
real com configuração privada Linux x86_64, sem exportações/hooks novos.
Verifica consultas após liberar outra referência, uma chamada por visita,
instalação/substituição de metadados com transferência explícita dos antigos,
argumento nulo, referência preservada e destruição final uma vez. Testa mutex
adquirido dentro do callback por inspeção restrita ao teste, quatro threads
incrementando metadados exclusivamente por visitas e progresso de uma visita em
outro bucket enquanto um bucket está bloqueado externamente. Nenhum callback
espera/reentra na hash. O teste de acquire agora instala metadados pela visita
operacional, sem escrita direta no slot.

Validação no contêiner gcc-tm: rebuild da libgomp PIC/não PIC com -Wall -Werror
e check-hash-entry/check-hash-acquire/check-hash-create-destroy/check/check-lifecycle
passaram. Testes entry/acquire passaram com AddressSanitizer e
UndefinedBehaviorSanitizer. DejaGNU indisponível; não executada suíte completa.
Os relatos anteriores abaixo são históricos.

## Aquisição e consulta com retenção — 2026-10-28

Implementadas newtm_hash_acquire e newtm_hash_lookup_acquire. O índice usa
uintptr_t com fmix32/fmix64 por UINTPTR_MAX, sem descartar bits inferiores, e
módulo bucket_count; outras larguras são rejeitadas na compilação. Busca e
retenção ocorrem sob o mutex do bucket; acquire insere se ausente, valida
tamanho antes de incrementar e aloca sob lock. Lookup não insere. Nulos,
tamanho zero, ausência e overflow seguem os contratos, limpando out válido.

Implementada também release, necessária para fechar o ciclo das entradas reais
na ponte sem vazamentos: decrementa/desliga sob lock e destrói metadados e entrada
fora dele na última referência. Address/size, visit e count continuam esqueletos.
A ponte GOMP verifica create/acquire/destroy, publica somente handles válidos,
rejeita registro sem região/endereço/tamanho válido e mantém flat nesting.
Todos os printf/fflush e seus formatos foram preservados fora dos mutexes dos
buckets. Não há reservas, dados privados ou STM; lookup não tem chamador de produção.

Teste permanente: make -C transaction_test -B check-hash-acquire. Inclui o fonte
real com configuração privada Linux x86_64, sem exportar ABI ou hooks. Cobre
nulos, ausência sem inserção, tamanho divergente, overflow de referências/contagem
por injeção interna restaurada, colisões, identidade, retenção por lookup,
liberação parcial/final, destrutor uma vez fora do lock e quatro threads com
barreiras para a mesma chave e chaves distintas. Testa mistura na largura nativa
com 32768 endereços por padrão, duas bases, seis incrementos e cinco capacidades;
32 bits não foram executados nativamente nesta etapa.

Validação no contêiner gcc-tm com build local: rebuild PIC/não PIC da libgomp e
check-hash-acquire/check-hash-create-destroy/check/check-lifecycle passaram com
-O2 -Wall -Werror. Teste novo passou também com GCC do contêiner e
AddressSanitizer/UndefinedBehaviorSanitizer. Uma sonda C++ com template,
referência e exceção passou com a libstdc++ e libgomp locais, incluindo outra
região após o catch; fonte/log em transaction_test/.build/acquire_bridge.cc/.log
são evidência exploratória local, não teste permanente. DejaGNU permanece
indisponível.
Os relatos anteriores abaixo são históricos e não definem o estado atual.

## Implementação incremental de create/destroy — 2026-10-28

Implementadas somente newtm_hash_create e newtm_hash_destroy em libgomp/newtm-hash.c.
create limpa out válido, rejeita nulos/capacidade zero, verifica overflow antes
de alocar, inicializa todos os buckets/mutexes e publica a tabela por último.
destroy exige exclusão externa de novos usuários e conclusão das operações,
visitas e destruidores; verifica todos os buckets antes de destruir recursos.
BUSY preserva tabela e referências; OK libera mutexes, buckets e tabela vazia.
count zero não estabelece quiescência. Printf/fflush e formatos foram mantidos;
essas duas operações não adquirem mutexes de buckets.

As outras sete primitivas permanecem esqueletos. A ponte agora recebe uma tabela
real, mas ainda recebe entradas nulas e ignora retornos; sua adaptação integral
permanece para a etapa das operações sobre entradas. Não há hash de entradas,
reservas ou STM funcional. Relatos anteriores abaixo são históricos.

Teste permanente: make -C transaction_test -B check-hash-create-destroy.
O teste inclui o fonte real com headers/configuração privados do build Linux
x86_64 e liga o archive local, sem exportar símbolos ou inserir hooks na ABI.
Insere uma entrada ociosa apenas por preparação interna de teste para exercer
BUSY antes de acquire existir. Não confundir isso com inserção operacional.
Cobre nulos, capacidade zero, overflow de alocação, capacidades 1/3/1024,
tabelas independentes, inicialização dos mutexes, BUSY repetido no último bucket
sem destruir buckets anteriores e destruição após retirar a entrada de teste.
Não valida quiescência automaticamente nem executa destruição concorrente.

Validação no contêiner gcc-tm (glibc 2.39), build em /usr/src/gcc-tm/gcc-build:

- Rebuild da libgomp PIC/não PIC com -Wall -Werror: passou.
- Teste novo e make -B check check-lifecycle com -O2 -Wall -Werror: passaram; exemplos com 1/2/4
  threads, aninhamento e 24 chamadas dos stubs de ciclo de vida.
- Teste novo com GCC do contêiner e AddressSanitizer/UndefinedBehaviorSanitizer:
  passou, sem diagnóstico dos sanitizadores.
- ldd confirmou libgomp local nos exemplos; nm confirmou newtm_hash_*
  ausentes da API dinâmica e GOMP_transaction_* preservados em GOMP_NEWTM_1.0.
- DejaGNU não disponível no contêiner; subconjuntos não executados nesta etapa.

## Documentos de referência e integração futura

| Documento | Responsabilidade |
| --- | --- |
| [SCOPE_AGENT](OPENMP_TRANSACTION_SCOPE_AGENT.md) | Resolução, tipos e preservação de identidade para os acessos. |
| [HASH_AGENT](OPENMP_TRANSACTION_HASH_AGENT.md) | Estrutura da hash, referências e política transacional geral. |
| [ACCESS_AGENT](OPENMP_TRANSACTION_ACCESS_AGENT.md) | Contratos propostos de newtm_read/write, sincronização e pontos de chamada. |
| [ABORT](OPENMP_TRANSACTION_ABORT.md) | Propagação de falha, cleanups, descarte e reinício externo. |

As primitivas newtm_read/write possuem somente esqueletos em
libgomp/newtm-transaction.c/.h, com assinaturas e estados internos declarados.
O contrato funcional permanece proposto; não existe ABI consolidada da ponte
de acesso GOMP_*. Elas imprimem parâmetros e retornam INVALID, sem acessar os
objetos ou modificar buffers/contexto. Os testes históricos abaixo não são
validação funcional dessas primitivas.

A integração funcional depende de hash/contexto reais, preservação do mapeamento
objeto/deslocamento antes da eliminação do marcador GIMPLE, instrumentação com
remapeamento OpenMP correto, reservas e dados privados. Cada retorno precisa ser
verificado antes de usar o resultado. CONFLICT condena a tentativa externa;
INVALID não autoriza retry. As primitivas sinalizam falha sem executar a limpeza
completa: o controle de abort conduz cleanups, descarte e retirada de reservas.

Permanecem pendentes a propagação segura entre funções, os acessos por cleanups,
a definição da ponte e a distinção entre commit, descarte e retry no encerramento
atualmente representado por TRY_FINALLY. Tipos, aliases e operações sem suporte
precisam ser restringidos. Registro e rastreamento não comprovam STM funcional.

## Definição da máquina de estados — 2026-10-21

Acrescentado `enum newtm_attempt_state` em `libgomp/newtm-transaction.h`,
separado dos resultados de hash/acesso/ciclo de vida: ACTIVE, CONFLICTED,
INVALID, PUBLISHING e CLOSED, com prefixo NEWTM_ATTEMPT_. O contexto permanece
opaco; os corpos das oito funções permanecem stubs. Nenhuma assinatura de função,
ABI exportada, chamada da ponte GOMP ou integração do compilador foi alterada.

A especificação completa está em
[ABORT — máquina de estados](OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa):
transições permitidas, operações rejeitadas, precedência de INVALID sobre conflito,
publicação irrevogável após preparação falível e CLOSED terminal. Espera não é
estado da tentativa; após descarte por conflito, nova entrada cria outra tentativa.
Também foi corrigida a regra resumida de abort para não sugerir retry de toda
falha de commit, inclusive INVALID.

Detalhada a prevenção de dupla limpeza no contrato: um único slot proprietário,
empréstimos sem poder de liberação, end externo como consumidor após publicação
total ou discard como consumidor exclusivo do controle externo após cleanups.
O consumidor marca CLOSED, zera profundidade e retira o contexto do slot **antes
da primeira liberação**. Esta ordem explicita a retirada antecipada do handle;
erros detectáveis antes do consumo continuam preservando o slot. Somente o
consumidor conserva ponteiro privado até terminar; falha estrutural durante a
limpeza é fatal. Segundo consumo pelo slot vazio é INVALID, sem tocar memória
liberada. CLOSED/slot NULL não provam commit ou término da limpeza; OK exige
concluir a operação. Essas guardas são contrato para a implementação futura,
não comportamento já executado pelos stubs.

Atualizados inventário, agentes de panorama/hash/acesso e comentários dos
esqueletos para usar a mesma máquina. A hash deixa de descrever espera como
estado e retorno da mesma tentativa a ativa. O agente de abort preserva a
pendência do protocolo de cleanups e exige guardar causa efetiva após eles,
antes do descarte. `stm_*` continua exclusivamente conceitual.

Validação executada nesta etapa:

- `make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp -j3 all`: passou,
  recompilando newtm-transaction PIC/não PIC com -Wall -Werror e religando as
  bibliotecas. Log local: `/tmp/newtm-state-build.log`.
- `make -C transaction_test check-lifecycle check`: passou. O teste permanente
  lifecycle passou de 22 para 24 chamadas, acrescentando discard repetido e
  end após discard sobre o mesmo slot opaco. Todas retornam INVALID, preservam
  slots/canários e mantêm outermost=false; seis símbolos presentes no archive
  e ausentes da API dinâmica. Verificador e README atualizados.
- Exemplos com 1/2/4 threads e aninhamento lexical/por função passaram com o
  runtime reconstruído. Continuam verificando rastreamento, não STM.

Não houve rebuild do compilador nem execução DejaGNU nesta etapa: a alteração
de runtime limita-se à declaração do enum e aos comentários dos contratos.
Transições, exclusividade operacional do descarte, publicação e retry reais
precisarão de testes funcionais quando houver contexto/recursos implementados.
Os stubs continuam retornando INVALID, sem anunciar confirmação bem-sucedida.

## Auditoria geral após os esqueletos

Ver [OPENMP_TRANSACTION_AUDIT.md](OPENMP_TRANSACTION_AUDIT.md) para a verificação
executada em 2026-10-21: builds incrementais, 383 PASS DejaGNU em transaction e
critical (C/C++/libgomp), exemplos, contratos dos 17 serviços internos, ABI,
LTO, módulo C++ e remapeamento de objetos. Os resultados finais não contêm
FAIL, UNRESOLVED ou UNSUPPORTED; as primeiras falhas por carregamento da libgomp
do sistema foram diagnosticadas e repetidas com o runtime local.

Correção em verificar_lifecycle.py: recusar -O/-OO/PYTHONOPTIMIZE para não
aprovar logs/símbolos sem executar as asserções. O teste normal e a rejeição da
execução otimizada foram verificados. Essa correção não altera o runtime.
O relatório inclui comandos reproduzíveis; nesta configuração, passar
GCC_UNDER_TEST/GXX_UNDER_TEST somente como variáveis make não mudou o driver,
e LD_LIBRARY_PATH foi necessário para selecionar a biblioteca na execução.

## Esqueletos do ciclo de vida

Acrescentados em `libgomp/newtm-transaction.c/.h`, sem novos arquivos de runtime:

- Primitivas: `newtm_begin`, `newtm_register`, `newtm_end`, `newtm_discard`.
- Helpers internos compartilhados: `newtm_fail`, `newtm_backoff`.
- Saída interna e confirmação externa são fases de end; retry permanece fluxo
  gerado pelo compilador. Não há símbolos próprios de commit/leave/retry.
- `enum newtm_lifecycle_status` define OK, CONFLICT e INVALID em domínio próprio,
  distinto de acesso e hash. Não implementa a máquina de estados do contexto.

Os stubs imprimem parâmetros e fazem fflush(stdout), retornando sempre
NEWTM_LIFECYCLE_INVALID. begin escreve false em outermost não nulo; preserva
slot de entrada/saída. Nenhum deles inspeciona contexto, slot, tabela ou objeto,
aloca, adquire lock/reserva, confirma, descarta, registra falha ou espera.
O contexto permanece opaco. Não foram ligados à ponte GOMP nem ao compilador;
Makefile.am/in já incluem o fonte/cabeçalho e não precisaram de alteração.

Escolha de contrato interno para a implementação futura: end em falha preserva
slot/profundidade/recursos; o controle externo executa cleanups e chama discard
explicitamente, guardando a causa fora do contexto. Substitui a proposta de
end-false com descarte implícito; agente de abort e inventário foram atualizados.
OK de discard significa limpeza, não commit; só causa CONFLICT permite backoff.
newtm_fail recebe causa de acesso e devolve causa efetiva lifecycle, preservando
prioridade de INVALID. Backoff recebe contagem de conflitos positiva, primeira=1,
saturada em SIZE_MAX pelo chamador; fórmula da espera ainda futura. Nenhum
contrato funcional é executado pelos stubs. `stm_*` segue conceitual.

Validação executada nesta etapa:

- `make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp -j3 all`: passou;
  compilação PIC/não PIC com -Wall -Werror e ligação compartilhada/estática.
  Log local: `/tmp/newtm-lifecycle-build.log`.
- `make -C transaction_test check-lifecycle`: passou. Teste permanente
  `lifecycle_stubs.c` ligado ao archive do build, sem exportar funções internas.
  São 22 chamadas com nulos, slots nulo/preenchido, canários de contexto/tabela/
  objeto, causas OK/CONFLICT/INVALID/desconhecida, tamanhos 0/SIZE_MAX e contagens
  0/1/SIZE_MAX. Confere INVALID, slots/dados preservados e outermost=false.
- `verificar_lifecycle.py` verifica argumentos/contagens dos logs e usa nm para
  conferir os seis símbolos no archive e sua ausência da API dinâmica. Executa
  com timeout para detectar bloqueio acidental; não é prova de backoff funcional.
  Log reproduzível em `transaction_test/.build/lifecycle_stubs.log`.
- `make -C transaction_test -B check`: recompilou exemplos e passou com 1/2/4
  threads, aninhamento lexical/por função, duplicatas e liberação externa.
- readelf confirmou GLOBAL HIDDEN nos seis símbolos do objeto PIC; ldd confirmou
  a libgomp do build local carregada pelo exemplo. Assinaturas/enums do inventário
  e links locais também foram conferidos contra os fontes.

Não foi executada a suíte DejaGNU ou feito rebuild do compilador nesta etapa:
seus fontes e a ABI GOMP não mudaram. Os testes cobrem contratos dos stubs e a
compatibilidade do rastreamento; não demonstram isolamento/commit/retry reais.

## Esqueletos das primitivas de acesso

Criados `libgomp/newtm-transaction.c` e `.h` com newtm_read/newtm_write, contexto
opaco e enum newtm_access_status. Cada corpo faz um printf com ctx, object,
offset, destination/source e size, seguido de fflush(stdout), e retorna
NEWTM_ACCESS_INVALID. Não valida ou desreferencia parâmetros, não modifica
buffers/contexto e não implementa hash, reservas, cópias privadas ou abort.

Os arquivos estão nos fontes da libgomp em Makefile.am/Makefile.in, com objeto
e dependência de compilação correspondentes. O cabeçalho não é instalado e os
símbolos permanecem ocultos. Nenhuma chamada foi inserida no compilador ou na
ponte GOMP_transaction_*; não há ABI pública nova.

Validação desta etapa:

- `make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp -j3 all` passou,
  compilando os dois modos PIC/não PIC com -Wall -Werror e ligando bibliotecas
  compartilhada e estática. Log final: `/tmp/newtm-access-skeleton-build-final.log`.
- Teste C isolado ligado à biblioteca estática conferiu quatro chamadas,
  parâmetros impressos, retorno INVALID, buffers/contexto inalterados, ponteiros
  nulos, tamanho zero e SIZE_MAX. Fonte, executável e log estão em
  `/tmp/newtm-access-skeleton/`. O teste não usa contexto transacional real:
  verifica a ausência de operações do stub, não a semântica funcional.
- nm/readelf confirmaram que newtm_read/write não são exportadas pela biblioteca
  compartilhada; evidência em `/tmp/newtm-access-skeleton/symbols.log`.
- `make -C transaction_test check` passou com a libgomp reconstruída: máximo
  com 1/2/4 threads e caso de aninhamento lexical/por função.

Também foram revisados todos os agentes: contratos consolidados da hash e seus
callbacks, ponte atual, ciclo de vida conceitual, acessos, helpers do compilador
e serviços critical existentes. Assinaturas internas dos stubs estão declaradas;
contratos funcionais e a futura ABI de ponte continuam sujeitos às pendências
explicitadas. Não foi executada a suíte completa do GCC/libgomp nesta etapa.

## Inventário e localização dos contratos

O inventário central está em
[OPENMP_TRANSACTION_PRIMITIVES.md](OPENMP_TRANSACTION_PRIMITIVES.md): registra
as assinaturas internas existentes, proprietário dos recursos, estados, efeitos
atuais/funcionais futuros e ponto de chamada de cada operação. Abrange hash,
callbacks, acessos, ponte de rastreamento, esqueletos de ciclo de vida e os cinco
helpers do compilador. Os agentes especializados preservam os contratos completos.

A consolidação de 2026-10-21 foi documental, conferida contra declarações,
definições e chamadores. Nenhum símbolo, enum, ABI ou esqueleto novo foi criado.
Os nomes stm_* continuam conceituais e operações sem assinatura definida estão
marcadas como tal. Confirmação, backoff e retry não exigem novos símbolos por si.
Após essa consolidação inicial, os esqueletos de ciclo de vida foram
acrescentados conforme a seção acima; as operações reais permanecem futuras. Fontes atuais em
`/usr/src/gcc-omp-transaction`; caminhos antigos no histórico são preservados.

Funções genéricas do GCC/libgomp e primitivas critical são referências existentes,
não novas APIs STM. Não confundir retornos dos stubs com contratos funcionais.

### Contratos dos helpers atuais do compilador

Estes helpers executam na compilação, não sobre memória transacional do programa.
Erros de sintaxe/semântica seguem diagnósticos e recuperação do front-end; não
são CONFLICT/INVALID do runtime. Contratos de alocação/GC são os usuais do GCC.

| Helper | Pré-condições | Pós-condições atuais |
| --- | --- | --- |
| newtm_c_parser_transaction | Parser C válido na posição esperada pelo dispatcher; contexto de construção e bindings ativos; localização válida e if_p sob contrato do parser de bloco. | Consome a diretiva e analisa o corpo; resolve lista antes de entrar no corpo; preserva declarações aceitas na ordem; constrói/acrescenta OMP_TRANSACTION. Entradas inválidas são diagnosticadas e recuperadas até EOL, sem certificar validade de lista parcial. |
| newtm_cp_parser_transaction | Parser/token de pragma válidos e contexto do dispatcher; bindings e estado C++ coerentes. | Mesmo percurso, com busca C++, acesso/ambiguidade, referências externas e dados para instanciação. Diagnóstico já emitido não implica nó de sucesso semanticamente válido. |
| newtm_finish_transaction | Localização, corpo e lista de declarações como árvores GCC válidas/rastreadas; lista conforme representação aceita, possivelmente vazia. | Constrói OMP_TRANSACTION com tipo void, corpo e lista, acrescenta ao fluxo e retorna o nó. Não resolve nomes, registra endereços ou instrumenta acessos. |
| newtm_build_transaction | Sequência GIMPLE válida e pertencente ao corpo a transferir, possivelmente vazia. | Retorna novo GIMPLE_OMP_TRANSACTION/GSS_OMP com esse corpo; chamado na gimplificação e na cópia/remapeamento em tree-inline.cc. Não copia a lista GENERIC, não emite acesso ou commit. |
| newtm_lower_transaction | Iterador válido sobre GIMPLE_OMP_TRANSACTION, contexto omp existente e corpo preparado para lower_omp no contexto externo. | Transforma o corpo com ctx->outer e substitui o marcador pela sequência; chamadas de rastreamento já presentes são preservadas. Não consultar o marcador removido; não há instrumentação funcional. |

Na implementação futura, preservar/remapear metadados, atualizar consumidores e
inserir acessos conforme o agente de acesso antes de remover o marcador. Esse
trabalho altera as pós-condições e deve atualizar esta tabela na mesma entrega.

## Instrumentação de chamadas à hash (etapa atual)

Os parsers preservam as declarações dos objetos no segundo operando de
`OMP_TRANSACTION`; nomes que não designam variáveis/parâmetros são rejeitados.
A instanciação C++ substitui essa lista. Na gimplificação, a lista é convertida
em chamadas `GOMP_transaction_begin`, `GOMP_transaction_register(address, size)`
e `GOMP_transaction_end`; a saída usa `GIMPLE_TRY_FINALLY`. Referências C++
registram o objeto referido. Exigem-se tipos completos de tamanho constante.
As chamadas comuns passam pelo remapeamento de dados OpenMP existente, e a
lista não precisa sobreviver no marcador GIMPLE.

A ponte provisória na libgomp chama `newtm_hash_create` uma vez, no primeiro
uso, `newtm_hash_acquire` para cada objeto distinto no contexto, e
`newtm_hash_release` somente ao sair do nível externo. Aninhamento por chamada
de função também compartilha esse contexto. No encerramento normal da
biblioteca, chama `newtm_hash_destroy` se houve uso. As outras primitivas não
são chamadas artificialmente: ainda não há operações de acesso transacional,
metadados ou diagnóstico de contagem que as exijam.

As primitivas apenas imprimem seus argumentos. Os antigos `gomp_fatal` foram
substituídos por saídas definidas: estados INVALID, handles nulos e consultas
nulas/zero. A ponte ignora esses estados **apenas nesta etapa de instrumentação**;
não há tabela ou entradas reais, e os logs mostram handles nulos. Uma lista
mínima por execução controla as mensagens de aquisição/liberação e duplicatas;
não armazena valores dos objetos nem implementa hash, reservas ou conflitos.
O log consiste exclusivamente nos `printf` existentes, sem outro mecanismo.

O contexto acompanha execução síncrona na thread. Tasks com suspensão/migração
e dispositivos não estão suportados. O corpo continua acessando memória comum;
a saída excepcional apenas limpa o contexto, sem commit, abort ou rollback.
Os relatos abaixo são históricos e não descrevem integralmente esta etapa.

Validação desta etapa:

- Build GCC concluído com `make -C /usr/src/gcc-build/gcc -j6 all`, seguido
  de checagem incremental; libgomp reconstruída com `make -j3 all`.
- Novos testes `transaction-log-*`: C, 8 PASS; C++, 33 PASS; libgomp,
  6 PASS (compilação, execução e padrão de saída em C e C++).
- Regressão `critical-*`: C, 20 PASS; C++, 60 PASS.
- Conjunto `transaction-*`: C, 46 PASS/7 FAIL; C++, 165 PASS/21 FAIL.
  Sem UNRESOLVED nos resultados finais. Todas as linhas excedentes que
  causaram FAIL são mensagens `[newtm]` de depuração já presentes nos parsers
  e em omplower; não foram filtradas dos testes históricos para esconder falhas.
- Nove execuções adicionais passaram: três programas C/C++ em O0, O2 e
  O2+LTO, cobrindo quatro threads, duplicatas, aninhamento léxico/por função,
  tipos e tamanhos, referências, lambdas e cleanup durante exceção. Um teste
  adicional confirmou endereços das cópias private/reduction; outro confirmou
  exportação/importação e instanciação de template em módulo C++.
- Testes diretos da ponte confirmaram liberação somente na saída externa e
  80 pares acquire/release sob quatro threads, com uma criação/destruição global.

Os testes de execução precisam da libgomp reconstruída. Na regressão GCC,
foi necessário definir GCC_UNDER_TEST/GXX_UNDER_TEST com `-L` e rpath para
`/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/.libs`: o driver padrão estava
ligando a versão antiga e falhava com símbolos GOMP_transaction_* ausentes.
Não foi feita instalação do compilador ou runtime, nem executada a suíte completa.

Evidências: `/tmp/newtm-calls-build-final.log`,
`/tmp/newtm-calls-build-check.log`, `/tmp/newtm-calls-lambda-build.log`,
`/tmp/newtm-calls-runtime-final.log`, `/tmp/newtm-calls-verify.py`,
`/tmp/newtm-calls-regression.py`, `/tmp/newtm-calls-module/` e os resumos/logs
nos diretórios `testsuite-newtm-calls`, `testsuite-newtm-calls-all`
(critical) e `testsuite-newtm-calls-transaction` sob o build GCC. Os resultados
da libgomp estão em seu diretório `testsuite/libgomp.sum` e `.log`.

## Leitura do histórico

As seções a partir de “Histórico de implementação” registram etapas anteriores,
sem valor normativo para trabalho novo. Descrições de listas descartadas,
ausência de chamadas runtime e saída incondicional não se aplicam ao código
atual. Os resultados de testes ali registrados são históricos, não reproduzidos
por esta atualização. O estado atual é descrito no início deste arquivo.

## Ajustes de manutenção

- A resolução C++ usa `cp_parser_lookup_name_simple`, com verificações da
  linguagem e diagnóstico de busca quando necessário. C distingue ausência de
  declaração de entidade que não é variável. Os diagnósticos apontam o argumento.
- A saída de tipos e de lowering do compilador é opcional, habilitada por
  `-ftransaction-debug`, em stderr. Os logs do runtime continuam em stdout.
- A lista é consumida na gimplificação; o marcador GIMPLE guarda o corpo com
  as chamadas, não uma lista separada. Instrumentação futura deve planejar como
  preservar a identidade dos objetos quando precisar dela no lowering.
- O tratamento real dos estados de erro da hash é requisito da transição para
  uma hash funcional. Enquanto as primitivas forem stubs, INVALID e handles
  nulos são esperados exclusivamente pela ponte de rastreamento. Não reutilizar
  essa ponte como implementação funcional sem revisar seus chamadores.

## Validação dos ajustes de manutenção

Rebuild e checagem incremental concluídos com
`make -C /usr/src/gcc-build/gcc -j6 all`. Self-tests internos não estão
habilitados neste build. Não houve alteração de fontes da libgomp nesta etapa;
os testes usaram a biblioteca anteriormente reconstruída em
`/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/.libs`.

Resultados finais DejaGNU:

| Seleção | C | C++ |
| --- | --- | --- |
| `gomp.exp=transaction-*` | 56 PASS | 213 PASS |
| `gomp.exp=critical-*` | 20 PASS | 60 PASS |

Sem FAIL, UNRESOLVED ou UNSUPPORTED nesses resumos finais. Os novos testes
`transaction-lookup-1.c` e `.C` cobrem visibilidade, recuperação, acesso privado
e ambiguidade. As expectativas de nome ausente foram atualizadas para os
diagnósticos próprios de C/C++; não foram filtrados erros inesperados.

Passaram também 21 comandos de verificação direta: silêncio por padrão,
habilitação/desabilitação explícita de `-ftransaction-debug`, tipo da variável
local que oculta a global, distinção de nome ausente, templates/lambdas e
compilação/execução do teste de rastreamento em C/C++ com O0, O2 e LTO.
Esses resultados validam manutenção e rastreamento, não memória transacional.

Comandos reproduzíveis e evidências:

- `/tmp/newtm-adjust-build.log` e `/tmp/newtm-adjust-build-check.log`.
- `/tmp/newtm-adjust-regression.py`: comandos DejaGNU, passando o compilador e
  os caminhos de ligação via RUNTESTFLAGS. Uma tentativa inicial sem esse
  encaminhamento ligou uma libgomp antiga e falhou; os resultados finais usam
  a biblioteca correta, conforme os comandos dos logs.
- `/usr/src/gcc-build/gcc/testsuite-newtm-adjust-final/`: .sum/.log de transaction.
- `/usr/src/gcc-build/gcc/testsuite-newtm-adjust/`: .sum/.log de critical.
- `/tmp/newtm-adjust-verify.py` e `/tmp/newtm-adjust-verify/results.json`:
  verificações diretas e seus logs individuais.

A suíte completa não foi executada. Não houve instalação do compilador.

# Histórico de implementação (não normativo)



## Depuração de identificadores e tipos

Os parsers C e C++ agora consultam `lookup_name` durante a análise de cada
identificador, antes de entrar no corpo, e escrevem em `stderr`:

```text
[newtm] transaction: contador: int
[newtm] transaction: valor: double
[newtm] transaction: ausente: undef
```

A saída é incondicional para cada argumento, inclusive repetições. Não é
emitida uma linha para a forma sem argumentos. A ausência de declaração
visível imprime `undef`, sem emitir erro de nome não declarado. A consulta é
apenas para depuração: não cria uso avaliado, captura ou armazenamento na IR.
Os tipos são obtidos de `TREE_TYPE` de declarações válidas; C usa
`print_generic_expr` e C++ usa `type_as_string`, preservando referências e
tipos dependentes. Typedefs podem aparecer com seu nome de alias.

Em C++, a consulta usa os bindings ativos diretamente, evitando os diagnósticos
e efeitos do wrapper do parser para esta instrumentação. Ela não constitui uma
validação completa de acessibilidade. Sobrecargas sem tipo único aparecem como
`<overload>`; entidades encontradas sem tipo utilizável aparecem como
`<no type>`. Um namespace não tem tipo de objeto, mesmo que o nó interno
contenha `void_type_node`. Tipos dependentes são mostrados na definição do
template, por exemplo `T&`; não há nova mensagem na instanciação.

Validação: rebuild incremental em `/usr/src/gcc-build/gcc`; verificações
diretas de C/C++ confirmaram parâmetros, qualificadores, ponteiros, arrays,
structs, aliases, ocultação, repetições, nomes ausentes, escopos encerrados,
declarações posteriores e locais exclusivos do corpo. Também foram verificados
referências, membros e templates C++, além da recuperação após lista vazia.

Execução de `gomp.exp=transaction-*`, com
`TESTSUITEDIR=testsuite-newtm-scope-debug`: C, 36 PASS/7 FAIL; C++,
126 PASS/21 FAIL. As falhas são verificações de saída adicional: incluem as
mensagens preexistentes de `omp-low.cc` e a saída de depuração solicitada nos
testes `transaction-4.c` e `transaction-10.c`. Os testes de execução passaram.
Não foram modificadas as expectativas da suíte para ocultar essas mensagens.
Evidências: `/tmp/newtm-scope-debug/`,
`/tmp/newtm-scope-debug-build.log` e os `.sum`/`.log` no diretório de testes
acima. A suíte completa não foi executada.

## Resultado

A extensão está implementada em C e C++ nesta árvore GCC 16.2.0. Aceita
`#pragma omp transaction` seguido de um bloco estruturado, sem cláusulas. A sintaxe também aceita uma
lista opcional não vazia de identificadores separados por vírgula entre
parênteses: `#pragma omp transaction(a, b, c)`. Os nomes não são resolvidos
como variáveis, não são armazenados na representação intermediária e ainda
não têm efeito semântico.
O corpo executa normalmente para cada thread que encontrar a região, usando
o ambiente de dados da construção OpenMP externa. Regiões aninhadas são aceitas.

Foram consultados `OPENMP_AGENT.md` e `OPENMP_CRITICAL_AGENT.md`, e o percurso
foi conferido diretamente nos fontes. Esses dois documentos não foram alterados.

## Representação e processamento

```text
PRAGMA_OMP_TRANSACTION
  -> newtm_c_parser_transaction / newtm_cp_parser_transaction
  -> newtm_finish_transaction
OMP_TRANSACTION (um operando: corpo)
  -> gimplify_expr: caso OMP_TRANSACTION
  -> newtm_build_transaction
GIMPLE_OMP_TRANSACTION (layout GSS_OMP)
  -> verificações de bloco estruturado
  -> varredura OpenMP
  -> newtm_lower_transaction, no passe omplower
sequência GIMPLE comum, preservando binds e cleanups do corpo
  -> passes de controle, exceções, CFG, otimização e geração de código
```

O construtor GIMPLE está em `gcc/gimple.cc`, declarado em `gcc/gimple.h`.
A conversão é chamada pelo dispatcher de `gcc/gimplify.cc`.
O lowering específico está em `gcc/omp-low.cc`: transforma recursivamente o
corpo e substitui a instrução por essa sequência com `gsi_replace_with_seq`.

O código `GIMPLE_OMP_TRANSACTION` permanece explícito até esse lowering.
Não é convertido em `GIMPLE_OMP_CRITICAL`, `GIMPLE_TRANSACTION` ou outro alias.
Não sobrevive até `ompexp`, portanto não exige um caso novo em `omp-expand.cc`.

O passe de varredura registra um contexto para permitir o lowering de regiões
isoladas, mas percorre o corpo com o contexto externo. O lowering também usa
esse contexto externo. Assim, a extensão não oculta regras de aninhamento nem
o tratamento das variáveis da construção OpenMP envolvente.

## Funções novas e componentes reutilizados

As cinco funções específicas usam o prefixo exigido:

| Função | Arquivo | Responsabilidade |
| --- | --- | --- |
| `newtm_c_parser_transaction` | `gcc/c/c-parser.cc` | Parser C; rejeita tokens adicionais na diretiva e analisa o corpo. |
| `newtm_cp_parser_transaction` | `gcc/cp/parser.cc` | Parser C++; preserva a localização e analisa o corpo. |
| `newtm_finish_transaction` | `gcc/c-family/c-omp.cc` | Finalizador compartilhado que constrói `OMP_TRANSACTION`. |
| `newtm_build_transaction` | `gcc/gimple.cc` | Aloca `GIMPLE_OMP_TRANSACTION` e define seu corpo. |
| `newtm_lower_transaction` | `gcc/omp-low.cc` | Substitui a região pelo corpo sequencial transformado. |

Não foram criados tipos auxiliares, estruturas ou variáveis globais específicas.
A representação reutiliza `gimple_statement_omp`, pelo layout `GSS_OMP`.
Os identificadores novos foram pesquisados antes da implementação; não foram
encontrados conflitos com `newtm_` ou `OMP_TRANSACTION` nos fontes examinados.

Foram reutilizados o registro adiado de pragmas, os dispatchers OpenMP,
os parsers de blocos estruturados, `build1_loc`, `add_stmt`, `gimple_alloc`,
`gimple_omp_set_body`, os acessores genéricos de corpo, a cópia de sequências,
os walkers OpenMP, a impressão de regiões com apenas corpo e as verificações
existentes de blocos estruturados e aninhamento.

## Arquivos alterados

| Grupo | Arquivos |
| --- | --- |
| Registro e classificação | `gcc/c-family/c-pragma.h`, `gcc/c-family/c-pragma.cc`, `gcc/c-family/c-common.h`, `gcc/c-family/c-omp.cc` |
| Parsing | `gcc/c/c-parser.cc`, `gcc/cp/parser.cc` |
| GENERIC | `gcc/tree.def`, `gcc/tree.h`, `gcc/tree-pretty-print.cc` |
| GIMPLE | `gcc/gimple.def`, `gcc/gimple.h`, `gcc/gimple.cc`, `gcc/gimple-walk.cc`, `gcc/gimple-pretty-print.cc` |
| Conversão e lowering | `gcc/gimplify.cc`, `gcc/omp-low.cc`, `gcc/gimple-low.cc` |
| Cópia e funções aninhadas | `gcc/tree-inline.cc`, `gcc/tree-nested.cc` |
| Templates, módulos e classificação C++ | `gcc/cp/pt.cc`, `gcc/cp/module.cc`, `gcc/cp/constexpr.cc` |
| Manual | `gcc/doc/extend.texi` |

São 22 arquivos de implementação e um arquivo do manual. Foram adicionados
dez testes e este relatório. Não foram alterados fontes ou testes da `libgomp`
nem a infraestrutura de GNU Transactional Memory.

`CASE_GIMPLE_OMP`, em `gimple.h`, inclui o código novo; seus consumidores
genéricos, como o verificador de `tree-cfg.cc`, passam a reconhecê-lo sem
alterações locais. `gimple-low.cc` verifica a invariável de que a região já
deve ter sido eliminada em `omplower`. Os streamers de GIMPLE/LTO foram
examinados; a região é removida antes da serialização normal. Módulos C++
precisam reconhecer a árvore que pode permanecer em uma definição de template,
por isso `cp/module.cc` recebeu sua classificação explícita como OpenMP.

## Testes adicionados

| Arquivo | Cobertura |
| --- | --- |
| `gcc/testsuite/c-c++-common/gomp/transaction-1.c` | Execução, região vazia, declarações, atribuições, condição, laço, aninhamento e dump legível antes/depois do lowering. |
| `gcc/testsuite/c-c++-common/gomp/transaction-2.c` | Dump bruto com `gimple_omp_transaction`; ausência de alias e de chamadas GOMP/ITM; assembly sem essas chamadas. |
| `gcc/testsuite/c-c++-common/gomp/transaction-3.c` | Execução em `parallel`, redução externa, regiões aninhadas e worksharing dentro da região. |
| `gcc/testsuite/c-c++-common/gomp/transaction-4.c` | Rejeição de cláusulas e nome, com recuperação para a construção seguinte. |
| `gcc/testsuite/c-c++-common/gomp/transaction-5.c` | Contexto de arquivo inválido e corpo ausente. |
| `gcc/testsuite/c-c++-common/gomp/transaction-6.c` | Rejeição de saltos que entram ou saem da região. |
| `gcc/testsuite/c-c++-common/gomp/transaction-7.c` | Preserva a proibição de regiões críticas aninhadas com o mesmo nome, mesmo com transaction intermediária. |
| `gcc/testsuite/c-c++-common/gomp/transaction-8.c` | `_Pragma`, composição com critical e contagem das chamadas de sincronização explicitamente solicitadas. |
| `gcc/testsuite/gcc.dg/gomp/transaction-1.c` | Função aninhada C com acesso a variável externa. |
| `gcc/testsuite/g++.dg/gomp/transaction-1.C` | Template e execução de construtor/destrutor de objeto local. |

## Compilação e resultados

Foi utilizada a árvore existente `/usr/src/gcc-build`, no estágio 3,
configurada para C, C++ e LTO. DejaGNU foi instalado para executar a suíte.
O rebuild foi realizado com:

```sh
make -C /usr/src/gcc-build/gcc -j3 all
```

O comando terminou com sucesso e foi seguido de uma checagem incremental,
também bem-sucedida, para incorporar as últimas edições feitas durante o rebuild.
Houve um aviso de possível variável não inicializada em `gcc/expmed.cc`, arquivo
não alterado. O build informa que os self-tests internos não estão habilitados.
Não foi feita instalação do compilador reconstruído nem um novo bootstrap
completo de três estágios.

Comandos principais dos testes executados:

```sh
make -C /usr/src/gcc-build/gcc check-gcc TESTSUITEDIR=testsuite-newtm RUNTESTFLAGS='gomp.exp=transaction-*'
make -C /usr/src/gcc-build/gcc check-g++ TESTSUITEDIR=testsuite-newtm RUNTESTFLAGS='gomp.exp=transaction-*'
make -C /usr/src/gcc-build/gcc check-gcc TESTSUITEDIR=testsuite-newtm-critical RUNTESTFLAGS='gomp.exp=critical-*'
make -C /usr/src/gcc-build/gcc check-g++ TESTSUITEDIR=testsuite-newtm-critical RUNTESTFLAGS='gomp.exp=critical-*'
make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp check RUNTESTFLAGS='c.exp=critical-*.c c++.exp=critical-*.c'
```

| Conjunto | Resultado |
| --- | --- |
| Novos testes, C | 33 PASS |
| Novos testes, C++ | 117 PASS, com variações de padrão de linguagem executadas pelo driver |
| Regressão critical, C | 20 PASS; mesmos resultados da referência anterior ao rebuild |
| Regressão critical, C++ | 60 PASS; mesmos resultados da referência anterior ao rebuild |
| Regressão critical, libgomp | 28 PASS; mesmos resultados da referência anterior ao rebuild |
| Execuções adicionais | 12 PASS: C/C++, O0/O2/O2+LTO, uma/quatro threads |
| Módulo C++ | 1 PASS: exportação/importação e instanciação de template contendo transaction |

Os resumos desses subconjuntos não contêm FAIL, UNRESOLVED ou UNSUPPORTED.
Os testes de execução foram efetivamente executados, conforme registrado nos
arquivos `.sum` e `.log`. A suíte completa do GCC/OpenMP não foi executada.

Resultados e evidências locais:

- `/tmp/newtm-build.log` e `/tmp/newtm-build-final.log`: rebuild e checagem incremental.
- `/usr/src/gcc-build/gcc/testsuite-newtm/gcc/gcc.sum` e `gcc.log`.
- `/usr/src/gcc-build/gcc/testsuite-newtm/g++/g++.sum` e `g++.log`.
- `/usr/src/gcc-build/gcc/testsuite-newtm-critical/`: resumos/logs de critical.
- `/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/testsuite/libgomp.sum` e `libgomp.log`.
- `/tmp/newtm-critical-baseline-c.sum`, `/tmp/newtm-critical-baseline-cxx.sum`
  e `/tmp/newtm-critical-runtime-baseline.sum`: referência anterior ao rebuild.
- `/tmp/newtm-integration-checks/results.json`: verificações adicionais.
- `/tmp/newtm-implementation.patch`: patch de revisão dos fontes/manual/testes.

A inspeção direta produziu, antes do lowering:

```text
gimple_omp_transaction <
  BODY <
    gimple_assign <mem_ref, _1, *value, NULL, NULL>
    gimple_assign <plus_expr, _2, _1, 1, NULL>
    gimple_assign <ssa_name, *value, _2, NULL, NULL>
  > >
```

Depois de `omplower`, permaneceu somente:

```text
_1 = *value;
_2 = _1 + 1;
*value = _2;
```

Os dumps e assembly dessa inspeção estão em
`/tmp/newtm-representation.c.007t.gimple`,
`/tmp/newtm-representation.c.010t.omplower` e
`/tmp/newtm-representation.s`. Nenhum deles contém chamadas GOMP/ITM.

## Limitações e extensão futura

A diretiva é experimental e não pertence ao padrão OpenMP. Não oferece
atomicidade, isolamento, rollback, sincronização ou operações de memória
transacional. Não aceita cláusulas; aceita a lista opcional de identificadores descrita acima. É registrada por `-fopenmp`,
não por `-fopenmp-simd` isoladamente. A entrada foi implementada apenas em C e
C++; não foi adicionada sintaxe Fortran. Permanecem as restrições da
infraestrutura OpenMP, incluindo blocos estruturados, contextos de aninhamento
e rejeição em funções constexpr C++.

A validação foi feita em x86_64 GNU/Linux. Nenhum dispositivo de offload foi
validado, e não há promessa de identidade transacional entre host/dispositivo.
Com `-fopenmp`, o driver continua usando as regras usuais de ligação OpenMP;
a região transaction em si não adiciona chamadas ao runtime.

Para uma semântica transacional futura, os pontos explícitos de extensão são
`OMP_TRANSACTION`, `GIMPLE_OMP_TRANSACTION`, seu construtor e
`newtm_lower_transaction`. Campos, cláusulas, validações e mecanismos de
execução futuros devem ser acrescentados somente após definir essa semântica.
O código atual não escolhe locks, GNU TM ou qualquer runtime transacional como
implementação futura implícita.

## Extensão sintática: lista de identificadores

Os parsers C e C++ aceitam a forma sem argumentos e a forma
`transaction (identificador [, identificador] ...)`. Listas vazias,
expressões, separadores ausentes e vírgulas finais são rejeitados, com
recuperação até o fim do pragma e análise normal do corpo seguinte.
Os identificadores podem ser repetidos e não precisam estar declarados.
Esta alteração é apenas sintática: os operandos de GENERIC/GIMPLE e o
lowering permanecem os mesmos.

O teste `transaction-4.c` agora aceita `(named)`. Foram acrescentados
`transaction-9.c` (execução, lista simples/múltipla, macros, `_Pragma`,
aninhamento e compatibilidade com a forma original) e `transaction-10.c`
(diagnósticos e recuperação de erros), ambos compartilhados por C e C++.

Validação desta extensão: rebuild com `make -C /usr/src/gcc-build/gcc -j3 all`
concluído com sucesso. Executados `check-gcc` e `check-g++`, com
`TESTSUITEDIR=testsuite-newtm-identifiers` e `RUNTESTFLAGS='gomp.exp=transaction-*'`.
Resultados: C, 38 PASS e 5 FAIL; C++, 132 PASS e 15 FAIL. Todas as verificações
dos testes novos e do `transaction-4.c` atualizado passaram, incluindo execução
em C e C++ (gnu++98, gnu++20 e gnu++26). As falhas restantes são exclusivamente
saída inesperada das mensagens `[newtm] Inicio/Fim do tratamento das variaveis
da transacao`, provenientes de alterações já existentes em `gcc/omp-low.cc`,
preservadas nesta tarefa. O teste novo de execução filtra essas mensagens.
Não houve ERROR, UNRESOLVED ou UNSUPPORTED nos resumos finais.
Logs de build/testes: `/tmp/newtm-identifiers-build.log`,
`/tmp/newtm-identifiers-test-c.log` e `/tmp/newtm-identifiers-test-cxx.log`.
