# Invalidação e revalidação transitiva — subetapa 6.5.2

Atualização do bloco 7: a [composição atual do grafo](OPENMP_TRANSACTION_CURRENT_GRAPH.md)
publica aprovação agregada somente na nova API e na janela pré-SSA do frontend.
Este componente ou certificado histórico conserva seu contrato próprio e não
concede aprovação atual isoladamente. A autoridade agregada é revogada antes
de transformações. A [reconstrução LTO do bloco 8](OPENMP_TRANSACTION_LTO_GRAPH.md)
cria gerações independentes na mesma API; a aceitação integral da 6.6 permanece
pendente.


Este contrato trata exclusivamente dos certificados históricos construídos antes
do lowering. Uma revisão local registra dependências declaradas e sua situação;
a revogação propaga-se aos chamadores até o ponto fixo, incluindo ciclos. A
invalidação propagada não apaga por si só uma prova local independente. Escopos
de cópia não herdam aprovação.

O certificado guarda IDs de revisões, arestas, componentes e uma fotografia da
identidade do tipo de função, do tipo de retorno e da lista de tipos de
argumentos. A consulta detecta alterações observáveis dessas ligações e de
arestas e invalida os certificados dependentes. Uma revisão em que se observou
alteração direta fica marcada: restaurar os valores antigos não restaura sua
aprovação; requer nova prova local. A fotografia compara identidades e elementos
da lista, não é uma comparação profunda de estruturas de tipos ou atributos.
Alterações transitórias que ocorram e sejam revertidas sem passar por uma
consulta exigem que a transformação invoque a invalidação explícita; a fotografia
não substitui os pontos de invalidação.

A revalidação é explícita e cria um novo certificado, sem ressuscitar os
anteriores. Confere assinaturas registradas, arestas das revisões reaproveitadas,
revisões locais fechadas e atuais, preflight, controlador, fechamento completo
e componentes; aceita invalidação meramente propagada, mas não revisão
marcada por alteração direta nem prova local revogada. A publicação e a retirada
das marcas propagadas ocorrem somente depois de todas as verificações. A
notificação de dependentes não substitui a invalidação da evidência diretamente
alterada. Estes mecanismos são interfaces de certificação histórica, não uma
permissão para utilizar a prova em IR remapeado.

Os testes `check-transitivo` exercitam recursão direta e mútua em C/C++, checking
0/2, revogação antes e depois da publicação, alteração e restauração de aresta,
substituição de assinatura e mutação da lista de argumentos, rejeição de
revalidação incompleta e recuperação de invalidação propagada sem reaproveitar
certificado anterior. A interface explícita de revalidação é exercitada pelo
plugin de teste; ainda não há chamada automática no pipeline do compilador para
reconstruir um grafo após uma transformação. A 6.5.3 continua responsável pelas regras e integração de
remapeamento, outlining e fronteiras LTO, inclusive tipos/layouts e pontos de
invalidação correspondentes. Não se consulta descriptor retirado em omplower.
