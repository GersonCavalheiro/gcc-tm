# Etapa 6.6 — evidências consolidadas

**Estado vigente em 2026-11-11: aceitação aprovada e 6.6 concluída no domínio
experimental documentado.** Ver o
[parecer final](OPENMP_TRANSACTION_ACCEPTANCE_FINAL.md). A consolidação
parcial abaixo e as entradas posteriores registram a sequência histórica.

Consolidação de 2026-11-10. **Aceitação PARCIAL; 6.6 ainda aberta.**
Não foram executados novos testes nesta consolidação. O resultado reúne
evidências existentes, valida seus marcadores e preserva cópias com SHA-256.

## Evidências recentes

| Conjunto | Resultado registrado | Alcance |
| --- | --- | --- |
| LTO negativo checking=0 | 68 recusas esperadas; 137 verificações | 17 mutações × C/C++ × O0/O2; compilação e link checking=0; mutação aplicada, diagnóstico de recusa e ausência de ICE. |
| LTO positivo checking=0 | 53 verificações; 24 execuções | C/C++ × O0/O2 × observe/rebuild/revoke × DSO/archive; aprovação WPA/LTRANS, reconstrução e revogação. |
| Controlador/produtor checking=0 | 41 verificações; 16 execuções | Oito combinações C/C++ × O0/O2 × controller/producer; invalidação e não ressurreição após restaurar IR; DSO/archive. |

Total desses três conjuntos: **231 verificações e 40 execuções**.
As 68 recusas são compilações/links negativos, não execuções de programas.
As verificações incluem construção de plugins e operações heterogêneas;
não representam 231 casos independentes ou uma medida de consumo de cota.

O comando de controlador/produtor compila com sucesso após restaurar o IR:
seu oráculo é invalidação da geração, não recusa definitiva de compilação.
O texto antigo dos resumos lista configurações além da seção executada;
o alcance válido é somente o da tabela acima e dos comandos registrados.

## Evidências históricas

Foram preservados os report.json existentes das baterias anteriores,
incluindo integração (1078 verificações/930 execuções), controlador (225
verificações), ABI v1 (88), chamadas (75), obrigações, inspeção, guardas,
fluxo, status, vida útil, falhas, propriedade e composição.
As contagens e estados declarados estão individualizados no JSON consolidado.
Não somar esses totais aos resultados recentes: têm datas/estados distintos
e podem compartilhar testes. Relatórios históricos não foram reexecutados.

O checkpoint também registra 426/122 de grafo LTO e 543/212 de grafo atual.
Esses totais permanecem declarações históricas do checkpoint, não resultados
novos desta consolidação. Componentes DEPENDENT não concedem aprovação agregada.

## Rastreabilidade V01–V08

| Regras | Evidência recente relacionada | O que ainda impede fechar a regra na 6.6 |
| --- | --- | --- |
| V01–V02 | missing/map/roots e raw, além da cobertura histórica | Revisão integrada da preservação/classificação de obrigações e efeitos. |
| V03–V04 | layout/guard/ssa_use/phi e context; contratos históricos de memória/ABI | Ligação completa à autorização real do runtime e aos casos normativos. |
| V05 | status/channel e positivos | Auditoria dos diagnósticos e ligação ao conjunto integrado de publicação. |
| V06 | channel/entry e invalidação de produtor; chamadas históricas | Consolidação integral de chamadas/capturas/resultados no domínio suportado. |
| V07 | owner/entry e invalidação do controlador; integração histórica | Regressão atual de propriedade, limpeza, commit e retry. |
| V08 | version/mode/envelope/component; rebuild/revoke e invalidação nativa | Demais fases/configurações LTO e regressão checking=2 após alterações dos testes. |

Essa associação identifica evidência relevante; não declara que uma mutação
isolada demonstra toda a regra. A matriz detalhada está no
[levantamento](OPENMP_TRANSACTION_ACCEPTANCE_SURVEY.md).

## Artefatos e reprodução

Snapshot desta consolidação:
[report.json](../transaction_test/.build/acceptance/20261110T113250935665Z/report.json).
O diretório contém cópias dos logs, resumos, relatórios históricos, scripts
e documentos utilizados. Cada arquivo consta do manifesto com origem,
caminho da cópia e SHA-256. Arquivos .build são locais e ignorados pelo Git;
este documento e o script não substituem o arquivamento desse diretório.

Para criar outra consolidação, sem executar baterias:

```sh
python3 transaction_test/consolidar_aceitacao.py
```

O script exige as evidências recentes, confere os marcadores dos 68 negativos,
dos 24 positivos e dos oito casos nativos, preserva os relatórios históricos
e escreve PARTIAL. Erros deixam o diretório marcado INCOMPLETE. O código de
saída de cada processo não está nos logs; a conclusão bem-sucedida dos scripts
foi registrada na sessão. A releitura de logs não substitui essa execução.

Os comandos de teste originais são:

```sh
python3 transaction_test/verificar_grafo_lto.py gcc-build negative 0
python3 transaction_test/verificar_grafo_lto.py gcc-build positive 0
python3 transaction_test/verificar_grafo_atual.py gcc-build controller-checking0
```

## Pendências de aceitação

- Confirmar correspondência entre build e fontes: utilizou-se o build existente.
  HEAD/status e scripts preservados descrevem a consolidação; não demonstram
  retrospectivamente o estado integral dos fontes quando os testes rodaram.
- Executar regressões completas transaction/critical com DejaGNU e preservar
  .sum/.log; o verificador simplificado não cobre dg-final nem critical-*.
- Auditar diagnósticos, casos de mesa e oráculos integrados V01–V08.
- Validar demais configurações LTO relevantes e regressão checking=2.
- Consolidar os resultados finais e atualizar o panorama do projeto.

O domínio continua nativo Linux x86_64, com os limites conservadores já
documentados. Não houve ampliação do suporte de linguagem, tipos ou frames.

## Confirmação posterior do build — 2026-11-10

`make -C gcc-build -j2` concluído com saída zero. A primeira tentativa
falhou no link da libgomp por referências antigas a /usr/src/gcc-tm.
Foram corrigidos somente quatro arquivos gerados no build da libgomp:
config.status, Makefile, libtool e testsuite/Makefile. A repetição integral
terminou sem diagnósticos warning:/error:. Não houve mudança em fontes.

Validação curta: controller_basic.c compilado, ligado e executado em oito
combinações C/C++ × com/sem LTO × DSO/archive, O2 e checking=0. Todas passaram.
ldd confirmou a libgomp local nos quatro executáveis DSO e sua ausência nos
quatro executáveis ligados ao archive. As asserções do fixture ficaram ativas.

Logs, comandos, saídas e hashes de fontes STM, arquivos modificados e binários:
[build-confirmation/report.json](../transaction_test/.build/build-confirmation/report.json).
Essa confirmação é incremental, conforme dependências do make, sem limpeza
integral ou bootstrap. Ela atualiza o estado do build para as próximas
validações; não transforma resultados históricos em testes do build novo.
O snapshot consolidado anterior permanece intacto com sua ressalva original.

## Auditoria posterior de cobertura e diagnóstico

A [auditoria V01–V08](OPENMP_TRANSACTION_ACCEPTANCE_AUDIT.md) reexecutou
17 recusas LTO representativas após a confirmação do build, cinco positivos
(baseline e quatro combinações dos casos normativos), duas provas sintéticas
e a recusa ativa de membro transitivo. Todos os oráculos executados passaram.
Foram confirmadas lacunas de diagnóstico e de contraprovas integradas
específicas; o relatório detalha detector real versus nome da mutação.
Não foi concedida aceitação integral. Os novos resultados estão em
transaction_test/.build/acceptance-audit/, sem alterar o snapshot anterior.


Contraprovas posteriores: 28 recusas e oito controles executados em C/C++,
O0/O2, checking=0/2 (agregado somente C). Demonstram PRIVATE com identidade
trocada, consumo antecipado V05, WRITE derivado retirado e recomposição após
fechar a janela de prova. Ver seção final da auditoria e
transaction_test/.build/integrated-counterproof/report.json. Diagnósticos e
regressões restantes continuam pendentes.

Diagnósticos reproduzidos corrigidos posteriormente: ver
[relatório de diagnósticos](OPENMP_TRANSACTION_DIAGNOSTICS.md).
all-gcc atualizado; 45 recusas verificadas com regra/operação/função e
localização, além de 13 execuções positivas. Os testes passam a exigir esses
campos. Permanecem limites de cadeia intermediária e mensagens antigas,
além das regressões completas; a 6.6 não foi concluída.

Fases LTO ampliadas posteriormente: ver
[aceitação LTO dirigida](OPENMP_TRANSACTION_LTO_ACCEPTANCE.md).
126 casos novos nas sete configurações passaram; com regressões auxiliares,
a rodada soma 167 recusas esperadas e 62 execuções positivas. WPA/LTRANS,
monolítica, incremental, partições e fat foram exercitados. A matriz nova
usa pareamentos explícitos, sem alegação de produto cartesiano completo.

Regressões DejaGNU transaction/critical concluídas: **629 PASS em seis
suítes**, sem resultados inesperados nas execuções aceitas. Foram corrigidas
diretivas e expectativas antigas de cinco testes, sem mudança no compilador
ou runtime. Ver [relatório DejaGNU](OPENMP_TRANSACTION_DEJAGNU.md) e
.build/dejagnu/accepted/report.json. Próximo fechamento: revisão final de
rastreabilidade V01–V08 e dos limites documentados.


Conclusão vigente da revisão documental e estática:
[6.6 não concluída por diagnósticos obrigatórios](OPENMP_TRANSACTION_FINAL_REVIEW.md).
Os resultados positivos preservados permanecem válidos em seus escopos;
a conclusão não os converte em aprovação de requisitos ainda não atendidos.

Após as correções de diagnóstico dos passos 2–3, passaram a matriz do
[passo 5](OPENMP_TRANSACTION_DIAGNOSTIC_STEP5.md) (93 negativos/45 controles)
e as regressões do [passo 6](OPENMP_TRANSACTION_DIAGNOSTIC_STEP6.md)
(629 PASS DejaGNU e 317 verificações/32 execuções transitivas).
Essas são rodadas novas identificadas, sem somar os mesmos testes históricos
como casos independentes. Falta emitir o novo parecer do passo 7; a revisão
anterior e seus achados são históricos, com as correções posteriores registradas.
