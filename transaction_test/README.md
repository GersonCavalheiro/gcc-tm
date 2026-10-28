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
