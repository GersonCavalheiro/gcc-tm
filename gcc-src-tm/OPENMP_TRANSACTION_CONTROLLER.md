# Controle externo gerado — etapa 5.5

**Atualização 6.4.6 — 2026-11-04:** a
[prova do protocolo proprietário](OPENMP_TRANSACTION_OWNERSHIP.md) está implementada.
Demonstra admissão, propriedade/vida útil do slot, confirmação, condenação e
limpeza, contador saturante, fatal e retry integral com preparação fresca.
As pendências históricas abaixo relativas à 6.4.6 estão superadas. Composição
segue na 6.4.7, certificação/remapeamento na 6.5 e aceitação integral na 6.6.
Não há COBERTA nem certificado novo; ABI, runtime e layouts preservados.

Implementado em 2026-10-31. A opção explícita `-fopenmp-transaction-stm`, usada
com `-fopenmp`, ativa a execução STM do pragma C/C++ no subconjunto preparado.
O padrão e `-fno-openmp-transaction-stm` conservam o rastreamento anterior.
No modo funcional, corpos ou identidades de entrada incompletos são erros de
compilação; não há fallback para rastreamento ou memória comum.

## Entrada, propriedade e tentativas

No fim da gimplificação, depois da produção de descritores e antes de
tree-nested/omplower, `newtm_clone_control` substitui cada região externa no
corpo original por um BIND com controlador. Funções preparadas continuam
emprestando contexto e tratando regiões internas por nest_begin/nest_end;
não criam outro proprietário nem fazem retry interno.

O controlador declara um slot exclusivo do tipo correto, inicializado em NULL,
com endereço estável até consumo. Declara também outermost, status explícito,
contador de conflitos size_t e vetor privado de bindings. Os objetos originais
capturados continuam nas suas ativações, inclusive locais de parallel.

O fluxo gerado é:

1. Begin sobre slot vazio; conferir status e outermost=1.
2. Preencher bindings de entrada validados e chamar o auxiliar preparado.
3. Conferir status do auxiliar e status efetivo do contexto.
4. End somente em sucesso; continuar o programa somente após confirmação e limpeza.
5. Em falha com contexto, chamar fail para reter/traduzir a causa, incrementar
   o contador com saturação em SIZE_MAX e chamar after_failure exatamente uma vez.
6. Somente CONFLICT retornado com slot vazio autoriza voltar ao begin. Erros
   permanentes limpam e terminam; não repetem o bloco.

Cada tentativa chama novamente o corpo inteiro. Seus registros, inicializadores,
automáticos e chamadas preparadas são executados de novo; nenhum prefixo bem
sucedido é reaproveitado. Os efeitos nos objetos preexistentes permanecem na
visão privada até commit. Não há confirmação em finally.

End falho conserva contexto e recursos para esse mesmo epílogo. O controlador
chama fail também nesse caminho, sem confiar que uma falha retornada tenha
condenado o contexto. Tags desconhecidas tornam-se INVALID. Falha de entrada
sem contexto vai diretamente ao diagnóstico fatal, sem after_failure vazio.

## Identidades de entrada e remapeamento

`newtm_descriptor_function` registra descritores específicos dos canais de
captura na entrada da região, antes de percorrer seu corpo especulativo. São
construídos a partir da proveniência e operandos atuais, não de receitas do
plano diagnóstico. Capturar um objeto ponteiro e capturar a identidade de seu
valor continuam sendo canais separados, na ordem conhecida pelo auxiliar.

O controlador consome imediatamente esses descritores para gerar campos e
guardas. Retira a associação da região e todos os metadados do corpo original
substituído. O código resultante, com declarações nos BINDs apropriados, segue
pelo remapeamento nativo do GCC. Não há consulta operacional após outlining.

Uma referência C++ recebe cópia privada de sua representação imutável, sem ler
o conteúdo do referido. O auxiliar recebe o endereço dessa cópia e identidades
separadas para representação e referido. Esse armazenamento é transporte interno
inacessível ao programa; não torna o objeto referido privado. A identidade do
referido pode vir da proveniência demonstrada ou de seu endereço de referência
e tipo completo, conservando a exigência de registro antes do acesso.

Depois de uma região externa, a análise invalida mapas de definições/proveniência
que poderiam apontar para operações do corpo original removido. Uma região
posterior precisa de prova nova para canais ponteiro que dela dependam; não se
transporta silenciosamente um resultado especulativo antigo para outra tentativa.
Casos sem identidade de entrada executável são diagnosticados. Essa política
conservadora não amplia o subconjunto nem reconstrói proveniência em runtime.

## Certificação transitiva

As declarações dos auxiliares ativados são registradas como raízes persistentes.
Após analyze_functions preparar e baixar os corpos, e antes do reset das tabelas
transitórias, `newtm_clone_certify_regions` exige a prontidão de cada grafo.
Usa os resumos da 5.4, incluindo ciclos e recusas transitivas; não retém nem
consulta descritores. Um callee incompleto impede produzir um programa funcional.

Frames GNU incompatíveis continuam recusados. O transporte explícito de ambiente
léxico (alternativa B) permanece [possibilidade futura](OPENMP_TRANSACTION_DECISIONS.md).
Não foram ampliados tipos, exceções/cleanups, chamadas externas/indiretas,
recursão com ponteiros ou suporte a dispositivos/offload.

## Extensão da ABI

`GOMP_transaction_stm_v1_fatal(uint32_t)` é um serviço void/noreturn, exportado
em `GOMP_NEWTM_2.1`, derivada de `GOMP_NEWTM_2.0`. Diagnostica erro terminal do
controlador sem contexto vivo; não executa limpeza nem aceita propriedade de slot.
Os serviços anteriores conservam símbolos e versões. Layouts de slot,
`newtm_binding_v1` e `newtm_clone_binding_v1` permanecem inalterados.

O construtor GCC aceita destino de status NULL somente para esse serviço.
Seu builtin é cold/noreturn/nothrow. Com contexto existente, erro permanente
continua passando pelo after_failure da ABI 2.0, que descarta antes de fatal.

## Validação e trabalho restante

`make -C transaction_test check-controlador`: 225 verificações e 106 execuções
aprovadas com pragma real, sem plugin de invocação. C/C++ O0/O2, LTO e ligação
dinâmica/archive; execução normal, conflito no corpo e no end, retry completo,
avaliação única, recriação de automáticos, ponteiros/resultados, referências,
templates, capturas GNU compatíveis, regiões sequenciais/vazias, aninhamento,
capturas após outlining e conflito real em parallel.

Wrappers exclusivos de testes injetam falhas e verificam originais intactos
antes de commit e depois de descarte. Testes archive com inspeção interna
comprovam slot consumido, lease liberada e objetos preservados antes de fatal
em entrada falha e em INVALID após escrita privada. Há recusas de callee
transitivo incompleto e frame incompatível, conferência de símbolos/versionamento
e preservação do modo padrão de rastreamento.
Casos de proveniência stale entre regiões são recusados. Dois casos GCC de
controlador e recusa de frame foram compilados diretamente em C/C++, conforme
aplicável, com conferência dos dumps e diagnósticos.

Regressões aprovadas: clones 209/48 execuções, preparação 37/8, leituras 52/12,
escritas 56/12, chamadas 75/18, ocorrências 63/63, descritores 36/36,
subconjunto 201 e ABI 88. A matriz não executou a suíte DejaGNU completa.

A 5.5 entrega o controlador funcional opcional. A
[validação integrada ampliada da 5.6](OPENMP_TRANSACTION_INTEGRATION_VALIDATION.md)
foi concluída: o alvo combinado passou 1303 verificações e 1036 execuções.
O verificador integral que recusa qualquer
operação admitida residual continua na tarefa 6; esta entrega não o substitui.
