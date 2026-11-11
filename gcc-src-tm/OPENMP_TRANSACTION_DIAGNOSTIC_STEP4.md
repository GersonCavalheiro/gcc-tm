# Correção de diagnósticos — passo 4 concluído: negativos preparados

Data: 2026-11-11. A matriz foi preparada e os oráculos registrados em
[diagnostic_negative_plan.json](../transaction_test/diagnostic_negative_plan.json).
Não houve alteração de produção, novo build ou execução da matriz completa
nesta etapa. O passo 5 continua sendo uma execução separada.

## Casos obrigatórios

| Caso | Preparação | Regra e detector exigidos |
| --- | --- | --- |
| N01: assinatura incompatível | parameter_owner em graph_cycle.c | V06; assinatura perde correspondência de parâmetros. |
| N02: guarda de overflow ausente | offset_flag em memory_static_array.c | V03; ausência da guarda de overflow. |
| N03: membro não preparado | novo plugin/fixture unprepared_member | V06; current graph contains an unprepared member; resultado REFUSED e proved=0. |
| N04: PRIVATE constante sem alocação válida | global_private em storage_rule.c | V04; constant PRIVATE classification lacks an activation-owned allocation. |

O catálogo inclui seis controles negativos complementares: assinatura versus
proprietário do controlador (V06/V07), tag inválida e ramo incorreto (V04),
faixa inválida (V03) e preservação da primeira falha (V03).

Todos exigem retorno de erro, causa e regra específicas, operação/função/
localização, ausência de ICE e ausência de objeto de saída. Um erro genérico
de parser ou falha de infraestrutura não satisfaz o oráculo. Cada fixture
tem controle válido compilado, ligado e executado.

## Novo caso de membro não preparado

O plugin primeiro observa APPROVED, proved=31 e dependências zero. Por meio
das interfaces internas existentes de contratos, acrescenta uma função comum,
com definição presente e sem preparação, à lista declarada de dependências
da raiz. O CFG e suas chamadas permanecem inalterados. A recomposição usa
newtm_current_verify, sem invocar diretamente um componente.

Exige-se a recusa específica D34/V06 e nenhum resultado parcial aprovado.
Este caso testa fechamento da dependência declarada, não inserção de uma
chamada física a uma função de assinatura inválida, que poderia ser recusada
antes de chegar ao detector pretendido. Não foram criados hooks de produção.

Ensaio mínimo aprovado nesta etapa: **uma recusa e um controle executado**,
C/O0/checking=0. A matriz C/C++ × checking=0/2 está preparada no runner,
mas não foi executada integralmente aqui.

```sh
python3 transaction_test/verificar_membro_nao_preparado.py gcc-build --smoke
```

Os relatórios ficam em transaction_test/.build/unprepared-member/<rodada>/,
com comandos, códigos de saída e hashes dos logs. O diretório é local e
ignorado pelo Git.

## Execução preparada para o passo 5

```sh
python3 transaction_test/verificar_diagnosticos_inventariados.py gcc-build
python3 transaction_test/verificar_regra_armazenamento.py gcc-build
python3 transaction_test/verificar_membro_nao_preparado.py gcc-build
python3 transaction_test/auditar_aceitacao.py gcc-build
```

Previsão verificável se todos passarem:

| Runner | Negativos | Controles executados |
| --- | ---: | ---: |
| Diagnósticos inventariados | 32 | 20 |
| Regra de armazenamento | 40 | 16 |
| Membro não preparado | 4 | 4 |
| Regressão dirigida LTO | 17 | 5 |
| Total previsto | 93 | 45 |

Esses totais são **plano**, não resultado novo. Os dois primeiros conjuntos
já tiveram execuções nas etapas anteriores, identificadas nos seus relatórios.
Repeti-los ou reaproveitar evidências deve considerar o estado do build e
dos fontes, sem transformar o plano em aprovação automática.

## Limites explícitos

- PRIVATE constante literal é teste do componente real com mapas isolados,
  não mutação integrada da tag. A contraprova integrada de identidade PRIVATE
  é distinta e já tem runner próprio.
- A regressão LTO cobre 17 mutações representativas e cinco positivos; não
  injeta cada caso N01–N10 em cada fase. A matriz de fases anterior conserva
  seu próprio alcance e suas ressalvas.
- A matriz nativa de assinatura/overflow/membro usa O0 e checking=0/2;
  armazenamento tem O0/O2. Não se declara produto cartesiano maior.
- Não foram preparados IR malformados só para executar retornos defensivos
  de todos os 38 emissores. A correspondência estática desses emissores
  permanece separada da cobertura dinâmica representativa.

O passo 4 está concluído como preparação. A execução/validação do passo 5,
as regressões afetadas do passo 6 e o novo parecer do passo 7 não foram
declarados concluídos; a 6.6 permanece aberta.

Atualização posterior: [passo 5 executado e aprovado](OPENMP_TRANSACTION_DIAGNOSTIC_STEP5.md),
com os 93 negativos e 45 controles previstos. Os números da tabela acima
passaram a ter evidência de execução nessa rodada, sem alterar seus limites.
