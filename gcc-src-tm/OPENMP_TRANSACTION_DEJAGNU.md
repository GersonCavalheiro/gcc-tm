# Etapa 6.6 — regressões DejaGNU transaction/critical

Execução em 2026-11-10, nativa Linux x86_64. **Seis suítes aprovadas, 629 PASS,
zero FAIL, XPASS, UNRESOLVED, UNTESTED ou ERROR nas execuções aceitas.**
Esses são resultados DejaGNU, não contagens de programas independentes.
As verificações dg-final, diagnósticos, dumps, execução e padrões de saída
previstos em cada teste foram interpretados pelo harness real.

| Suíte | PASS |
| --- | ---: |
| Compilador C — transaction-* | 116 |
| Compilador C++ — transaction-* | 399 |
| libgomp C/C++ — transaction-* | 6 |
| Compilador C — critical-* | 20 |
| Compilador C++ — critical-* | 60 |
| libgomp C/C++ — critical-* | 28 |
| Total | 629 |

Os padrões do compilador usam gomp.exp, incluindo os casos compartilhados
C/C++ e específicos de cada frontend. O C++ executou os modos de linguagem
selecionados pelo harness, incluindo gnu++98, gnu++20 e gnu++26. A libgomp
usou c.exp e c++.exp com transaction-*.c ou critical-*.c. Drivers de outros
alvos podem aparecer no log sem testes selecionados. Não se trata da suíte
GCC completa, nem de outros alvos ou sanitizadores.

## Infraestrutura e biblioteca selecionada

DejaGNU e Expect não estavam instalados. A tentativa de instalação no
sistema exigiu senha de sudo e não foi realizada. Foram baixados os pacotes
Ubuntu dejagnu, expect e tcl-expect e extraídos em /tmp/gcc-tm-dejagnu/root,
sem privilégios administrativos. Foi ajustado o caminho absoluto de
libexpect no pkgIndex.tcl extraído. Versões: DejaGNU 1.6.3, Expect 5.45.4,
Tcl 8.6. Os pacotes foram preservados junto das evidências.

O site.exp gerado do compilador recebeu TEST_ALWAYS_FLAGS com -B para o
diretório local da libgomp, -L e rpath para .libs. Isso permite ao driver não
instalado localizar libgomp.spec no link. LD_LIBRARY_PATH também aponta para
a libgomp local; os logs de runtime confirmam os caminhos do build e os
drivers xgcc/xg++. O ambiente usa OMP_NUM_THREADS=4 e OMP_DYNAMIC=FALSE.

## Correções necessárias nos testes

Não foi alterado código do compilador ou runtime nesta rodada. O DejaGNU
revelou problemas que o verificador simplificado não interpretava:

- transaction-6.c e transaction-7.c tinham duas diretivas dg-* na mesma
  linha, separadas por comentários; o parser recebia tokens de comentário
  como opções. As diretivas foram separadas e as referências de linha ajustadas.
- transaction-occurrence-1.c e transaction-subset-2.c proibiam qualquer STM
  no dump completo. Isso também encontrava os clones preparados, cuja
  existência já faz parte da implementação. O teste agora exige localizar
  main e proíbe STM no corpo original dessa função. A verificação foi validada
  com Tcl contra um dump real com clones e contra o mesmo main com uma
  operação STM injetada: o primeiro é aceito, o segundo é detectado.
- transaction-log-1.C exigia object diretamente na chamada de registro;
  a referência agora é avaliada em object.N_M. O teste exige a carga da
  referência e exatamente um registro usando esse temporário.

As rodadas iniciais com infraestrutura ou expectativas inválidas foram
preservadas com passed=false. A consolidação seleciona somente os resultados
aprovados de cada suíte; não inclui os FAIL iniciais no total final nem os
omite dos arquivos de origem.

## Evidências e reprodução

[report.json](../transaction_test/.build/dejagnu/accepted/report.json) consolida
as seis suítes. O mesmo diretório preserva .sum, .log, saídas do make,
comandos, hashes SHA-256, versões, pacotes e fontes dos testes corrigidos.
A validação do oráculo Tcl está em oracle-validation.tcl/.log. Os artefatos
em .build são locais e ignorados pelo Git; não equivalem a commit ou backup.

Runner novo:

```sh
python3 transaction_test/verificar_dejagnu.py gcc-build /tmp/gcc-tm-dejagnu/root
```

Com DejaGNU/Expect instalados no PATH, basta passar gcc-build. Cada execução
cria diretório próprio, usa TESTSUITEDIR exclusivo para os frontends e copia
os resultados da libgomp antes que a próxima seleção os sobrescreva. Sucesso
exige PASS não vazio, relatórios presentes e ausência de resultados inesperados;
o retorno zero do make sozinho não concede aprovação.

## Estado da 6.6

A pendência das regressões DejaGNU transaction/critical está concluída no
domínio acima. Falta a revisão final de rastreabilidade V01–V08 e dos limites
documentados antes de declarar aceitação integral. Permanecem as ressalvas
das matrizes LTO dirigidas e das mensagens antigas fora da auditoria.
