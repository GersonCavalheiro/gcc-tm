# Certificação transitiva pré-lowering — subetapa 6.5.1

Atualização do bloco 7: a [composição atual do grafo](OPENMP_TRANSACTION_CURRENT_GRAPH.md)
publica aprovação agregada somente na nova API e na janela pré-SSA do frontend.
Este componente ou certificado histórico conserva seu contrato próprio e não
concede aprovação atual isoladamente. A autoridade agregada é revogada antes
de transformações. A [reconstrução LTO do bloco 8](OPENMP_TRANSACTION_LTO_GRAPH.md)
cria gerações independentes na mesma API; a aceitação integral da 6.6 permanece
pendente.


A subetapa registra um resultado transitivo histórico para cada região externa,
identificado pela revisão do controlador e pelas revisões locais de todas as
funções alcançáveis. A publicação é atômica: cada membro precisa estar preparado,
possuir prova local completa e válida, e ter as mesmas dependências no preflight
e no resumo independente. O controlador precisa estar aprovado e depender do
auxiliar da região. Arestas ausentes/excedentes, revisões antigas ou duplicadas,
stubs recusados e dependências incompletas impedem a publicação.

O grafo é percorrido por raiz e seus componentes fortemente conexos são
identificados por Kosaraju iterativo. Todos os membros e suas dependências são
verificados antes de publicar qualquer resultado; uma recusa em ciclo bloqueia
a raiz inteira. O registro conserva IDs escalares de revisão, declarações,
componentes e snapshots das arestas, sem GIMPLE ou descritores. A interface
fornece os IDs individualmente, sem expor a lista interna de membros.

A invalidação de uma revisão ou de sua dependência revoga os resultados que a
contêm. Na consulta, a alteração de uma lista de dependências também invalida
o resultado e propaga o estado aos chamadores; restaurar a lista não ressuscita
a aprovação. Revisões antigas não podem ser usadas para publicar novo resultado.
A propagação e a reconstrução completas continuam na segunda subetapa.

O resultado certificado é **histórico e relativo ao grafo pré-lowering**. Ele
não autoriza reutilização de evidência no IR remapeado e não é serializado como
certificado LTO. A terceira subetapa deverá definir reconstrução ou transferência
verificada após remapeamento, recusando os casos sem prova. O suporte funcional
LTO existente não demonstra, por si só, certificação através dessa fronteira.

`make -C transaction_test check-transitivo`: 33 verificações em C/C++, checking
0/2. Inclui recursão simples/mútua e composição dos componentes, revisão
invalidada antes/depois da publicação, arestas ausentes/excedentes, grafos
incompletos/duplicados, publicação atômica e mutação de aresta. As mutações do
plugin são testes; não são inseridas no programa funcional. O build all-gcc
passou; seus self-tests internos estão desabilitados nesta configuração.

Também passaram composição (93 verificações/16 execuções/816 cenários),
proprietário (77/16/2356), falhas (77/16/1856), integração com controlador
(1303 verificações/1036 execuções) e chamadas (75/18). Resultados limitados ao
ambiente nativo Linux x86_64; não representam o testsuite completo do GCC.
ABI, runtime e layouts preservados. Logs: `transaction_test/.build/graph/`.
