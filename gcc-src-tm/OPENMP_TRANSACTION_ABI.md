# ABI interna STM v1 — especificação, integração desativada

Decisões vigentes, estado das propostas e condições para extensões futuras:
[registro central dos agentes](OPENMP_TRANSACTION_DECISIONS.md).


Esta especificação define o contrato entre futuras versões preparadas pelo
compilador e o runtime interno já implementado. Não acrescenta símbolos à DSO,
não modifica a ABI de rastreamento `GOMP_NEWTM_1.0` e não ativa execução STM do
`#pragma omp transaction`. Os planos de identidade desta entrega são metadados
sem avaliação; não são chamadas de acesso, clones executáveis ou verificações
dinâmicas de limites já emitidas.

## Seleção e ligação

Preparar automaticamente chamadas diretas com definição disponível na mesma
unidade de compilação, inclusive definições posteriores e instanciações de
templates. Validar transitivamente os corpos. Chamadas indiretas, virtuais,
variádicas e externas sem preparação conhecida são rejeitadas; atributos
pure/const/nothrow não fornecem preparação. Componentes recursivos precisam de
protótipos de todos os clones antes da geração dos corpos.

A assinatura e o símbolo públicos originais permanecem intactos. O clone tem
ligação interna ou visibilidade hidden, sufixo de assembler `.newtm.v1` e não
está sujeito à interposição da versão pública. Instanciações/COMDAT devem
preservar a associação ODR com a definição original; LTO deve preservar essa
associação sem transformar o clone em ABI pública. Os nomes são reservados à
geração futura, não símbolos entregues nesta etapa.

## Tipos de transporte

- Contexto: `struct newtm_context *`, opaco e emprestado ao clone.
- Deslocamentos, extensões e tamanhos: `size_t`, em bytes.
- Status na fronteira compilador/runtime: inteiro sem sinal de 32 bits, com
  constantes fixas 0=OK, 1=CONFLICT e 2=INVALID. Valores desconhecidos são INVALID.
  As traduções dos enums internos de hash/acesso/lifecycle são explícitas.
- Classificação do objeto canônico: inteiro sem sinal de 32 bits,
  0=SCALAR, 1=AGGREGATE. Referências classificam o referido; arrays classificam
  como AGGREGATE, mesmo com um só elemento. Tamanho/alinhamento não inferem kind.

Descritor lógico de um argumento ou resultado ponteiro/referência:

```c
struct newtm_binding_v1 {
  void *object;       /* endereço canônico original, nunca buffer privado */
  size_t object_size;
  size_t offset;
  uint32_t kind;
};
```

Esta estrutura especifica campos e unidades, não representação serializada ou
layout independente de target. Ambos os lados usam o mesmo ABI de target. O
descritor não possui referência à hash, não registra implicitamente o objeto e
não autoriza um acesso. O contexto é a autoridade para registro, tamanho, kind
e reserva; descritores incompatíveis produzem INVALID.

Na primeira implementação, o compilador deve provar um objeto canônico único.
Ponteiros de origem arbitrária, resultados de inteiros/endereço externo e
retornos sem identidade demonstrável são rejeitados. Retornos por referência
estão excluídos. A extensão para identidades alternativas/ponteiro nulo deve
definir explicitamente o respectivo descritor antes de admiti-los; não usar
object=NULL como fallback de memória comum.

## Assinatura das funções preparadas

Para uma função original `R f(A1 a1, ..., An an)`, o clone recebe:

```text
uint32_t f.newtm.v1(ctx, bindings, a1, ..., an, result [, result_binding])
```

`bindings` é um vetor somente leitura de descritores, na ordem dos parâmetros
ponteiro/referência da assinatura original. O protótipo do clone determina seu
comprimento; funções variádicas não são admitidas. Argumentos por valor seguem
as convenções originais de target e suas cópias são privados da chamada. Uma
referência continua recebendo seu endereço lógico conforme o ABI C++, acrescido
do descritor canônico correspondente. Não acessar a visão comum por esse endereço.

Para R diferente de void, `result` aponta para armazenamento tipado privado do
chamador. Para R=void, esse parâmetro é omitido. Se R é ponteiro admitido,
`result_binding` é obrigatório e transporta a identidade do valor retornado.
Outras categorias não recebem result_binding. Agregados C admitidos usam
armazenamento de resultado do tipo original; classes C++ permanecem excluídas.

Resultados e descritores de resultado só são preenchidos em OK. Em CONFLICT ou
INVALID ficam integralmente preservados; o chamador verifica status antes de
consumir qualquer resultado. O clone conserva resultados intermediários em
temporários e copia para as saídas apenas no epílogo de sucesso. Retornos por
referência e endereços de temporários privados que escapem são rejeitados.

Índices, argumentos e expressões de endereço são avaliados exatamente uma vez,
na ordem determinada pela linguagem. O descritor de resultado é produzido pela
chamada, não pela reavaliação de seus argumentos no chamador. Um deslocamento
dependente de parâmetro pertence ao corpo preparado que o calcula; o chamador
usa o descritor retornado. Receitas no plano atual não autorizam repetir índices
ou executar expressões fora de seu escopo.

## Identidade, limites e permissões

`s.campo` e `vetor[i]` usam a chave do objeto listado inteiro e offset em bytes.
Uma referência listada a subobjeto conserva a extensão e classificação do referido:
`int &r = a[1]` registra apenas esse escalar. O plano conserva nove campos:
declaração base física, offset da base registrada, extensão registrada, kind,
offset relativo do acesso, tamanho do acesso, modo (1 leitura, 2 escrita,
4 preparação de chamada), função e localização. Receitas não são operandos
executáveis dos walkers genéricos; cópias remapeiam as declarações explicitamente.
O objeto inteiro continua sendo a unidade de conflito. Ponteiro listado registra
o objeto ponteiro, não a memória apontada. `&x` mantém o endereço original.
Aliases provados são normalizados para a mesma chave. O plano retém declarações
resolvidas, não nomes a pesquisar novamente; cópia/outlining remapeiam declarações.

Antes de formar um endereço dinâmico, verificar índice/deslocamento e
`offset <= extent && size <= extent-offset`, sem somas/multiplicações que possam
transbordar. Índices inferiores à origem do array também são inválidos. Campos
de bit, uniões, membros flexíveis, VLA, volatile e atômicos são excluídos. Validar
arrays/agregados recursivamente; writes const são proibidos. O filtro inicial
rejeita agregados/arrays contendo ponteiros, porque ainda não acompanha a
proveniência desses valores individualmente. Leituras, inicializações e
atribuições de ponteiros escalares exigem identidade conhecida; publicação de
endereços de objetos privados e conversões ponteiro/inteiro são rejeitadas. Temporários privados
permitidos são recriados a cada tentativa e não escapam.

Registro deve ter sido alcançado antes de cada acesso a dados preexistentes,
inclusive leituras constantes que possam ser dobradas pelo front-end. Não há
registro implícito nem fallback. A análise usa interseção de registros em desvios
e não assume que um laço execute. Casos sem prova estática de registro são
rejeitados nesta etapa. Identidades de ponteiros modificados em laços são
invalidadas conservadoramente; vínculos ponteiro/referência recursivos e efeitos
de ponteiros em ciclos recursivos exigem identidade por ativação e são rejeitados.
Mutações de ponteiros encapsuladas em SAVE_EXPR também são conservadoramente
rejeitadas. Isso não impede recursão escalar sem esses vínculos. Futura integração só pode transferir essa verificação
para o runtime mantendo o mesmo contrato de falha permanente.

O runtime admite intervalos globalmente, antes da aquisição da hash. Chaves
iguais só compartilham uma geração com tamanho/kind compatíveis. Intervalos
distintos sobrepostos são INVALID, mesmo entre contextos ou sem reserva ativa.
Uma aquisição pendente já retém sua faixa. Remover a faixa somente após terminar
release/destrutor da hash, mantendo a lease durante toda a operação.

## Fronteira futura GOMP

Código de aplicação ligado à DSO não pode chamar símbolos hidden `newtm_*`.
A ativação exigirá outra família de símbolos exportados/versionados, proposta
`GOMP_transaction_stm_v1_*`, em versão própria `GOMP_NEWTM_2.0`. Não alterar
silenciosamente as assinaturas void da ponte de rastreamento existente.

Operações propostas, com status de 32 bits:

```text
begin(slot, outermost)
register(ctx, object, extent, kind)
read(ctx, object, offset, destination, size)
write(ctx, object, offset, source, size)
end(slot)
after_failure(slot, consecutive_conflicts)
```

Begin externo adquire/transfere a lease internamente; begin interno compartilha
o slot canônico. Novos slots proprietários copiados não são permitidos. O
compilador pode reutilizar o executor de corpos preparados ou gerar laço/epílogos
equivalentes, mas deve especificar e testar a adaptação de captura/outlining
antes de ativar uma dessas formas.

## Propagação e encerramento

Falhas seguem imediatamente por epílogos de funções preparadas até o controle
externo. Não consumir saída inválida nem continuar efeitos normais. INVALID
domina CONFLICT. Funções internas não confirmam nem descartam a tentativa
externa. Exceções, cleanups do usuário, E/S, syscalls, locks e alocação/liberação
do programa estão excluídos; o runtime mantém sua própria limpeza obrigatória.

Confirmação externa valida/prepara tudo antes de PUBLISHING; publica somente
modificações, retendo todas as reservas até terminar todas as cópias. OK exige
limpeza completa. Em falha anterior à publicação, preservar causa e recursos
até o descarte explícito, executado uma vez após os epílogos suportados.

Depois do descarte completo, CONFLICT aplica backoff e reinicia toda a região
externa, recriando registros e temporários. INVALID faz diagnóstico fatal após
limpeza e nunca repete. Não usar setjmp/longjmp através de frames C/C++, não
confirmar em finally e não converter exceções em conflitos.

## Estado da preparação para geração de código

A [avaliação de prontidão](OPENMP_TRANSACTION_INTEGRATION_READINESS.md) confirma
que esta especificação ainda não é uma ABI executável do compilador. A ativação
exige ligação entre ocorrências/temporários, descritores materializados, clones
e epílogos de status. Não reavaliar receitas do plano nem usar a ABI void antiga
como se propagasse falhas. A autorização condicionada do usuário não altera o
fato de que esses mecanismos ainda não foram implementados.

## Preparação de operandos concluída: etapa 1

A [vinculação de ocorrências](OPENMP_TRANSACTION_OCCURRENCES.md) oferece pontos de
consumo em High GIMPLE, com operandos efetivos e proveniência. O layout de seus
sites não é ABI runtime. A geração futura de descritores e acessos deve consumir
os operandos avaliados antes de folds/CFG/SSA; não executar o plano diagnóstico.
Source-only não tem resultado e exige reconciliação ou diagnóstico. Isso não
implementa as assinaturas executáveis, clones ou propagação de falhas acima.

## Descritores internos implementados: etapa 2

O [contrato de descritores](OPENMP_TRANSACTION_DESCRIPTORS.md) materializa os
campos de registro/acesso em High GIMPLE, sem alterar assinaturas GOMP. Kind vem
do tipo listado e a chave continua lógica. Incoming e result são canais
explícitos para a futura ligação de parâmetros/resultados; não constituem uma
ABI de chamada executável. Unknown/source-only/stale não fornecem resultado
válido. Clones devem reconstruir descritores após remapeamento e transportar
contexto/status antes de instrumentar acessos. A validade de faixa calculada
precisa ser consumida antes da operação; a ponte atual ainda não o faz.
