# Verificação do projeto transaction — 2026-10-21

## Escopo e conclusão

Verificação da extensão transaction nesta árvore GCC 16.2.0, em x86_64 GNU/Linux:
front-ends C/C++, representações, gimplificação/lowering, ponte de rastreamento,
17 serviços internos (hash, acesso e ciclo de vida), ABI, build, testes e agentes.
Não é uma auditoria integral de todos os componentes do GCC ou prova formal de
concorrência. Não houve implementação de operações STM nesta tarefa.

O estado observado corresponde ao projeto de esqueletos: diretiva integrada,
rastreamento funcional e operações transacionais ainda indisponíveis. Nenhuma
falha foi encontrada nos subconjuntos funcionais executados com o runtime certo.
Foi encontrado e corrigido um falso positivo no verificador Python dos stubs.

Foram considerados OPENMP_AGENT, CRITICAL_AGENT, PROJECT_AGENT, SCOPE_AGENT,
HASH_AGENT, HASH_IMPLEMENTATION_AGENT, ACCESS_AGENT, ABORT, PRIMITIVES e
IMPLEMENTATION. Contratos futuros foram separados do comportamento atual e
resultados históricos não foram contados como validação desta execução.

## Problema encontrado e correção

`transaction_test/verificar_lifecycle.py` usava assert para conferir logs e
símbolos. Python com -O/-OO (ou PYTHONOPTIMIZE) removia essas verificações.
Reprodução: passando o diretório da libgomp do sistema
`/usr/lib/gcc/x86_64-linux-gnu/13`, o script normal falhava corretamente por não
achar os símbolos newtm no archive; com -O imprimia sucesso e terminava com zero.
O executável de teste continuava ligado ao archive local, portanto esse caso
expõe especificamente a falta de validação da biblioteca selecionada no script.

Correção: o script agora recusa execução com asserções desabilitadas antes de
rodar os testes. -O e -OO passaram a falhar com mensagem explícita; o alvo normal
check-lifecycle passou novamente. O teste C já remove NDEBUG antes de incluir
assert.h e continua verificando os retornos e a preservação dos dados.

Evidências locais: `/tmp/newtm-project-audit/checker-normal.log`,
`checker-optimized.log` e `checker-fix.log` no mesmo diretório. Essa alteração
atinge somente a infraestrutura de validação, sem mudança de ABI ou semântica.

## Build e testes executados

GCC e libgomp passaram em builds incrementais separados dos fontes. O build da
libgomp compilou os stubs PIC/não PIC com -Wall -Werror e religou os archives/DSO.
Não foi feito bootstrap limpo ou reconstrução de toda a árvore do zero.
DejaGNU/Expect/Tcl estavam ausentes e foram instalados para executar os testes.

Resultados finais, contados nos arquivos .sum:

| Seleção | PASS | FAIL | UNRESOLVED | UNSUPPORTED |
| --- | ---: | ---: | ---: | ---: |
| GCC C transaction-* | 56 | 0 | 0 | 0 |
| GCC C++ transaction-* | 213 | 0 | 0 | 0 |
| GCC C critical-* | 20 | 0 | 0 | 0 |
| GCC C++ critical-* | 60 | 0 | 0 | 0 |
| libgomp transaction-* em C/C++ | 6 | 0 | 0 | 0 |
| libgomp critical-* em C/C++ | 28 | 0 | 0 | 0 |
| **Total** | **383** | **0** | **0** | **0** |

Os números contam verificações DejaGNU, não arquivos ou programas distintos.
C++ incluiu as variações de padrão selecionadas pelo driver. Foram conferidos
.log e .sum; os testes de runtime tiveram execução, não apenas compilação.

Também passaram:

- `make -C transaction_test -B check check-lifecycle`: recompilação dos exemplos,
  máximo com 1/2/4 threads, aninhamento lexical/por função, duplicatas e liberação
  externa; 22 chamadas dos seis stubs de ciclo de vida. check-lifecycle foi
  repetido após corrigir seu verificador.
- Teste direto dos nove serviços hash e read/write, ligado ao archive local:
  14 chamadas, saídas NULL/zero/INVALID, preservação de buffers e armazenamento
  opaco, argumentos nulos/limites e callbacks que não podem ser invocados pelos
  stubs. Não cria tabela/contexto real nem testa reservas/concorrência STM.
- 14 verificações diretas de integração: C/C++ em O0, O2 e O2+LTO (6), modos
  sem OpenMP e somente SIMD em ambas as linguagens (4), atributo OpenMP C++ (1),
  identidade dos endereços private/reduction (1), rejeição de VLA listado (1),
  caracterização da aceitação atual de volatile/taskyield (1).
- Exportação/importação de módulo C++20 com template que registra referência;
  compilação, ligação e execução passaram.
- Depuração: -ftransaction-debug produziu mensagens em stderr; compilações
  diretas normais ficaram silenciosas. Logs de rastreamento continuam em stdout.
- Comparação das 17 assinaturas internas, dois callbacks e três enums com o
  inventário; links locais e inclusão dos fontes em Makefile.am/in.
- nm/readelf: newtm_* ausentes da API dinâmica e funções globais dos objetos
  newtm marcadas HIDDEN. A ponte GOMP_transaction_begin/register/end permanece
  exportada em GOMP_NEWTM_1.0.

Os programas de integração C++ cobriram referência/template, lambda e destrutor
na propagação de exceção, seguidos de outra região para verificar o equilíbrio
da ponte. Isso comprova cleanup de rastreamento, não suporte a exceções na STM.
O teste de identidade comparou os endereços registrados com as instâncias reais
private/reduction dentro da região parallel externa.

## Problemas do ambiente de teste resolvidos

A primeira seleção repetiu o mesmo arquivo .exp com filtros diferentes. O
último filtro prevaleceu, executando critical; transaction foi então executado
separadamente. Não somar resultados de uma seleção que não executou testes.

Na primeira execução transaction pelo driver GCC houve 5 FAIL em C e 15 em C++:
os executáveis carregaram `/lib/x86_64-linux-gnu/libgomp.so.1` e falharam por
`GOMP_NEWTM_1.0` ausente. Os argumentos GCC_UNDER_TEST/GXX_UNDER_TEST passados
somente como variáveis make não alteraram os comandos efetivos desse driver.

A repetição com LD_LIBRARY_PATH apontando para a libgomp local produziu os
56/213 PASS acima. O log confirma o caminho na execução. As falhas iniciais
continuam preservadas em `testsuite-newtm-audit-transaction-c` e
`testsuite-newtm-audit-transaction-cxx`, sob `/usr/src/gcc-build/gcc`.
Não foi necessário alterar expectativas ou filtrar mensagens para obter sucesso.

## Coerência técnica e pendências

| Área | Resultado da inspeção | Trabalho funcional ainda necessário |
| --- | --- | --- |
| Front-ends | Registro com -fopenmp, busca de nomes antes do corpo, lista de declarações e substituição em templates presentes. | Matriz precisa de tipos/aliases/efeitos e diagnósticos do subconjunto STM. |
| IR | OMP_TRANSACTION tem corpo/lista; GIMPLE_OMP_TRANSACTION guarda corpo. Walkers, cópia, dumps e remoção em omplower presentes. | Preservar identidade para instrumentação antes de remover marcador; instrumentar cada acesso uma vez. |
| Ponte | Profundidade e deduplicação por endereço equilibram create/acquire/release/destroy de rastreamento. | Ao implementar hash, tratar estados reais e nunca publicar handles inválidos. |
| Hash | Nove stubs cumprem saídas atuais; callbacks não são executados. | Tabela, mutexes por bucket, referências reais e política de remoção. |
| Acesso | Read/write imprimem e preservam dados; não há chamadas geradas. | Reservas, valores privados, limites, instrumentação e propagação segura. |
| Ciclo de vida | Quatro primitivas e dois helpers ocultos; begin inicializa outermost=false, slots preservados. | Contexto/máquina de estados, entrada/saída efetivas, confirmação, falha, descarte e backoff. |
| Documentos | Assinaturas atuais e fases/helper coerentes; stm_* continua conceitual. | Manter contratos sincronizados quando stubs virarem operações reais. |

Bloqueadores para anunciar STM funcional, já previstos nos agentes:

1. **Corpo ainda acessa memória comum.** Não há isolamento, rollback, publicação
   privada ou retry. O exemplo concorrente usa critical externo de propósito.
2. **Restrições transitivas de tarefas ainda não são impostas integralmente.**
   A sonda com taskyield/volatile foi aceita pelo compilador. A proibição dos
   agentes é contrato futuro, não garantia de diagnóstico atual; TLS funcional
   depende de impedir suspensão/troca de tarefa em toda a tentativa.
3. **Propagação entre funções e cleanups permanece pendente.** newtm_fail só
   possui esqueleto, não transfere controle. Definir como interromper o corpo no
   primeiro erro antes de ligar acessos gerados; finally trace não pode confirmar.
4. **Leituras externas, aliases e tipos especiais ainda exigem decisões.**
   Não deduzir suporte a classes/volatile/atômicos por aceitação atual do parser.
5. **Cobertura permanente menor que a exploratória.** Ciclo de vida tem teste
   permanente; as verificações diretas de hash/acesso e casos extras desta
   auditoria ficam em /tmp. Promover casos relevantes à suíte quando implementar
   operações reais; testes de stubs não devem virar critérios de sucesso STM.

A política futura de descarte permanece explícita: end que falha preserva
recursos, o controle externo executa cleanups e chama discard uma vez. INVALID
permanente prevalece sobre CONFLICT; OK de descarte não equivale a commit.
A espera só ocorre após limpeza completa e reinício pertence ao controle externo.
A preferência por construir esqueletos antes das implementações foi preservada.

## Reprodução e evidências

Comandos de build/exemplos:

```sh
make -C /usr/src/gcc-build/gcc -j3 all
make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp -j3 all
make -C /usr/src/gcc-omp-transaction/transaction_test -B check check-lifecycle
```

Para reproduzir transaction no driver GCC neste host:

```sh
LD_LIBRARY_PATH=/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/.libs OMP_NUM_THREADS=4 OMP_DYNAMIC=FALSE make -C /usr/src/gcc-build/gcc check-gcc TESTSUITEDIR=testsuite-newtm-audit-final-c RUNTESTFLAGS='gomp.exp=transaction-*'
LD_LIBRARY_PATH=/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/.libs OMP_NUM_THREADS=4 OMP_DYNAMIC=FALSE make -C /usr/src/gcc-build/gcc check-g++ TESTSUITEDIR=testsuite-newtm-audit-final-cxx RUNTESTFLAGS='gomp.exp=transaction-*'
```

Para critical, executar separadamente com `gomp.exp=critical-*`, preservando
.sum/.log de cada seleção em diretórios distintos. Na libgomp:

```sh
OMP_NUM_THREADS=4 OMP_DYNAMIC=FALSE make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp check RUNTESTFLAGS='c.exp=transaction-*.c c++.exp=transaction-*.c'
OMP_NUM_THREADS=4 OMP_DYNAMIC=FALSE make -C /usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp check RUNTESTFLAGS='c.exp=critical-*.c c++.exp=critical-*.c'
```

A segunda chamada sobrescreve o resumo normal da libgomp: guardar cópias antes.
Nesta auditoria, critical foi executado antes de transaction e preservado em /tmp.

Resumos finais:

- `/usr/src/gcc-build/gcc/testsuite-newtm-audit-final-c/gcc/gcc.sum` e .log.
- `/usr/src/gcc-build/gcc/testsuite-newtm-audit-final-cxx/g++/g++.sum` e .log.
- `/usr/src/gcc-build/gcc/testsuite-newtm-audit-c/gcc/gcc.sum` (critical) e .log.
- `/usr/src/gcc-build/gcc/testsuite-newtm-audit-cxx/g++/g++.sum` (critical) e .log.
- `/usr/src/gcc-build/x86_64-pc-linux-gnu/libgomp/testsuite/libgomp.sum` e .log.
- `/tmp/newtm-project-audit/libgomp-critical.sum` e .log.

`/tmp/newtm-project-audit/dejagnu-summary.json` reúne as contagens finais.
Esse diretório também contém fontes, comandos/resultados de integração,
runtime-stubs.c e logs, módulo C++ e conferência estática. Logs de build e
invocação ficam em `/tmp/newtm-audit-*.log`; são evidências locais, não arquivos
necessários para construir/distribuir o projeto.

Limites: suíte completa GCC/OpenMP não executada; sem novo bootstrap, targets
alternativos, Fortran transaction, dispositivos/offload ou validação de STM
concorrente. Os resultados caracterizam esta configuração e esta etapa do projeto.
