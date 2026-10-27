# Agente de especificação: etapa de hash operacional de transaction

## Missão e autorização

Especificar a passagem da hash de esqueletos para operações reais e a adaptação
simultânea da ponte GOMP_transaction_begin/register/end. Este documento não
implementa nem autoriza antecipadamente essa etapa: sua execução depende de
tarefa posterior do usuário. Não implementar STM como consequência desta etapa.

Referências normativas do projeto:

- [Guia geral](OPENMP_AGENT.md).
- [Panorama e estado atual](OPENMP_TRANSACTION_PROJECT_AGENT.md).
- [Contratos da hash e da ponte](OPENMP_TRANSACTION_HASH_AGENT.md).
- [Inventário e histórico](OPENMP_TRANSACTION_IMPLEMENTATION.md).
- [Identidade e tipos](OPENMP_TRANSACTION_SCOPE_AGENT.md).
- [Acessos futuros](OPENMP_TRANSACTION_ACCESS_AGENT.md).
- [Abort e reinício futuros](OPENMP_TRANSACTION_ABORT.md).

Conferir os fontes antes de executar. Os resultados registrados em outros
documentos são históricos, não validação desta entrega. Trabalhar nos fontes
em /usr/src/gcc-omp-transaction e construir em /usr/src/gcc-build.

A consolidação e os seis esqueletos internos de ciclo de vida foram entregues.
A execução desta etapa funcional continua posterior aos esqueletos, conforme a
sequência solicitada; verificar o estado no inventário antes de implementar.

## Resultado esperado e limites

Ao concluir a etapa, a tabela deve armazenar entradas reais, compartilhar a
mesma entrada entre referências simultâneas à mesma chave e proteger estrutura,
referências e callbacks com mutexes por bucket. A ponte deve possuir referências
válidas e liberá-las uma vez na saída externa, mantendo flat nesting.

Registro não concede reserva de leitura/escrita e não acessa o valor do objeto.
O corpo da diretiva continua acessando memória comum. Permanecem futuros:
instrumentação de acessos, reservas, cópias privadas, commit, abort, rollback,
retry e backoff. newtm_read/write e os seis serviços de ciclo de vida
permanecem esqueletos internos. Não ligá-los à ponte como operações funcionais. Não ampliar
gramática, tipos, tasking, suporte a dispositivos ou ABI pública nesta etapa.

## Decisão de projeto a registrar

O agente da hash apresenta a remoção na última referência como proposta técnica,
embora já a use nos contratos concebidos e no cabeçalho. Esta especificação
adota essa proposta como desenho recomendado para a etapa; sua criação não a
transforma em decisão anteriormente aprovada pelo usuário. Na tarefa de
implementação, explicitar essa escolha e verificar se a autorização recebida
abrange o desenho. Não inventar uma aprovação histórica nem substituir a
política silenciosamente.

## Estrutura e invariantes

- Uma tabela global por instância da libgomp na ponte; capacidade inicial 1.024.
- Criação da primitiva aceita qualquer capacidade positiva representável.
- Colisões por encadeamento, capacidade imutável e sem redimensionamento.
- Chave por igualdade exata do endereço; tamanho positivo imutável por entrada.
- Hash de endereços considera também objetos de alinhamento pequeno; a escolha
  do bucket funciona para capacidades que não são potências de dois.
- Uma referência física por aquisição bem-sucedida. Referência não é reserva
  transacional nem prolonga a vida do objeto do programa.
- Bucket protege lista, contadores, referências e acesso aos metadados.
- Nenhum mutex permanece adquirido após retorno normal. Nenhum mutex de bucket
  envolve o corpo da diretiva ou espera por outra transação.
- Apenas a contagem diagnóstica adquire todos os buckets, em ordem crescente,
  liberando-os em ordem inversa. Não há contador global com mutex por aquisição.
- Destruição exige quiescência de operações, callbacks e destruidores. Contagem
  zero, isoladamente, não prova quiescência.

## Primitivas a implementar

Preservar assinaturas internas e os contratos completos do agente da hash.

| Primitiva | Resultado exigido |
| --- | --- |
| newtm_hash_create | Inicializa tabela e todos os mutexes antes de publicar *out; rejeita zero/nulos e overflow. |
| newtm_hash_destroy | Libera tabela vazia; BUSY preserva integralmente tabela não vazia. |
| newtm_hash_acquire | Insere ou retém a entrada existente sob lock; incompatibilidade de tamanho não altera referências. |
| newtm_hash_lookup_acquire | Retém uma entrada existente sem inserção; ausência retorna NOT_FOUND. |
| newtm_hash_release | Consome uma referência; na última, desliga sob lock e destrói metadados/entrada fora dele. |
| newtm_hash_entry_address | Consulta chave imutável com referência retida. |
| newtm_hash_entry_size | Consulta tamanho imutável com referência retida. |
| newtm_hash_entry_visit | Executa callback curto uma vez sob lock, preservando a referência. |
| newtm_hash_count | Obtém fotografia consistente do número de entradas, saturada em SIZE_MAX. |

Nas operações com estado, falhas retornáveis usam NEWTM_HASH_INVALID,
NOT_FOUND, SIZE_MISMATCH, BUSY ou OVERFLOW conforme o contrato. Saída out válida
fica NULL na falha; não deixar inserções ou incrementos parciais. Verificar
overflow de alocações, referências e contadores antes da alteração.
Falta de memória segue gomp_malloc/gomp_malloc_cleared e gomp_fatal; não criar
um estado recuperável fictício. Violações com ponteiros pendentes não têm
garantia de diagnóstico seguro.

policy_data pertence à entrada após instalação. visit não pode reentrar na
tabela, executar código do programa ou inverter ordem de locks. Ao substituir
metadados, responde pelo destino dos anteriores. destroy_policy é obrigatório
quando houver metadados não nulos; executa exatamente uma vez por geração
retirada, sem lock de bucket, sem reentrada e sem alterar o objeto do programa.
Uma nova geração com a mesma chave pode existir durante essa destruição.

## Adaptação obrigatória da ponte na mesma entrega

1. Inicializar a tabela uma vez sob o lock de inicialização existente. Publicar
   newtm_trace_initialized somente depois de OK e de obter tabela válida.
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
7. No encerramento quiescente, verificar o retorno de destroy. BUSY representa
   erro de ciclo de vida; não ignorar nem forçar remoção de entradas vivas.

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

Definir o destino dos printf atuais: se preservados, não imprimir sob locks de
bucket; se retirados ou alterados, atualizar os testes de saída e a documentação
para o comportamento escolhido. Logs não podem ser a única evidência de uma
operação real. Não alterar expectativas para esconder regressões.

Atualizar panorama, agente da hash, inventário de implementação, manual e
transaction_test/README.md quando o comportamento mudar. Remover descrições
obsoletas de handles nulos e hash sem implementação, mantendo explícita a
ausência de isolamento transacional. Identificar o histórico como histórico.

## Validação necessária

Antes de escrever testes, definir como ligar primitivas ocultas no ambiente de
teste, preferindo infraestrutura interna compatível com o build da libgomp.
Registrar o mecanismo reproduzível; não depender exclusivamente de fontes em
/tmp nem ampliar a ABI. Hooks de falha/contadores devem ficar restritos a testes.

| Grupo | Evidência exigida |
| --- | --- |
| Estrutura | Capacidades 1 e não potência de dois, colisões, chaves distintas, aquisição repetida, consulta ausente, identidade e tamanho. |
| Falhas | Nulos previstos, tamanho zero, SIZE_MISMATCH sem alteração, out limpo, BUSY preservando tabela e overflow antes de alterar estado. |
| Referências | Liberação parcial/final, consulta retendo entrada após outra release e remoção somente na última referência. |
| Metadados | visit com exclusão correta, substituição com propriedade definida e destrutor exatamente uma vez fora do lock. |
| Concorrência | Mesma chave, colisões, progresso em buckets distintos e contagem concorrente sem corrupção ou deadlock. |
| Ponte C/C++ | Registro antes do corpo, duplicatas, aninhamento lexical/por função, retenção até saída externa, threads distintas e limpeza excepcional suportada. |
| Integração | Templates, referências C++, cópias private/reduction externas, dumps e ligação com a libgomp reconstruída. |

Usar coordenação determinística em testes concorrentes, sem sleep como prova de
ordenação. Não executar count/reentrada a partir de visit para tentar provar
liberação do lock. Para overflow impraticável por volume, usar mecanismo interno
controlado e documentado; distinguir cobertura por teste de inspeção do código.
Se testar falta de memória, fazê-lo em processo separado com injeção controlada.

Reconstruir libgomp em /usr/src/gcc-build e executar os testes internos e os
subconjuntos DejaGNU C/C++ pertinentes a transaction. Reconstruir GCC apenas se
os seus fontes mudarem. Executar também os exemplos locais com recompilação
forçada. Conferir a biblioteca efetivamente carregada e os arquivos .sum/.log;
reportar PASS, FAIL, UNRESOLVED e UNSUPPORTED separadamente.

## Critérios de conclusão

A etapa termina quando as nove primitivas têm comportamento real verificado,
a ponte usa referências válidas sem ignorar erros, recursos e locks têm
propriedade definida, testes reproduzíveis cobrem os invariantes e a documentação
descreve o novo estado. Relatar arquivos alterados, comandos, resultados e
limitações. Não declarar STM, isolamento, rollback ou retry implementados.

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
