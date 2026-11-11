# Bloco 6 da 6.5.3 — cobertura residual no CFG atual

Atualização do bloco 7: a [composição atual do grafo](OPENMP_TRANSACTION_CURRENT_GRAPH.md)
publica aprovação agregada somente na nova API e na janela pré-SSA do frontend.
Este componente ou certificado histórico conserva seu contrato próprio e não
concede aprovação atual isoladamente. A autoridade agregada é revogada antes
de transformações. A [reconstrução LTO do bloco 8](OPENMP_TRANSACTION_LTO_GRAPH.md)
cria gerações independentes na mesma API; a aceitação integral da 6.6 permanece
pendente.


O componente fecha a classificação de efeitos no frontend remapeado. Cada
operação real é examinada novamente; nomes, DECL_ARTIFICIAL, EMITTED e o tipo
ABI isolado não autorizam um acesso ou chamada. Uma leitura/escrita direta
sem categoria demonstrada é recusada, mesmo quando as operações instrumentadas
anteriores continuam presentes e corretas.

## Janela e resultados

Na janela High GIMPLE, antes de retirar descritores e BINDs,
`coverage_snapshot` registra declarações locais/parâmetros, identidades das
chamadas, aridade e as cópias admitidas de representação de referência C++.
Os contratos dos corpos preparados e dos controladores ficam em mapas distintos:
o mesmo auxiliar é uma função preparada e o destino de um controlador.
Não se conservam instruções, SSA_NAME, descritores ou receitas executáveis.

`newtm_coverage_verify_cfg` percorre o corpo preparado atual inteiro.
`newtm_coverage_verify_entry` reconstrói o subgrafo do controlador pelas mesmas
fronteiras de vida útil do bloco 5, desde a abertura do slot, incluindo as
operações anteriores à sua inicialização. Código comum, laços externos e EH de C++
ao redor ficam fora desse subgrafo. São admitidos vários controladores numa
função e controladores extraídos por parallel. A consulta tardia a descritores
ou a presunção de sobrevivência dos rótulos não faz parte da implementação.

Os getters `newtm_coverage_get` e `newtm_coverage_entry_get` publicam observações
separadas por função e por entrada, com estado REFUSED antes de iniciar a prova.
Em sucesso, o estado é DEPENDENT com IDENTITY | CHANNELS | STATUS | OWNER (15).
IDENTITY vale 8; os bits anteriores e seus resultados permanecem preservados.
Isso expressa requisitos da composição do bloco 7, sem liberar o grafo atual.
`current_approved` permanece falso. Não há reutilização automática depois de
mutação nem serialização de um certificado atual em LTO.

## Categorias verificadas

- Valores escalares da ativação e valores SSA atuais são cálculos de valor.
  A declaração opcional de debug de um SSA não é uma alocação de memória.
- Memória local direta exige declaração pertencente ao contrato da função e
  intervalo constante contido na alocação atual. Cópias/casts de ponteiros são
  resolvidos na definição que alcança o uso; deslocamentos e limites são
  confrontados sem somas que possam transbordar.
- Acessos diretos PRIVATE a dados do programa exigem uma operação concreta
  reconstruída pela prova de identidade do bloco 2. O mapa de permissões usa
  as instruções atuais, dura apenas a inspeção e distingue leitura de escrita.
  Encontrar uma operação original não autoriza outra instrução idêntica.
- Canais incoming/result/binding-out são reconhecidos pela assinatura e pela
  definição atual do ponteiro. Direção e intervalo efetivo são conferidos;
  conteúdo, vida útil, produtores e publicação continuam dependências das
  provas independentes dos blocos 3–5.
- Campos remapeados exigem correspondência de declaração, layout e instância
  de frame atual. Ler um campo de receptor que contém o endereço de uma captura
  não equivale a ler o objeto capturado. Produtores desses endereços continuam
  sujeitos à correspondência do bloco 1.
- A exceção de cópia da representação de referência C++ conserva declaração
  de origem e posição do produtor; não libera leituras arbitrárias da captura.
- Chamadas diretas preparadas e serviços STM têm assinatura atual conferida e
  precisam consumir uma identidade de operação de origem ainda não usada.
  Chamadas extras, duplicadas, indiretas, originais não preparadas ou builtins
  fora do protocolo são recusadas. Atributos pure/const não concedem permissão.
- Os IFNs de overflow ADD/SUB/MUL têm aridade e tipos inteiros/resultados
  complexos verificados; seus operandos continuam sujeitos ao percurso de memória.

## Operações introduzidas pelo lowering

`tree-nested` registra somente declarações do frame construído, sua função,
campo de CFA e encadeamento lexical. A inspeção atual verifica os campos e sua
pertença ao frame; um record de tipo parecido não herda essa autorização.
O builtin CFA exige a forma efetivamente emitida pelo GCC e uma definição
atual que alcance a escrita no campo de depuração do frame registrado.
Uma chamada CFA extra ou sem esse consumidor é recusada.

O hook não conserva GIMPLE nem modifica ABI/runtime. Os campos de programa do
frame precisam corresponder aos locais conhecidos; campos de manutenção são
os registrados no ponto real de construção, e o layout é validado no IR atual.
Clobbers de objetos privados finitos são classificados como operações de vida
útil; clobbers de memória compartilhada não recebem aprovação por não emitirem
um store de máquina.

## Percurso e recusas

São inspecionados destino e operandos das atribuições, argumentos, resultado e
static chain das chamadas, condições, seletor de switch, valores de retorno
e elementos de construtores. Efeitos escondidos nesses operandos, incluindo
chamadas com flags de efeito retirados, são recusados pelo código da operação.
O percurso diferencia formar um endereço de ler
seu objeto, mas examina ponteiros, subscritos e deslocamentos usados no endereço.
Assim, um cálculo de endereço pode conter uma leitura residual e ser recusado.

O corpo inteiro é percorrido, incluindo instruções de blocos não alcançáveis;
falta de alcance não concede categoria a um efeito desconhecido. Debug, labels,
NOP e PREDICT permanecem anotações sem execução de memória. Volatile, asm,
controle não local, estruturas OpenMP remanescentes, handlers nos corpos
preparados e demais efeitos não classificados são recusados. A análise não
altera o CFG examinado e não usa TREE_ADDRESSABLE como prova de não alias.

## Validação e continuação

`make -C transaction_test check-cobertura` compila o plugin de mutações reais
após ompexp e chama o verificador de produção. A base precisa ser aceita antes
de cada injeção. Positivos incluem valores escalares novos, cálculo de endereço
sem leitura do objeto, aritmética interna válida, memória privada, chamadas
aninhadas, frames GNU, referências C++, EH externo e parallel. Os executáveis
usam libgomp por DSO e arquivo estático, em C/C++, O0/O2 e checking 0/2, com GC
forçado na matriz principal.
Resultado: 695 verificações e 156 execuções.

Negativos incluem leitura/escrita global ou por alias, memória escondida em
endereços e condições, argumentos de IFN, retornos, volatile com flags de
instrução retirados, chamadas extras e originais/indiretas, CFA sem produtor,
asm, chamadas escondidas em construtores e duplicação de efeitos PRIVATE. O observador verifica os dois tipos de
componente e a ausência de publicação de current_approved.

Passaram propriedade atual (553/88), status atual (449/100), canais (241/44),
memória (281/32), grafo (317), controlador (225/106), integração (1078/930),
ABI/versionamento (88) e chamadas (75/18). Passaram também as provas anteriores
de propriedade, fluxo, falhas/aninhamento, composição, vida útil, guardas e
status, com seus cenários sintéticos. Build all-gcc sem avisos; self-tests
internos desabilitados neste build.

Permanecem o bloco 7 (composição, publicação e invalidação atuais), o bloco 8
(reconstrução em LTO) e a etapa 6.6 (aceitação integral). LTO funcional não é
certificação reconstruída em LTO. ABI, layouts e canais incoming/result são
preservados. Alternativa B continua otimização futura. Resultados nativos
limitados ao Linux x86_64 validado nesta árvore. O fixture de agregados
compartilhados permanece admitido apenas em C, conforme a suíte de integração
existente; o bloco não amplia o domínio de tipos em C++.
