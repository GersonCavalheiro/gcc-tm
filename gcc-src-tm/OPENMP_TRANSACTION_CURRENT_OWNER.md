# Bloco 5 da 6.5.3 — propriedade no CFG remapeado

Atualização do bloco 7: a [composição atual do grafo](OPENMP_TRANSACTION_CURRENT_GRAPH.md)
publica aprovação agregada somente na nova API e na janela pré-SSA do frontend.
Este componente ou certificado histórico conserva seu contrato próprio e não
concede aprovação atual isoladamente. A autoridade agregada é revogada antes
de transformações. A [reconstrução LTO do bloco 8](OPENMP_TRANSACTION_LTO_GRAPH.md)
cria gerações independentes na mesma API; a aceitação integral da 6.6 permanece
pendente.


O componente reconstrói o protocolo proprietário no CFG nativo pré-SSA,
depois de baixar funções GNU e extrair regiões OpenMP. Não reutiliza a
aprovação High GIMPLE como prova atual. A integração ocorre em
`check_lowered_controller`, após correspondência de capturas, canais e status.

## Contrato e fronteiras

Na janela de preparação, `newtm_owner_snapshot_entry` consome o controlador
emitido e registra, por auxiliar delegado, apenas declarações de origem, a
identidade de slot/contador e termos escalares dos predicados de validação.
Não conserva instruções, descritores, expressões executáveis ou SSA_NAME.

A emissão delimita a vida útil do slot com `CLOBBER_STORAGE_BEGIN` antes da
inicialização e `CLOBBER_STORAGE_END` na continuação normal. Essas operações
não introduzem serviços de runtime. A abertura permite reentrada lexical em
laços; o encerramento fornece uma fronteira operacional verificável, mesmo
quando o GCC remove rótulos sem usuários. Não se presume que um rótulo, UID
ou descritor sobreviva ao outlining.

`newtm_owner_verify_entry` encontra a abertura e o encerramento atuais
pela proveniência das declarações e constrói um subgrafo da função atual.
O subgrafo retém todas as instruções e todas as saídas entre as fronteiras,
inclusive operações acrescentadas posteriormente. Recusa entradas externas
no interior, fronteiras ambíguas, retorno, saída da função antes da fronteira
e sumidouros sem término classificado. Chamadas noreturn do CFG possuem saída
de término explícita, identificada pela operação atual.

O encerramento só pode ser alcançado em estado COMMITTED. Os transportes
privados do controlador não podem ser usados fora do protocolo; clobbers
lexicais e debug não autorizam continuação com contexto vivo. Código comum,
laços e EH de C++ ao redor permanecem fora do subgrafo. Mais de um controlador
na mesma função é verificado separadamente.

O modelo acumula também os transportes de identidade efetivamente encontrados
no CFG atual, incluindo cópias novas sem declaração na janela antiga. Esses
transportes entram no confronto de usos externos. Subscritos, deslocamentos
e ponteiros consumidos por endereços são inspecionados: o endereço de um objeto
não lê seu conteúdo, mas usar um contexto como índice ou dado não é permitido.

## Prova independente do protocolo

O modelo de `newtm-owner.cc` é aplicado ao subgrafo atual, com estados
separados por fase, valores abstratos e fatos obrigatórios nas junções:

- Slot local tipado, único e vazio; contador inicializado; BEGIN explícito.
- Sucesso da própria admissão e outermost igual a 1, obtido do produtor real.
- Auxiliar delegado, STATUS final e END nessa ordem, com contexto da tentativa.
- Contextos emprestados não se tornam slots proprietários; os corpos
  preparados continuam sujeitos à prova atual de falhas/aninhamento do bloco 4.
- Falha de entrada com slot vazio termina sem condenar um contexto inexistente;
  com slot retido exige FAIL e AFTER_FAILURE, como as demais falhas.
- Causa original preservada, condenação única e limpeza pela mesma identidade
  de slot/contador; não há commit após falha ou limpeza duplicada.
- Contador positivo, incremento único sob limite e saturação sem overflow.
- Somente CONFLICT efetivo de AFTER_FAILURE autoriza retry. Falha permanente
  ou status desconhecido não autoriza readmissão. Conflito limpo não termina
  pelo caminho fatal: repete o bloco completo.
- BEGIN invalida os temporários e campos da tentativa anterior. O retry
  precisa refazer admissão, validação e preparação antes de executar o auxiliar.
- Ciclos que evitam limpeza/readmissão, contexto consumido, sobrescritas,
  escapes de slot e clobbers de armazenamento vivo são recusados.

As arestas de validação são reconstruídas a partir dos predicados e das
definições atuais, com polaridade explícita. Status desconhecidos de BEGIN,
STATUS e END são considerados, incluindo as duas possibilidades de posse
na entrada falha. A análise não modifica o CFG examinado.

O resultado acessível por `newtm_owner_get(helper)` é DEPENDENT, com
CHANNELS | STATUS (3): ele prova o protocolo, não o conteúdo completo dos
canais nem os efeitos do auxiliar. Memória, canais e status conservam suas
dependências anteriores até a composição do bloco 7. O resultado é uma
observação de uma inspeção, não um certificado reutilizável após mutação.
`current_approved` permanece falso.

## Validação e limites

`make -C transaction_test check-proprietario-atual` usa mutações reais após
ompexp e chama diretamente o verificador de produção. A matriz cobre C/C++,
O0/O2, checking 0/2, GC forçado e execução com DSO/archive. Positivos incluem
cópia do contexto, comparação invertida, sobrescrita de resultado morto,
laços, múltiplos controladores, funções GNU, aninhamento e parallel.
Resultado: 553 verificações e 88 execuções. Antes de cada mutação, a base atual
precisa passar pelo mesmo verificador. O observador também confirma que não
houve publicação de `current_approved`.

Negativos retiram serviços e fronteiras, falsificam sucesso/outermost,
trocam slot/contexto/causa, escapam o endereço do slot, escrevem por alias
sem TREE_ADDRESSABLE, alteram contador/limite, autorizam retry permanente,
ignoram readmissão/preparação, retornam com status desconhecido e usam contexto
consumido. Há testes de cópia nova de contexto usada após a fronteira, contexto
convertido em subscrito, entrada externa no interior e término após conflito.

Os testes de controlador/integração verificam a fronteira executável, limpeza
de entrada falha, preservação de saídas, confirmação/descarte e retry real,
além dos símbolos versionados e das ligações dinâmica e estática. Resultados
nativos Linux x86_64; LTO funcional não equivale à reconstrução da prova em LTO.

Passaram status atual (449/100), memória (281/32), canais (241/44), controlador
(225/106), integração (1078/930), grafo (317), ABI v1 (88) e chamadas (75/18).
Passaram também propriedade anterior, fluxo, falhas/aninhamento, composição,
vida útil, guardas e status anterior, incluindo seus cenários sintéticos.
Build all-gcc sem avisos; self-tests internos desabilitados neste build.

ABI, runtime, layouts e canais incoming/result permanecem preservados.
A [cobertura residual do bloco 6](OPENMP_TRANSACTION_CURRENT_COVERAGE.md)
está implementada, incluindo o prefixo desde a abertura de vida útil do slot;
composição, publicação e invalidação atuais ao 7; reconstrução em LTO ao 8;
aceitação integral à 6.6. Alternativa B permanece otimização futura.
