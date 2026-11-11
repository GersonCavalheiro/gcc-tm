# Correção de diagnósticos — passo 1: inventário estático

Data: 2026-11-10. **Passo 1 concluído; nenhuma correção de produção ou bateria
executada.** Foram encontrados 36 pontos de emissão em dez funções de
newtm-clone.cc, com 33 mensagens distintas: 35 pontos sem contexto estruturado
e um com contexto parcial. A classificação é proposta para implementação,
não resultado de uma nova execução negativa.

O escopo é o caminho atual de current_body e newtm_clone_verify_current,
incluindo controlador e check_offset_arithmetic. As cinco famílias de
componentes delegados são registradas separadamente; o inventário não pretende
listar todas as mensagens do parser, preflight histórico ou do GCC inteiro.

Fonte completa, mensagens e metadados por ponto:
[diagnostic_inventory.json](../transaction_test/diagnostic_inventory.json).
O arquivo inclui SHA-256 do fonte, variável de escopo disponível e notas sobre
condições que precisam ser separadas. IDs D01–D36 identificam os pontos;
linhas são localizadores desta revisão, não identidades persistentes.

## Caminho e classificação

```
newtm_current_verify
  -> newtm_clone_verify_current
       -> current_body
            -> assinatura / operandos / chamadas / contexto
            -> registro / memória -> aritmética de offset
            -> memória / canais / status / cobertura (componentes)
       -> check_lowered_controller
            -> operandos / origem de captura
            -> canais / status / propriedade / cobertura da entrada
       -> resultados de entrada e produtores de outlining
```

| IDs | Função | Pontos | Classificação principal |
| --- | --- | ---: | --- |
| D01–D01 | check_prepared_signature | 1 | V06 |
| D02–D02 | check_remapped_operands | 1 | V03 |
| D03–D07 | check_lowered_controller | 5 | V08, CONDITIONAL, V06, V07 |
| D08–D08 | check_lowered_context | 1 | V06 |
| D09–D11 | check_lowered_calls | 3 | V06 |
| D12–D15 | check_lowered_registrations | 4 | V02, V06, V03 |
| D16–D22 | check_offset_arithmetic | 7 | V03 |
| D23–D29 | check_lowered_memory | 7 | V08, V02, V06, V01, V03 |
| D30–D32 | current_body | 3 | V08 |
| D33–D36 | newtm_clone_verify_current | 4 | V07, V06, V08 |

## Pontos individuais

| ID | Linha | Regra proposta | Operação/categoria | Estado |
| --- | ---: | --- | --- | --- |
| D01 | [newtm-clone.cc:527](gcc/newtm-clone.cc#L527) | V06 | prepared signature | antigo |
| D02 | [newtm-clone.cc:574](gcc/newtm-clone.cc#L574) | V03 | frame operand | antigo |
| D03 | [newtm-clone.cc:610](gcc/newtm-clone.cc#L610) | V08 | controller snapshot | antigo |
| D04 | [newtm-clone.cc:613](gcc/newtm-clone.cc#L613) | CONDITIONAL | controller owner / prepared signature | antigo |
| D05 | [newtm-clone.cc:615](gcc/newtm-clone.cc#L615) | V06 | capture origin | antigo |
| D06 | [newtm-clone.cc:625](gcc/newtm-clone.cc#L625) | V08 | controller proof freshness | antigo |
| D07 | [newtm-clone.cc:631](gcc/newtm-clone.cc#L631) | V07 | unique body invocation | antigo |
| D08 | [newtm-clone.cc:807](gcc/newtm-clone.cc#L807) | V06 | borrowed context call | parcial |
| D09 | [newtm-clone.cc:823](gcc/newtm-clone.cc#L823) | V06 | prepared call | antigo |
| D10 | [newtm-clone.cc:837](gcc/newtm-clone.cc#L837) | V06 | prepared call | antigo |
| D11 | [newtm-clone.cc:840](gcc/newtm-clone.cc#L840) | V06 | prepared call | antigo |
| D12 | [newtm-clone.cc:863](gcc/newtm-clone.cc#L863) | V02 | REGISTER CFG inspection | antigo |
| D13 | [newtm-clone.cc:873](gcc/newtm-clone.cc#L873) | V06 | REGISTER service signature | antigo |
| D14 | [newtm-clone.cc:890](gcc/newtm-clone.cc#L890) | V03 | REGISTER | antigo |
| D15 | [newtm-clone.cc:892](gcc/newtm-clone.cc#L892) | V03 | REGISTER | antigo |
| D16 | [newtm-clone.cc:903](gcc/newtm-clone.cc#L903) | V03 | offset arithmetic | antigo |
| D17 | [newtm-clone.cc:915](gcc/newtm-clone.cc#L915) | V03 | offset arithmetic | antigo |
| D18 | [newtm-clone.cc:922](gcc/newtm-clone.cc#L922) | V03 | offset arithmetic | antigo |
| D19 | [newtm-clone.cc:929](gcc/newtm-clone.cc#L929) | V03 | offset arithmetic | antigo |
| D20 | [newtm-clone.cc:944](gcc/newtm-clone.cc#L944) | V03 | offset arithmetic | antigo |
| D21 | [newtm-clone.cc:949](gcc/newtm-clone.cc#L949) | V03 | offset arithmetic | antigo |
| D22 | [newtm-clone.cc:966](gcc/newtm-clone.cc#L966) | V03 | offset arithmetic | antigo |
| D23 | [newtm-clone.cc:976](gcc/newtm-clone.cc#L976) | V08 | preparation summary | antigo |
| D24 | [newtm-clone.cc:980](gcc/newtm-clone.cc#L980) | V02 | READ/WRITE CFG inspection | antigo |
| D25 | [newtm-clone.cc:992](gcc/newtm-clone.cc#L992) | V06 | READ/WRITE service signature | antigo |
| D26 | [newtm-clone.cc:998](gcc/newtm-clone.cc#L998) | V01 | required memory operation | antigo |
| D27 | [newtm-clone.cc:1008](gcc/newtm-clone.cc#L1008) | V03 | READ/WRITE | antigo |
| D28 | [newtm-clone.cc:1027](gcc/newtm-clone.cc#L1027) | V03 | READ/WRITE | antigo |
| D29 | [newtm-clone.cc:1034](gcc/newtm-clone.cc#L1034) | V03 | READ/WRITE | antigo |
| D30 | [newtm-clone.cc:2704](gcc/newtm-clone.cc#L2704) | V08 | current body proof | antigo |
| D31 | [newtm-clone.cc:2720](gcc/newtm-clone.cc#L2720) | V08 | current body proof | antigo |
| D32 | [newtm-clone.cc:2726](gcc/newtm-clone.cc#L2726) | V08 | current body proof | antigo |
| D33 | [newtm-clone.cc:2738](gcc/newtm-clone.cc#L2738) | V07 | source controller ownership | antigo |
| D34 | [newtm-clone.cc:2746](gcc/newtm-clone.cc#L2746) | V06 | prepared graph member | antigo |
| D35 | [newtm-clone.cc:2757](gcc/newtm-clone.cc#L2757) | V08 | current graph proof | antigo |
| D36 | [newtm-clone.cc:2774](gcc/newtm-clone.cc#L2774) | V08 | current graph proof | antigo |

As referências de linha servem à leitura do fonte e também estão no JSON;
o renderer local pode abrir o arquivo sem posicionar na âncora.

## Decisões necessárias antes da alteração

- **D04, controlador:** separar propriedade/owner (V07) de assinatura da
  invocação (V06); o erro atual reúne as duas condições com OR.
- **D02, operando remapeado:** V03 para parâmetro de outro frame ou campo
  estranho ao tipo atual. Relação com canais/capturas não torna toda essa
  análise automaticamente V06.
- **D12/D24, CFG não inspecionável:** V02 pela impossibilidade de classificar
  operações reais; preservar a causa do builder, sem alegar acesso inválido
  específico que ainda não foi demonstrado.
- **D13/D25, número de operandos de serviço:** V06 pela assinatura do serviço;
  identidade/faixa/kind dos operandos são checagens posteriores V03.
- **D16–D22, aritmética:** manter os sete locais, inclusive os quatro que
  compartilham texto. Profundidade excedida é limite de prova conservador,
  não confirmação de overflow em execução.
- **D23/D30/D31/D32/D35/D36, evidência atual:** V08 para snapshot, freshness e
  resultado ausente. Informar qual componente/produção falta, sem rotular
  toda ausência como uma falha semântica específica de V02, V06 ou V07.
- **D34, membro recusado:** V06 pelo fechamento do grafo; conservar também
  a causa de preflight registrada quando existir, sem substituí-la por uma
  frase genérica de dependência.

## Contexto existente e lacunas

D08 já tem regra V06, operação, função e fallback de localização, mas não usa
a nota comum de raiz/controlador. Os outros 35 pontos precisam da apresentação
estruturada. Os campos fn, helper, node->decl, root, parent e self->owner
estão disponíveis nos respectivos locais; não é necessário consultar
um descritor retirado para identificar o escopo.

Para locais que usam gimple_location, é necessário verificar UNKNOWN_LOCATION
e usar a declaração da função como fallback. Onde a operação desapareceu,
a categoria da obrigação deve ser informada sem fabricar uma instrução atual.

newtm_current_verify não completa mensagens antigas já emitidas: seu fallback
só roda quando !seen_error(). A mensagem genérica de composição tem contexto
V08, porém não deve substituir a causa específica dos verificadores.

## Componentes delegados e fronteiras

Memória, canais, status, propriedade e cobertura já usam
newtm_current_diagnose. A cobertura de corpo e controlador não é intercambiável:
propriedade e cobertura de entrada continuam obrigatórias no agregado.
A falha PRIVATE de resolver::ranges que chega a fail com padrão V03 permanece
identificada para o **passo 3**, sem alteração neste inventário.

Há retornos internos sem emissão própria: argumentos inválidos da API,
função sem CFG e produtor inválido. Os predicados puros (assinatura de chamada,
origem de captura, registro e identidade) transferem a causa ao seu chamador.
Alguns casos sem CFG são precedidos por um snapshot que já falha; não criar
um teste com IR malformado apenas para forçar cada retorno defensivo.
No passo 2, verificar a emissão única pelo chamador/fallback antes de decidir
se esses retornos precisam de mensagem própria.

record_capture_origins e require_prepared_graph têm recusas de preparação
adjacentes, anteriores ao fechamento atual. Não foram incluídas nos 36 pontos.
Se a correção envolver sua apresentação, declarar essa ampliação de escopo
explicitamente, em vez de alegar que este inventário já cobre o parser inteiro.

## Entrega para o passo 2

Usar os IDs e a associação de regra/categoria para uniformizar os diagnósticos,
separando D04 e preservando causas específicas. Depois preparar negativos
que exercitem os caminhos corretos, com controle válido e checking=0/2.
Este documento não comprova alcançabilidade por um fixture válido para cada
ramo; essa é uma tarefa dos passos de preparação/execução dos negativos.
A 6.6 permanece aberta.
