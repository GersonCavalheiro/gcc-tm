# Correspondência semântica após remapeamento — bloco 1 da 6.5.3

A certificação no frontend exige a relação entre controlador, corpo preparado,
parâmetros e capturas efetivamente presentes nos CFGs após tree-nested,
omplower e outlining. O certificado resultante ainda é histórico: os demais
blocos da 6.5.3 devem demonstrar memória, canais, fluxo e aceitação atual.

## Representação e eventos

A criação do controlador resolve cada argumento de captura enquanto sua
sequência avaliada está disponível. O resumo conserva somente declaração de
origem e papel (endereço do objeto ou valor transportado), na ordem dos
argumentos. Não conserva GIMPLE, descritores, instruções de acesso ou código
para reavaliar expressões. A assinatura preparada conserva tipos e identidades
posicionais dos parâmetros formais, incluindo incoming/result.

Os eventos reais de cópia de declarações em tree-inline e de movimentação do
CFG para outra função registram a origem das novas declarações. omp-low e
tree-nested registram a relação entre campo e declaração, distinguindo campo
por valor de campo contendo endereço. O evento de outlining registra a função
de origem. A consulta não depende de nomes ou de DECL_ABSTRACT_ORIGIN, que o
lowering pode retirar. Origens conflitantes ou cíclicas não autorizam prova.

## Inspeção independente do corpo atual

A checagem percorre os CFGs definidos e exige uma invocação única do auxiliar
por controlador, em um corpo de origem demonstrada. Confere parâmetros e
campos contra seus proprietários atuais. A identidade posicional dos formais
impede que parâmetros de mesmo tipo sejam permutados silenciosamente.

Para cada argumento de captura, reconstrói a origem usando as definições
atuais, cópias, conversões sem perda da representação de ponteiro, endereços
e campos cujo remapeamento foi registrado. Compara a origem e o papel com o
resumo produzido antes do remapeamento. Para campos contendo endereços,
inspeciona também os produtores existentes nos CFGs: armazenar o endereço de
outra declaração no campo não é autorizado por sua identidade ou seu tipo.

O domínio é conservador: definições múltiplas não resolvidas, expressões não
reconhecidas, profundidade superior a 64, deslocamentos não nulos sem regra
de correspondência, conversões que perdem bits e remapeamentos desconhecidos
são recusados. Não se amplia o subconjunto admitido pela integração.

## Limite com os demais blocos

O bloco demonstra a correspondência semântica das declarações/capturas e dos
canais posicionais através dos remapeamentos suportados no frontend. Não
certifica o objeto dinâmico apontado, limites, kind, PRIVATE/REGISTERED,
validade de buffers, inicialização, vida útil ou consumo sob sucesso. Essas
propriedades pertencem aos blocos 2–6. Certificação atual e reconstrução em
LTO pertencem aos blocos 7–8; current_approved permanece falso.

A fotografia de tipos não é comparação profunda de layouts. Os campos são
reconhecidos pelos eventos e pelo record atual; extensão e faixa da memória
que transportam serão provadas pelos blocos de memória e canais.

## Validação

A matriz transitiva contém pares positivos/negativos em C/C++, checking 0/2.
Inclui troca de argumentos de mesmo tipo, troca dos parâmetros formais,
troca de campos compartilhados no consumidor, endereços trocados no produtor,
truncamento de ponteiro, chamada removida/duplicada, proprietário de parâmetro
ou origem de outlining adulterados. O caso positivo com duas capturas int
compartilhadas é ligado dinamicamente e estaticamente e executado, verificando
os valores finais. As mutações são exclusivas dos plugins de teste.

O suporte funcional é adicionalmente exercitado pelas matrizes existentes de
controlador, integração e chamadas, com referências C++, parallel, O0/O2,
LTO e bibliotecas dinâmica/estática. O isolamento dos registros em LTO não
substitui sua reconstrução no bloco 8.

Resultado da matriz transitiva: 157 verificações e oito execuções positivas,
mais composição (93 verificações/16 execuções/816 cenários). O build all-gcc
passou sem avisos; self-tests internos estão desabilitados neste build.

O bloco 2 foi concluído posteriormente como componente de
[identidade e armazenamento no CFG](OPENMP_TRANSACTION_MEMORY.md), com
dependências explícitas dos blocos 3–5. Cobertura, aceitação atual e LTO seguem
nos blocos 6–8; a aprovação integral atual permanece falsa.
