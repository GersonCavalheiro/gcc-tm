# Inventário e acompanhamento de obrigações — etapa 6.2

**Atualização 6.4.7 — 2026-11-04:** a [composição local](OPENMP_TRANSACTION_COMPOSITION.md) foi implementada. Pendências históricas referentes à 6.4.7 estão superadas; certificação transitiva e remapeamento seguem na 6.5, e aceitação integral na 6.6.

**Atualização 6.4.6 — 2026-11-04:** a
[prova do protocolo proprietário](OPENMP_TRANSACTION_OWNERSHIP.md) está implementada.
Demonstra admissão, propriedade/vida útil do slot, confirmação, condenação e
limpeza, contador saturante, fatal e retry integral com preparação fresca.
As pendências históricas abaixo relativas à 6.4.6 estão superadas. Composição
segue na 6.4.7, certificação/remapeamento na 6.5 e aceitação integral na 6.6.
Não há COBERTA nem certificado novo; ABI, runtime e layouts preservados.

**Atualização 6.4.5 — 2026-11-04:** a
[prova de falhas e aninhamento](OPENMP_TRANSACTION_FAILURES.md) está implementada.
Exige causa preservada, retorno efetivo de FAIL, contexto emprestado e níveis
balanceados em sucesso; recusa continuação normal e ciclos após falha.
As pendências históricas abaixo relativas à 6.4.5 estão superadas. Controlador
e composição seguem nas 6.4.6–6.4.7; certificação/remapeamento, na 6.5.
Não há COBERTA nem certificado novo.

**Atualização 6.3:** a [inspeção independente](OPENMP_TRANSACTION_INSPECTION.md)
já confronta o inventário com o GIMPLE final e recusa falhas estruturais.
Snapshots operacionais separados duram somente até o fechamento da janela.
As descrições da 6.2 abaixo são históricas quanto à pendência da inspeção;
continuam atuais seus estados, derivações e ausência de certificação completa.

Implementado em 2026-10-31 em `gcc/newtm-obligation.{cc,h}`, conforme o
[contrato da 6.1](OPENMP_TRANSACTION_VERIFICATION.md). A entrega registra trabalho
e tratamentos declarados; não certifica cobertura. A inspeção independente,
as provas de fluxo e a certificação do verificador permanecem nas 6.3–6.6.

## Inventário

`newtm_obligation_begin` abre um escopo com proprietário, papel, identidade e
geração próprios e percorre o GIMPLE real. Funções preparadas são inventariadas
antes do preflight e de reconcile_reads. No modo funcional, cada região original
é inventariada antes de sua substituição pelo controlador.

O percurso registra O01–O07: leituras, escritas, chamadas, registros, identidade,
publicação/retorno e controle. Inclui chamadas internas, argumentos em memória,
destinos de chamada em memória, ponteiros, testemunhos source-only, efeitos não
classificados e entradas por papel/índice dos descritores. Operações sem vínculo
de ocorrência continuam presentes; não são automaticamente consideradas internas.
Cálculo escalar puro não vira artificialmente um acesso STM.

Uma instrução pode gerar várias obrigações. `instance` agrupa suas categorias;
`id` identifica cada obrigação. IDs de operação, origem e grupo são conservados
quando existem; operações sem ocorrência recebem identidade de inventário sem
origem fictícia. Esses IDs são internos à compilação, não identificadores de ABI
ou persistentes entre unidades/LTO.

## Dados e duração

Registros históricos são vetores GC, com proprietário/callees como declarações
e fatos escalares: IDs, escopo, geração, categoria, localização, papel, índice,
presença de campos de descritor, estado de identidade e classificação de
armazenamento. O callee observado permanece separado do alvo declarado pelo
instrumentador. Há ancestrais de cópia, pai/filhas de derivação e produtor de
operações emitidas.

Não são guardados como histórico ponteiros a instruções, descritores, operandos
de acesso ou expressões de origem. O mapa de âncoras GIMPLE só existe enquanto
um escopo está aberto. `newtm_obligation_end` retira essas âncoras antes de
tree-nested/omplower, preservando os registros. Forget/retire também retiram
âncoras sem apagar obrigações. O reset encerra a janela e libera o índice
transitório por operação; resumos não se tornam planos de acesso tardios.

Os fatos de descritor são snapshots, não provas de endereço, registro alcançado
ou guardas. A produção e confirmação das evidências necessárias ao verificador
deverão consumir operandos enquanto válidos nas etapas seguintes.

Foi incluída a declaração de marcação GC de `gimple *&` por `gimple-ssa.h`
nos módulos que geram a marcação do mapa. Isso permite percorrer também os
operandos de instruções destacadas; marcar só a chave não bastaria. As raízes
operacionais são descartadas ao fechar o escopo. Não há transformação SSA nova.

## Tratamento declarado e estados

O instrumentador registra STM, PRIVATE, DYNAMIC, chamada preparada, publicação,
ciclo de vida, emissão interna declarada ou delegação ao auxiliar. Esses rótulos
não são uma classificação independente de segurança.

Os estados de obrigação são PENDENTE, DERIVADA, COBERTA e RECUSADA. COBERTA fica
reservado: nenhuma API desta etapa o emite. Declarar um tratamento mantém a
obrigação pendente; um pai com filhas fica derivado e também não está aprovado.
Não se produzem certificados LOCAL_APROVADO ou GRAFO_APROVADO nesta etapa.

A validade do snapshot é CURRENT, INVALIDATED ou RETIRED. CURRENT apenas indica
associação à geração em análise, não prova semântica. Declarar tratamento não
restaura evidência invalidada. A checagem `newtm_obligation_consistent` confere
integridade do inventário; não autoriza ativar um corpo nem reprova todas as
obrigações pendentes. A barreira funcional atual permanece a da tarefa 5.

## Derivação, reconciliação e recusa

- Cópias agregadas e resultados de chamadas em memória conservam a obrigação
  WRITE original, ligada ao store derivado com identidade própria. O mesmo
  vínculo é mantido para a obrigação de identidade do destino quando presente.
- A reconciliação registra testemunho e leitura correspondente antes de retirar
  o testemunho. O pai fica DERIVADA/RECONCILED; não fica COBERTA. A correspondência
  é declarada após a regra estrutural existente, sem reavaliar a origem.
- O controlador registra delegação do corpo original ao auxiliar e inventaria
  seu código emitido. Retirar o corpo original não descarrega suas obrigações.
- Em recusa do preflight, as obrigações e a causa permanecem RECUSADAS. As
  operações do stub de falha também pertencem ao escopo recusado; sua ausência
  de acessos normais não certifica o corpo original.

As diferenças de GIMPLE dos front-ends são preservadas. A matriz observa stores
derivados reais em C; o fixture C++ pode emitir chamada e store já separados.
As regras de derivação também são exercitadas por IR sintético nos dois front-ends.

## Cópias e invalidação

`gimple_copy` recebe obrigações próprias e evidência invalidada, sem herdar
tratamento ou aprovação. A origem/grupo e o ancestral por operação são mantidos
quando disponíveis. O histórico aponta a revisão corrente inventariada da
instrução original, sem combinar revisões antigas. Cópias anteriores ao
inventário serão descobertas no percurso; cópias sem associação disponível
não recebem prova inventada e continuam sujeitas à inspeção da 6.3.

A invalidação explícita alcança obrigações da mesma instrução, pais/filhas de
derivação e operações dependentes do produtor. Estados afetados voltam a
PENDENTE; RECUSADA não é silenciosamente reaberta. Relações e tratamentos antigos
permanecem apenas como histórico invalidado. Reconstrução exige nova revisão.

Ainda não há observador geral de mutações do IR, inferência de privacidade ou
auditoria pós-remapeamento. A propagação transitiva de certificados entre
chamadores é trabalho da 6.5. Nenhuma dessas tarefas é substituída pelo índice
histórico ou pelas contagens do inventário.

## Consulta e validação

A interface oferece consulta por ID, visão dos fatos históricos, presença de
âncora, integridade e dump solicitado pelo consumidor. Não há saída nova por
padrão nem símbolos novos na libgomp. ABI, layouts e opções permanecem iguais.

`make -C transaction_test check-obrigacoes` compila uma sonda exclusiva dos
testes. Ela inspeciona o inventário após gimplificação, força GC e exercita IR
sintético sem inseri-lo no programa. Confere operações sem vínculo, rótulos sem
COBERTA, derivação, invalidação entre categorias, forget sem apagamento, recusa
persistente e cópia durante/depois da janela, sem herdar evidência válida.
As checagens da sonda são executadas também com `-fchecking=0`.

O programa de teste executa o pragma real com leituras/escritas, funções,
ponteiros, retorno e agregados C. A matriz cobre C/C++, O0/O2, LTO,
DSO/archive, checking 0/2, reconciliação e recusa de testemunho sem leitura.
Os logs e `report.json` ficam em `transaction_test/.build/obligations/`.

Resultados finais: GCC recompilado; obrigações 73 verificações/16 execuções
aprovadas, incluindo quatro configurações funcionais O2 com checking 0 e oito
compilações de recusa em modo de rastreamento com checking 0/2. A aceitação
integrada passou 1303 verificações/1036 execuções. Regressões aprovadas:
clones 209/48 execuções, preparação 37/8, leituras 52/12, escritas 56/12,
chamadas 75/18, ocorrências 63/63, descritores 36/36, subconjunto 201 e ABI 88.
Não foi executada a suíte DejaGNU completa.

Esta entrega não é o teste por mutações do verificador completo da 6.6. A
próxima etapa é a 6.3: confrontar independentemente o inventário e o IR emitido,
sem usar o tratamento declarado como aprovação automática.
