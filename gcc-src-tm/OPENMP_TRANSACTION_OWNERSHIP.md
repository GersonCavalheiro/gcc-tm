# Prova do controlador proprietário — subetapa 6.4.6

Atualização da 6.5.3: o [bloco 5](OPENMP_TRANSACTION_CURRENT_OWNER.md) aplica
a prova ao CFG remapeado, com fronteiras atuais de vida útil do slot. O texto
abaixo descreve a prova histórica High GIMPLE; não autoriza reutilizá-la
após outlining. A composição atual continua no bloco 7.

**Atualização 6.4.7 — 2026-11-04:** a [composição local](OPENMP_TRANSACTION_COMPOSITION.md) foi implementada. Pendências históricas referentes à 6.4.7 estão superadas; certificação transitiva e remapeamento seguem na 6.5, e aceitação integral na 6.6.

Implementada em 2026-11-04 em `gcc/newtm-owner.{cc,h}`, integrada a
`newtm_obligation_verify` após as provas de guardas, status e vida útil.
Implementa a alternativa A aprovada: análise própria sobre o grafo da 6.4.1,
separada do [contrato dos frames emprestados](OPENMP_TRANSACTION_FAILURES.md).
Não foi necessário modificar a emissão funcional, a ABI ou o runtime.

## Critério de conclusão e domínio

Todo caminho do controlador precisa admitir uma tentativa com propriedade
exclusiva, executar seu auxiliar inteiro, confirmar somente em sucesso e
consumir os recursos antes da continuação ou do retry. Falha permanente não
pode virar conflito; limpeza não pode ser omitida nem duplicada. A prova
independe dos nomes dos temporários, dos rótulos e da posição textual do epílogo.

O critério local está atendido, inclusive com checking 0. Ausência de prova
gera recusa V07; obrigações de arestas ausentes ou alteradas conservam V01/V08.
As provas anteriores continuam obrigatórias. Não há COBERTA nem certificado
novo: composição com o inventário fica na 6.4.7 e certificação transitiva e
remapeamento na 6.5. A aceitação integral por mutações permanece na 6.6.

O domínio conservador admite um BEGIN estático, slot/outermost/contador locais
distintos com os tipos ABI corretos e o auxiliar exigido pela região delegada.
O inventário identifica essa obrigação; só a inspeção das chamadas e caminhos
reais demonstra seu cumprimento. Outro auxiliar ou serviço é recusado. A
inspeção estrutural continua exigindo uma invocação estática única do auxiliar.

## Estados e propriedade

O grafo produto possui 17 partições por nó: inicial, entrada, corpo concluído,
status final aprovado, confirmação consumida, falhas CONFLICT/INVALID/desconhecida
com ou sem contexto, validação permanente, condenação CONFLICT/INVALID e
limpeza CONFLICT/INVALID/desconhecida. Cada partição transporta fatos de status,
identidade, disponibilidade por tentativa, guardas e atualização do contador.
Junções unem possibilidades e conservam apenas os fatos demonstrados em todos
os predecessores; a possibilidade de uso de contexto consumido não desaparece.

O slot deve ser inicializado em NULL antes da primeira admissão. Seu endereço
só pode ser entregue aos serviços proprietários sobre o mesmo armazenamento.
Cópias escalares do contexto são empréstimos, não novos slots. Endereços do
slot, outermost, contador e transportadores de contexto não podem escapar;
escritas indiretas nesses canais e resultados de chamadas sobrepostos a eles
são recusados. Slot, outermost e contador precisam compartilhar a ativação
lexical; sair de seu escopo antes de concluir o protocolo ou encerrar/reiniciar
seu armazenamento por clobber é recusado. Após END ou limpeza, os empréstimos
ficam inválidos.

BEGIN bem-sucedido produz contexto e outermost, mas não aprova o teste de
outermost: o caminho precisa demonstrar a comparação do valor produzido com 1.
Forjar uma constante ou reutilizar uma aprovação anterior não satisfaz a prova.
A falha de BEGIN é conservadoramente examinada com slot vazio e com contexto
retido: o primeiro caminho termina por FATAL; o segundo exige condenação e
limpeza. Isso também exercita entradas falhas com recursos ainda presentes.

## Confirmação e falhas

A sequência demonstrável é BEGIN/outermost, auxiliar, STATUS e END. Cada
operação precisa ter sucesso antes da seguinte. O auxiliar e STATUS recebem
o contexto desta admissão; END recebe o endereço do slot proprietário.
Nenhuma saída normal é aceita antes da confirmação consumida. A prova usa as
pós-condições da ABI: END externo bem-sucedido publica e esvazia o slot, e
END falho conserva o contexto para a limpeza.

Falhas de chamadas conservam a causa por definição; cópias sem estreitamento
podem transportá-la, mas constantes e resultados antigos não a substituem.
Guardas locais geram falha permanente e não podem inventar CONFLICT. FAIL
precisa receber o contexto retido e a causa correta, exatamente uma vez por
tentativa condenada no controlador. Depois disso só se admite a preparação
limitada do contador e a limpeza; corpo, STATUS e END normais são recusados.

AFTER_FAILURE requer condenação anterior e um contador positivo atualizado
com saturação. A prova acompanha o incremento unitário sob limite SIZE_MAX e
o braço que conserva SIZE_MAX; a junção não perde essa garantia. Reset após
iniciar tentativas, wrap, incremento duplicado e contador substituído falham.

O contrato do runtime é explícito: AFTER_FAILURE consome a tentativa e pode
terminar o processo; em retorno normal nativo entrega CONFLICT. INVALID
prevalece sobre CONFLICT. O modelo também admite, conservadoramente, retornos
INVALID/desconhecidos com slot consumido para verificar os braços defensivos
que devem terminar por FATAL. Nenhum deles autoriza retry. A decisão usa o
resultado efetivo de AFTER_FAILURE, não uma causa anterior ou literal forjado.
O resultado de FAIL não precisa ser devolvido por este frame proprietário:
AFTER_FAILURE consulta o estado condenado e estabelece a fronteira consumidora.

## Retry integral e preparação fresca

Um retorno à admissão exige CONFLICT efetivo comprovado e slot vazio. Desviar
ao corpo, à preparação ou à confirmação sem novo BEGIN é recusado. Na nova
admissão, definições temporárias e fatos de campos são invalidados; preservam-se
somente o contador persistente e os novos resultados de entrada.

Cada campo do incoming escrito pelo controlador precisa ser preparado nesta
tentativa, ou vir de uma definição integral fresca do objeto. As dependências
escalares também precisam ser atuais. A completude semântica por campos e
tipos continua sendo demonstrada pela 6.4.4; esta análise acrescenta a fronteira
entre tentativas. Portanto, uma aprovação ou inicialização da tentativa anterior
não libera um prefixo que o retry tenha pulado. Fontes ordinárias não definidas
pelo controlador seguem o contrato de execução definida da linguagem.

Após estabilizar os fatos, uma verificação de ciclos exige que cada ciclo do
controlador atravesse uma readmissão autorizada. Não são aceitos laços de falha,
preparação, espera ou saída que evitem essa fronteira. Os laços do programa
continuam no auxiliar preparado; não se afirma terminação geral nem ausência
de starvation sob conflitos repetidos.

Todas as evidências operacionais são temporárias. Grafo e análise terminam
antes do fechamento da janela e de tree-nested/omplower; não há consulta tardia
a descritores nem alteração de GIMPLE/CFG/SSA pelo verificador. A propriedade
e o balanceamento transitivos dos callees ainda dependem da certificação 6.5.
Layouts `newtm_binding_v1`/`newtm_clone_binding_v1`, canais incoming/result,
opções e alternativa B futura permanecem preservados.

## Testes e resultados

`make -C transaction_test check-proprietario`: **77 verificações, 16 execuções
e 2356 cenários sintéticos**. São 61 casos diretos em 28 compilações e 27
mutações negativas pelo inspetor completo nas 24 compilações com preparação,
inclusive tracing. As mutações não são inseridas no programa executado.
Janelas sintéticas são reconstruídas, sem reutilizar aprovação ou consultar
descritores do programa original. Positivos do inspetor completo são os
controladores realmente emitidos e executados, incluindo o original de uma
função aninhada e a versão emprestada dessa função.

Cobertura: entrada incompleta, slot copiado/forjado, outermost ausente ou
substituído, chamadas fora de ordem, confirmação/limpeza omitida ou duplicada,
causa perdida, contexto consumido, escapes/escritas indiretas, contador sem
limite/resetado, retry parcial, ciclo condenado, incoming e dependências de
tentativa anterior. Positivos incluem cópias de status/causa, guarda booleana,
guarda equivalente do contador e construção integral do incoming.

A matriz inclui C/C++, O0/O2, LTO, DSO/archive, checking 0/2, GC forçado,
imutabilidade de IR/CFG/propriedades e ausência de dependência executável dos
símbolos internos do verificador. Logs/JSON: `transaction_test/.build/owner/`.
Build `all-gcc` passou sem avisos novos; self-tests internos estão desabilitados
nesta configuração e não são contabilizados como testes executados.

Também passaram todas as matrizes anteriores de obrigações, inspeção, fluxo,
guardas, status, vida útil e falhas. Integração e controlador: **1303 verificações
e 1036 execuções**. Clones 209/48, preparação 37/8, leituras 52/12, escritas
56/12, chamadas 75/18, ocorrências 63, descritores 36, subconjunto 201 e ABI v1
88. Incluem tradução de status, falhas de entrada/END, saídas preservadas,
limpeza/retry completo e ligação/símbolos/versionamento.

```sh
make -C transaction_test check-proprietario check-falhas check-vida-util check-status check-guardas check-fluxo check-inspecao check-obrigacoes
make -C transaction_test check-integracao check-clones check-preparacao check-leituras check-escritas check-chamadas check-ocorrencias check-descritores check-subset check-abi-v1
```

Resultados limitados ao ambiente nativo Linux x86_64. Não representam o
testsuite completo do GCC, suporte a outros targets ou certificação transitiva.
