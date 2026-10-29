# Prontidão para integração funcional com o compilador — 2026-10-29

Estado após a etapa 2: [descritores internos implementados](OPENMP_TRANSACTION_DESCRIPTORS.md).
Existem campos materializados de objeto lógico, extensão, deslocamento, kind e
validade, além de canais explícitos para bindings de parâmetros/resultados.
Faltam ABI executável, clones, consumo das validações/acessos STM e epílogos.
A matriz histórica abaixo descreve a inspeção anterior, não novas pendências
para repetir as etapas 1 e 2. A integração funcional permanece desativada.


Atualização após a etapa 1: [vínculos operacionais implementados](OPENMP_TRANSACTION_OCCURRENCES.md).
A sonda e a matriz abaixo registram a inspeção anterior. Agora existem sites,
grupos e vínculos para operandos GIMPLE realmente avaliados; o plano diagnóstico
continua não executável. Testemunhos source-only são explícitos, sem resultado.
ABI executável, instrumentação STM, clones e epílogos ainda faltam;
a conclusão geral sobre não ativar a integração permanece válida.

Resultado: **o runtime permite preparar a integração, mas o caminho atual do
compilador ainda não permite ativá-la com as garantias solicitadas**. O critério
«nenhum acesso admitido contorna a visão transacional; falhas propagam sem valores
fictícios» não foi atingido. Esta verificação não é uma entrega de instrumentação.

O usuário autorizou avaliar e implementar se viável. A inspeção e a sonda de
código gerado identificaram pré-requisitos estruturais pendentes, especialmente
vinculação de ocorrências e avaliação única; a ativação não foi realizada. Não
substituir isso por uma ligação parcial que mantenha acessos comuns no corpo.

## Verificação por requisito

| Requisito | Base existente | Lacuna que impede ativação segura |
| --- | --- | --- |
| Transportar registros e kind | Lista TREE e plano GIMPLE conservam identidade, extensão e classificação. A API privada recebe kind. | O builtin executável ainda é void GOMP_transaction_register(void *, size_t), com dois argumentos. Não transporta contexto, kind ou status nem chama o registro STM funcional. |
| Instrumentar leituras/escritas | Diagnósticos registram receitas de acessos. Read/write privados foram testados. | Gimplificação e omplower deixam cargas, stores e chamadas comuns no corpo. O plano só é conservado, copiado e impresso; nenhum passe o consome para emitir acessos STM. |
| Preservar avaliação única | O código comum já decompõe efeitos em temporários GIMPLE. | A receita no plano conserva expressões como index++. Falta associação estável entre a ocorrência de origem e o temporário que representa seu valor já avaliado. Reavaliar a receita não é seguro. |
| Preservar endereços lógicos | A análise conhece objeto físico, base registrada, extent e offsets; o runtime recebe a chave original. | Faltam descritores executáveis e transporte de bindings nos parâmetros/resultados, inclusive referências a subobjetos. Não é correto substituir ponteiros do programa por buffers privados. |
| Gerar funções preparadas | A seleção direta e a validação transitiva produzem prepare(function). | Não existem clones com contexto/bindings/status/result-out. A chamada continua dirigida à função comum. O plano de uma análise de chamada pode referir objetos do chamador e não é, isoladamente, um corpo genérico de clone. |
| Gerar epílogos de falha | newtm_execute e newtm_after_failure fazem commit/descarte/retry para callbacks explicitamente preparados. | O código gerado não recebe status, não desvia em falhas nem impede consumo de resultados. O finally existente serve ao rastreamento; não é um epílogo STM de sucesso/falha. |

## Evidência concreta de geração

Sonda compilada com o GCC local, -fopenmp -O0 -fno-inline, dumps gimple e omplower:

```c
static int update (int *p, int n)
{
  *p += n;
  return *p;
}
int main (void)
{
  int values[4] = { 0 }, index = 0, result = 0;
  int *alias = &values[0];
  #pragma omp transaction (values, index, result, alias)
  {
    values[index++] = 3;
    result = update (alias, 2);
  }
  return !(index == 1 && values[0] == 5 && result == 5);
}
```

O plano registra `kind=1` para values, `kind=0` para escalares e `prepare(update)`.
A receita de offset de values ainda contém `(long unsigned int) index++ * 4`.
Entretanto, o corpo GIMPLE já preservou o índice antigo num temporário:

```text
index.0_1 = index;
index.1_2 = index.0_1;
_3 = index.1_2 + 1;
index = _3;
values[index.1_2] = 3;
alias.2_4 = alias;
_5 = update (alias.2_4, 2);
result = _5;
```

Depois de omplower, esses acessos continuam comuns. A função update também
continua contendo `*p` comuns e retorno int comum. Não há clone preparado nem
teste de status antes de `result = _5`. O executável referencia somente
GOMP_transaction_begin/register/end da versão GOMP_NEWTM_1.0.

A sonda executou com sucesso e index terminou em um. Isso verifica a semântica
comum atual, não avaliação única de uma instrumentação STM inexistente. Também
não testa propagação de falhas do compilador: não há canal de falha gerado.

Artefatos locais em transaction_test/.build/prontidao-integracao/: sonda.c,
sonda.c.007t.gimple, sonda.c.010t.omplower, simbolos.txt e execucao.log.

## Preparação necessária antes da ativação

1. **Vincular ocorrências a operações executáveis.** Associar cada acesso e
   registro ao nó de origem e aos temporários produzidos pela gimplificação,
   inclusive constantes preservadas, RMW e desvios. Localização textual ou
   coincidência de expressão não substituem identidade de ocorrência. O plano
   atual pode continuar servindo ao diagnóstico, mas não deve ser reexecutado.
2. **Materializar descritores e bindings.** Avaliar endereço, índices e argumentos
   uma vez; validar limites e overflow antes dos acessos; conservar endereço
   lógico, base efetivamente registrada, extent, offset e kind. Resolver o
   transporte de parâmetros/resultados de clones e capturas do bloco externo.
   Distinguir objetos privados por tentativa de objetos preexistentes; nunca
   inferir essa distinção de uma consulta à hash que falhou.
3. **Implementar a ABI executável.** Acrescentar builtins e serviços públicos
   versionados de contexto/registro/read/write/encerramento com status explícito.
   Adaptar enums internos explicitamente. Não exportar simplesmente os símbolos
   hidden newtm_* nem mudar silenciosamente a ABI void de rastreamento.
4. **Gerar clones e controle externo.** Criar protótipos antes dos corpos para
   ciclos recursivos; preservar a assinatura comum; usar resultado privado por
   saída, escrito apenas em OK. Compartilhar contexto no aninhamento e propagar
   cada falha até o epílogo externo, sem consumir saída inválida. A escolha entre
   callback adaptado ao executor e laço/epílogos gerados deve ser concretizada
   junto com captura/outlining. Não usar finally para confirmar incondicionalmente.
5. **Verificar cobertura e fechar a ativação.** Um verificador da representação
   transformada deve recusar acessos/chamadas residuais a dados preexistentes.
   Casos sem prova de transformação segura devem ser diagnosticados, nunca
   executados silenciosamente em memória comum. Não reduzir o subconjunto sem
   identificar as novas restrições na documentação e nos diagnósticos.

Essa sequência é uma proposta de implementação dos contratos já definidos; não
acrescenta suporte a chamadas externas, classes ou outras extensões adiadas.
A seleção do mecanismo concreto de controle externo ainda não foi implementada.

## Porta de aceitação da integração

Para declarar concluída a integração, exigir simultaneamente:

- Registro emitido com contexto, identidade, extent e kind corretos; status
  verificado antes de continuar.
- Cargas/stores preexistentes transformados em acessos à visão privada; privados
  genuínos da tentativa identificados por prova, sem caminho alternativo comum.
- Índices com efeitos, ponteiros, referências e resultados de chamadas avaliados
  uma vez; limites e identidade preservados em O0/O2 e com otimizações relevantes.
- Chamadas transacionais dirigidas a clones preparados; chamadas comuns fora da
  tentativa conservadas. Clones retornam status e saídas preservadas em falha.
- Conflito em função interna não retorna zero, NULL ou outro valor inventado:
  desvia ao epílogo, descarta e reinicia o prefixo externo inteiro. INVALID limpa
  uma vez e termina fatalmente, sem repetição nem execução do restante do corpo.
- Nenhuma publicação antecipada, nenhum commit no caminho de erro e nenhuma
  falha recuperável depois de começar PUBLISHING.
- Testes de código gerado e execução concorrente, incluindo arrays/campos,
  aliases, referências a subobjetos, recursão admitida, avaliação única, outputs
  sentinela preservados e conflito/falha em chamadas preparadas.

## Runtime reverificado nesta avaliação

check-internos passou novamente com -O2 -Wall -Wextra -Werror: contador em
1/2/4 threads e carga em 1/4 threads, mil operações por thread, proporções 1:9,
1:1, 9:1, 0:1 e 1:0, mais rejeição de argumentos inválidos. Os callbacks desses
exemplos verificam status manualmente e são ligados ao archive local. Esse
resultado sustenta a base funcional do runtime; não transfere a garantia para
código gerado pelo compilador, que continua só com rastreamento.
