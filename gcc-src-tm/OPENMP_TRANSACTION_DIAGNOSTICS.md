# Etapa 6.6 — diagnósticos das provas atuais

Atualização de 2026-11-10. Corrigidas as lacunas reproduzidas nas recusas
de memória, canais, status, propriedade, cobertura, composição e contratos
LTO. As causas originais permanecem no erro; notas acrescentam regra,
espécie de operação e declaração da função. Instruções sem localização usam
a localização da função. A composição informa também raiz e controlador.

Exemplo real após a mudança:

```text
integrated_counterproof.c:5:12: error: transaction transformed status proof failed: V05: result or binding consumed before its own call succeeded
integrated_counterproof.c:5:12: note: transaction rule V05, operation assignment, function ‘relay’
integrated_counterproof.c:5:12: note: prepared scope ‘relay.newtm.v1’
integrated_counterproof.c:5:12: note: while verifying transaction root ‘main.newtm.body.3149.newtm.v1’
integrated_counterproof.c:8:5: note: transaction controller ‘main’
```

O número no nome interno pode variar. A leitura residual LTO (raw), antes
sem localização, passou a indicar lto_graph.c:6:12, função recurse, regra V02,
operação assignment, raiz e controlador. Não são impressos valores do programa
nem consultados descritores retirados para compor as notas.

## Classificação e implementação

- Memória: V03 para identidade/faixa; V04 para acesso PRIVATE que perdeu
  sua instância; V01 para operação obrigatória ausente ou com papel alterado.
- Canais: V06. Status: V05, preservando a regra mais específica contida na
  causa quando a análise compartilhada reporta V07, por exemplo.
- Propriedade: V07; cobertura residual: V02; composição fora da janela: V08.
- Contratos LTO: V08 por padrão; assinatura/chamador usam V06, raiz de entrada
  ausente usa V01 e invocação do controlador ausente usa V07.

newtm_current_diagnose centraliza as cinco famílias de componentes e observa
a raiz apenas durante a verificação agregada. newtm-lto.cc preserva os textos
de recusa e acrescenta contexto declarativo. A recusa de contexto emprestado
em newtm-clone.cc também recebeu regra/operação/função e fallback de localização.
Não houve mudança nos critérios de aceitação, layouts, ABI ou runtime.

## Validação executada

- all-gcc incremental: saída zero, sem warning:/error:; self-tests internos
  continuam desabilitados nesta configuração.
- Auditoria LTO dirigida: 17 mutações recusadas pelo detector específico,
  todas com regra/operação/função e localização; controle sem mutação e quatro
  positivos dos casos normativos aprovados e executados.
- Contraprovas nativas: 28 recusas e oito controles aprovados/executados,
  C/C++, O0/O2, checking=0/2; agregado somente C. Todas as recusas passaram
  na checagem de contexto. PRIVATE/V04, consumo antecipado/V05, WRITE/V01 e
  janela encerrada/V08 foram também confrontados com a regra específica.

Total dirigido: **45 recusas verificadas e 13 execuções positivas**.
Os runners agora exigem contexto e localização, além do detector e dos
oráculos funcionais anteriores. Não foram executadas as suítes completas.

Comandos:

```sh
make -C gcc-build -j2 all-gcc
python3 transaction_test/auditar_aceitacao.py gcc-build
python3 transaction_test/verificar_contraprovas_integradas.py gcc-build
```

Evidência preservada: [report.json](../transaction_test/.build/diagnostics/report.json),
relatórios de cada runner, logs, build.log e manifesto SHA-256 no mesmo
diretório. O estado anterior nos relatórios históricos não é reescrito.

## Limites e trabalho restante

O contexto raiz/controlador/membro não reconstrói toda cadeia intermediária
de chamadas ou ancestrais de descritores expirados. As demais recusas antigas
de preflight/remapeamento não foram todas convertidas para a apresentação
nova. Portanto estes resultados fecham as lacunas reproduzidas, sem declarar
conformidade de todas as mensagens possíveis do compilador.
A regressão DejaGNU e as configurações LTO restantes continuam necessárias
antes de concluir a 6.6.
