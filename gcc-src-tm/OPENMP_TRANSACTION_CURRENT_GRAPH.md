# Bloco 7 da 6.5.3 — composição e certificado atual do frontend

O bloco 7 compõe as provas dos blocos 1–6 em um certificado independente do
grafo histórico. A publicação é atômica por raiz transacional: ou todos os
membros e o controlador satisfazem o contrato atual, ou nenhum membro recebe
autoridade parcial. A implementação está em `gcc/newtm-current.{cc,h}`, com
reconstrução dos componentes em `newtm-clone.cc` e notificações nos módulos
de memória e obrigações. A ABI, runtime, layouts e canais incoming/result
permanecem preservados.

## Composição e fechamento

`newtm_current_verify(root)` percorre novamente o fechamento de dependências
do preflight, incluindo recursão simples/mútua, sem usar aprovação histórica
como evidência atual. Cada corpo precisa estar preparado e passar pelas provas
de assinatura, operandos remapeados, chamadas, contexto, registros, memória,
canais, status e cobertura. Chamadas observadas precisam corresponder ao grafo
admitido; dependências conservadoras continuam sujeitas à verificação.

O controlador é localizado pela chamada real e única ao auxiliar, inclusive
em função extraída por outlining. São reexecutadas as provas de origem das
capturas, canais, status, propriedade e cobertura da entrada. Os produtores
dos frames e seus ancestrais de outlining também pertencem ao certificado;
seu código comum não recebe permissão de acesso transacional por isso.

A máscara agregada é CHANNELS | STATUS | OWNER | IDENTITY | COVERAGE (31).
Cada corpo fornece 27; o controlador fornece 31; ancestrais produtores têm
máscara zero e somente sustentam correspondência e frescor. A aprovação exige
fechamento completo, membros únicos, raiz/controlador presentes, evidências
estáveis e nenhuma dependência sem fornecedor. Só então `newtm_current_get`
expõe `APPROVED`, `current_approved=true`, `proved=required=31` e dependências
zero. Falha produz `REFUSED`, zero membros publicados e zero prova agregada.

Os getters dos componentes conservam seus estados DEPENDENT e suas máscaras;
não ganham aprovação de grafo isoladamente. O getter histórico
`newtm_obligation_graph_get` continua com `current_approved=false`: seu contrato
não transfere aprovação para IR remapeado. A autoridade atual pertence apenas
à nova API, à raiz e à geração observadas.

## Frescor e invalidação

O certificado mantém declarações e uma cópia escalar dos dados relevantes do
CFG pré-SSA: operações/operandos, arestas, assinaturas, layout, atributos,
inicializadores constantes, opções da função, static chain e relações EH.
Compara bytes completos, sem depender de colisões de hash. Não retém ponteiros
de GIMPLE, SSA_NAME, blocos básicos ou descritores. Debug e caches auxiliares
não são evidências. Formas não representáveis e limites de tamanho/profundidade
recusam o certificado conservadoramente.

Cada consulta reconstrói essa fotografia na janela viva. Alterar um corpo,
controlador, produtor, assinatura, layout ou aresta revoga todas as raízes que
contêm o membro. Notificações de mudança de obrigações/remapeamento e fatos
de clones também revogam; mudanças genéricas de metadados usam invalidação
conservadora de todas as raízes. Reexecutar um componente isolado retira a
autoridade agregada. Durante uma composição completa, evidências compartilhadas
são preparadas sem revogar outras raízes quando o IR permanece idêntico.

Restaurar o IR não reanima uma geração INVALID. É necessária uma composição
completa nova, com novo identificador. Falhas durante a composição retiram a
autoridade de raízes que dependiam dos componentes tocados. Não há publicação
incremental de uma componente fortemente conexa incompleta.

## Janela e limites

A certificação ocorre depois de lowering/outlining e antes de SSA, enquanto os
CFGs nativos estão disponíveis. Os contratos de origem foram congelados antes
da retirada dos descritores na entrada de omplower; nenhuma consulta tardia a
descritores ocorre. O callback ALL_IPA_PASSES_START ainda pode observar o
certificado; imediatamente depois dele `newtm_current_end_frontend` revoga
todas as gerações e fecha a janela, antes de transformações IPA/streaming.

O certificado do frontend não é serializado nem transferido para LTO. O
[bloco 8](OPENMP_TRANSACTION_LTO_GRAPH.md) acrescenta outras janelas e gerações
à mesma API por reconstrução dos corpos carregados, sem herdar esta aprovação.
A aceitação integral por mutações continua na etapa 6.6. Resultados
nativos em Linux x86_64 não constituem validação de outros alvos. A alternativa
B permanece otimização futura.

## Validação

Resultado do novo teste: **543 verificações e 212 execuções aprovadas**.

`make -C transaction_test check-grafo-atual` exercita publicação, provas
isoladas sem aprovação, três raízes com SCC compartilhada e raiz independente,
mutações de operandos/assinaturas/layouts/arestas, notificação e metadados,
rechecagem isolada, restauração sem ressurreição, composição recusada sem
publicação parcial e reconstrução em geração nova. Mutações adicionais
verificam controlador e produtor real de capturas após outlining.

A matriz cobre C/C++, O0/O2, checking 0/2, GC forçado, DSO/archive, funções GNU,
EH externo, parallel e ausência de autoridade nas fronteiras IPA/LTO. Os
resultados finais e regressões estão no [checkpoint](OPENMP_TRANSACTION_RETOMADA.md).
O teste antigo de mutação do layout compartilhado chama a prova de memória
imediatamente após alterar o campo: isso evita depender de qual raiz detecta
primeiro a inconsistência comum a memória e canais, preservando a exigência
do diagnóstico específico de memória.

Regressões concluídas: cobertura atual 695/156, propriedade atual 553/88,
status atual 449/100, canais 241/44, memória 281/32, grafo histórico 317,
controlador 225/106, integração 1078/930, ABI/versionamento 88 e chamadas
75/18. Build all-gcc sem avisos; self-tests internos desabilitados neste build.
Higiene do diff e sintaxe Python aprovadas. Estes são os resultados do bloco 7;
a reconstrução LTO tem validação própria no relatório do bloco 8. Permanece a
aceitação integral da 6.6.
