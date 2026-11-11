# Correção de diagnósticos — passo 3 concluído

O resolver de newtm-memory.cc passa a conservar causa e regra juntas, em
metadados locais da análise. Falhas de identidade/faixa conservam V03;
classificação de armazenamento usa V04. A primeira falha não é sobrescrita.
Não houve alteração da ABI/runtime, dos predicados de aceitação ou do formato
LTO: os novos campos não são serializados nem concedem certificados.

## Correção

- PRIVATE constante sem alocação pertencente à ativação registra V04.
- Domínio da tag de armazenamento e escolha do ramo PRIVATE/REGISTERED
  registram V04. Nulidade, extensão, identidade, faixa e aritmética continuam
  com V03. A seleção é feita no ponto que conhece o predicado, não por busca
  de palavras na mensagem.
- memory_range_fail transporta a classificação explícita ao diagnóstico.
  O consumidor de status também preserva a causa V04 quando uma alternativa
  PRIVATE não pode ser provada.
- O solver compartilhado pode produzir uma causa genérica prefixada V03.
  Para os predicados de armazenamento, a causa principal passa a descrever
  a guarda de classificação; o texto do solver é preservado em nota
  "underlying guard analysis". Assim ele não substitui a regra V04 do caso.

Exemplo confirmado:

```text
error: transaction transformed memory proof failed: constant PRIVATE classification lacks an activation-owned allocation
note: transaction rule V04, operation storage classification, function ‘storage_probe’
```

## Validação dirigida

O runner verificar_regra_armazenamento.py cobre C/C++, O0/O2 e checking=0/2,
em 56 casos: 40 negativos e 16 controles positivos compilados/ligados/executados.

| Caso | Resultado esperado e confirmado |
| --- | --- |
| Objeto global forjado como PRIVATE constante | Recusa V04, causa original preservada. |
| Tag fora do domínio | Recusa V04, detalhe do solver preservado. |
| Acesso PRIVATE no ramo REGISTERED | Recusa V04. |
| Extensão/faixa inválida | Recusa V03; não foi convertida indiscriminadamente em V04. |
| Falha anterior de identidade antes de PRIVATE inválido | Primeira causa e V03 preservadas. |
| Alocação própria com PRIVATE válido | Classificação aceita. |
| Tupla de endereço REGISTERED válida | Classificação aceita. |

Esse teste inclui o fonte real newtm-memory.cc em um plugin com símbolos
ocultos e mapas privados isolados; não acrescenta hooks à produção. Exercita
resolver::ranges e o consumidor real do diagnóstico. É um teste de componente,
não uma mutação integrada de tag no grafo. Os controles de classificação usam
faixa vazia para isolar armazenamento; não demonstram autorização de READ/WRITE
de um objeto não registrado no runtime.

Como regressão integrada separada, passaram os 36 casos existentes:
28 recusas e oito controles executados, C/C++ O0/O2 checking=0/2, agregado
somente C. Não se somam esses números a verificações históricas como se
fossem casos independentes de toda a implementação.

Comandos principais:

```sh
make -C gcc-build -j2 all-gcc
python3 transaction_test/verificar_regra_armazenamento.py gcc-build
python3 transaction_test/verificar_contraprovas_integradas.py gcc-build
python3 transaction_test/verificar_memoria.py gcc-build
python3 transaction_test/verificar_status_atual.py gcc-build
```

As últimas duas regressões são registradas no fechamento abaixo. O build
incremental desta etapa terminou sem warning:/error:; self-tests internos
continuam desabilitados na configuração existente. A etapa não implica
build limpo, bootstrap ou aprovação integral automática da 6.6.

## Fechamento das regressões

As suítes existentes terminaram aprovadas no build atualizado:

- Memória remapeada: **281 verificações e 32 execuções**.
- Status atual: **449 verificações e 100 execuções**, incluindo GC, DSO/archive,
  C/C++, O0/O2 e checking=0/2 conforme a matriz do runner.

Os 56 casos de componente e 36 casos integrados também passaram.
Não houve nova rodada completa LTO ou DejaGNU neste passo.

Consolidação e manifesto SHA-256:
[report.json](../transaction_test/.build/diagnostic-step3/report.json).
O diretório preserva comandos, códigos de saída dos runners dirigidos,
logs de regressão da rodada, build e fontes alterados. A classificação
PRIVATE/V04 identificada na revisão final está corrigida neste escopo.
Os passos seguintes de preparação/validação final continuam sujeitos à
rastreabilidade explícita, sem concluir automaticamente a 6.6.
