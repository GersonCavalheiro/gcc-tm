# Canais no CFG remapeado — bloco 3 da 6.5.3

Atualização do bloco 7: a [composição atual do grafo](OPENMP_TRANSACTION_CURRENT_GRAPH.md)
publica aprovação agregada somente na nova API e na janela pré-SSA do frontend.
Este componente ou certificado histórico conserva seu contrato próprio e não
concede aprovação atual isoladamente. A autoridade agregada é revogada antes
de transformações. A [reconstrução LTO do bloco 8](OPENMP_TRANSACTION_LTO_GRAPH.md)
cria gerações independentes na mesma API; a aceitação integral da 6.6 permanece
pendente.


Atualização do bloco 5: a [prova proprietária atual](OPENMP_TRANSACTION_CURRENT_OWNER.md)
está implementada. As dependências dos componentes permanecem explícitas até
sua composição no bloco 7; `current_approved` permanece falso.

Implementação em `gcc/newtm-memory.cc`, com o analisador de inicialização e
vida útil de `gcc/newtm-lifetime.{cc,h}`. Bloco 3 concluído como componente do
frontend no domínio descrito abaixo; não concede aprovação integral atual.

Atualização posterior: o [bloco 4](OPENMP_TRANSACTION_CURRENT_STATUS.md)
implementa a prova atual de status e publicação, ainda dependente de OWNER.
Compor os resultados dos componentes e publicar a aprovação atual continua
no bloco 7; a consulta dos canais não se torna uma aprovação integral.

## Contrato e fronteira

`newtm_channels_verify_cfg` inspeciona o CFG atual das funções preparadas,
após os remapeamentos e antes de SSA, em `check_local_graph`.
`newtm_channels_verify_entry` verifica a entrada encontrada pela chamada única
ao auxiliar no controlador, incluindo o corpo extraído por outlining.
A correspondência de capturas e dos produtores dos frames continua sendo
conferida independentemente pelo bloco 1.
Um parâmetro de recepção de frame OpenMP só é admitido após confrontar a
origem real do outlining, a assinatura, as capturas e seus produtores no CFG.
O layout e a identidade desse parâmetro ainda são verificados pelo modelo
comum; um ponteiro arbitrário de tipo compatível não adquire essa permissão.

As obrigações de produção e consumo dos canais são resumidas durante a janela
válida da preparação. `incoming`, os slots e seus campos, argumentos e
publicações de valor/binding conservam termos escalares de identidade, layout,
produtores que chegam ao uso e condições obrigatórias desses produtores.
Não se conservam descritores, instruções, SSA_NAME ou receitas para reexecutar
expressões. A inspeção atual reconstrói os termos a partir do CFG real.

O resumo da entrada do controlador é produzido antes de encerrar sua inspeção.
Outlining pode acrescentar condições externas, mas não retirar uma condição
exigida pelo produtor original. Os escritores e os campos continuam sendo
comparados; condições adicionais não autorizam alterar um slot.

## Assinaturas, objetos e intervalos

A extensão do incoming vem da assinatura preservada. Contar os PARM_DECL do
corpo original é incorreto nessa fronteira: O2 pode já ter eliminado esse
corpo e seus parâmetros. O número de bindings explícitos agora usa os tipos
dos argumentos do protótipo; os bindings de captura conservam seu contrato.

Buffers de chamadas preparadas e READ/WRITE precisam ter endereço demonstrável
na ativação atual, tipo compatível, alinhamento suficiente e intervalo contido
na alocação efetiva. Incoming vazio exige ponteiro nulo. Todos os slots
necessários são considerados; conferir apenas o primeiro elemento não basta.

Incoming, saída de valor e saída de binding não podem se sobrepor na mesma
chamada. Os intervalos usam limites por subtração, sem somas que possam
ultrapassar a extensão. Memória do programa pode ter aliases legítimos:
isso não permite usar um transporte interno como argumento do programa.

Referências conhecidas aos canais formais são confrontadas com sua extensão
e direção pela assinatura atual, inclusive através de cópias e deslocamentos
constantes. Sobrescrever os ponteiros dos canais é recusado. Incoming é
somente leitura também no rastreamento de possíveis aliases.

## Inicialização, vida útil e empréstimos

O analisador é reconstruído para o CFG atual. A descoberta de objetos inclui
as declarações presentes nos operandos, bases de agregados e buffers usados
somente por endereço; a ausência de BIND depois do lowering não transforma
esses objetos em memória externa implicitamente válida.

Todos os campos semânticos de entrada e todas as fontes de WRITE precisam
estar inicializados em cada caminho que chega ao uso. Padding não é dado.
Construtores, cópias, junções, ciclos, aliases, efeitos de chamadas e clobbers
participam da análise. Os estados antigos não sobrevivem ao encerramento ou
reinício da vida útil do armazenamento.

Depois do lowering, as transições lexicais são reconstruídas dos BLOCKs do
GIMPLE atual e das declarações desses blocos. Cada caminho conserva se está
dentro do escopo; nós vazios carregam esse estado até a próxima instrução.
Sair e reentrar num escopo reinicia seus objetos, inclusive em laços. Um
clobber de término seguido de uso dentro do mesmo escopo não reinicia o objeto.
Essa reconstrução não usa os BINDs nem operandos anteriores ao remapeamento.

Buffers só podem ser emprestados nos papéis admitidos. Passar incoming como
dado do programa ou publicar um endereço privado da própria ativação é
recusado. Capturas de referência C++ conservam o buffer de representação
inicializado. O controlador também precisa produzir integralmente sua entrada.

No corpo que contém o controlador, o código comum ao redor da transação
continua seguindo as regras da linguagem. A exclusividade e as recusas de
escape se aplicam aos transportes internos; a vida útil dos objetos ordinários
é exigida nos empréstimos e na produção dos transportes. A aproximação de
efeitos de uma chamada preparada não converte o retorno comum posterior numa
prova geral de ponteiros do programa.
O adaptador inclui as transferências de exceção do código C++ comum que contém
o controlador, conservando as arestas de unwind para a análise. Essa modalidade
é exclusiva da inspeção da entrada: os corpos preparados continuam recusando
exceções. Não há ampliação do subconjunto transacional para EH/C++.

## Resultados e dependências

`newtm_channels_get` apresenta o último resultado: DEPENDENT ou REFUSED,
quantidade de operações confrontadas e dependências STATUS | OWNER.
A consulta não é um certificado reutilizável depois de outra transformação.
Uma nova inspeção começa sem aproveitar o resultado anterior.

Definir que uma operação produz um buffer não autoriza consumi-lo quando
falha. O sucesso da instância correta, a disponibilidade das saídas e a
preservação delas em caminhos de falha foram implementados no bloco 4. Propriedade da
tentativa, limpeza e retry continuam no bloco 5. Cobertura de efeitos
residuais, composição e publicação da aprovação atual, e reconstrução LTO
continuam nos blocos 6–8. `current_approved` permanece falso.

Layouts ABI, runtime e canais incoming/result foram preservados. Este bloco
não amplia o subconjunto de chamadas ou tipos nem implementa a alternativa B
de transporte explícito do ambiente léxico.

## Verificação

`make -C transaction_test check-canais` usa GIMPLE real após `ompexp`, com
probes que invocam o verificador de produção e com a integração normal do
pipeline. Os negativos cobrem campos ausentes, slots trocados, extensão,
sobreposição, aliases, expiração, escape, escrita no incoming e adulteração
de valor/binding publicados. A entrada externa é exercitada diretamente e
em controladores extraídos por parallel.

Positivos incluem canais vazios, resultados escalares e ponteiros, aliases
legítimos de dados do programa, cópias equivalentes de ponteiros de buffers,
C/C++, O0/O2, checking 0/2, GC forçado e DSO/archive. Exceções efetivamente
lançadas e capturadas antes e depois da transação são exercitadas em C++,
também com ligação dinâmica/estática; não são exceções dentro de clones.

O teste antigo de ponteiro/binding corrompidos de `check-vida-util` agora injeta
a falha no início de IPA, após conferir que a nova inspeção dos canais passou.
A mesma adulteração anterior à certificação é recusada por `check-canais`.
Assim continuam separados o teste de recusa do compilador e o teste da defesa
executável do chamador. O plugin não cria uma aprovação nem altera o runtime.

## Resultados — 2026-11-06

- `all-gcc`: concluído sem avisos ou erros; self-tests internos desabilitados.
- Canais: 241 verificações e 44 execuções.
- Memória: 281 verificações e 32 execuções.
- Grafo transitivo: 317 verificações.
- Controlador: 225 verificações e 106 execuções.
- Integração: 1078 verificações e 930 execuções, incluindo falhas por posição,
  retry integral e repetido, visões privadas, GC e LTO funcional.
- Vida útil: 126 verificações, 32 execuções e 1708 cenários sintéticos.
- Fluxo: 73 verificações, 16 execuções e 816 cenários sintéticos.
- Guardas: 102 verificações, 24 execuções e 1036 cenários sintéticos.
- Composição: 93 verificações, 16 execuções e 816 cenários sintéticos.
- Status: 77 verificações, 16 execuções e 1680 cenários sintéticos.
- Falhas: 77 verificações, 16 execuções e 1856 cenários sintéticos.
- Propriedade: 77 verificações, 16 execuções e 2356 cenários sintéticos.
- ABI v1: 88 verificações, incluindo símbolos/versionamento e DSO/archive.
- Chamadas: 75 verificações e 18 execuções, incluindo LTO funcional.

Logs específicos em `transaction_test/.build/channels/`. Resultados nativos
Linux x86_64. LTO funcional não demonstra reconstrução da certificação em LTO.
Os testes não encerram as dependências dos blocos 5–8 nem a aceitação da 6.6.
