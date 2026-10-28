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

# Agente de implementação — Abort e reinício de transações STM em OpenMP

## Estado e precedência

Este arquivo especifica trabalho futuro; não autoriza sua execução fora da tarefa
solicitada. A diretiva atual registra objetos numa hash operacional, com
leases de vida útil e encerramento do proprietário; abort/retry continuam
esqueletos, sem STM. Para a política inicial, aplicar
as restrições de dados e efeitos de OPENMP_TRANSACTION_HASH_AGENT.md.
Consultar [OPENMP_TRANSACTION_ACCESS_AGENT.md](OPENMP_TRANSACTION_ACCESS_AGENT.md)
para os contratos propostos de leitura/escrita. A hash mantém registros e
referências; as primitivas de acesso controlam reservas e dados privados; este
agente rege a propagação da falha, cleanups, descarte e reinício.
Assinaturas stm_* abaixo são pseudocódigo, sem ABI aprovada; newtm_read/write
e as seis operações newtm_* de ciclo de vida têm esqueletos internos declarados.
Seus contratos funcionais ainda não são executados. O inventário
[PRIMITIVES](OPENMP_TRANSACTION_PRIMITIVES.md) registra as assinaturas e a divisão
entre primitivas, helpers e fases. A propagação entre funções foi definida
por versões transacionais com contexto/status explícitos e resultado só em
sucesso; geração da cadeia de chamadas e seus epílogos ainda não implementadas.
A etapa dos esqueletos escolheu descarte explícito: end que falha preserva a
tentativa e o controle externo chama discard após cleanups; isso substitui o
modelo anterior de descarte implícito por end-false.

A camada proprietária OPEN/CLOSING/CLOSED já está implementada e não é a
máquina de estados da tentativa abaixo. A integração futura deve preservar a
lease externa enquanto houver referências, tentativas e cleanups dependentes;
consumir uma tentativa não equivale a fechar a tabela global. Usar visit e
release operacionais para metadados/referências, sem inferir reserva pela mera
presença na hash ou pela participação no proprietário.

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

Os ramos de desenrolamento/cleanups do usuário preservados nos contratos
funcionais gerais abaixo pertencem à extensão futura explicitamente suportada,
não ao primeiro subconjunto. No subconjunto inicial, falhas retornadas pelas
versões preparadas seguem epílogos com limpeza interna e descarte externo.

## Papel

Atue como agente de engenharia de compiladores e runtime, responsável por implementar o tratamento de falha de commit (*abort*) e a repetição de tentativas da diretiva experimental `#pragma omp transaction(...)` no GCC e na `libgomp`. Preserve a semântica abaixo. Não acrescente uma diretiva `abort` explícita nesta etapa.

## Escopo e decisões consolidadas

1. Uma transação é delimitada lexicalmente pelo bloco estruturado associado a `transaction`; o `{` não corresponde necessariamente a uma instrução ou endereço de máquina.
2. A primeira diretiva `transaction` encontrada dinamicamente por uma thread inicia a transação efetiva (*outermost*). Diretivas subsequentes, inclusive em outras funções, participam da mesma transação: **flat nesting**.
3. A lista de variáveis de cada diretiva adiciona endereços/objetos ao conjunto transacional efetivo. Uma transação interna não cria commit independente.
4. O encerramento normal de uma diretiva interna apenas conclui sua participação. Somente o encerramento da diretiva externa tenta validar e publicar as escritas.
5. Se o commit externo falhar, o controle externo conduz cleanups e descarte explícito. Somente conflito permite reiniciar **todo o bloco externo**, incluindo chamadas a funções internas, após descarte e espera bem-sucedidos. INVALID e exceções não autorizam retry; nunca reiniciar isoladamente o bloco interno.
6. Não há abort explícito nem transferência assíncrona de controle. Impedir o acesso conflitante antes de executá-lo e não continuar normalmente com valores inválidos. A falha pode ser propagada até o epílogo externo somente por um caminho seguro que impeça acessos e efeitos posteriores indevidos. A propagação entre funções usará versões internas com contexto/status explícitos e resultado só em sucesso; restringir casos ainda sem geração/epílogo seguro. **Não utilizar `setjmp`/`longjmp` para o caminho normal de retry**.
7. O estado transacional e a profundidade são locais à thread **sob a restrição de ausência de pontos de escalonamento de tarefas durante toda a transação**. Estruturas de coordenação (como uma tabela hash global de metadados) podem ser compartilhadas, com sincronização adequada.
8. Uma região `transaction` executa integralmente em uma única tarefa OpenMP, sem pontos de escalonamento de tarefas durante sua execução dinâmica. Transações aninhadas na mesma tarefa participam da transação externa. Não exigir `tied` como condição adicional: a ausência de suspensão/migração durante a transação é a propriedade decisiva.

## Transformação conceitual no compilador

Entrada:

```c
#pragma omp transaction(a, b)
{
    a++;
    b += a;
}
```

Saída conceitual (não é C executável nem ABI; stm_* são nomes ilustrativos):

```text
iniciar tentativa; verificar resultado e guardar indicador de entrada externa
registrar lista; verificar cada resultado
executar corpo instrumentado; desviar imediatamente em falha
stm_end: saída interna ou confirmação externa conforme profundidade
se houver falha de acesso/encerramento:
    propagar até o controle externo, executando cleanups suportados
    preservar a causa fora do contexto
    stm_rollback: descartar uma vez; exigir limpeza bem-sucedida
    se a causa era CONFLICT: esperar sem reservas e repetir bloco externo inteiro
    se permanente ou exceção do usuário: tratamento próprio, sem retry
```

**Invariante fundamental:** nenhuma falha pode reiniciar apenas o bloco interno.
Após falha de acesso, impedir a continuação normal e propagar a causa para o
controle da tentativa externa por caminho seguro. Isso não significa executar
o restante do corpo até uma falha de commit: é proibido consumir resultado
inválido ou produzir efeitos posteriores indevidos. Falha permanente não é
conflito repetível. Rejeitar casos sem mecanismo seguro de propagação.

### API ilustrativa do runtime

- `stm_begin(lista_de_enderecos) -> outermost`: se não houver transação ativa, inicializa estado e retorna verdadeiro; caso contrário, incrementa profundidade, registra novas variáveis e retorna falso.
- `stm_read` / `stm_write`: nomes ilustrativos das operações detalhadas como
  `newtm_read` / `newtm_write` no agente de acesso; verificar o estado antes de
  consumir o resultado ou continuar com efeitos do programa.
- `stm_end`: saída normal conceitual; interna reduz profundidade, externa saudável
  publica e libera recursos. Falha preserva tentativa para descarte explícito.
  O esqueleto interno é newtm_end(slot), com enum de resultado, sem bool de entrada.
- `stm_rollback`: descarte exatamente uma vez por tentativa fracassada, com
  propriedade definida na seção de pré/pós-condições abaixo; evitar rollback duplicado.

O indicador outermost de begin só vale após OK e orienta o controle externo.
newtm_end consulta profundidade/estado/proprietário, sem recebê-lo como parâmetro.
Ao concluir ou descartar, encerrar a tentativa e consumir seu slot conforme o
protocolo abaixo. Ausência de contexto não é um estado reutilizável da tentativa.

## Máquina de estados da tentativa

Definição consolidada em 2026-10-21. `enum newtm_attempt_state` está declarado
em `libgomp/newtm-transaction.h`; esta seção define suas transições e propriedade
para a implementação. Contexto privado, criação, condenação e descarte já são
funcionais; PUBLISHING e confirmação externa continuam futuros. O enum não é
retorno de operação, ABI pública, nem estado do contexto de rastreamento GOMP.
Não converter entre ele e os enums de resultado por cast ou comparação numérica.

### Estados e invariantes

| Estado | Significado e recursos |
| --- | --- |
| `NEWTM_ATTEMPT_ACTIVE` | Tentativa apta a registrar/acessar objetos e entrar/sair de níveis. Profundidade positiva; contexto retém referências, reservas e buffers adquiridos. Preparação falível da confirmação ainda ocorre aqui. |
| `NEWTM_ATTEMPT_CONFLICTED` | Tentativa inteira condenada por conflito. Nenhum novo acesso normal, registro, nível ou confirmação; preserva profundidade e recursos para cleanups suportados e descarte externo. Pode ser agravada para INVALID. |
| `NEWTM_ATTEMPT_INVALID` | Tentativa válida como estrutura, mas permanentemente inválida como execução. Mesmas restrições e retenção de recursos de CONFLICTED; não volta a CONFLICTED/ACTIVE e não autoriza retry. Não significa ponteiro inválido. |
| `NEWTM_ATTEMPT_PUBLISHING` | Confirmação externa irrevogável dentro de end, profundidade 1. Toda preparação falível e validação concluídas; mantém todas as reservas até publicar todas as escritas. Não admite acessos normais, falha retornável ou descarte. |
| `NEWTM_ATTEMPT_CLOSED` | Estado lógico terminal. Slot proprietário desvinculado; somente o consumidor end/discard conserva acesso privado aos recursos residuais para liberá-los. Não implica, sozinho, publicação, término da limpeza ou sucesso. Após free, não há contexto consultável. |

O estado CLOSED é estabelecido **antes da primeira liberação**, permitindo
rejeitar novo consumo enquanto a estrutura ainda existe. Não se cria um estado
adicional de limpeza: ela é uma fase exclusiva do consumidor de CLOSED.
Profundidade vira zero ao fechar; todas as participações internas são consumidas.
Nenhuma operação normal observa um contexto CLOSED pelo slot proprietário.

### Transições permitidas

Na tabela, os nomes curtos correspondem a `NEWTM_ATTEMPT_*`. Toda chamada requer
execução proprietária e argumentos válidos; as regras de erro estão abaixo.
Ausência de contexto (`*slot == NULL`) não integra o enum.

| Origem | Evento/responsável | Destino e efeito |
| --- | --- | --- |
| Sem contexto | begin externo bem-sucedido | Nova ACTIVE, profundidade 1; publicar no slot somente depois da inicialização completa. |
| ACTIVE | begin interno / register / read / write bem-sucedido | ACTIVE; só begin incrementa profundidade; registro e acesso mantêm seus contratos próprios. |
| ACTIVE, profundidade > 1 | end interno bem-sucedido | ACTIVE, profundidade menos 1; nenhum recurso liberado ou valor publicado. |
| ACTIVE | Conflito detectado por acesso/preparação de end, registrado por fail | CONFLICTED; preservar slot, profundidade e recursos, sem realizar o acesso ou publicar. |
| ACTIVE ou CONFLICTED | Violação permanente detectada em contexto válido e proprietário | INVALID; preservar recursos e profundidade. |
| CONFLICTED | Nova sinalização de conflito | CONFLICTED; nenhuma limpeza. |
| INVALID | Sinalização de qualquer causa | INVALID; causa permanente nunca é rebaixada. |
| ACTIVE, profundidade 1 | end externo termina toda preparação falível | PUBLISHING, antes da primeira escrita original; somente end conduz esta fase. |
| PUBLISHING | end termina todas as publicações | CLOSED, desvincula slot e zera profundidade; end libera recursos e só então retorna OK externo. Vale também para tentativa sem escritas. |
| ACTIVE / CONFLICTED / INVALID | discard pelo controle externo após cleanups suportados | CLOSED, desvincula slot e zera profundidade; discard libera recursos sem publicação e só então retorna OK de limpeza. ACTIVE cobre desenrolamento explicitamente suportado. |
| CLOSED | Nenhuma nova operação | Sem transição; consumidor já designado conclui liberação física. Memória liberada não é um novo estado. |

Transições não listadas são proibidas. Em particular, CONFLICTED/INVALID não
chegam a PUBLISHING; PUBLISHING não retorna a ACTIVE, CONFLICTED ou INVALID;
CLOSED não reabre. Backoff ocorre **sem contexto**, após descarte concluído por
conflito; begin seguinte cria outra tentativa, mesmo que o alocador reutilize
o endereço. Contador de conflitos pertence ao controle externo, não à tentativa.

### Rejeições e resultados

- Em CONFLICTED, begin/register/read/write/end não executam seu efeito normal
  e devolvem CONFLICT no domínio da operação; uma violação permanente detectada
  tem prioridade e agrava o estado para INVALID. Em INVALID, devolvem INVALID.
  Não mudar profundidade em end rejeitado, inclusive no nível interno.
- fail só aceita contexto proprietário ainda não publicado. Causa ACCESS_OK
  ou desconhecida é uso permanente inválido; em contexto válido condena para
  INVALID. Nunca retorna OK e nunca limpa recursos ou transfere controle.
- Nulo, slot vazio quando se exige tentativa, contexto alheio ou outra entrada
  inválida detectável retornam INVALID sem consumir o slot ou alterar memória
  alheia. begin admite slot vazio para criar uma tentativa. Não desreferenciar
  ponteiro pendente para tentar validá-lo; sua detecção não é garantida.
- Nenhuma chamada pública interna pode reentrar em PUBLISHING ou na liberação
  de CLOSED. Violação estrutural detectada durante publicação/liberação é fatal:
  não retornar falha parcial, tentar rollback ou reexecutar a limpeza.
  Um CLOSED ainda vivo e acessível por empréstimo legítimo não autoriza operação
  normal nem transferência da propriedade; tentativa de uso é inválida.
- end externo só retorna OK depois de publicar tudo **e** concluir a liberação;
  end interno OK significa apenas saída de nível. discard OK significa somente
  limpeza concluída. Estado CLOSED e slot NULL não distinguem esses resultados.

### Proprietário único e prevenção de dupla limpeza

Cada tentativa possui **um único slot proprietário canônico**, compartilhado
por todas as entradas aninhadas da mesma execução. Não copiar seu ponteiro para
outro slot e tratá-lo como segundo proprietário. Ponteiros usados por registro,
acesso e fail são empréstimos; não podem liberar o contexto nem sobreviver a seu
consumo. As chamadas são serializadas pela execução proprietária, sem suspensão
de tarefa; este protocolo não promete arbitrar duas threads consumindo o mesmo
contexto nem exige, nesta definição, um novo lock ou API de destruição.

| Caminho | Quem conduz cleanups de linguagem | Quem consome e libera recursos da tentativa |
| --- | --- | --- |
| Saída interna saudável | Fluxo normal da linguagem | Ninguém; end apenas reduz profundidade. |
| Confirmação externa | Fluxo normal, antes da publicação | Uma chamada de end externo após publicar tudo. Não chamar discard depois. |
| Conflito, INVALID ou desenrolamento suportado | Propagação até o controle externo, com recursos ainda válidos | Uma chamada de discard feita exclusivamente pelo controle externo. Epílogos internos não descartam nem executam end normal durante essa propagação. |
| Falha de begin sem criar tentativa | Somente recursos locais eventualmente preparados pela entrada | begin desfaz preparação parcial; slot permanece NULL, sem discard de contexto inexistente. |

Protocolo obrigatório do consumidor funcional (fase, não nova função):

1. Validar slot, proprietário, estado e caminho de consumo antes de modificar
   qualquer recurso. Falha detectável nessa validação preserva o slot.
2. No descarte, concluir antes todos os cleanups suportados. Capturar a causa
   **efetiva após os cleanups** e eventual exceção fora do contexto: erro
   permanente surgido no desenrolamento prevalece sobre conflito. Não presumir
   que read/write normais funcionem em contexto condenado; o protocolo especial
   de cleanups do usuário pertence à extensão futura; esses casos são rejeitados
   inicialmente. A limpeza interna de recursos permanece obrigatória.
3. Guardar contexto em variável privada do consumidor; marcar CLOSED, zerar
   profundidade e escrever NULL no slot canônico, sem callback ou reentrada
   entre essas ações. Nenhuma liberação começa antes de ambas as guardas
   (estado terminal e retirada do slot). No caminho de end, isso só ocorre
   depois de todas as escritas terem sido publicadas sob reservas.
4. Consumidor retira reservas/participações antes de liberar as referências que
   as sustentam, libera buffers e registros uma única vez e finalmente contexto.
   No descarte, abandonar valores privados sem copiá-los para os originais.
   Nenhum callback/destrutor desta fase pode reentrar no ciclo de vida, criar
   tentativa ou repetir liberação. Falha estrutural irrecuperável é fatal.
5. Só após a liberação completa retornar OK correspondente à operação. Nenhum
   epílogo deve testar apenas slot NULL para concluir sucesso. No descarte,
   conferir resultado e causa salva: só CONFLICT, sem exceção pendente ou erro
   permanente, permite backoff e nova tentativa.

Assim, segundo discard/end usando o **mesmo slot**, já vazio, retorna INVALID
sem segunda limpeza. Cópias antigas de ponteiro não são handles alternativos;
usá-las depois do consumo viola o contrato e não pode ser corrigido consultando
um campo na memória liberada. Detecção de endereço reutilizado não é prometida.
Nem finalmente nem exceção devem executar um segundo consumidor após sucesso.

### Limite dos esqueletos e validação futura

Begin/fail/discard já implementam as transições antes de publicação, e end reduz
somente níveis internos. End externo condena com INVALID sem publicar. Os stubs
restantes continuam retornando INVALID; seus logs não comprovam STM funcional.
O teste de lifecycle verifica consumo único, causa dominante e liberação real.

Ao implementá-los, testar transições válidas e rejeitadas, prioridade INVALID,
falha de end interno sem reduzir profundidade, falha antes da publicação com
recursos intactos, ausência de falha retornável após entrar em PUBLISHING,
exceção/erro em cleanup, ausência de retry permanente e contador de liberações
exatamente uma vez tanto no commit quanto no descarte. Testar segundo consumo
via slot vazio; não fabricar teste que leia contexto já liberado. Não usar o
teste atual dos stubs como prova dessas garantias funcionais.

## Pré e pós-condições das operações de ciclo de vida

Os nomes stm_* são conceituais. As assinaturas internas newtm_* constam no
inventário e no cabeçalho newtm-transaction.h; a ponte GOMP void continua com
contrato de rastreamento separado. Os efeitos funcionais abaixo permanecem futuros.

### stm_begin(lista_de_enderecos) -> outermost

Pré-condições: runtime inicializado, execução proprietária válida, sem pontos de
escalonamento de tarefas; não há tentativa ou a existente está ativa e não
condenada. Lista válida segundo o contrato de registro, objetos vivos e tipos
suportados. Profundidade e contadores podem crescer sem overflow.

Pós-condições em sucesso: se externo, contexto/tentativa novos, profundidade um,
indicador outermost=true em OK; se interno, mesma identidade e dados privados,
profundidade +1 e outermost=false em OK. No modelo conceitual com lista, objetos
registrados sem leitura antecipada nem reserva. Duplicatas
são idempotentes e listas internas ampliam o conjunto a partir dessa entrada.
Nenhuma publicação, reinicialização da tentativa externa ou herança por outra
thread é permitida.

Falha: não publicar nível parcial; preservar slot/profundidade e escrever
outermost=false quando a saída for válida. O enum lifecycle separa sucesso,
conflito e erro permanente; o bool não codifica erro. Alocação é fatal.
newtm_begin recebe lease a transferir, slot e outermost; o registro ocorre separadamente por
newtm_register após entrada OK. Contexto condenado não entra em nível normal.

### stm_end: encerramento normal; interface interna newtm_end(slot)

Pré-condições: entrada correspondente ainda ativa, proprietário correto,
profundidade positiva e término normal de região; slot e proprietário válidos.
Não chamar esse caminho depois de abort que já desfez o nível. Falhas
de acesso já sinalizadas desviam para descarte; não executar o restante do corpo
para alcançar end. Cleanups que precisem da visão ativa devem ter protocolo e
ordem definidos antes de sua invalidação.

Pós-condições internas: retorno NEWTM_LIFECYCLE_OK, profundidade -1, identidade, reservas,
registros e valores privados preservados; nenhum commit independente. Tentativa
condenada não pode retornar OK como se estivesse saudável.

Pós-condições externas em sucesso: retorno NEWTM_LIFECYCLE_OK somente depois de publicar todas
as escritas com reservas mantidas e depois retirar todas as participações e
referências, liberar recursos e desativar contexto/profundidade. A execução não
pode observar publicação parcial pelo protocolo.

Em CONFLICT/INVALID antes da publicação, end preserva slot, profundidade,
reservas, referências e buffers para os cleanups e o descarte explícito pelo
controle externo. Contexto válido fica condenado pela causa efetiva, mantendo
prioridade de INVALID sobre CONFLICT. Não chamar end para consumir a falha como término saudável.
Após o descarte OK, repetir apenas se a causa salva for CONFLICT; falha permanente
tem tratamento separado. Não inventar revalidação por versões ou conflito de
commit obrigatório: a política detecta conflitos nos acessos. Depois da primeira
publicação, nenhuma falha recuperável ou rollback é permitido.

Esta escolha substitui a proposta anterior de end-false que já descartava a
tentativa. Nenhum chamador funcional existia para migrar; a ponte GOMP permanece
independente e os stubs retornam INVALID sem executar qualquer descarte.

### stm_rollback: operação de descarte

Pré-condições: contexto válido e proprietário, tentativa ainda não publicada,
condenada ou encaminhada explicitamente para descarte. Nenhum acesso concorrente
usa seus buffers. Executar os cleanups suportados enquanto os recursos requeridos
continuam válidos, antes de descartá-los. Não usar rollback após commit ou após
outro descarte da mesma tentativa.

Pós-condições: nenhuma escrita especulativa publicada; reservas/participações
retiradas, referências/buffers liberados exatamente uma vez, todos os níveis
internos desfeitos, tentativa CLOSED e contexto liberado/slot NULL. Profundidade
zerada antes da liberação, sem consultar contexto depois de free. Valores originais não
precisam ser restaurados pois não foram escritos; efeitos externos não são
revertidos. Preservar a causa para escolher retry ou erro permanente.

O dono do descarte é sempre o controle externo: chama newtm_discard após falha
de acesso ou end, com cleanups suportados já executados e causa salva fora do
contexto. OK consome o contexto, zera slot e significa somente limpeza concluída;
INVALID detectável preserva slot e não permite retry. Não retorna CONFLICT.
Rollback não reinicia código nem espera. Duplo descarte é violação, não uma
idempotência garantida. Falha estrutural na limpeza não autoriza publicar,
continuar com estado inconsistente ou repetir indefinidamente.

### Confirmação como fase e backoff como helper interno

Confirmação: exige tentativa saudável, recursos preparados e reservas compatíveis
mantidas para todos os objetos; nenhuma operação falível após a primeira escrita
publicada. Ao concluir, todas as escritas são visíveis pelo protocolo antes de
liberar qualquer reserva. Falha recuperável só é possível antes da publicação.
Essa é uma fase de end, não a exigência de uma nova função com ABI já escolhida.

Backoff: exige descarte concluído, nenhuma reserva/mutex retido e nenhuma referência
a buffers invalidados. Ao retornar, não modifica dados do programa nem reativa o
contexto: apenas permite nova tentativa. Contadores e duração usam aritmética
limitada, espera progressiva com teto por espera e tentativas sem limite conforme
a política. Não promete justiça ou conclusão; falha permanente não entra nesse
caminho. O helper interno newtm_backoff(size_t attempt) recebe contagem consecutiva de
conflitos, primeira espera=1, saturada em SIZE_MAX pelo chamador. Fórmula temporal
e jitter continuam pendentes; o stub não espera e retorna INVALID.

## Estados de acesso e passagem para descarte

Usar o contrato proposto do agente de acesso sem duplicar suas assinaturas:

| Retorno | Ação do chamador e do controle transacional |
| --- | --- |
| `NEWTM_ACCESS_OK` | Consumir o resultado e continuar a tentativa ativa. |
| `NEWTM_ACCESS_CONFLICT` | Não consumir resultado; a tentativa inteira está condenada. Propagar a falha e conduzir descarte antes do retry externo. |
| `NEWTM_ACCESS_INVALID` | Falha permanente detectada; impedir commit, não consumir resultado nem iniciar retry automático. Propagar até o controle externo, limpar com segurança e emitir diagnóstico fatal; sem novo resultado na linguagem. |

A primitiva não deve liberar globalmente reservas, buffers e referências ao
retornar falha. Para contexto válido e proprietário correto, registrar a causa
sem perder o motivo permanente. Não acessar contexto inválido/de outra execução
para tentar condená-lo. A hash não implementa essa máquina de estados.

Sequência de tratamento de uma tentativa condenada:

1. Interromper o caminho normal no primeiro retorno de falha. Preservar a causa;
   recusar novos acessos normais e impedir qualquer confirmação da tentativa.
2. Propagar para o controle externo, inclusive através de chamadas preparadas,
   executando os cleanups suportados enquanto contexto, reservas e dados privados
   necessários ainda estão válidos. Não usar o caminho normal de acesso em
   contexto condenado como solução implícita para cleanups.
3. Descartar os valores especulativos, retirar reservas/participações e liberar
   referências/buffers conforme sua propriedade, exatamente uma vez por tentativa.
   Desfazer a profundidade inteira sem executar depois epílogos normais de níveis
   internos já desfeitos.
4. Em CONFLICT, aguardar sem reservas/locks e reiniciar todo o bloco externo,
   recriando temporários e processando novamente as listas dinâmicas.
5. Em INVALID detectado com contexto válido, concluir a limpeza segura e emitir
   diagnóstico fatal. Nunca repetir, publicar ou devolver resultado à linguagem.
   Falta de memória permanece fatal.

Decisão do usuário: gerar versões internas transacionais com contexto e
status explícitos, preservando a assinatura pública. Preencher resultado
somente em OK; em falha, o chamador não o usa e segue ao próprio epílogo de
falha, propagando a causa pela cadeia. Funções internas não descartam nem
confirmam a tentativa externa. O contexto/lease permanece válido até todos os
cleanups suportados e o descarte único externo. Não há retorno fictício para
simular sucesso, nem setjmp/longjmp no retry normal.

Chamadas indiretas ou externas sem preparação continuam restritas; pure/const
não prova que uma função lê a visão privada. A ABI das versões geradas ainda precisa de especificação e implementação.
Exceções/cleanups do usuário são rejeitados inicialmente; o protocolo especial
para acessos de destrutores em tentativa condenada é trabalho futuro.
Não converter exceção em CONFLICT nem executar commit durante desenrolamento.
A limpeza interna continua obrigatória, sem usar read/write normais para
contornar os guards de uma tentativa condenada. Ver o
[contrato de chamadas](OPENMP_TRANSACTION_HASH_AGENT.md#versões-transacionais-e-propagação-de-falha).

O TRY_FINALLY atual pertence ao rastreamento. Na integração funcional, separar
commit no término normal, descarte no desenrolamento e decisão de reinício.
Nunca confirmar automaticamente no finally de uma tentativa condenada.

## Semântica dos dados e efeitos colaterais

**Garantia restrita aos dados transacionais:** o commit bem-sucedido deve tornar visíveis as escritas transacionais como uma unidade, segundo o protocolo de isolamento e validação escolhido. Uma tentativa abortada não pode deixar alterações visíveis nesses objetos. Leituras devem observar as próprias escritas especulativas (*read-your-writes*).

**Dados não transacionais:** aplicar a política restritiva do agente da hash:
leituras e escritas em objetos cuja vida começou antes da transação externa exigem registro
prévio na lista externa ou em uma lista interna já processada. Não ampliar o
conjunto implicitamente. Restringir E/S e chamadas com efeitos externos que não
possam ser descartados ou repetidos com segurança; diagnosticar os casos
identificáveis. Esses efeitos não são revertidos pelo protocolo e a ausência de
diagnóstico não constitui suporte. A permissão anterior de leituras externas
comuns foi substituída: todos os acessos preexistentes exigem registro explícito.
Ponteiros sem identidade conhecida continuam rejeitados, sem fallback comum.

Variáveis locais automáticas merecem análise separada: ao reiniciar o bloco via salto gerado, o compilador deve preservar a semântica de C/C++ e a inicialização/vida útil dos objetos. Não pressupor que `goto retry` reverta variáveis locais modificadas na tentativa anterior. Para C++, considerar construtores, destrutores e exceções; não emitir saltos que violem regras de inicialização ou destruam incorretamente objetos.

**Adição tardia de variáveis:** quando uma transação interna acrescenta um objeto ao conjunto transacional, isso não torna retroativamente transacionais acessos anteriores ao mesmo objeto. Definir uma política explícita: proibir tais acessos, diagnosticá-los quando possível, ou declarar que estão fora da garantia. Não fazer correções silenciosas.

**Concorrência:** o protocolo deve assegurar que validação e publicação não tenham janela de corrida, e que threads concorrentes não observem escritas parciais. Uma tabela hash global com sinalizadores `limpo/sujo` não basta, por si só, para assegurar isolamento, versionamento, detecção de conflitos e publicação atômica. Especificar política de conflitos, ownership/versionamento, ordem de memória e sincronização antes de declarar a implementação correta.

## Interação com tarefas OpenMP e pontos de escalonamento

**Regra semântica obrigatória:** uma região `transaction` deve executar integralmente no contexto de uma única tarefa OpenMP, **sem encontrar pontos de escalonamento de tarefas** desde a abertura da transação externa até seu encerramento. A proibição é **dinâmica e transitiva**: alcança transações internas e todas as funções chamadas durante a tentativa, inclusive código compilado separadamente. Uma tarefa pode executar `transaction` e tarefas distintas podem executar transações concorrentes; o que não se admite é suspender ou trocar de tarefa enquanto uma transação está ativa na thread.

**Proibir dentro de uma transação ativa:**

- `taskyield` e `taskwait`, incluindo variantes com dependências;
- criação/encontro de `task`, `taskloop` e demais construções geradoras de tarefas, pois podem introduzir pontos de escalonamento, ainda que uma tarefa seja executada imediatamente;
- `taskgroup`, `barrier` e pontos de espera/sincronização que introduzam escalonamento;
- `parallel` e construções de compartilhamento de trabalho (`single`, `sections`, `for`/`do` etc.) **na versão inicial**, como restrição conservadora para evitar tarefas implícitas, barreiras e caminhos de sincronização; não alegar que toda variante dessas construções necessariamente suspende a tarefa;
- cancelamento e outros caminhos que escapem ao epílogo transacional.

O conjunto acima é uma **política inicial deliberadamente conservadora**, não uma enumeração exaustiva de todos os pontos de escalonamento da especificação OpenMP. Conferir a versão da especificação adotada e abranger pontos implícitos, inclusive os introduzidos por chamadas ao runtime. Uma construção não listada também deve ser rejeitada se puder encontrar um ponto de escalonamento durante a transação.

**`tied` não basta:** a afinidade de uma tarefa `tied` à thread que a iniciou não impede sua suspensão e a execução de tarefas descendentes pela mesma thread; um TLS contendo a transação da tarefa suspensa poderia ser confundido com o contexto de outra tarefa. **Não exigir `tied` por si só**: se nenhum ponto de escalonamento for encontrado enquanto a transação está ativa, uma tarefa `untied` também não migra nesse intervalo. A regra deve ser formulada em termos de ausência de escalonamento, e não apenas de tipo da tarefa.

**`critical`, `atomic` e locks:** não são automaticamente proibidos pela regra de pontos de escalonamento, mas tampouco são automaticamente seguros. `critical` e locks podem bloquear ou causar deadlock em combinação com outras transações; `atomic`, `critical` e operações de lock que alterem estado não transacional produzem efeitos que **não são desfeitos** por rollback. Proibir ou diagnosticar o acesso a objetos transacionais por vias não instrumentadas; documentar explicitamente o tratamento de operações bloqueantes e de ordem de aquisição de locks. No primeiro subconjunto, rejeitar locks do usuário e estas sincronizações do
programa dentro da tentativa. O critical da carga atual é apoio ao rastreamento,
a reavaliar quando houver teste de isolamento STM real; não autoriza seu uso
no futuro subconjunto funcional.

**Verificação em duas camadas:**

1. **GCC:** diagnosticar/rejeitar construções reconhecidas estaticamente dentro de `transaction`, inclusive saídas que contornem o epílogo; verificar o alcance de chamadas quando houver informação suficiente, sem presumir que uma função desconhecida é segura.
2. **`libgomp`:** acrescentar verificações nos caminhos de runtime que possam gerar pontos de escalonamento, consultando o estado transacional da thread e **falhando de modo explícito antes de suspender a tarefa**. Definir um mecanismo de erro/diagnóstico; não simplesmente marcar a transação para retry, pois uma violação estrutural persistente causaria repetição infinita. Chamadas de bibliotecas externas que realizem escalonamento sem passar pelos pontos instrumentados podem exigir restrição contratual adicional.

**Efeitos colaterais não transacionais:** obedecem às restrições acima. Ausência
de pontos de escalonamento não autoriza E/S, escritas externas não listadas ou
chamadas sem suporte. Exceções e cleanups exigem contrato próprio e não devem
ser convertidos em repetição automática.

### Exemplos de aceitação e rejeição

Permitido, desde que o corpo não contenha ponto de escalonamento nem chamada que o introduza:

```c
#pragma omp task
{
    #pragma omp transaction(a)
    { a++; }
}
```

Permitido: `transaction` externa chamando função que executa `transaction` interna, ambas na mesma tarefa e sem ponto de escalonamento.

Rejeitar:

```c
#pragma omp transaction(a)
{
    a++;
    #pragma omp taskyield
}
```

Rejeitar mesmo que a função esteja em outra unidade de tradução (diagnóstico estático quando possível, senão detecção em runtime):

```c
void auxiliar(void) { /* pode chamar omp_taskyield() ou encontrar taskwait */ }
#pragma omp transaction(a)
{
    a++;
    auxiliar();
}
```

## Restrições iniciais de controle de fluxo

- O reinício é feito por fluxo de controle gerado no **bloco externo**, não por endereço obtido do caractere `{`.
- Transações aninhadas em outras funções são suportadas se todas as chamadas executarem na mesma thread e retornarem normalmente até o commit externo.
- Não permitir que um `return`, `goto` para fora do bloco, `break`/`continue` que escape à região, `longjmp`, exceção C++ ou cancelamento OpenMP pule silenciosamente o epílogo transacional. Implementar tratamento explícito ou emitir diagnóstico/rejeitar a construção na primeira versão.
- Proibir pontos de escalonamento de tarefas durante toda a transação ativa, inclusive em chamadas indiretas; essa restrição torna o estado TLS apropriado durante a tentativa, sem exigir que a tarefa seja `tied`.
- Não fazer retry infinito de falhas permanentes sem política documentada de limitação, backoff ou erro.

## Plano de implementação

1. Identificar o parsing e a representação intermediária apropriados para a nova diretiva GCC; representar a lista de variáveis e o corpo estruturado.
2. Implementar lowering com início, epílogo e aresta de retry. Assegurar que o rótulo de retry pertença à região externa dinâmica e que o compilador não gere salto de retry para um bloco interno.
3. Implementar estado TLS: ativo, profundidade, conjuntos de leitura/escrita, metadados de validação e estado de falha.
4. Instrumentar leituras/escritas conforme o agente de acesso, preservando
   identidade e remapeamento OpenMP. Verificar cada retorno antes do uso do
   resultado; propagar CONFLICT e INVALID por caminhos distintos. Manter
   escritas privadas e reservas conforme a política do agente da hash.
5. Implementar validação, publicação atômica e descarte no runtime; documentar a linearização do commit.
6. Tratar inclusão dinâmica de variáveis por transações internas e garantir que o commit interno não publique alterações.
7. Implementar diagnósticos no GCC e verificações na `libgomp` para pontos de escalonamento de tarefas, inclusive em funções chamadas; rejeitar a violação sem permitir suspensão nem retry infinito.
8. Documentar a semântica limitada dos efeitos colaterais não transacionais e as restrições de controle de fluxo.
9. Adicionar testes de compilação, execução concorrente, aninhamento entre funções, conflito e reinício.

## Testes mínimos de aceitação

- Transação isolada: commit bem-sucedido exatamente uma vez.
- Conflito entre duas threads: ao menos uma tentativa é repetida; resultado final dos objetos transacionais equivale a uma ordem serial válida para os casos suportados.
- Aninhamento na mesma função: apenas a diretiva externa publica.
- Aninhamento em outra função: falha no commit externo reinicia a chamada à função interna e todo o bloco externo.
- Função com `transaction` chamada isoladamente: sua diretiva torna-se externa dinamicamente e faz commit próprio.
- Inclusão de nova variável em diretiva interna: o objeto participa do commit externo.
- Efeito não transacional incompatível: rejeição/diagnóstico conforme o subconjunto definido; nunca afirmar rollback desse efeito.
- Saídas antecipadas e exceções: rejeição clara ou epílogo correto, nunca vazamento silencioso de transação ativa.
- `transaction` em tarefa `tied` e `untied` sem pontos de escalonamento: execução correta, inclusive com outras tarefas transacionando em paralelo.
- `taskyield`, `taskwait`, geração de `task`/`taskloop`, `taskgroup`, barreiras e construções conservadoramente proibidas dentro de transação: diagnóstico ou erro explícito antes de qualquer suspensão.
- Função chamada (inclusive compilada separadamente) que alcança ponto de escalonamento: detecção em runtime onde houver interceptação, sem corromper TLS nem reiniciar indefinidamente.
- `critical`, `atomic` e locks: testar apenas combinações explicitamente suportadas; demonstrar que efeitos não transacionais não são revertidos e detectar combinações proibidas.
- Falha de leitura: destino preservado e nenhum consumo posterior de resultado
  inválido; falha de escrita: nenhuma aplicação parcial da alteração solicitada.
- INVALID: erro permanente sem retry infinito nem mascaramento como CONFLICT.
- Cleanups suportados veem contexto ainda válido; descarte e liberação ocorrem
  exatamente uma vez, sem epílogo normal residual de região interna desfeita.
- Após abort/commit: profundidade zero, metadados consistentes, nenhum lock ou estado especulativo abandonado.

## Critérios de entrega

Produzir alterações de código, testes reproduzíveis e documentação das decisões semânticas. Distinguir explicitamente garantias implementadas de limitações. Não acrescentar diretiva de abort explícito, não implementar nested commit independente e não utilizar `setjmp`/`longjmp` sem uma nova decisão de projeto justificada.


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

## Esqueletos de ciclo de vida entregues

Quatro primitivas: newtm_begin, newtm_register, newtm_end, newtm_discard. Dois
helpers compartilhados internos: newtm_fail e newtm_backoff. Saída interna e
confirmação externa são fases de end; retry é fluxo gerado, sem símbolos próprios.
newtm_fail registra futuramente a causa efetiva sem limpar ou transferir controle;
recebe enum de acesso e devolve enum lifecycle, com INVALID permanente dominante.
Read/write ainda não o chamam. Estado/layout do contexto não foram implementados.

Os seis stubs só imprimem parâmetros e fazem fflush; todos retornam
NEWTM_LIFECYCLE_INVALID. begin também inicializa outermost não nulo com false.
Slots e dados são preservados, inclusive em end/discard. Nenhuma função é
exportada, nenhuma ABI GOMP foi alterada e nenhum builtin foi acrescentado.
Contratos completos e validação: [PRIMITIVES](OPENMP_TRANSACTION_PRIMITIVES.md) e
[IMPLEMENTATION](OPENMP_TRANSACTION_IMPLEMENTATION.md).
