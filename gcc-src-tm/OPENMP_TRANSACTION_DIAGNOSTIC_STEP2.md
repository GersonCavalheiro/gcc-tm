# Correção de diagnósticos — passo 2 concluído

Os 36 IDs do [inventário](OPENMP_TRANSACTION_DIAGNOSTIC_INVENTORY.md) foram
uniformizados em newtm-clone.cc. Agora há 38 emissores, pois duas condições
combinadas foram separadas. A migração verificável está em
[diagnostic_inventory_step2.json](../transaction_test/diagnostic_inventory_step2.json),
com IDs anteriores, linhas atuais, regras, categorias e hashes dos fontes.

## Alterações

- Todos os emissores inventariados usam fallback de localização para a função
  e o contexto comum de regra/operação/função. As causas anteriores foram
  preservadas; as mensagens combinadas ganharam a distinção necessária.
- D04 separa propriedade do controlador (V07) de assinatura da invocação (V06).
- D35 distingue ausência de OWNER de ausência de cobertura da entrada (V08).
  D31 identifica o componente de corpo ausente. Ausência de evidência não
  é apresentada como uma falha semântica que ainda não foi demonstrada.
- D34 conserva a causa de rejeição de preflight quando disponível.
- D08 usa o contexto comum em vez da nota avulsa anterior.
- O contexto agregado mostra raiz/controlador e pode reconstruir uma cadeia
  de chamadas a partir dos CFGs atuais. O percurso trata ciclos e não usa
  descritores retirados, valores do programa ou reavaliação de expressões.

newtm_current_diagnostic_location e newtm_current_diagnostic_context são
interfaces internas do compilador. newtm_current_diagnose reutiliza-as,
mantendo o formato e a causa das recusas dos componentes já convertidos.
Não houve alteração da ABI/runtime nem ampliação do domínio de aceitação.

## Validação concluída

- all-gcc incremental concluído, sem warning:/error: no log final. Os
  self-tests internos continuam desabilitados na configuração existente.
- Conferência estática dos dez blocos de função: os 36 IDs antigos estão
  associados aos 38 emissores atuais, todos com contexto e fallback.
- 32 negativos dirigidos: oito mutações × C/C++ × checking=0/2, O0. Foram
  verificados detector específico, regra esperada, operação, função,
  localização, ausência de ICE e ausência de objeto de saída.
- 20 controles dos cinco fixtures correspondentes foram compilados, ligados
  e executados com a libgomp local.
- Regressão LTO dirigida: 17 recusas, controle sem mutação e quatro positivos
  dos casos normativos aprovados/executados, usando o novo build.

Total dirigido: **49 recusas verificadas e 25 execuções positivas**.
Isso não significa que os 38 emissores defensivos tenham sido executados
individualmente. O inventário completo tem conferência estática; a cobertura
dinâmica desta etapa é representativa e está explicitada nos comandos.

Exemplos confirmados: controller_channels reporta V06 e assinatura incompatível;
controller_owner reporta V07 e perda de proprietário; parameter_owner informa
V06 e o caminho raiz → recurse.newtm.v1 → other.newtm.v1. O raw LTO também
conserva seu detector V02 e agora mostra a chamada atual até recurse.

## Reprodução e evidências

```sh
make -C gcc-build -j2 all-gcc
python3 transaction_test/verificar_diagnosticos_inventariados.py gcc-build
python3 transaction_test/auditar_aceitacao.py gcc-build
```

Consolidação: [report.json](../transaction_test/.build/diagnostic-step2/report.json).
O diretório preserva logs, comandos, códigos de saída, relatórios por runner,
fontes alterados, migração e hashes SHA-256. Os snapshots anteriores não foram
reescritos. Os artefatos .build continuam locais e ignorados pelo Git.

## Limites e próximo passo

O passo 3 permanece pendente: resolver::ranges ainda pode propagar uma causa
PRIVATE para fail com o padrão V03. Esta alteração não resolveu nem mascarou
essa classificação. A cadeia nova descreve chamadas presentes no CFG; não
fabrica uma cadeia quando a aresta não existe nem ancestrais de descritores
expirados. As recusas de preparação adjacentes, fora dos 36 IDs inventariados,
não foram convertidas indiscriminadamente.

Esta etapa não conclui a 6.6, não executa a matriz completa dos passos seguintes
e não substitui o parecer final após a correção da classificação PRIVATE.
