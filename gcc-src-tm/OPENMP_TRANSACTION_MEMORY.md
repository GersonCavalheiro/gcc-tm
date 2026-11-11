# Identidade e armazenamento no CFG — bloco 2 da 6.5.3

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

Estado: bloco 2 concluído como componente do frontend. As dependências externas
abaixo exigem composição; não há aprovação integral atual da transação.

Atualização: o [bloco 3](OPENMP_TRANSACTION_CHANNELS.md) foi concluído como
componente dos canais no CFG atual. Sua prova ainda depende de STATUS | OWNER.
Compor essa evidência com os requisitos do modelo de memória e conceder uma
aprovação atual pertence ao bloco 7; a consulta de memória não muda para uma
aprovação integral por esse motivo.

A [prova de status do bloco 4](OPENMP_TRANSACTION_CURRENT_STATUS.md) também
foi concluída como componente atual, com dependência OWNER. O bloco 5 é o
próximo trabalho; os requisitos dos componentes ainda precisam ser compostos.

## Fronteira e resultado

`gcc/newtm-memory.{cc,h}` verifica os corpos preparados no CFG real do frontend,
depois dos remapeamentos e antes de SSA. A chamada integra `check_local_graph`;
uma recusa é erro de compilação, inclusive com checking 0. As verificações
estáticas anteriores permanecem como checagens adicionais. Identidades
transportadas passam obrigatoriamente pelo modelo comum, sem aprovação por
tipo ABI ou por uma guarda numericamente válida isolada.

O resultado é DEPENDENT ou REFUSED. DEPENDENT significa que as propriedades
locais de identidade/armazenamento passaram e que existem requisitos externos
expressos por CHANNELS, STATUS e OWNER. Não significa aprovação integral atual.
`newtm_memory_get` expõe a última inspeção, sua quantidade de operações e essas
dependências; não é certificado reutilizável depois de outra transformação.
Revisões e barreira de aceitação atual pertencem ao bloco 7. LTO pertence ao 8.

## Entrega 1 — modelo de objeto, instância e layout

A origem canônica de declaração e a instância física atual são tratadas
separadamente. Uma origem não pode colapsar duas alocações atuais. As alocações
locais são relativas à ativação da função; pointees recebidos conservam o caminho
do parâmetro/canal, slot, campo e deslocamento. Uma tag PRIVATE constante exige
alocação não estática da própria ativação. Uma tag transportada exige provar a
seleção PRIVATE/REGISTERED no ponto de uso, condicionada à validade de seu canal.
Declarações e nomes SSA de outra ativação não são variáveis da função atual.
A representação virtual de um campo só é aceita depois de verificar seu frame.
Alocações globais conservam a identidade própria; uma associação de declarações
não autoriza substituir um objeto global por outro.

Layouts são termos escalares congelados: classe, extensão, alinhamento,
largura/sinal e campos com posição, extensão e tipo. Pointees são descritos
profundamente por uma camada e, além dela, pelo cabeçalho da alocação; cada
dereferência descreve novamente o tipo acessado. Isso limita a recursão sem
confundir o layout do ponteiro com o do objeto referido.

Campos remapeados precisam pertencer ao record atual e conservar o tipo da
declaração representada, incluindo o papel por valor/endereço. A inspeção de
frames confere intervalos sem overflow, ausência de sobreposição, alinhamento, instância
local ou cadeia estática efetiva e consistência da ativação léxica. Não basta
reconhecer o nome do campo ou sua declaração de origem. Os produtores das
capturas continuam sujeitos à correspondência do bloco 1; vida útil e validade
do frame emprestado são requisitos do bloco 3.

## Entrega 2 — reconstrução coerente dos valores

Na janela válida são resumidos REGISTER, READ, WRITE, acessos privados e
formações de endereço. Cada contrato identifica o papel da operação, seu canal
de status ou resultado, o efeito privado e a tupla objeto/extensão/deslocamento/
tamanho/kind/storage/endereço lógico. Operações matemáticas conservam também
seus operandos, operador e representação esperados.

Os resumos são termos de prova com declarações e constantes; não conservam
GIMPLE, descritores ou expressões para reexecutar. SSA_NAME é representado por
declaração e número de versão, pois o nó SSA original contém vínculo com sua
instrução definidora. As âncoras temporárias são descartadas no fechamento,
inclusive em recusas e para operações não alcançáveis.

No CFG atual, os valores são reconstruídos pelas definições que chegam a cada
uso. Cópias legítimas e conversões sem efeito são normalizadas. Junções e laços
usam conjuntos de versões que chegam ao uso, sem desenrolamento. Cargas incluem
versões dos escritores que podem atingir sua memória. Os efeitos de chamadas
conservam destino, instância e argumentos, normalizando cópias desses argumentos.

Cada escritor conserva também seu histórico obrigatório de decisões de controle.
Junções intersectam fatos e uma nova avaliação substitui a decisão anterior do
predicado. Portanto, conservar apenas o conjunto de valores possíveis não basta:
inverter a condição que seleciona um índice ou trocar destinos de switch altera
a prova, mesmo com os mesmos escritores e limites numéricos válidos.

O adaptador do CFG conserva alternativas de switch, incluindo rótulos distintos
para a mesma aresta. Intervalos pequenos são atomizados em até 128 alternativas
para tolerar agrupamento de rótulos adjacentes pelo GCC; os maiores conservam seu
intervalo. Reagrupamentos que o domínio não demonstra são recusados. Não há
desenrolamento dos laços do programa nem reavaliação das expressões.

Escritas por aliases não recuperam uma aprovação anterior. O analisador de
guardas também descobre endereços nos operandos atuais, independentemente de
TREE_ADDRESSABLE. A separação entre alocações locais e canais emprestados gera
requisito explícito de não sobreposição para o bloco 3. Chamadas desconhecidas
não ganham efeito somente de leitura por seu protótipo.

## Entrega 3 — acessos, armazenamento e aritmética

Para cada operação são comparados os campos atuais com a identidade esperada.
Trocar campos/slots ou um objeto por outro numericamente compatível não conserva
a prova. As chamadas precisam usar os mesmos valores lógicos da tupla. Leituras
e escritas privadas conservam também o objeto e o efeito observado; escritas
repetidas no mesmo endereço não são associadas apenas pelo endereço.
Construtores conservam elementos e flags: inicialização com zeros, inicialização
sem limpeza e clobber são efeitos distintos. Constantes escalares, incluindo as
reais, conservam tipo e valor.

Antes do uso, o verificador exige objeto e extensão não nulos, kind e storage
admitidos, representabilidade do último byte e do deslocamento, deslocamento
dentro da extensão e tamanho dentro do restante. Os predicados usam clamp e
subtrações seguras. Acessos escalares não vazios cobrem o objeto inteiro.
O endereço lógico precisa corresponder ao objeto canônico mais o deslocamento.

O braço privado exige storage == PRIVATE. O braço de runtime exige storage
admitido e diferente de PRIVATE, portanto REGISTERED. Nenhuma falha de registro
é convertida em classificação privada.

O modelo verifica o índice/operador/conversão que efetivamente produz a
identidade, não somente a faixa do resultado final. Operações matemáticas
identificadas na preparação precisam conservar seus operandos e representação;
somas/subtrações sem overflow são demonstradas sobre os valores atuais.
Conversões estreitas exigem limites e os resultados de operações checadas
exigem a flag de sua própria instância. Esses requisitos valem nos usos
dependentes, inclusive nas formações de ponteiro anteriores a READ/WRITE.
Copiar ou projetar um resultado não requer a flag antecipadamente: ela precisa
estar estabelecida antes de consumir o resultado na operação dependente.

## Entrega 4 — fechamento, recusas e testes

Não há estado dinâmico pendente convertido em sucesso. Perder instância,
proveniência, layout ou guarda gera recusa. Profundidade superior a 64 e formas
que o domínio não reconstrói também são recusadas. O adaptador conserva seus
limites pré-SSA: PHIs, arestas anormais/excepcionais e efeitos sem regra não são
certificados. A análise não executa expressões do programa nem modifica seu CFG.

`make -C transaction_test check-memoria` cobre identidades dinâmicas, dois slots,
alocações privadas, retorno de ponteiros, registro de objetos locais capturados
e subobjetos de agregados em C. Os positivos são executados em C/C++, O0/O2,
checking 0/2, GC forçado e ligação dinâmica/estática. Cópias equivalentes também
precisam continuar aceitas e executáveis.

As mutações alteram objeto, extensão, kind, armazenamento, índice, conversão,
offset, alocação privada, layout, acessos privados e memória por aliases.
Incluem inversão da escolha do índice e troca de destinos de switch mantendo os
mesmos escritores, além de zeragem trocada por clobber, constante real alterada
e falsa correspondência entre declarações de objetos globais distintos.
Antecipam endereço e adulteram instância/intervalos de frames GNU. As recusas
precisam vir da inspeção de produção, com GIMPLE válido. Uma consulta isolada
adicional exerce descoberta de alias antes de atualizar TREE_ADDRESSABLE e
retira a mutação antes da verificação de consistência do pass manager. Outra
consulta isolada tenta usar armazenamento de outra ativação com uma origem
declarada artificialmente igual, restaurando o corpo após a recusa.

## Dependências que não são encerradas por este bloco

- Bloco 3: origem/produção e consumo dos canais, inicialização, vida útil,
  buffers, extensão efetiva dos objetos emprestados e não sobreposição foram
  implementados no componente independente de canais; sua composição com as
  dependências de status e propriedade ainda é necessária.
- Bloco 4: sucesso da instância correta antes de consumir saídas e identidade
  realmente disponibilizada por registros e chamadas foram implementados no
  componente de status; a composição e a dependência proprietária permanecem.
- Bloco 5: lease/propriedade da tentativa, profundidade e fronteiras de limpeza,
  confirmação, descarte e retry.
- Bloco 6: cobertura de todos os efeitos residuais, inclusive operações que
  não pertencem ao inventário de acessos resumidos.
- Blocos 7–8: publicação/invalidação da aprovação atual e reconstrução em LTO.

ABI, runtime, layouts de `newtm_binding_v1`/`newtm_clone_binding_v1`, canais
incoming/result e alternativa B futura permanecem preservados. O bloco não é
uma prova geral de aritmética C/C++ nem um detector de ponteiros pendentes.

## Resultados da validação — 2026-11-06

- Build `all-gcc`: concluído sem avisos; self-tests internos desabilitados.
- Memória: 281 verificações e 32 execuções, incluindo GC forçado, C/C++,
  checking 0/2, O0/O2 e bibliotecas dinâmica/estática.
- Grafo transitivo: 317 verificações.
- Controlador: 225 verificações e 106 execuções.
- Integração: 1078 verificações e 930 execuções, incluindo LTO funcional,
  falhas em todas as posições, retry completo, PROT_NONE e saídas preservadas.
- Fluxo: 73 verificações, 16 execuções e 816 cenários sintéticos.
- Guardas: 102 verificações, 24 execuções e 1036 cenários sintéticos.
- Composição: 93 verificações, 16 execuções e 816 cenários sintéticos.
- Status: 77 verificações, 16 execuções e 1680 cenários sintéticos.
- Vida útil: 126 verificações, 32 execuções e 1708 cenários sintéticos.
- Falhas: 77 verificações, 16 execuções e 1856 cenários sintéticos.
- Propriedade: 77 verificações, 16 execuções e 2356 cenários sintéticos.
- ABI v1: 88 verificações, incluindo símbolos/versionamento, DSO/archive e falhas.
- Chamadas: 75 verificações e 18 execuções, incluindo LTO funcional e recusas.

As contagens não substituem a prova nem encerram as dependências listadas.
Resultados limitados ao ambiente nativo Linux x86_64 deste build.
