# Inicialização, vida útil e publicação — subetapa 6.4.4

**Atualização 6.5.3, bloco 3:** a [prova dos canais no CFG remapeado](OPENMP_TRANSACTION_CHANNELS.md)
reconstrói objetos e transições lexicais atuais e confere incoming/result e
buffers após outlining. O teste da defesa executável injeta a corrupção no
início de IPA, depois de exigir a inspeção atual dos canais; adulterações
anteriores são recusadas pelo compilador. Layouts e runtime foram preservados.
Status e propriedade no CFG remapeado continuam nos blocos 4–5.

**Atualização 6.4.7 — 2026-11-04:** a [composição local](OPENMP_TRANSACTION_COMPOSITION.md) foi implementada. Pendências históricas referentes à 6.4.7 estão superadas; certificação transitiva e remapeamento seguem na 6.5, e aceitação integral na 6.6.

**Atualização 6.4.6 — 2026-11-04:** a
[prova do protocolo proprietário](OPENMP_TRANSACTION_OWNERSHIP.md) está implementada.
Demonstra admissão, propriedade/vida útil do slot, confirmação, condenação e
limpeza, contador saturante, fatal e retry integral com preparação fresca.
As pendências históricas abaixo relativas à 6.4.6 estão superadas. Composição
segue na 6.4.7, certificação/remapeamento na 6.5 e aceitação integral na 6.6.
Não há COBERTA nem certificado novo; ABI, runtime e layouts preservados.

**Atualização 6.4.5 — 2026-11-04:** a
[prova de falhas e aninhamento](OPENMP_TRANSACTION_FAILURES.md) está implementada.
Exige causa preservada, retorno efetivo de FAIL, contexto emprestado e níveis
balanceados em sucesso; recusa continuação normal e ciclos após falha.
As pendências históricas abaixo relativas à 6.4.5 estão superadas. Controlador
e composição seguem nas 6.4.6–6.4.7; certificação/remapeamento, na 6.5.
Não há COBERTA nem certificado novo.

Implementada em 2026-11-04 em `gcc/newtm-lifetime.{cc,h}`, integrada a
`newtm_obligation_verify` depois das provas de
[guardas](OPENMP_TRANSACTION_GUARDS.md) e
[status por operação](OPENMP_TRANSACTION_STATUS.md). Segue V04–V06 do
[contrato de verificação](OPENMP_TRANSACTION_VERIFICATION.md).

## Escopo e critério de conclusão

Antes de usar um transporte, todos os campos semânticos necessários precisam
estar inicializados em cada caminho alcançável. O objeto e as identidades que
ele transporta precisam continuar vivos. Buffers internos e seus endereços não
podem escapar dos empréstimos admitidos. Um retorno com sucesso precisa
entregar valor e binding completos sob o mesmo sucesso de publicação; um
retorno de falha preserva ambas as saídas.

A análise verifica o High GIMPLE real, inclusive com checking 0. Associações
do emissor identificam obrigações, sem aprová-las. O domínio continua fechado:
falta de prova gera V04/V05 e recusa do caminho funcional, sem fallback.
Não há estado COBERTA nem novo certificado local ou transitivo.

O critério local da 6.4.4 foi atendido para o domínio demonstrável descrito
abaixo; equivalências não reconhecidas continuam sendo recusadas.

Propagação completa de falhas/aninhamento (6.4.5), protocolo proprietário do
controlador (6.4.6), composição das provas (6.4.7), certificação/remapeamento
(6.5) e mutações integrais (6.6) permanecem separados.

## Campos definidos e buffers emprestados

Objetos ABI são divididos em campos por seus tipos, deslocamentos e tamanhos
reais do target. Records e arrays constantes são percorridos recursivamente;
padding não precisa ser inicializado. O estado distingue inicialização de cada
campo, validade lexical e origens possíveis de endereços armazenados.

Uma escrita completa define o campo quando seus operandos estão disponíveis.
Cópias e cálculos escalares propagam a definição; atribuir um temporário
indefinido a um campo não o inicializa. Construtores aninhados respeitam campos
explícitos, inicialização implícita a zero e CONSTRUCTOR_NO_CLEARING. Uma cópia
de agregado exige os campos de origem; clobbers retiram as definições afetadas.

Os campos dos bindings, buffers READ/WRITE, resultados de chamadas, pares de
overflow e temporários matemáticos de identidade são inspecionados. A prova
dos resultados de chamada compõe-se com a 6.4.3: a definição de saída não é
autorização para consumi-la quando a chamada falhou.

Antes de uma chamada preparada, o tamanho do incoming vem da assinatura real.
São verificados todos os seus elementos, inclusive a vida útil dos objetos
apontados por elementos posteriores ao primeiro. Incoming e resultados não
podem se sobrepor. WRITE exige fonte privada inicializada; saídas exigem
endereços locais dominantes e buffers tipados, conforme a inspeção estrutural.

Capturas de referência C++ usam uma representação privada imutável. O buffer
real de representação precisa estar inicializado antes do empréstimo ao
auxiliar, identificado pelo tipo da captura e pelo argumento atual. Isso não
autoriza leitura comum do referido. Nomes ou DECL_ARTIFICIAL não fornecem prova.

Esta etapa não é um detector geral de memória indefinida no programa original.
Memória ordinária segue o contrato de execução definida da linguagem; os
buffers do compilador e suas dependências escalares são verificados aqui.
Efeitos indiretos sobre locais ordinários não inventam prova de inicialização
de um transporte interno.

## Vida útil, origens e escapes

Entradas e saídas lexicais vêm do grafo da 6.4.1. Entrar por desvio num BIND
admitido não executa seus inicializadores. Entradas de nova ativação lexical
retiram fatos antigos; sair do escopo ou terminar armazenamento invalida o
objeto. CLOBBER_STORAGE_BEGIN/OBJECT_BEGIN reiniciam sua disponibilidade;
CLOBBER_STORAGE_END/OBJECT_END a encerram. Parâmetros pertencem à ativação.

O estado conserva conjuntos de origens de endereços, não um único ponteiro
escolhido numa junção. Cópias, agregados, aliases e cargas indiretas preservam
essas possibilidades. Uma leitura/emprestimo que possa usar objeto expirado
é recusada. Na junção, definições e vida útil precisam valer em todos os
predecessores; origens e possibilidades de publicação são reunidas.

Os transportes incoming/result emprestados possuem identidade separada dos
dados do programa aos quais seus campos se referem. Endereços desses buffers
só podem ser passados nos papéis previstos. Campos carregados do incoming
descrevem memória do chamador; PRIVATE do chamador não se confunde com objeto
privado criado pelo callee. Portanto, retornar uma identidade PRIVATE emprestada
continua possível, mas retornar endereço de um local do próprio frame é recusado.

Origens possíveis de resultados ponteiro e efeitos indiretos de chamadas
preparadas são propagados a partir dos argumentos de programa, incluindo
objetos alcançáveis por mais de uma indireção. Uma chamada pode alterar uma
variável ponteiro do chamador; isso não permite manter uma origem antiga como
se o objeto não pudesse ter mudado. Dados que possam conter endereço privado
não são publicados por WRITE ou por canais de retorno.

Essa propagação é conservadora. Não é um resumo preciso da relação entre cada
argumento e cada resultado do callee; relações não demonstradas podem provocar
recusa. Melhorar precisão não autoriza ampliar tipos, calls ou escapes. O
trabalho de certificação transitiva permanece na 6.5.

## Publicação conjunta

Os canais de saída são obtidos da assinatura preparada, não de seus nomes.
Somente stores completos e tipados para esses canais contam como publicação.
O status final precisa ser uma operação STATUS efetiva, dominante, associada
àquela instância de publicação e demonstrada como sucesso pela análise da
6.4.3. Um status anterior da entrada ou de outra publicação não a autoriza.

O verificador acompanha as possibilidades de nenhuma saída, só valor, só
binding ou ambas. Sucesso exige exatamente o conjunto previsto, uma única
vez; falha exige nenhuma escrita. Retornos ainda desconhecidos em frames
com saída são recusados. O valor do retorno de status é capturado no nó
GIMPLE_RETURN, antes de executar cleanups, e conferido na saída efetiva da
continuação. Não se reavalia seu operando.

Depois da primeira publicação não se admite chamada que possa falhar ou
alterar o contexto. Isso conserva o formato atual da emissão e evita falha
posterior com uma saída já modificada. A entrega dos dois canais sob o mesmo
sucesso não promete dois stores fisicamente simultâneos; são saídas privadas
do chamador, conforme o contrato ABI existente.

Para retornos ponteiro, a emissão agora confere, antes de qualquer store:
`uintptr(valor) == uintptr(objeto) + deslocamento`. A validade/faixa anterior
garante a representabilidade da soma, verificada também pela 6.4.2. O inspetor
reconstrói a igualdade a partir dos campos realmente escritos no binding
privado e do valor realmente publicado, sem aceitar D_VALID como aprovação.
Inconsistência segue por FAIL/INVALID e preserva ambas as saídas.

## Correção necessária no escopo dos descritores

A verificação encontrou temporários escalares de descritores declarados num
BIND interno e usados posteriormente, por exemplo ao acessar uma referência
C++ depois de uma transação aninhada. Suas declarações agora pertencem ao BIND
da ativação preparada. As avaliações continuam nos pontos originais: não se
reexecuta expressão nem se move cálculo para antes da avaliação correspondente.

Somente os temporários de descritores têm essa duração. Objetos do programa
conservam seus escopos e expiração; um ponteiro para um local encerrado continua
inválido mesmo que seu descritor escalar esteja no escopo da ativação.

## Janela, preservação e limites

Grafo, estados, campos e conjuntos de origens existem somente na inspeção da
janela atual. Árvores são emprestadas do IR enraizado pelo inventário; os
predicados reconstruídos usam as raízes temporárias da análise de guardas.
O analisador não modifica GIMPLE, CFG, SSA ou propriedades da função.

Todos esses objetos terminam antes de newtm_obligation_end, tree-nested e
omplower. Não se consulta um descritor depois de remapeamento/outlining nem se
conserva uma aprovação operacional por manter uma árvore alcançável pelo GC.
Layouts compartilhados, ABI/runtime, opções e alternativa B futura permanecem
preservados. Novos serviços de runtime não foram introduzidos.

Layouts não constantes, profundidade excessiva ou arrays de transporte com
mais de 4096 elementos são recusados. O domínio é conservador: nem toda
equivalência válida de IR será reconhecida. Vida útil do armazenamento externo
emprestado continua sendo precondição dos chamadores, como na ABI existente.

## Validação

`make -C transaction_test check-vida-util` executa probes isolados e pelo
inspetor completo, com C/C++, O0/O2, LTO, DSO/archive, checking 0/2 e GC forçado.
São conferidas também imutabilidade do IR/CFG/propriedades, campos ausentes,
arrays, construtores aninhados, origem indefinida, sobreposição, clobbers,
junções, ciclos, referências expiradas, escapes e publicação parcial.

Um teste executável usa clone filho deliberadamente corrompido por plugin de
teste depois da verificação local: entrega ponteiro e binding inconsistentes.
O pai precisa retornar INVALID e preservar as duas saídas. Isso testa a nova
fronteira executável; não certifica o corpo corrompido nem substitui as regras
de invalidação/remapeamento da 6.5. O plugin usa canais de assinatura e não
consulta descritores expirados. O par positivo confirma a publicação normal.

Referências C++ são exercitadas depois de bloco aninhado, inclusive quando o
bloco condicional não executa, com confirmação final e memória privada antes
do commit. Logs e JSON: `transaction_test/.build/lifetime/`.

Resultado: **126 verificações, 32 execuções e 1708 cenários sintéticos**,
com 61 cenários em cada uma das 28 compilações isoladas/integradas. Seis
cenários por compilação usam o inspetor completo para campos ausentes, sem
depender de uma declaração de tratamento do emissor.

Com a versão final do compilador também passaram:

| Matriz | Verificações | Execuções explicitamente contadas |
| --- | ---: | ---: |
| Controlador + integração | 1303 | 1036 |
| Status | 77 | 16 |
| Fluxo | 73 | 16 |
| Inspeção | 73 | 16 |
| Inventário de obrigações | 73 | 16 |
| Guardas | 102 | 24 |
| Clones | 209 | 48 |
| Preparação | 37 | 8 |
| Leituras | 52 | 12 |
| Escritas | 56 | 12 |
| Chamadas | 75 | 18 |
| Ocorrências | 63 | — |
| Descritores | 36 | — |
| Subconjunto | 201 | — |
| ABI v1, símbolos/versionamento e falhas | 88 | — |

Validação local em Linux x86_64; não anunciar cobertura nativa de 32 bits,
outros backends ou a suíte completa do GCC. Esses testes não substituem as
provas das subetapas ainda pendentes.
