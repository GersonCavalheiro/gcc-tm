# Etapa 5.2 — leituras da visão privada

O compilador substitui leituras preexistentes nos clones e auxiliares preparados
por `GOMP_transaction_stm_v1_read`. O padrão permanece em rastreamento.
Escritas STM e integração de chamadas foram entregues nas etapas 5.3–5.4;
a [ativação pelo controlador gerado](OPENMP_TRANSACTION_CONTROLLER.md) é opcional
por `-fopenmp -fopenmp-transaction-stm` desde a 5.5.

## Atualização da etapa 5.4

A [integração de chamadas e bindings](OPENMP_TRANSACTION_CALLS.md) valida todos
os canais exigidos e certifica o grafo transitivo por resumos persistentes após
a preparação dos corpos. Status precede consumo de resultados; frames GNU
incompatíveis continuam recusados. A ativação pelo controlador gerado fica para
a 5.5. Referências abaixo a integração de chamadas pendente são históricas.

## Atualização da etapa 5.3

As [escritas](OPENMP_TRANSACTION_WRITES.md) também são instrumentadas. A segunda
caminhada consome os WRITE derivados, mantendo a mesma janela aberta. Os testes
agora consultam a visão privada antes de confirmar e exigem que a memória
original permaneça intacta. As referências a escritas pendentes abaixo registram
o estado histórico da 5.2; a ponte de produção continua em rastreamento.

## Transformação e ordem

`newtm_clone_finish` mantém aberta a janela de preparação durante a transformação.
O preflight recusa um corpo incompleto inteiro antes de instrumentar qualquer
prefixo, inclusive nos clones de funções. Uma recusa emite apenas fail(INVALID)
e retorno; exigir a ativação do auxiliar produz diagnóstico. Não há substituição
por acesso comum quando falta identidade.

Para cada leitura vinculada à instrução GIMPLE atual:

1. Exigir identidade, tamanho, validade e classificação executáveis.
2. Verificar validade estrutural/de faixa antes de formar o endereço lógico.
3. Comparar o endereço efetivamente avaliado com objeto+offset usando inteiros
   de largura suficiente. Os limites já provam ausência de overflow; não se
   forma um ponteiro canônico fora do objeto.
4. Ler pela ABI em armazenamento temporário privado e verificar status.
5. Consumir o temporário somente em sucesso. Qualquer status não zero segue
   para o epílogo comum, que chama fail e retorna a causa efetiva.

REGISTERED sempre exige acesso pela ABI, mesmo quando a análise estática não
viu o registro no clone: ele pode ter sido alcançado pelo chamador. PRIVATE
provado mantém leitura direta após as validações. DYNAMIC valida a tag e gera
dois caminhos: leitura direta privada ou leitura registrada pela ABI. Tag
desconhecida nunca autoriza leitura comum.

Listar um objeto privado numa região interna não muda sua classificação. A
seleção de registro preserva a tag de armazenamento; a emissão registra somente
o caminho preexistente. Locais privados não deixam registros no contexto depois
de terminar sua função. Isso vale também para referências recebidas com uma
tag privada dinâmica.
As restrições anteriores que recusam registros capazes de sobreviver a locais
de callees continuam vigentes; não foi ampliado o subconjunto de linguagem.

A representação de um parâmetro ponteiro/referência pertence à chamada, inclusive
quando uma captura torna esse parâmetro endereçável. Sua raiz de identidade é
distinta da do apontado; um registro do referenciado não pode ser selecionado
para ler o slot interno que contém o endereço. O objeto referenciado conserva
a classificação recebida pelo transporte, sem tornar-se privado por isso.

Os operandos de índice/endereço já avaliados são reutilizados. Descritores
permanecem junto à operação que descrevem. A substituição define o resultado
original antes dos cálculos de descritores seguintes; não antecipa metadados
dependentes de uma leitura ainda não realizada. Isso inclui o valor de um
ponteiro capturado: primeiro ler o objeto ponteiro pela visão privada, depois
validar seu valor lógico e ler o apontado com o canal de identidade separado.

O buffer de leitura é tipado, mutável e privado. Não recebe zero ou outro valor
fictício. GIMPLE separa o carregamento em registrador do armazenamento em memória
quando necessário, inclusive no caminho privado dinâmico. Resultados escalares,
ponteiros e agregados só são entregues ao destino após a verificação de status.
Saídas de funções continuam sendo publicadas apenas pelo epílogo de sucesso.

## Escritas e metadados

Uma atribuição agregada pode conter leitura e escrita na mesma instrução. A
leitura é substituída; a escrita permanece como operação distinta para a 5.3,
com descritor WRITE do mesmo lvalue avaliado e ocorrência nova, conservando
origem/grupo. Não é uma cópia GIMPLE autorizada a reutilizar descritores stale.
A instrução consumida perde imediatamente seus metadados.

Escritas preexistentes ainda são comuns. Uma escrita comum após uma leitura
STM não atualiza seu cache privado: programas mistos não têm ainda a semântica
transacional completa. Testes da visão privada preparam alterações explicitamente
pela ABI write. A regressão de preparação mantém o teste de captura ponteiro
sem atribuição compartilhada ao próprio ponteiro; essa mutação pertence à 5.3.
Não anunciar ausência de publicação parcial ou rollback do pragma nesta etapa.

O retorno implícito de uma função não void produz status INVALID, tratado pelo
epílogo após gimplificação. Não se passa o contexto ABI ao analisador como se
fosse um argumento de memória do programa. Retornos não OK propagam fail antes
de deixar a função.

## Testemunhos de leituras const

Marcadores source-only não fornecem valores nem são executados novamente.
Alguns acompanham uma leitura real que sobreviveu à conversão. O preflight pode
reconciliar cada marcador com uma leitura posterior da mesma referência mediante
prova estrutural na mesma sequência linear:

- expandir somente definições de temporários do compilador, para comparação;
- normalizar wrappers de localização e referências de memória de offset zero;
- associar no máximo um marcador a cada leitura real;
- interromper a prova em escritas, chamadas com efeitos e fronteiras de controle.

Cálculos internos ADD/SUB/MUL_OVERFLOW dos descritores não são chamadas do
programa e não invalidam essa prova. Nenhuma expressão normalizada é emitida
como código; o acesso usa exclusivamente a instrução atual. Marcadores sem
correspondência conservam a recusa. Uma constante local que vira captura pode
ter leitura real no auxiliar; uma constante global efetivamente eliminada segue
sem resultado executável e é recusada.

O front-end C++ pode visitar a mesma expressão várias vezes em mark_use.
Marcadores repetidos são deduplicados somente para referência idêntica, mesma
função e mesma posição bruta de origem, retirando wrappers de localização.
Os nós específicos de parênteses C++ são reconhecidos pela marca semântica do
front-end, sem aceitar conversões reais de representação. Sequências que salvam
um ponteiro antes de calcular um índice conservam a identidade do valor à
direita e a avaliação única dos operandos GIMPLE.
Tokens distintos de macros e leituras em posições diferentes permanecem
distintos. Casos sem posição conhecida não recebem essa deduplicação.

## Validação

`make -C transaction_test check-leituras` compila chamadas emitidas pelo compilador
e usa plugin exclusivo de teste para entrar nos corpos dormentes. A matriz cobre
C/C++, O0/O2, libgomp dinâmica/archive e LTO, com debug/checking e GC forçado.

Os programas exercitam valores previamente preparados na visão privada,
armazenamento privado sem registro, índice incrementado uma vez, leituras em
laço e chamadas aninhadas, leituras repetidas e macros, agregados C, falha na
segunda leitura, status desconhecido, limites negativos, ausência de registro,
ponteiro capturado com valor privado diferente do original e identidade
incompatível. Uma página original protegida com PROT_NONE comprova que uma
leitura satisfeita pelo buffer privado não acessa a memória comum. Conflito real
entre threads preserva saídas; confirmação/descarte são explícitos nos testes.

A sonda verifica ausência de descritores READ registrados residuais nos corpos
transformados e preservação do WRITE após dividir cópia agregada. Isso não
substitui o verificador de cobertura da tarefa 6. Não houve ampliação do
subconjunto de linguagem, alvos ou offload.

Resultados finais: GCC recompilado; `check-leituras` passou com 52 verificações
e 12 execuções. As regressões passaram: preparação 37 verificações/8 execuções,
clones 209/48, ocorrências 63/63, descritores 36/36, subconjunto 201 e ABI 88.
Os três casos novos de compilação GCC tiveram seus dumps conferidos diretamente;
DejaGNU não foi executado. O runtime e os layouts compartilhados não foram alterados.
