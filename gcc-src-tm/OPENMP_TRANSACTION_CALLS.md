# Integração de chamadas preparadas — etapa 5.4

Implementada em 2026-10-31. Completa a validação da integração de chamadas e
bindings sobre a reescrita GENERIC/GIMPLE das etapas 4 e 5.1–5.3. O pragma
original conserva rastreamento por padrão. A
[etapa 5.5](OPENMP_TRANSACTION_CONTROLLER.md) ativa o controlador externo gerado
por opção explícita; a verificação integral de cobertura continua na etapa 6.

## Contrato por chamada

Antes de transformar um corpo, o preflight exige implementação preparada para
cada chamada de origem. O marcador interno de publicação é tratado separadamente
e eliminado na emissão. Chamadas de origem sem implementação preparada não
permanecem como chamadas comuns. Scaffolding interno sem vínculo de origem não
é classificado como chamada do programa por essa checagem.

Para cada aresta admitida, conferir a quantidade de argumentos e exigir
descritores executáveis para todos os argumentos ponteiro/referência e todos
os canais implícitos de captura, incluindo os que não aparecem na assinatura
original. Estado unknown/source-only/stale e canais incompletos recusam o corpo
inteiro. Resultados ponteiro usados exigem armazenamento privado para seu canal
result. Registro também exige um descritor presente, não somente a validade dos
descritores que eventualmente existirem.

A emissão existente passa contexto emprestado, vetor privado de bindings e
argumentos GIMPLE já avaliados ao clone. Preserva `newtm_binding_v1` e o transporte
`newtm_clone_binding_v1`, que reúne identidade e classificação REGISTERED/PRIVATE.
Não reexecuta expressões de origem nem infere privacidade de registro ausente.

Resultados de valor e binding ficam em buffers privados. O status é conferido
imediatamente antes de consumir qualquer saída. Uma atribuição de resultado a
objeto preexistente é derivada como store e instrumentada pela segunda caminhada
da 5.3. Falhas percorrem os epílogos dos chamadores sem confirmar, descartar ou
consumir o slot. Saídas do chamador permanecem preservadas em falha; escritas
anteriores da tentativa permanecem privadas até o descarte externo.

Capturas preservam separadamente a identidade do objeto ponteiro e a identidade
de seu valor. A mesma função preparada pode receber memória registrada e memória
privada de um chamador; a classificação acompanha cada ativação. Os limites de
tipos, escape, vida útil e recursão com ponteiros do subconjunto não foram ampliados.

## Prontidão transitiva e janela dos descritores

O preflight registra as dependências entre declarações de clones enquanto a
janela operacional está aberta, no fim da gimplificação, antes de tree-nested
e omplower. Esse resumo rastreado pelo GC não contém receitas de acesso nem
ponteiros para instruções ou descritores.

`newtm_clone_require_region` agora exige que o auxiliar e todo seu grafo
transitivo estejam preparados, sem recusa local. A caminhada trata ciclos com
um conjunto de visitados e diagnostica o callee incompleto, mesmo quando o corpo
do auxiliar raiz passou pelo seu próprio preflight.

Essa certificação exige que todos os corpos do grafo tenham terminado a
gimplificação. O proprietário pode ser baixado antes do seu auxiliar; não
confundir protótipo gerado com corpo pronto. Os testes fazem a certificação na
entrada das passagens IPA, usando exclusivamente os resumos persistentes. A
futura ativação deve respeitar essa ordem ou organizar uma preparação anterior;
não consultar descritores retirados como se sobrevivessem ao outlining.

## Funções GNU aninhadas

Manter a cadeia estática nativa somente quando a hierarquia de frames da
ativação preparada contém o proprietário exigido pelo callee. Chamadas de um
auxiliar destacado que exigiriam outro frame permanecem explicitamente recusadas.
O transporte explícito do ambiente léxico (alternativa B) está registrado como
[possibilidade futura](OPENMP_TRANSACTION_DECISIONS.md), fora da 5.4 e sem ganho
de desempenho demonstrado.

## Validação

`make -C transaction_test check-chamadas` usa plugin exclusivo dos testes para
invocar auxiliares/clones dormentes, conferir chamadas emitidas e certificar os
grafos. Não adiciona ganchos ao runtime ou ativação de produção.

A matriz positiva cobre C/C++, O0/O2, LTO, ligação dinâmica/archive e símbolos
versionados GOMP_NEWTM_2.0. Exercita retorno ponteiro encadeado, referências e
templates C++, capturas GNU transitivas em frames compatíveis, recursão escalar,
avaliação única de índice capturado, visão privada do objeto ponteiro e apontado,
chamadas do mesmo callee sobre memória registrada e privada, aninhamento,
confirmação/descarte e falhas CONFLICT/INVALID/tag desconhecida propagadas pelos
chamadores, preservando resultados de valor e binding.

Casos negativos C/C++ exigem diagnóstico transitivo de callee incompleto. Casos
C exigem diagnóstico de frame incompatível. Sem certificação, testes explícitos
dos stubs recusados verificam INVALID, preservação dos objetos e descarte; para
isso a sonda fornece capturas nulas que esses stubs não usam, sem reconstruir
declarações já remapeadas por tree-nested. Há GC forçado na sonda. O caso GCC
`transaction-call-1.c` é compilado diretamente em C/C++ e seus dumps conferidos;
isso não representa execução da suíte DejaGNU completa.

Resultados: GCC recompilado; chamadas 75 verificações/18 execuções, clones
209/48, preparação 37/8, leituras 52/12 e escritas 56/12, todos aprovados.
Passaram também ocorrências 63/63, descritores 36/36, subconjunto 201 e ABI 88.
