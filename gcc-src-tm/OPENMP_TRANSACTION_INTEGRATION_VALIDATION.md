# Validação integrada — etapa 5.6

Concluída em 2026-10-31. A tarefa 5 tem preparação, leituras, escritas,
chamadas/bindings, controlador externo opcional e validação integrada concluídos.
A execução usa `-fopenmp -fopenmp-transaction-stm`; o padrão conserva rastreamento.
Não foi necessário alterar o compilador ou o runtime nesta etapa. Os novos
cenários respeitam o subconjunto e os contratos existentes.

## Revisão de cobertura

A matriz da 5.5 já exercitava execução normal, um conflito no corpo ou no end,
aninhamento, capturas em parallel, referências, chamadas, regiões sequenciais,
recusas e limpeza real antes de fatal. A 5.6 conserva essa matriz como
dependência obrigatória e acrescenta os cruzamentos abaixo.

| Propriedade | Evidência nova ou reaproveitada |
| --- | --- |
| Falha antes de consumir resultados | `integration.c`: falhas em todas as posições das operações emitidas e proibição de outra operação normal após a falha. |
| Retry completo | Dois conflitos consecutivos por posição; duas sequências iguais ao prefixo original, dois descartes e uma terceira execução completa. Contador positivo 1/2 e slot proprietário estável e consumido entre tentativas. |
| Avaliação única | Índice incrementado em argumento de chamada que retorna ponteiro; incremento de local privado por callee; resultados finais verificam uma avaliação por tentativa bem-sucedida. |
| Saídas e ausência de publicação parcial | Originais de array, escalar, índice, resultado e objeto ponteiro são inspecionados antes de cada operação e antes/depois de descarte. Valores finais aparecem somente após commit. |
| Erro permanente e tag desconhecida | INVALID e 77 na primeira/última posição de cada espécie de operação; exatamente um epílogo e nenhuma confirmação/repetição. Entrada sem contexto termina diretamente. |
| Limpeza antes de fatal | `controller_cleanup.c` da 5.5 comprova slot vazio, lease liberada e originais preservados antes de fatal, inclusive falha de entrada após aquisição de lease. |
| Visão privada real | `integration_views.c`: original PROT_NONE durante escrita privada, leitura pelo callee, conflito, descarte e retry; proteção retirada apenas no end, antes da publicação. |
| Agregados e aliases | `integration_aggregate.c`, somente C: fonte modificada privadamente, retorno agregado de callee, store derivado, cópia sobre o próprio destino e conflito após prefixo escrito. |
| Controle de fluxo | `integration_control.c`: laço e ramificações dentro do bloco, conflito no meio do laço e recriação do acumulador privado. |
| Capturas após outlining | `integration_parallel.c`: quatro threads, 128 commits disputando contador, instâncias locais distintas, ponteiros, chamadas aninhadas, capturas GNU transitivas em frames compatíveis e templates/referências C++. |
| Assinatura comum preservada | O mesmo callee de `integration_parallel.c` é chamado normalmente fora da transação, sem contexto ou bindings explícitos. |
| Limites e overflow | `integration_bounds.c`: índices 0/1, -1/2, extremos de 32 e 64 bits; guardas recusam os inválidos antes do READ, após prefixo privado, com resultado original preservado. |
| Coleta de lixo do compilador | As compilações dos executáveis novos usam checking, debug, `ggc-min-expand=0` e `ggc-min-heapsize=0`, sem plugin de invocação. |
| Ligação e versionamento | `readelf` confere dependência de libgomp apenas na ligação dinâmica; a matriz da 5.5 confere símbolos GOMP_NEWTM_2.0/2.1 e preservação do modo padrão. |

## Método de injeção

Os wrappers são exclusivos dos executáveis de teste, na fronteira ABI. Não há
hooks novos no produto, sondas que substituem chamadas do programa ou invocação
manual de clones/auxiliares. O próprio pragma emite o controlador e as chamadas.

Cada configuração estabelece sua sequência de referência. Neste fixture foram
41 operações, cobrindo begin, register, read, write, status, nest_begin,
nest_end e end. Injeta-se CONFLICT em cada posição por duas tentativas, seguido
de sucesso. Uma falha de begin sem contexto é terminal, mesmo se a tag injetada
for CONFLICT; não se inventa um contexto para cleanup/retry.

Para INVALID e tag desconhecida, selecionam-se as primeiras e últimas posições
de cada espécie, verificando o prefixo e o diagnóstico fatal. FAIL e
after_failure são observados como propagação/limpeza, não recebem falhas
artificiais que violem seus contratos de consumo. Asserções permanecem ativas
em todos os executáveis. Os conflitos reais são exercitados separadamente em
parallel; não se afirma que o agendador produza uma contagem fixa de conflitos.

## Resultados e reprodução

Executar na raiz do repositório:

```sh
make -C transaction_test check-integracao
```

O alvo depende de `check-controlador`:

| Matriz | Verificações | Execuções |
| --- | ---: | ---: |
| Controlador 5.5, reexecutada | 225 | 106 |
| Cenários integrados 5.6 | 1078 | 930 |
| Total aprovado | 1303 | 1036 |

São 12 configurações: C/C++, O0/O2, O2 com LTO e ligação dinâmica/archive.
Os agregados C são verificados nas seis configurações C; não se admitiram
classes C++ por equivalência com structs C. A inspeção de leases da matriz
anterior usa o archive, sem exportar interfaces internas.

Logs por caso, objetos, executáveis, dumps e `report.json` ficam em
`transaction_test/.build/integration/` e `.build/controller/`. O relatório novo
registra sucesso global, número de execuções e cobertura de operações por
configuração. Há timeout de 90 segundos por compilação, ligação ou execução.
O teste PROT_NONE é específico deste host Linux com páginas de 4096 bytes,
verificadas em execução. Não foi executada a suíte DejaGNU completa ou offload.

## Limitações preservadas e critério de conclusão

`integration_global_pointer_refused.c` confirma em C/C++ O0/O2 a recusa de um
valor ponteiro global cuja identidade conhecida antes do pragma não possui
canal incoming no auxiliar. Não tratá-lo como memória privada nem anunciar
suporte executável porque a análise de origem o admite. Esse caso permanece
conservadoramente recusado, sem fallback. As recusas de proveniência stale,
callee incompleto e frame GNU incompatível continuam na matriz da 5.5.

A alternativa B de ambiente léxico explícito permanece futura. Não foram
ampliados tipos, vida útil, escapes, chamadas externas/indiretas, exceções,
cleanups ou recursão com ponteiros. O controlador continua opt-in.

O critério da 5.6 foi atendido para os cenários e configurações acima: execução
real de ponta a ponta, falhas propagadas antes de uso, saídas preservadas,
ausência de publicação parcial e retry completo. A tarefa 6 continua separada:
implementar o verificador que recusa qualquer operação admitida residual.
Esta matriz de testes não é uma prova de cobertura de todas as transformações
ou de todos os programas aceitos pela análise de origem.

O [contrato da etapa 6.1](OPENMP_TRANSACTION_VERIFICATION.md) já define as
obrigações, evidências e invalidações do verificador. O
[inventário da 6.2](OPENMP_TRANSACTION_OBLIGATIONS.md) implementa acompanhamento
e tratamentos declarados. Inspeção independente e nova barreira de aceitação
ainda dependem das etapas 6.3–6.6.
