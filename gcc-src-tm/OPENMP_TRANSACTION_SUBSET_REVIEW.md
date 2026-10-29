# Revisão de diagnósticos, identidade e ABI — 2026-10-29

Atualização posterior: [etapa 1 de vinculação de ocorrências implementada](OPENMP_TRANSACTION_OCCURRENCES.md),
com operandos GIMPLE avaliados e sem ativação STM. As evidências da etapa estão
nesse contrato; resultados históricos abaixo continuam restritos à sua entrega.


Decisões vigentes, estado das propostas e condições para extensões futuras:
[registro central dos agentes](OPENMP_TRANSACTION_DECISIONS.md).


Entrega autorizada: filtros/diagnósticos do subconjunto, plano canônico e rejeição
de registros sobrepostos; ABI especificada sem integração funcional com GOMP.

## Implementação

`gcc/newtm-analysis.cc` analisa a unidade antes da gimplificação e das otimizações
interprocedurais. Preserva corpos, tipos e inicializadores de origem e marcadores
semânticos de leituras constantes. Os marcadores são DEBUG_BEGIN_STMT inertes
associados por identidade de árvore; não há associação heurística por linha ou
texto. Isso impede que macros, expressões em várias linhas, constantes ou
constexpr eliminem a exigência de registro. Operandos não avaliados de sizeof,
alignof e noexcept não contam como leituras; VLA permanece fora do subconjunto.

A análise mantém registros alcançados, privados da tentativa e vínculos de
ponteiros/referências. Faz interseção nos desvios e não assume execução de laços.
Valida transitivamente chamadas diretas, incluindo definições posteriores,
funções C aninhadas e templates instanciados. Anota preparação e acessos no plano;
ainda não gera clones executáveis nem propagação de status no programa compilado.
Indiretas, externas sem definição preparada, variádicas, efeitos OpenMP internos,
assembly, alocação/liberação do programa, exceções e cleanups são rejeitados.
Atributos pure/const não tornam uma chamada externa preparada.

Campos e elementos são normalizados para objeto físico e deslocamentos em bytes.
O plano guarda também a base e extensão efetivamente registradas, classificação,
tamanho/modo/função/localização. Uma referência a elemento registra apenas esse
escalar, não todo o array. Aliases iguais deduplicam; sobreposição estática de
registros distintos é diagnosticada. Os metadados sobrevivem em TREE/GIMPLE até
omplower, que remove a região; não são acessos executáveis ou testes dinâmicos
já emitidos. Cópias remapeiam explicitamente as declarações.

A ponte de rastreamento já toma endereços. A revisão encontrou e corrigiu um
problema de marcação tardia: listar uma variável depois de usos anteriores podia
corromper a representação SSA de um corpo OpenMP outlined. Os objetos são agora
marcados endereçáveis antes de qualquer parte da função ser gimplificada.

`libgomp/newtm-transaction.c` mantém índice global de intervalos estáveis de
registros. A admissão é atômica antes da aquisição da hash, inclusive candidatos
pendentes. Endereço igual exige tamanho e kind iguais; intervalos distintos
sobrepostos produzem INVALID. Falha da hash retira o candidato sem registro
parcial. Teardown retira a faixa depois do release/destrutor da hash e antes de
liberar o registro; a lease permanece ativa até o fim. O mutex do índice não
engloba hash, alocação, acesso aos valores, diagnósticos ou callbacks. O índice
é linear nesta primeira implementação; a hash continua responsável pelas
referências e pelos metadados compartilhados. Registro não obtém reservas.

A [ABI interna v1](OPENMP_TRANSACTION_ABI.md) fixa contexto, classificação, status,
bindings e resultados das futuras funções preparadas, incluindo propriedade,
failure propagation e ordem de confirmação/limpeza. Nenhum símbolo novo GOMP
foi exportado e nenhuma integração STM funcional foi ativada.

## Limites explícitos

São rejeitados volatile, atômicos, uniões, bitfields, membros flexíveis, VLA,
tipos incompletos/de tamanho variável, classes C++, acesso parcial de escalares
complexos preexistentes e agregados/arrays contendo ponteiros. O último filtro
impede escape de endereços privados por cópia de representações cuja proveniência
interna ainda não é acompanhada. Ponteiros escalares admitidos precisam de
identidade demonstrável; casts ponteiro/inteiro e escape de privados são recusados.
Retornos por referência estão excluídos. Retornos ponteiro devem ter identidade
única e não podem designar locais da chamada encerrada.

A prova de ponteiros em laços é conservadora: mutações e chamadas invalidam os
vínculos relevantes; uma atribuição inequívoca posterior pode restabelecê-los.
Vínculos ponteiro/referência em recursão, efeitos de ponteiros no ciclo recursivo
e mutações encapsuladas em SAVE_EXPR são recusados até existir identidade por
ativação e controle de avaliação única. Recursão escalar simples é admitida.
Registros condicionais que não dominam o acesso são recusados, mesmo se uma
futura análise mais precisa puder demonstrar a execução do caminho.

O pragma ainda executa memória comum com rastreamento; não oferece isolamento,
commit STM ou retry automático. Os testes funcionais STM usam a API privada e
corpos explicitamente preparados. Exemplos mantêm critical externo e E/S fora
das regiões; isso não é prova de isolamento STM do compilador. Validade, extensão
real e vida útil das regiões fornecidas à API interna continuam pré-condições.

## Verificação realizada

- Reconstrução de GCC C/C++ e libgomp no contêiner gcc-tm-register.
- `check-subset`: 201 verificações, C/C++ em O0/O2, casos positivos e negativos,
  índices com efeito avaliados uma vez, ligação/execução com e sem LTO e scans
  dos planos canônicos. Relatório: transaction_test/.build/subset/resultado.json.
- `verificar_regressao_compilador.py`: 37/37 casos existentes de GCC e libgomp,
  aceitação/rejeição, ausência de ICE e execução dos casos dg-do run.
- `check-overlap`, `check-register`, `check-reservations`, `check-access`
  (inclui values), `check-commit`, `check-retry` (inclui failure), `check-lifecycle`
  e seis alvos de hash: passaram com -O2 -Wall -Werror.
- `check`: exemplos com 1/2/4 threads, aninhamento e cargas 1:9, 1:1 e 9:1,
  quatro threads e mil tentativas por thread: passaram.
- Índice e hash/contexto compilados com AddressSanitizer e UBSan, incluindo
  concorrência, aquisição pendente e rollback de aquisição: passaram com
  detect_leaks=1. Não foi executado ThreadSanitizer.
- `git diff --check`: sem erros de whitespace.

DejaGNU foi tentado, mas runtest/expect não estão instalados no contêiner.
A verificação direta não interpreta dg-final e não substitui uma execução
completa DejaGNU. Os casos de regressão foram mantidos/atualizados e dois novos
casos DejaGNU cobrem diagnósticos e representação sem ativação. Não foi executada
a suíte inteira do GCC, módulos C++ ou testes em outros targets.
