# Etapa 4: funções preparadas a partir de GENERIC

Implementação em `gcc/newtm-clone.{cc,h}`. Decisões explícitas do usuário:
copiar GENERIC antes da gimplificação e preservar `newtm_binding_v1`, definindo
transporte específico que acrescente classificação de armazenamento.
O padrão continua usando a ponte de rastreamento. A
[etapa 5.5](OPENMP_TRANSACTION_CONTROLLER.md) ativa o controlador funcional por
`-fopenmp -fopenmp-transaction-stm`, com certificação transitiva e sem fallback.

## Atualização da etapa 5.4

A [integração de chamadas e bindings](OPENMP_TRANSACTION_CALLS.md) valida todos
os canais exigidos e certifica o grafo transitivo por resumos persistentes após
a preparação dos corpos. Status precede consumo de resultados; frames GNU
incompatíveis continuam recusados. A ativação pelo controlador gerado fica para
a 5.5. Referências abaixo a integração de chamadas pendente são históricas.

## Atualização da etapa 5.1

A [preparação](OPENMP_TRANSACTION_PREPARATION.md) acrescenta auxiliares externos
com capturas por endereço explícito e epílogo comum de falha. Os clones de
funções conservam suas assinaturas. A ponte de produção continua em rastreamento.

## Atualização da etapa 5.2

As [leituras](OPENMP_TRANSACTION_READS.md) dos clones agora usam a visão privada,
com status explícito antes do consumo. Escritas compartilhadas permanecem para
5.3. A ABI das funções originais e a ponte de rastreamento são preservadas.

## Atualização da etapa 5.3

As [escritas](OPENMP_TRANSACTION_WRITES.md) dos clones também usam a visão
privada, com status explícito e consumo dos stores derivados na mesma janela.
A ponte de produção permanece em rastreamento; o slot segue no controle externo.

## Seleção, criação e ligação

Após `newtm_analyze_unit`, antes de `analyze_functions`, a seleção transitiva
já validada fornece todas as funções com corpo GENERIC disponível. A enumeração
usa o conjunto selecionado, incluindo funções GNU aninhadas que ainda não são
definições independentes na tabela de símbolos. Chamadas diretas e definições
posteriores/templates seguem a análise existente; não foi ampliado o subconjunto.

Criar primeiro todos os protótipos, depois copiar todos os corpos. A ordem usa
DECL_UID e pais lexicais precedem filhos. Recursão escalar/mútua pode referenciar
protótipos já criados. Os corpos são gimplificados independentemente, produzindo
sites/temporários/descritores novos, sem executar planos de diagnóstico.
Funções selecionadas sem clone executável não ficam silenciosamente comuns.

O clone recebe nome assembler original + `.newtm.v1`, ligação interna e a
associação com a definição da unidade. Não copia atributos pure/const como prova
de ausência de efeitos; conserva opções específicas de target/otimização e os
tipos/convenções de transporte dos parâmetros originais. A assinatura e o símbolo
originais permanecem intactos, inclusive chamadas comuns fora de transações.
Clones locais não são interpostos pelo símbolo público. Templates/inline podem
ter clones locais em cada TU, com referências aos mesmos objetos originais
inline/COMDAT. Os clones são mantidos para validação, mas o pragma não os chama.

O copiador usa os mecanismos de declaração, tipos, SAVE_EXPR e blocos de
`tree-inline`, remapeia parâmetros/resultados/locais e copia inicializadores.
Uma passagem complementar remapeia capturas também sob ADDR_EXPR, em
inicializadores e testemunhos de leituras eliminadas: as recursões internas do
copiador não passam sempre pelo callback externo. O C++ aloca estado de função
independente ao gimplificar clones; não compartilha pilhas mutáveis de parsing.
Planos de regiões copiados são limpos, evitando receitas específicas do chamador.

## Assinatura e transporte

Para `R f(A1, ..., An)`:

```text
uint32_t f.newtm.v1(ctx, bindings, A1, ..., An [, R *result]
                   [, newtm_clone_binding_v1 *result_binding])
```

`result` existe para R diferente de void. `result_binding` existe somente para
R ponteiro admitido; retornos por referência continuam excluídos. Contexto é
emprestado, não proprietário; o slot externo não é passado nem consumido.
`bindings` aponta para vetor somente leitura com comprimento conhecido pelo
grafo/protótipo preparado. Copiar argumentos por valor cria objetos privados da
chamada. Ponteiros/referências mantêm os valores/endereços lógicos originais.

`include/newtm-abi.h` preserva os quatro campos, ordem e layout nativo de
`newtm_binding_v1`. O novo layout, também construído no GCC, é:

```c
struct newtm_clone_binding_v1 {
  struct newtm_binding_v1 identity;
  uint32_t storage;
};
```

Storage fixo 0=REGISTERED (objeto preexistente, exige registro no contexto),
1=PRIVATE (objeto criado na tentativa, inclusive locais do chamador).
Não inferir privado de ausência na hash. Tags desconhecidas são INVALID.
Classificação é prova produzida pelo compilador; validar a tag não prova vida
útil ou autorização. O contexto continua autoridade para registro/reservas.
O membro identity mantém seu padding; não achatar/reutilizar o padding como
campo storage. Layout é do target, sem packing nem formato serializado.

A ordem inicial do vetor segue parâmetros ponteiro/referência da assinatura.
Funções GNU aninhadas acrescentam capturas por DECL_UID: identidade do objeto
capturado e, se necessário, identidade do valor ponteiro. O comprimento inclui
essas referências implícitas, conhecidas pelo compilador; a cadeia estática
nativa é tratada por tree-nested, sem alteração da assinatura pública original.
Capturas transitivas são propagadas até ponto fixo, inclusive entre irmãos.

Capturar `p` como objeto não é capturar a memória apontada. Os dois canais têm
raízes de proveniência distintas para impedir que registrar o objeto ponteiro
substitua a chave do seu apontado. O objeto capturado recebe a classificação
provada no chamador, inclusive se declarado dentro de uma região no chamador
comum. Valores desconhecidos não são lidos/fabricados para preencher bindings.
Capturas só usadas por endereço/escrita não exigem valor ponteiro de entrada.
Definições simples e sequenciadas antes da primeira leitura podem dispensar
esse canal de entrada; controle/ordem de avaliação não provados são tratados
conservadoramente. Casos sem identidade executável exigida são diagnosticados.

Pais e capturas usam declarações remapeadas da ativação preparada, não frames
da função original. Funções GNU aninhadas só são finalizadas pela conversão
nativa da cadeia estática; não são redefinidas nessa passagem.

## Canais e proveniência

`newtm_clone_scan_results` reserva armazenamento privado para o binding de cada
resultado ponteiro de chamada preparada. Antes da reescrita, a análise de
descritores resolve incoming dos formais/capturas e result dessas chamadas a
partir do transporte correspondente, conservando CHANNEL e acrescentando STORAGE.
Os campos não são preenchidos com valores fictícios nos corpos originais.
Só o clone trata a função inteira como tentativa: nos originais, a privacidade
é determinada pela região efetivamente aberta, sem classificar parâmetros e
locais anteriores ao pragma como privados por terem sido selecionados.

Canais de entrada são semeados uma vez. Invalidação por chamadas/stores não
ressuscita um binding antigo: uma nova definição demonstrável é necessária.
Isso preserva a regra de não usar proveniência stale nem memória comum por falta
de identidade. Resultados de chamada só ficam disponíveis depois de status OK.
Source-only continua testemunho sem valor; não é reexecutado pelo clone.

A construção consulta operandos GIMPLE efetivos, incluindo índices/argumentos
já avaliados. Deslocamentos/extensões/kind vêm dos descritores e canais da
ativação atual. Uma função não incorpora offsets calculados no escopo de outro
chamador. Identidade e classificação de resultados ponteiro vêm do callee.

## Status, retornos e aninhamento

`GOMP_transaction_stm_v1_status(ctx)` foi acrescentado à família versionada
GOMP_NEWTM_2.0. Observa estado/propriedade/profundidade sem mudar nesting, obter
reservas ou acessar valores do programa. OK=0, CONFLICT=1, INVALID=2; estados
inválidos não viram sucesso. Vida útil válida do ponteiro é pré-condição.

Cada clone verifica contexto na entrada e status efetivo antes de publicar uma
saída. Valida ponteiros obrigatórios, tags e forma/faixa dos bindings. Compara
argumentos lógicos explícitos à identidade por aritmética inteira checada, sem
formar ponteiro fora do objeto. Capturas não ganham uma leitura extra do valor
original só para construir metadados. Essa validação não substitui as permissões
de linguagem, o registro e os acessos STM que serão integrados na tarefa 5.

Chamadas entre clones recebem contexto/bindings e buffers privados. Verificar
status imediatamente antes de consumir qualquer saída. Falha chama fail para
reter a causa efetiva (INVALID domina conflito) e retorna ao chamador sem
publicar resultados. Resultados intermediários ficam privados; todos os testes
recuperáveis precedem as cópias finais do valor/binding. Temporários não recebem
zero/NULL fictícios; avisos de inicialização são suprimidos somente nos
armazenamentos internos cujo preenchimento é condicionado por status OK.

Um marcador interno de publicação ancora o valor já avaliado até a construção
do binding; é removido antes de callgraph/omplower e não gera símbolo externo.
Void omite result; queda sem valor em função não void nunca fabrica OK.
Retornos que escapam endereços de locais do callee continuam rejeitados.

Regiões internas usam nest_begin/register/nest_end com status explícito. Não
confirmam/descartam a tentativa externa. Saída normal equilibra profundidade;
falha propaga até o controlador externo, que consumirá todos os níveis.
O finally de rastreamento vira sequência normal no clone; não confirma nem
encerra nível no caminho de falha. Marcadores OpenMP internos já preparados são
retirados antes de diagnose_omp_blocks para permitir retornos gerados de status,
preservando BINDs. Os corpos originais mantêm os diagnósticos estruturados.

## Janela e metadados de transformação

Toda produção/consumo operacional desta etapa ocorre no fim da gimplificação,
antes de tree-nested e omplower. A conversão GNU e outlining podem remapear
objetos: consumidores posteriores precisam reconstruir os descritores.
A retirada na entrada de omplower não foi adiada. Clones não consultam tabelas
de descritores em execução; só instruções/argumentos normais sobrevivem.

Instruções substituídas perdem imediatamente os vínculos e descritores, inclusive
as chamadas destacadas que uma caminhada posterior do corpo não encontra.
Isso impede chaves obsoletas sob GC forçado/reutilização de instruções.

Uma chamada pode escrever diretamente em memória. Ao separá-la em result-out
privado e store após OK, a transformação mantém um descritor WRITE para o mesmo
lvalue avaliado e deriva uma ocorrência de store com ID novo e o mesmo grupo.
É transferência dentro da mesma ativação/janela, sem remapear operandos;
não é uma exceção para gimple_copy: cópias gerais continuam STALE e exigem
reconstrução. Stores que o próprio gimplificador já separou mantêm seus vínculos.

## Estado da validação

A matriz permanente `make -C transaction_test check-clones` exercita entrada nos
clones por plugin exclusivo dos testes; o programa de produção não é redirecionado.
Testes adicionais foram acrescentados a c-c++-common/gomp, gcc.dg/gomp e
g++.dg/gomp. Os resultados finais e limites estão registrados ao fim deste arquivo.

## Próximas etapas

A tarefa 4 não substitui leituras/escritas preexistentes por STM. Os corpos dos
clones ainda contêm esses acessos comuns e são dormentes no caminho de produção.
Na tarefa 5, consumir descritores antes dos remapeamentos, instrumentar acessos,
tratar storage e source-only/unknown, integrar captura/controle externo por slot,
confirmar somente em sucesso e propagar até descarte/retry externo completo.
Na tarefa 6, verificar cobertura e recusar qualquer operação residual indevida.
Não anunciar isolamento, rollback ou retry automático do pragma nesta entrega.

## Resultados finais

Compilação incremental de GCC e libgomp concluída. Após o ajuste final que
restringe a privacidade da ativação inteira aos clones reais, a matriz
`check-clones` passou com 209 verificações e 48 execuções: C/C++, O0/O2,
ligação dinâmica/estática, propagação de conflito/erro, saídas preservadas,
capturas GNU e múltiplas unidades com ODR/LTO.

As regressões passaram: 63 verificações de ocorrências, 36 de descritores,
201 do subconjunto admitido e 88 da ABI. As suítes do runtime de registro,
acesso, confirmação, retry e sobreposição também passaram. Os seis casos de
compilação dos testes GCC foram executados diretamente com debug e checking;
os dois casos negativos preservaram o diagnóstico de bloco estruturado.
DejaGNU não está instalado neste ambiente: suas diretivas dg-final não foram
executadas pelo harness. Não houve validação de alvos de 32 bits ou offload.
