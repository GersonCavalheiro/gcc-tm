## Vínculos de ocorrências na gimplificação

`make -C transaction_test check-ocorrencias` verifica avaliação única e os
metadados `newtm-bind` nos dumps High GIMPLE, sem ativar STM no pragma.
Inclui C/C++ em O0/O2, RMW, argumentos com efeitos, ponteiros/referências,
agregados e parallel. Um plugin de teste compilado com g++ força GC e verifica
cópias com operandos alterados, sem inseri-las no programa. Requer os headers e
o suporte a plugins do build GCC local. Comandos, logs, dumps e resultado.json
ficam em `.build/occurrences/`; timeout de 60s por compilação e 30s por execução.
O [contrato da etapa 1](../gcc-src-tm/OPENMP_TRANSACTION_OCCURRENCES.md) explica
proveniência, operandos atuais, âncoras source-only e a janela anterior a CFG/SSA.

## Exemplos funcionais com o runtime interno

[transaction_interno_simples.c](transaction_interno_simples.c) executa uma região
OpenMP parallel: cada thread faz cem incrementos transacionais de uma única
variável compartilhada. Ao final, verifica `valor == threads * 100`.

[transaction_interno_carga.c](transaction_interno_carga.c) é o equivalente STM de
transaction_carga.c: aceita `THREADS ESCRITAS:LEITURAS REPETICOES`, conserva a
sequência ponderada e verifica a invariância `inverse == ~value`. Registra o
agregado inteiro e acessa seus dois campos separadamente com offsetof/read/write.
Somente operações confirmadas entram nos contadores de leituras/escritas.

Ambos usam corpos explicitamente preparados para `newtm_execute`, com registro,
visão privada, reservas, commit, descarte e retry reais. Cada status de acesso é
traduzido explicitamente para lifecycle e propagado antes de consumir resultados.
O executor repete o corpo inteiro em conflito; temporários são recriados. Não há
critical ou E/S nos callbacks. No teste de carga, o descritor passado ao executor
é entrada/controle estável e não recebe resultados especulativos. Os objetos
compartilhados só são inspecionados diretamente depois do término da região
paralela; então o proprietário da hash é fechado explicitamente.

Para compilar e executar no ambiente do build, a partir da raiz:

```sh
make -C transaction_test exemplos-internos
OMP_NUM_THREADS=4 transaction_test/.build/transaction_interno_simples > /tmp/interno-simples.log
transaction_test/.build/transaction_interno_carga 4 1:9 1000 > /tmp/interno-carga.log
make -C transaction_test check-internos INTERNO_REPETICOES=1000
```

O compilador local e os caminhos aceitam GCC_BUILD/TRANSACTION_CC como os demais
alvos. A compilação usa -fopenmp; a ligação é uma etapa separada, sem -fopenmp,
com o archive local libgomp.a e -pthread. Isso dá acesso aos símbolos hidden sem
exportá-los e evita acrescentar uma segunda libgomp compartilhada pelo driver.
Não há ativação da integração STM do pragma.

`all` inclui os dois programas; `check` inclui `check-internos`. O alvo específico
verifica o contador com 1/2/4 threads por padrão, a carga com uma e CARGA_THREADS
threads nas proporções 1:9, 1:1, 9:1, 0:1 e 1:0, além de argumentos inválidos.
INTERNO_REPETICOES tem padrão 100; CARGA_THREADS tem padrão 4. Logs ficam em
`.build/interno-*.log`, com timeout de 60 segundos por execução. Não se exige um
número específico de conflitos: o escalonamento é variável. Confere-se balanço
de referências entre todas as tentativas e fechamento único da hash. Os logs
internos são verbosos e não constituem medição de desempenho.

Verificação desta entrega: -O2 -Wall -Wextra -Werror, contador com 1/2/4 threads
e cargas com 1/4 threads, mil operações por thread nas cinco proporções. Ligação
conferida sem dependência da DSO libgomp do sistema.

## Diagnósticos, identidade e sobreposição

`make -C transaction_test check-subset` compila a matriz C/C++ em O0/O2,
executa casos de índice avaliado uma vez com/sem LTO e verifica planos GIMPLE.
Resultados e comandos ficam em `.build/subset/resultado.json`; os logs são
separados por caso. Os planos não ativam execução STM no pragma.

`make -C transaction_test -B check-overlap` testa adjacência, incompatibilidade,
sobreposições nas duas ordens, deduplicação, aquisição pendente concorrente,
32 disputas de admissão e remoção do índice em commit/descarte.

`python3 transaction_test/verificar_regressao_compilador.py gcc-build` compila e
executa os casos existentes da extensão. É alternativa de verificação direta
quando DejaGNU não está disponível: não interpreta os scans Tcl de dg-final.
Os exemplos da ponte usam critical externo e listam também os dados preexistentes locais.
A carga de rastreamento já não mantém registros concorrentes dentro do critical;
a concorrência da hash e do índice é verificada pelos testes internos separados.

## Falha, descarte e retry de corpos preparados

`make -C transaction_test -B check-retry` usa a libgomp reconstruída e inclui
check-failure; aceita GCC_BUILD. Logs `.build/retry.log` e `.build/failure-*.log`.
Timeout de 60s no retry e 10s por processo de falha. O teste de retry cria conflito
real dentro de função aninhada, aguarda limpeza antes de liberar o concorrente
e verifica reexecução do corpo externo inteiro e liberação única de referências/
leases. Os dez processos de failure verificam fatal após limpeza e ausência de
retry de INVALID, incluindo status falso/desconhecido e nesting não balanceado.
Os wrappers são exclusivos dos testes e não acrescentam hooks ao runtime.

O executor newtm_execute é privado e recebe corpo explicitamente preparado:
registro, read/write e propagação de status são manuais. A assinatura está em
newtm-context.h. Não testa nem ativa retry do pragma compilado. Backoff é espera
ativa progressiva com teto, sem unidades de tempo ou garantia de progresso.

## Confirmação externa

`make -C transaction_test -B check-commit` usa o archive local da libgomp
reconstruída, aceita GCC_BUILD e grava `.build/commit.log` (timeout de 60s).
Cobre publicação escalar/parcial de agregados, ausência de publicação em falhas,
leitores compartilhados, nesting e limpeza completa. Wrappers de memcpy,
alocadores e liberações, restritos ao teste, pausam entre duas publicações e
confirmam conflito de read/write em ambos os objetos até terminar as cópias.
Páginas protegidas detectam escrita de faixas não modificadas. O teste não integra
a ponte GOMP, não testa retry e não injeta OOM ou ponteiros pendentes.

## Aceitação dos acessos read/write

`make -C transaction_test -B check-access` (a partir da raiz) executa check-values
e a matriz em access.c contra a libgomp reconstruída; aceita GCC_BUILD. Timeout
individual de 60 segundos, log `.build/access.log`. Cobre falhas de contexto,
registro, limites e buffers para ambas as operações, preservação das escritas
anteriores e do destino de leitura e diagnóstico da subfaixa conflitante. A
matriz confere a memória original após cada erro e descarte. OOM é fatal e não
é injetado por esse alvo. Ponteiros pendentes não são entradas de teste válidas.

## Valores privados e intervalos

`make -C transaction_test -B check-values` (a partir da raiz) usa o archive local
da libgomp reconstruída; aceita GCC_BUILD. Timeout de 60 segundos, log em
`.build/values.log`. Cobre armazenamento escalar, agregados parciais, uniões de
intervalos válidos/modificados (inclusive adjacência), read-your-writes, nesting,
limites/aliases conhecidos e falhas preservando dados. Um modelo de 500 operações
confere cada faixa válida/modificada; páginas protegidas detectam leitura de
campos vizinhos ou recarga indevida de dados já válidos. Duas threads verificam
leitura compartilhada, falha de promoção e escrita privada após descarte do outro
leitor, em 32 rodadas. Não testa publicação, retry ou integração com o compilador.

## Metadados e reservas internos

`make -C transaction_test -B check-reservations` (a partir da raiz) usa o archive
local da libgomp reconstruída, aceita GCC_BUILD e grava `.build/reservations.log`
com timeout de 60 segundos. Cobre criação sob demanda, leitores compartilhados,
escritor exclusivo, promoção, conflitos, reuso, nesting e descarte. Quatro threads
executam 16 rodadas por modo; --wrap=gomp_malloc_cleared, somente no executável de
teste, força candidatos concorrentes antes da instalação e detecta alocação sob
bucket por timeout. Página PROT_NONE garante ausência de acesso aos valores.
O alvo é separado do checkpoint e não testa read/write, publicação ou retry.

## Registro STM interno por tentativa

`make -C transaction_test -B check-register` (a partir da raiz) usa o archive da
libgomp reconstruída e os headers privados Linux x86_64. Aceita `GCC_BUILD`.
Verifica identidade/tamanho/kind, deduplicação inclusive aninhada, referências
compartilhadas entre contextos, descarte, contexto alheio/condenado e falhas sem
registros parciais (incluindo overflow injetado/restaurado). Uma página PROT_NONE
verifica que registrar não acessa valores; inspeções verificam ausência de
reservas, buffers e metadados. Timeout de 60 segundos; log `.build/register.log`.
O alvo é separado do checkpoint de oito alvos. Não valida acessos ou isolamento
STM, nem integra o registro interno à ponte pública de rastreamento.

# Exemplos da diretiva experimental transaction

Execute neste diretorio:

```sh
make check
```

O Makefile usa o GCC em `../gcc-build` pela localizacao relativa desta
arvore. Para outro build, use `make check GCC_BUILD=/caminho/do/build`.
O build deve conter C e a libgomp com os simbolos `GOMP_transaction_*`.
Os executaveis e logs ficam em `.build/`; o antigo `a.out` nao e utilizado.
Recompile os executaveis ao mudar de compilador ou flags, por exemplo:
`make -B check CFLAGS='-O0 -Wall -Wextra'`.

- `transaction_exemplo.c`: procura o maximo em paralelo, compara valor e posicao
  com uma referencia sequencial e testa um empate proposital. Lista os objetos
  efetivamente alterados (`maior` e `posicao`). Um `critical` externo sincroniza
  a atualizacao, pois transaction ainda nao oferece isolamento.
- `transaction_aninhamento.c`: executa uma regiao interna lexical e outra em
  funcao chamada. Exercita argumento repetido, reutilizacao do objeto externo e
  inclusao de um novo objeto na lista interna. Os objetos permanecem vivos ate
  a saida externa. Nao ha concorrencia neste caso.
- `transaction_carga.c`: carga parametrizada concorrente, com uma região
  transaction e uma operação lógica de leitura ou escrita por repetição.
  `verificar_carga.py` executa os três cenários de leitura/escrita do Makefile.
- `verificar.py`: verifica o resultado do maximo com 1, 2 e 4 threads solicitadas,
  as contagens dos logs e, no exemplo aninhado, a ausencia de liberacao nas saidas
  internas. Aceita equipes menores impostas pelo ambiente e relata seu tamanho.

Estes exemplos verificam o registro e a vida util da ponte de rastreamento.
A hash ja cria/destroi tabelas e adquire/consulta/libera referencias reais.
Address/size, visit e count também estão implementadas; as nove primitivas da
hash estão operacionais. Eles nao comprovam atomicidade, isolamento,
commit, abort ou reinicio. Os logs e sua verificacao precisarao ser revistos
quando a hash e a STM forem implementadas. Os `puts` internos do exemplo
aninhado servem apenas a esta etapa; nao representam E/S aprovada para uma
transacao com repeticao. Testes negativos de sintaxe, escopo e categorias de
identificadores continuam na suite DejaGNU do GCC (`transaction-*`).
## Carga parametrizada de leituras e escritas

O programa recebe exatamente três parâmetros, nesta ordem:

```sh
make all
.build/transaction_carga 4 1:9 1000
```

1. Número de threads a lançar, positivo e representável por int.
2. Relação **escritas:leituras**, com pesos inteiros não negativos e soma
   positiva representável por uint32_t. Por exemplo, 1:9 significa poucas
   escritas; 1:1, proporções iguais; 9:1, muitas escritas. Também aceita 0:1 e 1:0.
3. Número positivo de transações/repetições **por thread**. O total de
   transações precisa caber em unsigned long long.

Cada repetição executa uma região transaction sobre um objeto compartilhado.
Uma transação classificada como escrita faz read-modify-write; a relação não
conta instruções individuais de acesso. A sequência intercala os tipos de forma
determinística. Com N repetições e pesos W:R, cada thread faz
floor(N*W/(W+R)) escritas e as demais leituras; se N não completa um período,
a proporção é arredondada para baixo nas escritas.

Como ainda não há isolamento STM, um critical **dentro** da região protege os
valores compartilhados; o registro e a liberação de objetos continuam podendo
concorrer. O teste verifica contagens, valor final e consistência de um valor e
seu complemento. Não é um benchmark de STM nem comprovação de isolamento.
Desabilita equipes dinâmicas e falha claramente se o runtime não lançar a
quantidade pedida de threads, por exemplo por OMP_THREAD_LIMIT.

O alvo abaixo executa 1:9, 1:1 e 9:1 e confere também os logs da hash:

```sh
make check-carga
make check-carga CARGA_THREADS=8 CARGA_REPETICOES=10000
```

Os padrões são 4 threads e 1.000 repetições por thread. `make check` inclui
`check-carga` automaticamente. Aceita GCC_BUILD como os outros testes; use -B
para recompilar ao trocar o compilador ou flags. Logs em
`.build/transaction_carga-1-9.log`, `-1-1.log` e `-9-1.log`. O verificador limita
cada cenário a 60 segundos e usa Python com checagens explícitas, sem depender
de assert. `make run` continua executando apenas os exemplos sem argumentos.

## Criação e destruição da tabela hash

Execute `make -B check-hash-create-destroy` após reconstruir a libgomp.
O alvo aceita GCC_BUILD e usa headers/configuração privados do build Linux
x86_64, incluindo o fonte real e ligando o archive local, sem ampliar a ABI.
Verifica capacidades 1/3/1024, tabelas independentes, argumentos inválidos,
overflow, mutexes inicializados, BUSY repetido sem destruição parcial e liberação
da tabela vazia. Uma entrada ociosa é preparada apenas pelo teste para exercer
BUSY de forma independente do caminho de aquisição. Não testa inserção real ou estabelece
quiescência: esta continua sendo obrigação externa. Logs de depuração mantidos,
com saída em `.build/hash_create_destroy.log`.

## Aquisição, consulta e liberação de entradas

Execute `make -B check-hash-acquire` após reconstruir a libgomp. O teste inclui
o fonte real, usa headers/configuração privados do build Linux x86_64 e liga o
archive local. Cobre capacidades 1/3/1024, colisões, identidade compartilhada,
referências, NOT_FOUND sem inserção, SIZE_MISMATCH, nulos e overflow por injeção
interna controlada. Exercita concorrência com quatro threads e barreiras, sem
sleep, e liberação final/destrutor fora do mutex. Nenhum símbolo público ou
hook de produção foi acrescentado. Testa a mistura nativa com endereços
numéricos consecutivos/alinhados e capacidades 1/3/1009/1024/1536; não executa
nativamente a variante de outra largura. Logs em `.build/hash_acquire.log`,
com limite de 60 segundos. Aceita GCC_BUILD como os demais alvos.

## Consultas e visitas aos metadados

Execute `make -B check-hash-entry`. Usa configuração privada do build Linux
x86_64 e verifica endereço/tamanho imutáveis com referência retida, callback
sob mutex, referência preservada, instalação/substituição de metadados e
argumento nulo. Quatro threads atualizam um contador exclusivamente por visitas;
outro teste mantém um bucket bloqueado externamente enquanto uma visita em
outro bucket completa. Nenhum callback espera ou reentra na hash. A inspeção
do mutex Linux é restrita ao teste, sem hook/ABI novos. Log em
`.build/hash_entry.log`, limite de 60 segundos; aceita GCC_BUILD.

## Contagem consistente de entradas

Execute `make -B check-hash-count`. Conta entradas, não referências, com todos os
mutexes em ordem crescente e soma saturada em SIZE_MAX; libera em ordem inversa.
O teste verifica capacidades 1/3/1024, vazio, retenções repetidas, colisões,
saturação por injeção interna restaurada e concorrência com quatro threads.
Um destrutor com coordenação exclusiva de teste demonstra count zero durante
uma release ainda em andamento; destroy só é chamado depois de join. Não chamar
count dentro de callbacks/destruidores ou com mutex de bucket retido. Logs em
`.build/hash_count.log`, limite de 60 segundos, configuração privada Linux x86_64
e seleção de GCC_BUILD como nos demais testes. Printf/fflush fora dos locks.

## Diagnósticos sem inversão de locks

```sh
make -B check-hash-diagnostics
```

O teste interno Linux x86_64 mantém stdout ou stderr bloqueado enquanto outra
thread inicializa a ponte ou recebe falha de alocação injetada. Verifica que
nenhum log/diagnóstico fatal retém o lock global ou o mutex do bucket, mantendo
os formatos printf existentes. Quatro casos isolados: inicialização, OOM de
entrada, tabela e buckets. OOM continua fatal (saída 1); cada caso tem limite
de 10 segundos. Logs em `transaction_test/.build/hash_diagnostics-*.log`
(relativos à raiz); aceita GCC_BUILD. Interposição restrita ao teste, sem hooks
ou ABI novos. Em concorrência, o log create pode sair depois de operações que
já observaram a tabela publicada. A finalização agora usa o controle de admissão e participações descrito abaixo;
tabelas independentes continuam exigindo quiescência externa.

## Admissão e encerramento da tabela global

```sh
make -B check-hash-owner
```

O proprietário usa OPEN -> CLOSING -> CLOSED. O contexto externo retém uma
participação até concluir toda a limpeza; aninhamentos reutilizam-na. Durante
CLOSING novos usuários são recusados, mas os existentes podem terminar.
`newtm_hash_owner_close`, interface interna oculta, é não bloqueante quanto à
drenagem: retorna BUSY com usuários/coordenador/entradas residuais e pode ser
repetida após conclusão/join. Um único coordenador destrói a tabela drenada;
CLOSED não reinicializa e chamadas repetidas não destroem duas vezes.
Consumidores internos diretos da tabela global devem manter uma lease por todo
o intervalo de uso e referências; não podem destruir a tabela emprestada.

O finalizador automático faz uma tentativa: se ocupado, conserva a tabela e o
controle para recuperação pelo processo, sem esperar ou forçar limpeza. O mutex
de admissão sobrevive à tabela para rejeitar chamadas tardias. Isso não garante
segurança de dlclose: impedir execução atual/futura da biblioteca continua sendo
responsabilidade externa. Não há nova ABI pública GOMP ou semântica STM.

O teste Linux x86_64 cobre onze casos em processos separados, com limite de
15 segundos: sem uso, aninhamento, coordenadores concorrentes, limpeza e log de
criação pendentes, entradas residuais, corrida de admissão, erros/overflow,
chamada tardia e finalização com usuário ativo. Logs em
`transaction_test/.build/hash_owner-*.log` (relativos à raiz); aceita GCC_BUILD.

<<<<<<< HEAD

## Teste dos esqueletos de ciclo de vida

Depois de reconstruir a libgomp do build selecionado:

```sh
make check-lifecycle
```

`lifecycle_stubs.c` mantém seu nome histórico, mas agora testa contextos reais:
criação, transferência exclusiva da lease, aninhamento, rejeição de slot copiado
ou outra thread, overflow, INVALID dominante e limpeza sem publicação. Prepara
registros por acesso privado para testar buffers escalares/agregados, listas de
intervalos, leitores compartilhados e escritor; registro/reservas/acessos STM
normais continuam pendentes. A preparação manual não substitui esses serviços.

O verificador também executa processos que exigem diagnóstico fatal para
metadados com participações residuais e registro inválido na ponte, verificando
neste último a liberação antes da finalização. Os printf permanecem ativos,
incluindo kind escalar/agregado/tag desconhecida nos stubs de registro. O teste
liga ao archive libgomp.a e verifica ausência dos serviços internos na API
dinâmica. Requer compilador C, pthreads, Python 3 e nm.

`GCC_BUILD=/caminho/do/build` seleciona outro build. Logs em
`.build/lifecycle_stubs.log` e `.build/context-*.log`. O teste não comprova commit,
read/write STM, backoff ou isolamento. `make check` verifica exemplos e carga de
rastreamento, com sincronização auxiliar ainda necessária.

Os estados declarados em `enum newtm_attempt_state` e o protocolo de consumo
único estão especificados em
[ABORT](../gcc-src-tm/OPENMP_TRANSACTION_ABORT.md#máquina-de-estados-da-tentativa).
Este teste não executa transições nem libera recursos: verifica que os stubs
continuam sem efeitos e sem anunciar commit ou limpeza concluída.

O verificador lifecycle exige asserções Python habilitadas e recusa -O/-OO ou
PYTHONOPTIMIZE, evitando aprovar a validação de logs/símbolos sem executá-la.
Resultados da verificação geral e comandos DejaGNU com a libgomp local estão em
[OPENMP_TRANSACTION_AUDIT.md](../gcc-src-tm/OPENMP_TRANSACTION_AUDIT.md).
=======
>>>>>>> 775307665b55aaf0c7f28e25bcb02f20d9019458

## Checkpoint: contexto pronto para registro

`checkpoint_contexto_pronto_para_registro.py` reúne oito alvos: hash
(criação/destruição, acquire/lookup/release, consultas/visitas e contagem),
diagnósticos, owner, contexto e exemplos/carga. Seu cabeçalho descreve a cobertura
e as limitações. É uma referência de regressão da base atual antes de implementar
registro STM; não fecha as lacunas de cobertura identificadas para o contexto.

Reconstrua primeiro a libgomp do build escolhido. Depois, no contêiner gcc-tm:

```sh
make -C transaction_test check-checkpoint-contexto CFLAGS='-O2 -Wall -Werror'
```

O checkpoint força recompilação dos testes, executa os alvos sequencialmente e
interrompe na primeira falha ou timeout (180 segundos por alvo). Mantém os printf
nos logs e retorna código diferente de zero em falha. Variáveis GCC_BUILD,
TRANSACTION_CC, CFLAGS, CPPFLAGS, LDFLAGS, LDLIBS e parâmetros da carga são
repassados aos alvos. Não reconstrói a libgomp nem comprova correspondência entre
fontes alterados e um archive antigo.

Os logs individuais e `resultado.json` ficam em
`.build/checkpoint_contexto_pronto_para_registro/`. O relatório identifica as
variáveis usadas, comandos, resultados e o estado global da execução. Uma nova
execução invalida imediatamente a aprovação anterior. A mensagem final de
aprovação exige os oito alvos concluídos com sucesso.

Não verifica registro/acessos STM funcionais, isolamento, publicação ou retry.
Registros de limpeza são preparados manualmente; a carga ainda usa critical.
Sanitizadores e novos testes para lacunas do contexto não estão incluídos.

## Descritores de identidade do compilador

`make check-descritores` executa `verificar_descritores.py` com um plugin de teste
em High GIMPLE. Verifica campos calculados, avaliação única, identidade lógica,
kind/extensão de referências a elementos, limites por dimensão, índices extremos
e overflow em C/C++ O0/O2, canais de chamadas e descarte de metadados/cópias.
A sonda retorna antes do acesso inválido; essa proteção existe somente no teste.
Não valida integração funcional STM do pragma. Contrato e limites em
[descritores](../gcc-src-tm/OPENMP_TRANSACTION_DESCRIPTORS.md).
Relatórios e logs ficam em `.build/descriptors/`.
