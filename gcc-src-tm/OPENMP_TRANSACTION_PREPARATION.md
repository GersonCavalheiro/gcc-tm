# Etapa 5.1 — preparação da transformação

Decisão do usuário: alternativa A. Cada região externa tem um corpo auxiliar
preparado; o futuro chamador mantém o slot proprietário, retry e limpeza.
Este texto registra a preparação inicial de corpos e acessos. A
[etapa 5.5](OPENMP_TRANSACTION_CONTROLLER.md) ativa acessos STM e retry integral
por `-fopenmp -fopenmp-transaction-stm`; o padrão conserva rastreamento.

## Atualização da etapa 5.4

A [integração de chamadas e bindings](OPENMP_TRANSACTION_CALLS.md) valida todos
os canais exigidos e certifica o grafo transitivo por resumos persistentes após
a preparação dos corpos. Status precede consumo de resultados; frames GNU
incompatíveis continuam recusados. A ativação pelo controlador gerado fica para
a 5.5. Referências abaixo a integração de chamadas pendente são históricas.

## Atualização da etapa 5.2

As [leituras](OPENMP_TRANSACTION_READS.md) já são instrumentadas durante a janela
aberta de preparação. Corpos de funções também são recusados se incompletos.
Testemunhos const com leitura real comprovada são reconciliados; testemunhos
sem correspondência continuam recusados. Os resultados da 5.1 abaixo registram
seu estado histórico; a ponte de produção continua em rastreamento.

## Atualização da etapa 5.3

A janela aberta de preparação agora inclui a [segunda caminhada de escritas](OPENMP_TRANSACTION_WRITES.md),
que consome também os stores derivados. Testes de captura ponteiro verificam
escrita e leitura pela visão privada, antes da confirmação externa explícita.

## Corpos e capturas

`gcc/newtm-clone.cc` encontra regiões externas nas funções da unidade antes da
gimplificação. Cria uma fonte sintética independente, copia seus locais e
temporários GENERIC, inclusive TARGET_EXPR de C++, e reutiliza a preparação de
clones. Regiões internas pertencem ao mesmo corpo. A fonte sintética não é
emitida. As opções específicas de target e otimização da função de origem são
preservadas. O auxiliar tem ligação interna e assinatura:

```text
uint32_t owner.newtm.body.N.newtm.v1(
    ctx, const newtm_clone_binding_v1 *bindings,
    T0 *capture0, T1 *capture1, ...)
```

Capturas têm ordem por DECL_UID. Cada uma transporta o endereço lógico do
objeto; não transporta uma cópia antecipada de seu valor. Seu binding descreve
o objeto e seu armazenamento. Capturas ponteiro que precisam de proveniência
de entrada acrescentam um binding distinto para o apontado. Os dois canais
não compartilham raiz. Definições posteriores atualizam a proveniência;
invalidações não ressuscitam o canal inicial.

Os endereços são parâmetros explícitos, sem cadeia estática nativa do auxiliar.
Isso preserva locais criados dentro de parallel para o outlining OpenMP.
Não deslocar esses objetos para o frame da função externa. Capturas continuam
preexistentes à tentativa quando assim classificadas pelo chamador; não se
tornam privadas por passarem para um auxiliar. Apenas locais criados no corpo
pertencem automaticamente à ativação privada.

A ABI compartilhada e newtm_binding_v1 permanecem inalterados. O auxiliar
recebe contexto emprestado, nunca consome o slot e não entrega resultados por
retorno de linguagem. Modificações em objetos serão tratadas por STM nas etapas
5.2/5.3; temporários do bloco serão recriados a cada futura chamada de tentativa.
Chamadas diretas continuam usando os clones da etapa 4.

O begin/end da região raiz vira consulta de status: o controlador externo já
terá aberto a tentativa. Apenas regiões internas usam nest_begin/nest_end.
Registro verifica status. Uma falha não executa o encerramento normal interno.

## Janela operacional

`newtm_clone_bind_region` consome a associação GENERIC enquanto o nó ainda
está vivo e liga a instrução GIMPLE da região ao auxiliar. Não usa nomes ou
localizações para reconstruir essa identidade. Cópias não herdam a associação;
ela é retirada com os descritores na entrada de omplower, inclusive em corpos
vazios. O futuro controlador precisa consumir essa associação antes de remapear
o corpo ou estabelecer uma associação nova após o remapeamento.

A análise operacional dos acessos, em `gcc/newtm-access.{cc,h}`, roda no fim da
gimplificação, depois dos descritores e antes da reescrita de chamadas dos clones.
A API exige uma janela explicitamente aberta para a função corrente. A abertura
é recusada após a preparação ou a conclusão da gimplificação. Consultas
fora da janela são recusadas, mesmo que uma chave ainda exista numa tabela. Isso
impede assumir que um descritor ainda corresponde ao operando após tree-nested.

`newtm_access_describe` recupera o operando da instrução atual, verifica sua
correspondência ao descritor e exige objeto, extensão, deslocamento, tamanho,
kind, condição de validade e armazenamento. Os operandos são emprestados apenas
nessa janela; não são receitas para reexecutar índices ou expressões de origem.
Antes de formar endereço ou acessar memória, o futuro emissor deve verificar
VALID. A condição materializada mantém as verificações existentes sem overflow.
VALID é estrutural/de faixa. Na emissão dos acessos, verificar também a
correspondência entre o endereço lógico efetivamente avaliado e objeto+offset,
com aritmética checada. Isso é necessário especialmente para valores ponteiro
capturados: seu canal não autoriza ler antecipadamente o objeto ponteiro para
validá-lo. A comparação deve usar o valor obtido pelo acesso instrumentado,
antes de consumir a identidade no acesso seguinte.

Classificações:

| Classe | Tratamento nas próximas etapas |
|---|---|
| REGISTERED | Acesso preexistente: usar a ABI de leitura/escrita. O contexto verifica registro e reservas. |
| PRIVATE | Armazenamento privado provado pelo compilador. |
| DYNAMIC | Despachar pela tag registrada/privada do transporte da ativação, após sua validação. |
| REJECTED | Falta informação executável; não emitir acesso comum como substituição. |

UNREGISTERED concreto continua exigindo registro em runtime: um clone pode
receber um objeto registrado pelo chamador. Ausência na lista estática local
não prova privacidade. Incoming/result completos conservam canais e usam os
campos da ativação; canais incompletos, unknown, source-only e stale são recusados.
Argumentos ponteiro, registros e stores em destinos de chamadas também são
verificados. Uma testemunha source-only não vira uma leitura executada novamente.

Após a janela, somente contagens escalares por classificação e motivos de
recusa ficam disponíveis. Esses resumos sobrevivem ao GC e ao remapeamento;
não autorizam emissão nem contêm planos de acesso executáveis. Stores derivados
da divisão de chamadas conservam a transferência específica de descritor já
definida na etapa 4; consumidores devem tratá-los na mesma janela.

## Falhas e recusa

Clones e auxiliares usam um epílogo comum de falha por função: verificações
guardam a causa e desviam para fail(ctx, causa), retornando o status efetivo.
Não consomem o slot, não confirmam e não descartam. Publicação de saídas dos
clones continua restrita ao sucesso.

O preflight classifica operações vinculadas e registra recusas. Um auxiliar
incompleto é recusado por inteiro antes da reescrita parcial: seu corpo emitido
contém apenas fail(INVALID) e retorno, sem executar operações do programa.
`newtm_clone_require_region` produz diagnóstico de compilação ao tentar exigir
um corpo recusado ou ainda não preparado. O caminho de rastreamento original
permanece disponível nesta etapa intermediária; sua compilação não certifica
que o auxiliar esteja apto à futura ativação.

Chamadas de auxiliares para clones GNU aninhados que exigiriam outro frame
léxico são recusadas pelo preflight. Na tarefa 5.4, conservar essa recusa
explícita. O transporte explícito do ambiente léxico (alternativa B) está
registrado como [possibilidade futura](OPENMP_TRANSACTION_DECISIONS.md), fora do
escopo atual e sem ganho de desempenho demonstrado. Os clones GNU já preparados
na etapa 4 mantêm
seu tratamento nativo quando chamados dentro da hierarquia de frames correta.

Esta análise não é o verificador de cobertura da tarefa 6. Antes de ativar o
caminho funcional, integrar todos os consumidores, verificar seus resultados e
recusar qualquer operação ou chamada residual, inclusive no grafo transitivo.

## Validação

`make -C transaction_test check-preparacao` usa plugin exclusivo dos testes
para chamar auxiliares dormentes e inspecionar resumos. Exercita C/C++, O0/O2,
debug/checking, GC forçado, ligação dinâmica/archive, capturas em parallel,
índice incrementado uma vez, objeto ponteiro e apontado separados, múltiplas
regiões por função, aninhamento, contexto inválido/condenado, tag desconhecida e
recusa de leitura eliminada. A opção require da sonda exige diagnóstico de
compilação para corpos incompletos. O teste fecha o contexto enquanto as
capturas registradas ainda estão vivas.

Os acessos observados nesses testes ainda são comuns: o resultado valida a
preparação, o transporte e os caminhos de status, sem provar isolamento ou
rollback.

Resultados: 37 verificações e 8 execuções de preparação passaram. A regressão
de clones passou com 209 verificações e 48 execuções; ocorrências 63/63,
descritores 36/36, subconjunto 201 e ABI 88. GCC foi recompilado com sucesso.
O teste de preparação também verifica preservação de wrapv, as quatro classes
de acesso e ausência de associação operacional em cópias de regiões descartadas.
DejaGNU não está instalado; a matriz de clones compila diretamente os casos GCC
existentes. Não houve ampliação de validação para outros targets ou offload.
