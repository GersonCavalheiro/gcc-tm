# Correção de diagnósticos — passo 6 concluído

Data: 2026-11-11. **Regressões afetadas aprovadas no build do passo 5.**
Os hashes dos binários foram conferidos antes e depois da execução, sem
divergências. Não houve mudança de produção ou de testes nesta etapa.

## DejaGNU real

Foram repetidas as seis seleções transaction/critical, com a libgomp local:

| Seleção | PASS |
| --- | ---: |
| C — transaction-* | 116 |
| C++ — transaction-* | 399 |
| libgomp C/C++ — transaction-* | 6 |
| C — critical-* | 20 |
| C++ — critical-* | 60 |
| libgomp C/C++ — critical-* | 28 |
| Total | 629 |

Não houve FAIL, XPASS, UNRESOLVED, UNTESTED ou ERROR nas suítes aprovadas.
As verificações dg-final e as expectativas de diagnóstico foram interpretadas
pelo DejaGNU, incluindo os testes corrigidos na rodada anterior. Os valores
são resultados do harness, não 629 programas independentes.

DejaGNU/Expect continuam sendo usados por extração local, sem nova instalação
ou alteração do sistema. Os logs preservam versões, comandos e seleção do
build local. O ambiente usa quatro threads e OMP_DYNAMIC=FALSE.

## Regressão transitiva ampliada

verificar_transitivo.py concluiu **317 verificações e 32 execuções positivas**,
C/C++ e checking=0/2. Essa seleção amplia os negativos representativos do
passo 5 com as expectativas existentes para chamadas, assinaturas, contexto,
registros, aritmética de offset, identidade, capturas e controlador.

Também foram exercitados cópias equivalentes, fronteira LTO, recursão,
invalidação, revalidação histórica e mudança de assinatura. Os textos de causa
continuam compatíveis com os oráculos existentes após acrescentar as notas
estruturadas. Foram conferidos os 317 logs da rodada, sem ICE.

O grafo histórico continua sem autoridade para aprovar o IR atual: esta
regressão não substitui as contraprovas e certificados atuais do passo 5.
Os testes específicos dos campos novos já foram executados naquele passo;
esta rodada verifica compatibilidade das seleções afetadas, sem repetir
indiscriminadamente memória/status ou toda a matriz de fases LTO.

## Evidências e reprodução

[report.json](../transaction_test/.build/diagnostic-step6/report.json) consolida
as duas execuções, com **348 artefatos preservados** e SHA-256. Inclui .sum,
.log, saídas do make, versões, resumos, fontes dos runners/plugins e fontes
de produção pertinentes. Os relatórios do passo 5 permanecem separados.
Os arquivos .build continuam locais e ignorados pelo Git.

```sh
python3 transaction_test/verificar_dejagnu.py gcc-build /tmp/gcc-tm-dejagnu/root
python3 transaction_test/verificar_transitivo.py gcc-build
```

## Estado e limites

O passo 6 está concluído. As seleções DejaGNU são transaction/critical dos
drivers gomp de C/C++ e dos drivers C/C++ da libgomp; não são a suíte GCC
completa ou outros alvos. A regressão transitiva conserva seu domínio próprio.
Não foi executado novo produto cartesiano completo de opções/fases LTO.

**A 6.6 não foi encerrada nesta etapa.** Próximo trabalho: passo 7, revisão
final das evidências V01–V08, atualização do parecer e decisão explícita
de aceitação no domínio documentado.
