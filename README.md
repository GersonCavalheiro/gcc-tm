# GCC-TM: extensão experimental OpenMP transaction

Este projeto implementa memória transacional em software no GCC e na libgomp, com a diretiva própria `#pragma omp transaction`. A base é GCC 16.2.0, com desenvolvimento e aceitação experimental em Linux x86_64, C e C++.

A documentação está em **[docs/index.html — índice](docs/index.html)**. Ela reúne:

- [Modelo e políticas transacionais](docs/01-transactional-model.html).
- [Diretiva e opções](docs/02-transaction-directive.html).
- [Serviços internos](docs/03-internal-services.html).
- Exemplos menores de [conta bancária](docs/examples/01-bank-account.html) e [carga](docs/examples/02-workload.html).
- [Mapa da implementação](docs/04-implementation-map.html) e [alterações no GCC](docs/05-gcc-changes.html).
- [Ambiente, build e testes](docs/06-build-validation.html).

Com `-fopenmp`, a diretiva opera em modo de rastreamento. O modo STM funcional exige `-fopenmp -fopenmp-transaction-stm` e o compilador/runtime desta árvore. A diretiva não integra o padrão OpenMP e é distinta de GNU TM/libitm.

A [etapa 6.6 foi aprovada no domínio experimental documentado](gcc-src-tm/OPENMP_TRANSACTION_ACCEPTANCE_FINAL.md). Os limites e as evidências estão descritos no parecer e na documentação.

Os fontes estão em `gcc-src-tm/`; o build separado fica em `gcc-build/`; exemplos e verificadores ficam em `transaction_test/`. Documentos históricos de arquitetura e aceitação permanecem junto aos fontes.

Os [exemplos implementados](examples/index.html) oferecem versões sequencial, interna, com a diretiva e GNU TM, com Makefiles e comparação automática de resultados.

Experimentos de tempo para contador e carga: [protocolo e scripts](examples/benchmarks/index.html).
