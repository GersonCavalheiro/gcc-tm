# Correção de diagnósticos — passo 5 concluído

Data: 2026-11-11. **A matriz preparada no passo 4 foi executada e aprovada:
93 negativos esperados e 45 controles positivos executados.** Os totais
previstos foram confrontados com os resultados de cada runner, sem contar
rodadas anteriores como novas execuções.

## Build e fontes

Os hashes dos testes preparados e dos fontes registrados nos passos 2 e 3
foram conferidos antes da execução. all-gcc incremental terminou com saída
zero, sem warning:/error: no log. Foram registrados hashes dos compiladores
e da libgomp utilizada. Não houve alteração de fontes de produção nesta etapa.
Não se reivindica build limpo ou bootstrap; os self-tests permanecem
desabilitados na configuração existente.

## Resultados

| Conjunto | Negativos aprovados | Controles executados | Domínio |
| --- | ---: | ---: | --- |
| Diagnósticos inventariados | 32 | 20 | C/C++, O0, checking=0/2. |
| Regra de armazenamento | 40 | 16 | C/C++, O0/O2, checking=0/2. |
| Membro não preparado | 4 | 4 | C/C++, O0, checking=0/2; matriz completa do runner, sem --smoke. |
| Regressão dirigida LTO | 17 | 5 | Negativos representativos C/O0/checking=0; baseline e positivos normativos C/C++ O0/O2. |
| Total | 93 | 45 | Matrizes distintas, com alcance explícito. |

As causas e regras esperadas foram exigidas pelos runners, além de operação,
função, localização, ausência de ICE e ausência do objeto/executável negativo.
Os controles correspondentes foram compilados, ligados e executados.

Assinatura incompatível e overflow ausente acionaram V06 e V03 respectivamente.
O membro não preparado acionou D34/V06 na recomposição agregada, com proved=0.
PRIVATE constante sem alocação própria, tag inválida e ramo incorreto usaram
V04 no teste do resolver; faixa inválida e primeira causa anterior conservaram
V03. A regressão LTO confrontou seus detectores específicos, não apenas uma
mensagem de erro genérica.

## Evidências e reprodução

[report.json](../transaction_test/.build/diagnostic-step5/report.json) consolida
os quatro runners, a seleção de fontes, hashes de binários e o manifesto dos
artefatos. O diretório preserva relatórios por conjunto, comandos, códigos de
saída, logs, resumo do build e cópias dos fontes utilizados. Os snapshots de
etapas anteriores permanecem intactos; .build continua local e ignorado pelo Git.

```sh
make -C gcc-build -j2 all-gcc
python3 transaction_test/verificar_diagnosticos_inventariados.py gcc-build
python3 transaction_test/verificar_regra_armazenamento.py gcc-build
python3 transaction_test/verificar_membro_nao_preparado.py gcc-build
python3 transaction_test/auditar_aceitacao.py gcc-build
```

## Limites e estado

PRIVATE literal continua sendo teste do componente real com mapas isolados,
não uma mutação integrada de tag. O caso de membro não preparado acrescenta
uma dependência declarada após baseline aprovado, sem fabricar chamada física
de assinatura inválida. A regressão LTO não injeta todos os casos em todas as
fases/configurações; as matrizes de fases anteriores têm escopo próprio.

O passo 5 está concluído. Não foram executadas aqui novas suítes DejaGNU
completas ou o fechamento documental do passo 7. O próximo passo previsto
é o 6, regressões das mensagens/seleções afetadas. **A 6.6 permanece aberta.**

Atualização posterior: [passo 6 concluído](OPENMP_TRANSACTION_DIAGNOSTIC_STEP6.md),
com as regressões DejaGNU e transitivas aprovadas no mesmo build.
