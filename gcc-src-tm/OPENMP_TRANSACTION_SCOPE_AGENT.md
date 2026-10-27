# Agente orientador: escopo e tipos dos identificadores de transaction

## Missão e limites

Oriente a implementação da resolução dos identificadores de
`#pragma omp transaction(a, b)` nesta árvore GCC 16.2.0, para verificar sua
visibilidade no ponto da diretiva e obter seus tipos. Leia também
[OPENMP_AGENT.md](OPENMP_AGENT.md),
[OPENMP_CRITICAL_AGENT.md](OPENMP_CRITICAL_AGENT.md) e
[OPENMP_TRANSACTION_IMPLEMENTATION.md](OPENMP_TRANSACTION_IMPLEMENTATION.md) e
[OPENMP_TRANSACTION_ACCESS_AGENT.md](OPENMP_TRANSACTION_ACCESS_AGENT.md).

Este documento foi elaborado por inspeção dos fontes locais. Os caminhos são
relativos à raiz dos fontes `/usr/src/gcc-omp-transaction`; o build fica em
`/usr/src/gcc-build`. Localize funções pelo nome e confira seus chamadores.
A criação deste guia não implementa a validação no compilador nem altera a
semântica da diretiva. Não acrescente sincronização ou memória transacional
como consequência da resolução dos nomes.

## Estado atual

Os parsers resolvem os nomes antes do corpo, aceitam VAR_DECL/PARM_DECL e
preservam as declarações em OMP_TRANSACTION_OBJECTS. C++ usa a busca do parser
com verificação de acesso e ambiguidade e processa referências automáticas
externas. A instanciação substitui a lista em cp/pt.cc. A gimplificação exige
tipo completo de tamanho constante, registra referências C++ pelo objeto
referido e converte os itens em chamadas runtime. A lista não permanece como
operando do marcador GIMPLE. Tipos e lowering só são impressos com
-ftransaction-debug.

As orientações abaixo são critérios de manutenção e integração futura, não
uma lista de funcionalidades ausentes. Conferir o estado em
OPENMP_TRANSACTION_IMPLEMENTATION.md antes de alterar representações.

## Momento correto e contrato semântico

Resolva cada nome durante a análise da lista, antes de chamar
`c_parser_omp_structured_block` ou `cp_parser_omp_structured_block`. Nesse ponto,
os bindings ativos correspondem ao contexto da diretiva. Uma variável declarada
somente dentro do corpo não pode tornar válido um argumento da diretiva.
Uma declaração posterior no bloco também não deve ser encontrada por uma
busca adiada. Respeite as regras próprias de C++ para contexto de classe e
templates, em vez de simular visibilidade por ordem textual.

Use a busca da linguagem: ela trata escopos externos e ocultação por declarações
mais internas. Não procure identificadores por texto nos fontes, não percorra
apenas os locais da função e não use `DECL_CONTEXT` como teste de visibilidade.
Um nome global pode estar visível dentro de uma função, e o contexto de uma
declaração não informa sozinho qual binding está ativo.

Antes de implementar rejeições, defina quais entidades transaction admite.
O pedido de conhecer tipos não determina se funções, constantes de enumeração,
typedefs ou membros são argumentos válidos. Separe três resultados:

1. O nome foi encontrado segundo as regras da linguagem?
2. A entidade encontrada pertence a uma categoria permitida pela extensão?
3. Seu tipo já é conhecido ou depende de instanciação?

Se o contrato escolhido for **variáveis e parâmetros**, aceite inicialmente
`VAR_DECL` e `PARM_DECL`; documente essa escolha. Não rejeite parâmetros por
testar apenas `VAR_DECL`. Defina separadamente membros não estáticos
(`FIELD_DECL`), structured bindings, constantes e outros casos C++.
Preserve a gramática atual de identificadores simples: usar um parser de
expressões ou listas OpenMP mais amplo não deve habilitar silenciosamente
`ns::x`, `obj.x`, `*p`, seções de arrays ou expressões.

## Resolução no front-end C

Referências concretas:

- `gcc/c/c-decl.cc`: `lookup_name(tree name)` consulta o binding ordinário
  atual (`I_SYMBOL_BINDING`), considera sua visibilidade e retorna a declaração
  ou `NULL_TREE`. Leia as exceções existentes, inclusive versões de funções.
- `gcc/c/c-parser.cc`: no processamento de listas OpenMP, o trecho que chama
  `lookup_name(c_parser_peek_token(parser)->value)` mostra o diagnóstico via
  `undeclared_variable` e a recuperação com `error_mark_node`.
- `gcc/c/c-parser.cc`: `c_parser_omp_var_list_parens` é uma entrada para
  estudar esse fluxo; suas regras completas não são a gramática de transaction.

Em `newtm_c_parser_transaction`, capture `value` e `location` do token antes
da chamada que o consome. Depois de confirmar `CPP_NAME`, use o identificador
em `lookup_name`. Para ausência, reutilize `undeclared_variable(loc, id)`
conforme o padrão local e marque o item como inválido. Para uma declaração
encontrada, valide sua categoria antes de extrair o tipo.

Pseudocódigo, a adaptar à recuperação existente:

```text
confirmar token CPP_NAME
salvar id = token->value e loc = token->location
consumir identificador
decl = lookup_name(id)
se decl é nulo: diagnosticar nome não declarado
senão se decl é error_mark_node: propagar falha sem diagnóstico duplicado
senão: classificar entidade e obter TREE_TYPE(decl), quando aplicável
continuar vírgula / fechamento / fim do pragma
```

O namespace ordinário de C inclui variáveis, funções e typedefs; encontrar um
nome não prova que é variável. Um tag `struct S` pertence a outro namespace e
não deve ser buscado como variável. Confira também `id_kind`: o parser
aceita `CPP_NAME` sem restringir a `C_ID_ID`, e um typedef pode chegar como nome
com outra classificação. Não transforme uma restrição de categoria em uma
mensagem enganosa de identificador inexistente.

## Resolução no front-end C++

Referências concretas em `gcc/cp/parser.cc`:

- `cp_parser_identifier`: análise do identificador simples, sem resolver nome.
- `cp_parser_lookup_name_simple(parser, name, location)`: wrapper local de
  `cp_parser_lookup_name` com checagem de dependência e busca usual de nome
  que não foi solicitado como tipo, template ou namespace.
- `cp_parser_lookup_name`: busca conforme o estado do parser, tratamento de
  ambiguidades e checagem de acessibilidade. Seu resultado não é sempre uma
  declaração simples; pode incluir sobrecargas e representações dependentes.
- `cp_parser_omp_var_list_no_open`: exemplo de resolução com o wrapper,
  diagnóstico por `cp_parser_name_lookup_error`, e tratamento de referências
  a variáveis automáticas externas por `outer_automatic_var_p` e
  `process_outer_var_ref`.
- `gcc/cp/name-lookup.cc`: implementação de `lookup_name` e infraestrutura
  de busca C++. Não confunda sua interface com a função homônima de C.

Em `newtm_cp_parser_transaction`, preserve a localização antes de
`cp_parser_identifier`, guarde seu retorno e resolva o nome com o mecanismo
do parser. O padrão de `cp_parser_omp_var_list_no_open` é o ponto de partida;
não substitua toda a rotina pela lista OpenMP existente, que aceita sintaxe e
aplica tratamentos específicos de cláusulas diferentes.

Trate `error_mark_node` da análise e da resolução. Siga o padrão de diagnóstico
do chamador e confira se a busca já emitiu erro, evitando mensagens duplicadas.
Não aplique `TREE_TYPE` indiscriminadamente a `OVERLOAD`, `BASELINK`,
`SCOPE_REF` ou qualquer resultado ainda não classificado.

Encontrar uma declaração e ter acesso permitido são condições distintas. A
infraestrutura de busca pode emitir um erro de acesso e ainda retornar uma
declaração; um retorno diferente de `error_mark_node` não certifica sozinho
que toda a validação semântica passou.

Para lambdas, funções locais e membros, defina se listar um nome conta como
uso ou captura. O processamento de variáveis externas das cláusulas OpenMP é
uma referência a estudar, não uma operação a copiar sem esse contrato.
Obter metadados de tipo não deve causar inadvertidamente uma avaliação do
objeto ou uma conversão lvalue-to-rvalue.

## Como obter e interpretar o tipo

Após uma resolução válida e classificação da entidade, o tipo declarado de
uma variável ou parâmetro está em `TREE_TYPE(decl)`, definido em `gcc/tree.h`.
Verifique valores nulos e `error_mark_node` antes de usar predicados de tipo.

| Informação | Ponto de partida |
| --- | --- |
| Categoria da entidade | `TREE_CODE(decl)`, `DECL_P(decl)` |
| Tipo declarado da variável/parâmetro | `TREE_TYPE(decl)` |
| Categoria do tipo | `TREE_CODE(type)` e predicados pertinentes de `tree.h` |
| Qualificadores | `TYPE_QUALS(type)` |
| Tipo apontado, referenciado ou elemento de array | `TREE_TYPE(type)`, depois de verificar a categoria |
| Limites do array | `TYPE_DOMAIN(type)` quando o tipo é `ARRAY_TYPE` |
| Dependência de tipo C++ | `dependent_type_p(type)`, declarado em `gcc/cp/cp-tree.h` |
| Impressão em diagnóstico | formato `%qT` com um nó de tipo válido |

Preserve o tipo original. Não aplique `TYPE_MAIN_VARIANT` antes de registrar
qualificadores, pois ele remove diferenças que podem ser relevantes.
Uma referência C++ tem tipo declarado diferente do tipo da expressão após
`convert_from_reference`. Arrays não devem sofrer decay só para consultar seu
tipo. Um parâmetro declarado com sintaxe de array já pode ter sido ajustado
para ponteiro pelas regras da linguagem.

Tipo conhecido não significa tipo completo ou tamanho constante. Ponteiros
para tipos incompletos, arrays de tamanho variável e tipos dependentes exigem
tratamento próprio. Não use `TYPE_SIZE_UNIT` como teste de existência do tipo
e não converta seu valor para inteiro sem validar que é constante e cabe no
formato de destino. Não force layout de tipos para a simples consulta.

## Contrato de identidade para instrumentação de acessos

Nome visível, entidade admitida e tipo instrumentável são verificações distintas.
A aceitação atual de VAR_DECL/PARM_DECL com tipo completo de tamanho constante
não comprova suporte a todas as operações desse tipo. Na integração funcional,
validar o subconjunto do agente de acesso, respeitando const, volatile, atômicos,
vida útil e propriedades C++; não apagar qualificadores para admitir escritas.
As restrições dependentes devem ser repetidas após instanciação.

Preservar dados suficientes para o compilador construir os argumentos propostos
`object`, `offset` e `size`, sem resolver novamente o nome no lowering:

| Informação | Responsabilidade |
| --- | --- |
| Declaração resolvida e localização | Identidade lexical e diagnóstico do argumento/acesso. |
| Tipo e qualificadores originais | Permissão de leitura/escrita e classificação dos acessos admitidos. |
| Objeto canônico e extensão registrada | Unidade inteira de conflito e validação dos limites de subacessos. |
| Subobjeto e deslocamento em bytes | Campo/elemento suportado dentro do objeto canônico. |
| Expressões de endereço/índice | Preservar avaliação única e instrumentar suas próprias leituras quando necessário. |

A resolução da lista fornece a identidade inicial; o mapeamento de cada acesso
para subobjeto/deslocamento pertence à instrumentação posterior. Não antecipar
endereços de execução no parser nem avaliar índices ou objetos para obter tipos.
Ponteiros listados não incluem automaticamente a memória apontada; referências
C++ designam o referido. Aliases suportados compartilham a mesma chave canônica;
a hash de igualdade exata não resolve aliases ou sobreposições por conta própria.

Ao transportar esses dados, remapear declarações em templates, inlining,
funções aninhadas e outlining. Usar a instância efetiva de private/reduction
externos. Preservar metadados antes da varredura OpenMP quando afetarem captura;
o marcador GIMPLE atual guarda somente o corpo. Atualizar walkers, cópia e dumps
quando o layout mudar, sem guardar ponteiros para tokens ou estruturas efêmeras.

Conhecer o binding não autoriza acesso: o registro precisa ter ocorrido na
execução antes da operação. Uma lista interna não torna transacionais acessos
anteriores. Escritas externas não listadas permanecem restritas; a política de
leituras externas não listadas continua pendente. Não usar o novo contrato para
ampliar silenciosamente a gramática de identificadores simples.

## Templates e preservação da informação

Um parâmetro `T &x` de template pode estar perfeitamente visível mesmo quando
seu tipo ainda depende de `T`. Não emita “não declarado” só por não conhecer o
tipo concreto. Faça imediatamente as verificações independentes e adie as
restrições que precisam da substituição.

Se a informação for necessária somente para diagnóstico imediato de entidades
não dependentes, a consulta pode ficar no front-end. Se for necessária na
instanciação ou no lowering, preserve declarações ou representações
dependentes e localizações na árvore da construção; strings isoladas não
preservam identidade nem bindings.

Nesse segundo caso, será necessário planejar alterações de representação:

1. Preservar o armazenamento e os acessores existentes em `OMP_TRANSACTION`,
   `newtm_finish_transaction` e seus chamadores.
2. Em `gcc/cp/pt.cc`, separar o tratamento de transaction quando necessário,
   substituir os novos dados com a infraestrutura de templates e repetir as
   verificações dependentes. O caso atual já substitui corpo e lista; preservar esse tratamento.
3. Se os dados precisarem chegar ao lowering, atualizar também o layout GIMPLE,
   `newtm_build_transaction`, a conversão em `gcc/gimplify.cc` e seus consumidores.
4. Auditar walkers, cópia, dumps, módulos C++ e eventual serialização, conforme
   o tempo de vida dos dados. Não guardar ponteiros para tokens ou vetores
   temporários em nós persistentes; usar armazenamento rastreado pelo GC.

Não volte a buscar uma string de nome no lowering: o escopo léxico original
já pode ter terminado, e a ocultação torna essa busca incorreta. Preserve a
declaração resolvida e faça o remapeamento necessário para templates, cópia
ou outlining. A visibilidade lexical também não determina sozinha qual cópia
de uma variável `private` de uma região OpenMP externa será usada em runtime.

## Recuperação e testes para uma implementação posterior

Preserve a recuperação até `CPP_PRAGMA_EOL` e analise o corpo mesmo após erro
na lista, conforme o fluxo atual. Um argumento inválido não deve impedir a
análise dos seguintes nem corromper a próxima construção. Defina a política
de duplicatas separadamente; nomes repetidos são aceitos e deduplicados por endereço no contexto runtime.

Adicione casos compartilhados em `gcc/testsuite/c-c++-common/gomp/` e casos
C++ específicos em `gcc/testsuite/g++.dg/gomp/`, incluindo:

- Variáveis globais, locais e parâmetros visíveis; tipo esperado de cada um.
- Ocultação: global `int x` e local `double x`; resolver a declaração local.
- Nome não declarado, local de bloco encerrado, declaração posterior e nome
  declarado somente no corpo de transaction.
- Categorias encontradas mas não permitidas pelo contrato escolhido: typedef,
  função, enumeração e, em C++, namespace ou conjunto de sobrecargas.
- Ponteiros, arrays, qualificadores, structs e, em C++, referências e classes.
- C++: acesso, membros, lambdas, ambiguidades e templates com tipo dependente;
  testar definição e instanciação, inclusive falhas que só surgem nesta última.
- Macros, `_Pragma`, listas com múltiplos itens e recuperação depois de erro.
- Formas alternativas e módulos C++ se o armazenamento novo atingir esses caminhos.

Verifique a identidade da declaração e o tipo com dumps apropriados ou uma
instrumentação temporária controlada. Compilar sem erro, por si só, não prova
que o tipo correto foi selecionado. Evite mensagens incondicionais no compilador,
que já são uma fonte de falhas registrada no relatório de transaction.

Execute rebuild e os subconjuntos C/C++ de transaction no diretório de build,
seguindo os comandos do relatório de implementação. Confira `.sum` e `.log`,
distinga falhas anteriores de novas regressões e relate os testes realmente
executados. Este guia não executou build ou testes do compilador.

## Pré e pós-condições do trabalho de resolução

Os contratos dos parsers e construtores estão reunidos em
[OPENMP_TRANSACTION_IMPLEMENTATION.md](OPENMP_TRANSACTION_IMPLEMENTATION.md).
A resolução exige bindings válidos no ponto da lista e árvores/localizações
com vida útil adequada; em sucesso preserva identidade e tipo sem avaliar o
objeto. Em erro, diagnostica/recupera sem transformar entidade inválida em objeto
registrável nem certificar a validade dos demais dados parcialmente construídos.
Não adquire reservas nem cria efeito runtime. Instanciação e remapeamento devem
preservar os contratos antes de invocar os serviços do agente de acesso.

## Critério de conclusão da implementação

A resolução usa o escopo do ponto da diretiva, respeita ocultação e regras da
linguagem, distingue ausência de categoria inválida, obtém tipos sem conversões
indevidas e trata dependência C++ no momento correto. Qualquer informação usada
depois do parsing tem representação persistente e seus consumidores atualizados.
Na integração funcional, verificar também identidade canônica, mapeamento dos
subacessos e permissões segundo o agente de acesso, com testes de campo/elemento,
referências, aliases suportados, tipos rejeitados e private/reduction externos.
Não apresentar validação de nomes como prova de cobertura da instrumentação.
Relate o contrato de entidades aceitas, os testes e limitações remanescentes.

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
