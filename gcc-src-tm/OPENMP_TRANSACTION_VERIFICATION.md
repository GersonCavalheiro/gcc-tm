# Contrato de verificação da transformação — etapa 6.1

**Estado vigente em 2026-11-11: 6.6 aprovada e concluída no domínio
experimental documentado**, conforme o
[parecer final](OPENMP_TRANSACTION_ACCEPTANCE_FINAL.md).
As pendências da revisão anterior foram corrigidas e validadas; os requisitos
normativos permanecem iguais. As notas de andamento seguintes são históricas.

Revisão final de 2026-11-10: a 6.6 permanece aberta por conformidade de
**diagnósticos**, com constatações estáticas e rastreabilidade V01–V08 em
[OPENMP_TRANSACTION_FINAL_REVIEW.md](OPENMP_TRANSACTION_FINAL_REVIEW.md).
Os requisitos normativos abaixo não foram dispensados ou reduzidos.

Estado vigente da 6.5.3: blocos 1–8 concluídos no domínio documentado, incluindo
[composição do certificado atual](OPENMP_TRANSACTION_CURRENT_GRAPH.md) e
[reconstrução em LTO](OPENMP_TRANSACTION_LTO_GRAPH.md), com validação concluída.
Resta a aceitação integral (6.6). Este estado
prevalece sobre o histórico abaixo.


Definido em 2026-10-31 na etapa documental 6.1. Este contrato especifica
obrigações, evidências, derivações, estados e invalidação para o verificador.
O [inventário da 6.2](OPENMP_TRANSACTION_OBLIGATIONS.md) implementa seu
acompanhamento e tratamentos declarados. A [inspeção da 6.3](OPENMP_TRANSACTION_INSPECTION.md)
acrescenta recusa estrutural pelo GIMPLE real, sem emitir certificados completos.
O [grafo de fluxo da 6.4.1](OPENMP_TRANSACTION_FLOW.md) e as
[provas de guardas da 6.4.2](OPENMP_TRANSACTION_GUARDS.md) estão implementados,
assim como a [disponibilidade por status da 6.4.3](OPENMP_TRANSACTION_STATUS.md)
e a [inicialização, vida útil e publicação da 6.4.4](OPENMP_TRANSACTION_LIFETIME.md).
A [prova de falhas e aninhamento da 6.4.5](OPENMP_TRANSACTION_FAILURES.md)
também está implementada, assim como a
[prova do controlador proprietário da 6.4.6](OPENMP_TRANSACTION_OWNERSHIP.md).
A [composição local da 6.4.7](OPENMP_TRANSACTION_COMPOSITION.md) está implementada.
As etapas 6.5–6.6 seguem pendentes. A tarefa 5
continua funcional e opcional por `-fopenmp -fopenmp-transaction-stm`.
Os campos abaixo são o modelo lógico; interfaces/enums internos disponíveis
estão em `gcc/newtm-obligation.h`, sem alteração da ABI compartilhada.

## 1. Objetivo e domínio

Uma região funcional só poderá ser certificada quando todas as suas obrigações
forem resolvidas e a inspeção independente do IR confirmar o tratamento de
cada operação relevante. O domínio inclui controlador externo, auxiliar da
região e grafo transitivo de funções preparadas; inclui operações derivadas e
operações GIMPLE sem vínculo de origem.

A função pública original fora desses escopos não se torna transacional por
possuir clone. Clones dormentes recusados podem continuar existindo como stubs
de falha; nenhum grafo funcional ativado poderá ser certificado passando por
eles. Não ampliar tipos, chamadas, exceções, vida útil ou suporte a frames.
A alternativa B de ambiente léxico explícito permanece futura.

Os contratos D02–D13 do [registro de decisões](OPENMP_TRANSACTION_DECISIONS.md)
continuam normativos. A verificação não promete proteção contra acessos externos
não instrumentados, publicação fisicamente simultânea de vários objetos ou
ausência de starvation. A autorização de acesso e a correção de reservas,
publicação e limpeza continuam responsabilidades da ABI/runtime.

## 2. Identidade e propriedade dos registros de verificação

Separar identidade semântica, instância de operação e validade temporal:

| Informação | Contrato |
| --- | --- |
| Origem | Token e localização da construção original, quando disponíveis; não guardar a expressão como receita executável. |
| Instância | Identifica uma operação emitida naquela função/região. Origem igual não implica instância igual. |
| Grupo de avaliação | Relaciona operações de uma expressão, inclusive RMW, preservando o sequenciamento exigido pela linguagem. |
| Escopo | Declaração da função, região/controlador e papel: original, auxiliar ou clone. Não identificar escopo pelo prefixo do nome. |
| Geração | Versão local do IR e das evidências; transformação relevante invalida as provas dependentes. |
| Derivação | Relação explícita entre obrigações originais e operações resultantes; não equivale a herdar aprovação. |

Reutilizar os tokens de origem/grupo e a identidade de operação de
`newtm-occurrence` quando válidos, sem tratar DECL_UID ou endereço de instrução
como identificador persistente entre unidades, LTO ou remapeamentos. Uma
operação sem ocorrência recebe identidade própria de inventário; não é omitida.

O formato físico, alocação, acesso e serialização do modelo serão implementados
nas próximas etapas. Qualquer referência a árvores mantida pelo compilador
deve ter vida útil e rastreamento GC adequados. Não prolongar a vida operacional
de um descritor simplesmente mantendo seu endereço alcançável pelo GC.

## 3. Modelo lógico de obrigação

| Campo | Conteúdo obrigatório |
| --- | --- |
| Identificação | Instância, origem opcional, localização e grupo de avaliação. |
| Proprietário | Função, região, papel e geração do IR. |
| Categoria | Uma das categorias O01–O07 abaixo. |
| Operação observada | Instrução/operando atuais, papel e índice do operando, somente na janela válida. |
| Requisitos | Identidade, armazenamento, canais e condições necessárias àquela categoria. |
| Derivações | Obrigações-filhas e regra que explica a substituição/divisão. |
| Tratamento declarado | Resultado que o instrumentador afirma ter emitido, separado da decisão independente de cobertura. |
| Evidências | Referências às provas correspondentes, com validade e dependências próprias. |
| Resultado | Estado da obrigação e causa de recusa, quando houver. |

Não exigir bijeção entre nó de origem e instrução. Uma atribuição pode conter
leitura e escrita; uma chamada pode produzir também um store preexistente;
várias instruções podem pertencer a uma única avaliação de origem.

### Categorias

| ID | Categoria | Requisitos mínimos para resolução |
| --- | --- | --- |
| O01 | Leitura | Operando atual; faixa/identidade válidas; READ com guarda de sucesso antes do consumo, ou acesso direto com prova PRIVATE. |
| O02 | Escrita | Destino atual; faixa/identidade válidas; RHS já avaliado; WRITE com fonte privada estável e status antes de continuar, ou store PRIVATE comprovado. |
| O03 | Chamada | Implementação admitida; argumentos avaliados; bindings/capturas completos; contexto emprestado; status antes do uso de qualquer saída. |
| O04 | Registro | Objeto/extensão/kind correspondentes à lista e à ativação; serviço e status apropriados, ou dispensa provada por armazenamento PRIVATE conforme contrato existente. |
| O05 | Identidade e avaliação | Proveniência atual, canais incoming/result e distinção objeto ponteiro/apontado; aritmética checada e uso dos operandos GIMPLE efetivos, sem reavaliar origem. |
| O06 | Publicação e retorno | Valor e binding publicados somente após sucesso; saídas preservadas em falha; retorno de status correto em todos os caminhos. |
| O07 | Controle da tentativa | Entrada, aninhamento, falha, confirmação, limpeza e retry compatíveis com propriedade do slot e estado do contexto. |

O inventário também registra acessos internos emitidos quando exigem prova de
transporte ou de memória privada. Cálculo escalar puro sem acesso/efeito não
precisa virar obrigação STM, mas pode integrar evidência de O05 ou de guarda.

### Estados da obrigação

| Estado | Significado |
| --- | --- |
| PENDENTE | Inventariada; tratamento ainda não demonstrado. |
| DERIVADA | Substituída por obrigações-filhas identificadas; o pai ainda não está resolvido. |
| COBERTA | Requisitos da categoria demonstrados por evidências válidas e confirmados pela checagem independente competente. |
| RECUSADA | Tratamento ausente, incompleto ou não admitido; registra causa e bloqueia a certificação do escopo ativo. |

Transições admitidas: PENDENTE para DERIVADA, COBERTA ou RECUSADA; DERIVADA para
COBERTA somente após resolver todas as filhas e a regra de derivação, ou para
RECUSADA. Uma evidência invalidada reabre a obrigação afetada como PENDENTE,
ou a recusa se não houver reconstrução admissível. Guardar a revisão anterior
apenas como histórico, sem autorização de uso.

Retirar uma instrução, chamar forget/retire ou deixar de encontrar metadados
não é transição para COBERTA. Uma instância RECUSADA não muda de resultado
silenciosamente; uma reconstrução cria revisão explícita e exige nova análise.

## 4. Modelo lógico de evidência

Toda evidência identifica espécie, produtor/regra, escopo/geração, obrigações
que sustenta, dependências, âncoras atuais e estado de validade.

| Espécie | O que demonstra | O que não demonstra isoladamente |
| --- | --- | --- |
| Identidade/faixa | Objeto canônico, extensão, deslocamento, tamanho, kind e correspondência do endereço lógico. | Registro alcançado ou reserva concedida. |
| Armazenamento | Origem PRIVATE comprovada ou classificação DYNAMIC produzida por canal válido. | Privacidade apenas porque a tag vale 1 ou o objeto não está na hash. |
| Transporte | Binding por ativação, índice/quantidade, campos necessários, vida útil e buffers separados. | Proveniência de um valor arbitrário recebido sem contrato de produção. |
| Avaliação | Operandos já avaliados e relações de definição/uso e sequenciamento. | Permissão para executar testemunho ou impor ordem entre argumentos não exigida pela linguagem. |
| Guarda | Condição de validade ou status da operação específica separa sucesso e falha. | Autorização por uma consulta de status anterior ou não relacionada. |
| Fluxo | Caminhos de falha, retorno, consumo/publicação e propriedade do slot. | Correção interna de commit ou de sincronização do runtime. |
| Chamada | Declaração preparada, assinatura, canais e aresta transitiva válidos. | Aprovação de todo o grafo por verificar apenas o chamador. |
| Reconciliação | Testemunho associado por regra estrutural admitida a uma operação real, com uso exclusivo dessa correspondência. | Eliminação genérica de source-only ou aceitação por contagem semelhante. |

### Estados de validade

- VALIDA: corresponde ao IR, escopo e geração atuais; dependências válidas.
- INVALIDADA: houve mudança ou perda de uma dependência; não autoriza aceitação.
- RETIRADA: janela operacional encerrada; apenas histórico/resumo apropriado
  pode permanecer. Retirada não demonstra que a obrigação foi cumprida.

Contagens e flags de preparação existentes são fatos auxiliares, não provas
de cobertura. A 6.2 poderá registrar o tratamento declarado; isso não autoriza
marcar COBERTA ou LOCAL_APROVADO apenas com a afirmação do instrumentador.
O estado `NEWTM_DESC_UNREGISTERED` pode descrever identidade
estrutural conhecida na entrada; não autoriza acesso. INCOMING/RESULT precisam
de canais resolvidos para seus usos. UNKNOWN/SOURCE_ONLY/STALE não recebem
identidade fictícia nem fallback. Preservar as recusas conservadoras atuais.

### Acessos diretos internos

Distinguir armazenamento privado do programa de transporte interno do
compilador. Para transporte, exigir declaração/objeto identificado, espécie,
faixa, vida útil, usos permitidos e ausência de escape indevido. Exemplos:
slot, vetor de bindings, buffers READ/WRITE, saídas privadas de chamada e cópia
imutável de representação de referência C++.

Essa cópia de referência pode existir antes do begin como transporte interno;
isso não torna seu referido nem um objeto preexistente do programa privado.
DECL_ARTIFICIAL, nome de temporário, presença em frame ou falta de ocorrência
não substituem a prova. Não permitir acesso comum arbitrário por classificá-lo
como scaffolding. Helpers internos/builtins exigem espécie, assinatura e efeitos
admitidos; não há dispensa geral para builtin, pure, const ou nothrow.

## 5. Regras de derivação

| Transformação | Obrigações e evidências exigidas |
| --- | --- |
| RMW, incluindo incremento | O01 e O02 distintos, cálculo e grupo de avaliação preservados; READ/cálculo/WRITE usam a ABI existente. |
| Cópia agregada | Leitura checada para buffer privado e escrita checada a partir dele; nenhuma filha omitida ou reinstrumentação do buffer como objeto preexistente. |
| Resultado de chamada em memória | O03 mais O02 derivada para o destino já avaliado; store somente após status OK da chamada. |
| Resultado ponteiro | Valor e binding vinculados ao mesmo sucesso; canais só disponíveis após esse sucesso. |
| Despacho DYNAMIC | Guarda da tag e dois caminhos verificados: REGISTERED pela ABI e PRIVATE por prova transportada. Tag desconhecida segue à falha. |
| Reconciliação de testemunho | Registrar a correspondência antes de retirar o vínculo; usar a regra estrutural já admitida, sem reexecutar expressão. |
| Substituição da região por controlador | Relacionar região, auxiliar e escopo do controlador; retirar corpo original sem perder suas obrigações e exigir o grafo do auxiliar. |
| Corpo recusado substituído por stub | Preservar RECUSADA; a ausência de operações normais no stub não certifica o corpo original. |

As filhas recebem identidades de instância próprias. Uma filha não pode ser
reutilizada para resolver outra obrigação apenas por coincidência de nome,
posição ou contagem.
Compartilhar uma operação requer regra explícita de reconciliação/equivalência,
sem mudar a avaliação exigida ou os limites atuais.

## 6. Invalidação e retirada

| Evento | Consequência obrigatória |
| --- | --- |
| gimple_copy ou clonagem de instrução | Nova instância e evidência inicialmente inválida até reconstrução. A origem pode ser conservada; a aprovação não é herdada. |
| Alterar referência, índice, endereço, tipo, tamanho ou kind | Invalidar identidade/faixa e todas as provas que delas dependem. |
| Redefinir ponteiro ou chamada com efeito sobre ele | Invalidar a proveniência afetada conforme o contrato conservador; não ressuscitar incoming antigo. |
| Alterar storage, produtor ou índice do binding | Invalidar armazenamento/transporte e decisões de acesso dependentes. |
| Alterar chamada, assinatura ou captura | Invalidar a aresta, canais, saídas e certificações transitivas dependentes. |
| Mover/remover guarda ou alterar desvio | Invalidar provas de fluxo, consumo, publicação e encerramento afetadas. |
| Substituir/dividir instrução na mesma janela | Somente transferência explícita para os mesmos operandos e escopo, com registro das derivações e nova checagem; não reutilização irrestrita. |
| Remapear declaração, tree-nested ou outlining | Invalidar provas ligadas aos operandos/frame anteriores; reconstruir ou transportar com regra demonstrada para os operandos novos. |
| Entrada de omplower, forget/retire ou reset | Proibir uso operacional dos descritores/associações retirados. Preservar somente resumos apropriados à etapa posterior. |
| Reescrita posterior ou LTO relevante | Não reutilizar certificação apenas por nome/UID; adaptar a evidência aos remapeamentos suportados ou exigir nova verificação. |

As invalidações propagam-se às obrigações, aos resumos locais e aos chamadores
dependentes. Mudanças irrelevantes, por exemplo localização de debug sem efeito
semântico, podem conservar prova somente por regra definida, não por omissão.
Não resolver automaticamente uma invalidade aumentando a duração das tabelas.

## 7. Fases e certificados

| Fase | Entrada e saída contratadas |
| --- | --- |
| Inventário | Antes de consumidores destrutivos, inclusive reconcile_reads, substituições e forget. Enumerar operações reais e testemunhos; anotar transporte já inserido pelo compilador. |
| Verificação local | Após emissão completa de corpo, guardas de entrada, publicações e epílogos; confrontar obrigações e IR real, inclusive operações sem ocorrência. Resolver evidências enquanto os operandos ainda correspondem à ativação. |
| Certificação transitiva | Após preparação de todos os corpos. Exigir resultado local e arestas atuais de todo o grafo, tratando ciclos. Usar resumos persistentes, não descritores. |
| Verificação após remapeamento | Inspecionar o IR remapeado e as regras de transferência/invalidação; não consultar tabelas retiradas nem aceitar um bit antigo de aprovação como prova. |

A implementação deverá posicionar o fechamento da janela e a inspeção local
de modo a incluir também a entrada e o epílogo acrescentados no fim de
`newtm_clone_finish`. Não basta verificar o prefixo antes dessas estruturas.
As rotinas atuais de preflight não constituem essa inspeção final independente.

Antes de tree-nested/omplower, consumir os descritores necessários ou produzir
representação de prova específica para remapeamento. Uma referência antiga a
GIMPLE não se torna válida por sobreviver fisicamente em memória.

O certificado lógico separa: NAO_VERIFICADO, LOCAL_APROVADO, GRAFO_APROVADO,
INVALIDADO e RECUSADO. LOCAL_APROVADO não autoriza sozinho uma região ativa;
GRAFO_APROVADO exige todas as condições locais e transitivas atuais. O escopo
original/cópia não herda o certificado de outra instância.

Um resumo persistente pode conter identidade remapeável de função/região,
origem/localização, geração, resultados das regras, recusas e dependências.
Não pode ser usado como plano de acesso nem conter operandos/descritores
expirados para uma consulta operacional tardia. Uso em LTO exige suporte de
preservação/remapeamento próprio; esta especificação não o implementa.

## 8. Regras de aceitação e recusa

| ID | Condição obrigatória |
| --- | --- |
| V01 | Toda obrigação tem tratamento demonstrado ou recusa; não desaparece por retirada de metadados. |
| V02 | Toda operação relevante no IR real está classificada e sustentada, mesmo sem ocorrência. |
| V03 | Identidade/faixa correspondem aos operandos atuais; guardas precedem endereço inválido/acesso. |
| V04 | PRIVATE/transporte possuem prova; REGISTERED passa pela autorização da ABI; DYNAMIC tem todos os ramos corretos. |
| V05 | Valor e binding só são usados/publicados sob sucesso da operação correspondente. |
| V06 | Chamadas, capturas e resultados possuem canais completos e grafo preparado. |
| V07 | Entrada, aninhamento, falha, retorno, commit, descarte e retry respeitam propriedade/estado. |
| V08 | Toda evidência e certificação usadas correspondem ao escopo, geração e fase atuais. |

Falta de prova implica recusa do caminho funcional, sem fallback. O futuro
verificador deverá atuar também com checking desativado; asserts são auxiliares.
O diagnóstico deverá indicar regra, espécie de operação, localização de origem
ou do escopo, função e causa. Cadeia de chamada e ancestral de derivação devem
ser informados quando necessários para explicar a falha. Não imprimir valores
do programa nem reavaliar expressões para compor diagnóstico.

Essas regras não provam o algoritmo interno do runtime. Contratos da ABI,
reservas, publicação mantendo todas as reservas e limpeza antes de fatal
continuam apoiados por revisão e testes próprios. Falha de begin sem contexto
é terminal; com contexto, INVALID domina CONFLICT e passa pela limpeza externa.

## 9. Casos de mesa para implementação posterior

| Caso | Resultado contratual |
| --- | --- |
| a[i++] += f(&local_privado) | Grupos de avaliação conservados; acessos de i/a inventariados, chamada com transporte PRIVATE comprovado e stores/read correspondentes; nenhuma receita reexecutada. |
| *destino = callee(origem) | Chamada mais store derivado; resultado privado e store somente em sucesso. |
| *destino = *origem agregado | READ e WRITE independentes, conectados por buffer privado; retirar a escrita deixa o pai não resolvido. |
| Cópia/remapeamento com mesmo token de origem | Nova instância; aprovação anterior não basta, mesmo com nomes iguais. |
| Carga sem ocorrência em corpo preparado | Inspeção real cria/classifica a obrigação; sem prova de acesso interno/privado ou serviço correspondente, recusa. |
| Tag PRIVATE alterada sem prova de armazenamento | Recusa V04 mesmo que a comparação numérica aceite 1. |
| Guarda de status removida ou ligada à chamada errada | Recusa V05, ainda que o caminho não seja exercitado pelos testes de runtime. |
| Stub de falha sem operações residuais | Corpo continua RECUSADO e impede certificação de grafo funcional. |
| Tentativa de consulta após retirada dos descritores | Evidência RETIRADA não autoriza operação; usar resumo próprio ou reconstruir na fase adequada. |

Esses são exemplos normativos de decisão, não testes novos já executados.
Mutações executáveis e regressões do verificador pertencem à 6.6.

## 10. Critério de conclusão e sequência

A 6.1 termina com categorias O01–O07, modelo de obrigação/evidência,
derivações, invalidação, fases, certificados e regras V01–V08 definidos e
relacionados às interfaces existentes. Esse critério foi atendido por este
contrato e pela atualização dos documentos de orientação.

| Próxima etapa | Implementação ainda necessária |
| --- | --- |
| 6.2 concluída | Inventário, estados, tratamentos declarados, reconciliação/derivação e invalidação; ver contrato de implementação acima. |
| 6.3 concluída | Inspeção independente do GIMPLE real, substituições e classificação local, incluindo operações sem vínculo. Ver alcance e limites no contrato de implementação da 6.3. |
| 6.4 concluída | Grafo, provas locais, protocolo proprietário e composição no inventário concluídos; dependências transitivas seguem explícitas para a 6.5. |
| 6.5 em andamento | Certificação transitiva histórica [6.5.1](OPENMP_TRANSACTION_GRAPH.md) e [invalidação/revalidação 6.5.2](OPENMP_TRANSACTION_INVALIDATION.md) implementadas para o grafo histórico; remapeamentos seguem na 6.5.3. |
| 6.6 | Mutações negativas, positivos, regressões e aceitação integrada. |

Desdobramento da 6.4, conforme o planejamento aprovado:

| Subetapa | Critério de conclusão |
| --- | --- |
| 6.4.1 concluída | Grafo próprio do High GIMPLE, destinos/limpezas explícitos, estados finitos em ciclos, recusa de estruturas desconhecidas e IR preservado; ver contrato do fluxo. |
| 6.4.2 concluída | Provas independentes de validade/faixa/overflow antes dos cálculos e acessos dependentes; fatos por valor alcançável, invalidação por escrita e recusa V03; ver contrato de guardas. |
| 6.4.3 concluída | Status por definição de chamada e disponibilidade independente de valor/binding em todos os caminhos; invalidação por escritas/aliases e recusa V05; ver contrato de status. |
| 6.4.4 concluída | Inicialização por campo, operandos definidos, vida útil lexical e por clobbers, origens/escapes e publicação completa de valor/binding sob o mesmo sucesso; ver contrato de vida útil. |
| 6.4.5 concluída | Causa preservada e resultado efetivo de FAIL, contexto emprestado, níveis relativos balanceados em sucesso, ausência de continuação normal/ciclos em falha e arestas obrigatórias de aborto; ver contrato de falhas. |
| 6.4.6 concluída | Admissão/outermost, slot exclusivo e estável, auxiliar/STATUS/END em sucesso, condenação e limpeza únicas, contador saturante, fatal e retry integral com preparação fresca; ver contrato proprietário. |
| 6.4.7 concluída | Provas locais por obrigação e revisão, consolidação atômica, reconciliação, invalidação e diagnóstico; dependências transitivas explícitas para a 6.5. |

A base executável permanece a [validação da 5.6](OPENMP_TRANSACTION_INTEGRATION_VALIDATION.md):
1303 verificações e 1036 execuções previamente aprovadas. Esses números não
são testes da especificação 6.1 nem evidência de implementação do verificador.
Não houve mudança na ABI, no layout de newtm_binding_v1/newtm_clone_binding_v1,
nas opções ou no comportamento do compilador/runtime na etapa documental 6.1.
