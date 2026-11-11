# Etapa 6.6 — contraprovas nas fases e configurações LTO

Execução de 2026-11-10 concluída. **Todos os oráculos desta rodada passaram;
a 6.6 continua aberta.** Não houve alteração do compilador/runtime nesta
rodada, somente plugin, runner, alvo de teste e documentação.

## Matriz nova

126 casos: **98 recusas dirigidas e 28 controles válidos**, mais quatro
execuções do fallback nativo dos objetos fat. C/C++, agregado somente C.
Pareamentos dirigidos: O0/checking=0/DSO e O2/checking=2/archive. Não é o
produto cartesiano completo de otimização, checking e forma de ligação.

| Configuração | Fase escolhida para a mutação | Casos novos |
| --- | --- | --- |
| balanced com alvo WPA | WPA | 18 |
| balanced | LTRANS | 18 |
| max | LTRANS | 18 |
| 1to1 | LTRANS | 18 |
| none/monolítica | MONOLITHIC | 18 |
| incremental -r -flinker-output=rel | INCREMENTAL | 18 |
| objetos fat, balanced | LTRANS | 18 |

Em cada configuração são exercitados PRIVATE com acesso redirecionado (V04),
consumo antecipado de buffer READ (V05), WRITE de agregado retirado (V01,
somente C) e tentativa de recomposição após fechar a janela (V08).
Totais por mutação: 28 PRIVATE, 28 consumo antecipado, 14 WRITE e 28 retirada.

O plugin exige primeiro aprovação reconstruída na fase real, com proved=31
e dependências zero. Só então altera o SSA e invoca newtm_current_verify,
o verificador agregado de produção. Não chama um verificador isolado de
componente. Exige recusa atômica, proved=0, detector esperado, diagnóstico
com contexto, falha do link e ausência do executável/objeto de saída.
Na retirada encerra-se a janela sem alterar instruções/operandos antes da
tentativa de recomposição. Não se injeta um ponteiro de descritor expirado.

Os controles válidos precisam observar reconstrução na fase selecionada,
revogação antes da transformação seguinte e executar corretamente. O controle
incremental liga também o objeto intermediário em um executável final,
observando novas provas WPA/LTRANS. O objeto fat é executado tanto por LTO
quanto pelo fallback nativo.

## Regressões adicionais executadas

- 68 negativos antigos com checking=2: C/C++ × O0/O2 × 17 mutações,
  compilação e link checking=2; todos recusados sem ICE.
- units: 47 verificações/20 execuções; múltiplas unidades, partições,
  reconstrução monolítica/incremental e DSO/archive.
- format: 20 verificações/6 execuções; fat, raízes inativas e recusa da versão
  16.0 por este formato 16.1. Inclui uma recusa esperada de versão incompatível.
- templates: 11 verificações/4 execuções; templates/COMDAT, O0/O2 e DSO/archive.

Total da rodada, sem somar execuções históricas: **167 recusas esperadas e
62 execuções positivas**. A matriz nova contribui 98/32, os negativos antigos
68/0 e as suítes auxiliares 1/30. Contagens de verificações/subprocessos não
são contagens de casos independentes.

## Correções dos oráculos durante a preparação

Encerrar a janela LTO restaura atributos e opções de otimização. Por isso,
o snapshot completo da declaração pode mudar legitimamente. O teste de
retirada compara a sequência das instruções e seus operandos, em vez de
tratar a restauração de opções como corrupção do IR.

Partições max/1to1 podem conter apenas código comum e nenhuma raiz de prova.
O observador aceita essas unidades sem lhes atribuir certificado. Ainda
exige aprovação e mutação na unidade que realmente contém a transação.
O primeiro relatório da matriz ampliada ficou marcado passed=false por essa
suposição do observador; a matriz corrigida foi repetida e passou. Não foi
necessária correção na implementação de produção.

## Evidências e reprodução

Consolidação: [report.json](../transaction_test/.build/lto-phase-acceptance/report.json).
Contém os casos por fase, alcance, limitações e manifesto SHA-256. O mesmo
diretório preserva logs, relatórios das execuções aprovadas, plugins compilados,
fontes dos testes e resumos. Os arquivos .build são locais, ignorados pelo Git.

Comandos da rodada:

```sh
python3 transaction_test/verificar_contraprovas_lto.py gcc-build monolithic
python3 transaction_test/verificar_contraprovas_lto.py gcc-build wpa balanced max 1to1 incremental fat
python3 transaction_test/verificar_grafo_lto.py gcc-build negative 2
python3 transaction_test/verificar_grafo_lto.py gcc-build units 2
python3 transaction_test/verificar_grafo_lto.py gcc-build format 2
python3 transaction_test/verificar_grafo_lto.py gcc-build templates 2
```

`make -C transaction_test check-contraprovas-lto` executa a matriz nova inteira
em um diretório com identificador próprio, sem sobrescrever a rodada anterior.

## Limites restantes

- Não foi executado o produto cartesiano completo O0/O2 × checking0/2 ×
  DSO/archive para cada caso novo. Os pareamentos acima são explícitos.
- As mutações novas são posteriores à reconstrução na fase escolhida e
  exigem nova composição. Não houve injeção direta antes da primeira
  reconstrução de um arquivo LTRANS isolado. Os negativos de streaming
  anteriores continuam cobrindo o caminho de reconstrução a partir do frontend.
- O domínio permanece conservador, nativo Linux x86_64, sem ampliação de
  suporte a tipos, chamadas ou SSA arbitrariamente otimizado.
- Faltam as regressões DejaGNU completas transaction/critical e a revisão
  final das evidências V01–V08. As ressalvas sobre mensagens antigas e cadeia
  intermediária de diagnósticos também permanecem documentadas.
