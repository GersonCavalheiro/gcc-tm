# Etapa 5.3 — escritas da visão privada

Implementação em `gcc/newtm-clone.cc`, usando a ABI existente
`GOMP_transaction_stm_v1_write`. Não houve mudança do runtime, dos layouts
compartilhados ou do versionamento GOMP_NEWTM_2.0 nesta etapa. O padrão conserva
rastreamento; a [etapa 5.5](OPENMP_TRANSACTION_CONTROLLER.md) ativa o controlador
por `-fopenmp -fopenmp-transaction-stm` e acrescenta somente o serviço fatal em 2.1.

Decisões aprovadas pelo usuário: materializar valores em temporários privados
tipados, percorrer escritas numa segunda caminhada dentro da mesma janela e
manter operações read-modify-write como leitura checada, cálculo e escrita
checada. Não foi criada uma operação combinada nova na ABI.

## Caminhadas e janela dos descritores

Depois do preflight, a primeira caminhada transforma leituras e chamadas. Ela
pode dividir uma atribuição agregada em leitura mais store ou destacar o store
de resultado de chamada. Esses stores conservam descritor WRITE para o mesmo
lvalue avaliado e uma ocorrência nova, mantendo origem/grupo.

A segunda caminhada trata as escritas originais e essas escritas derivadas.
Ela roda antes de `newtm_access_end`, na mesma ativação, sem otimização nem
remapeamento entre caminhadas. Não reabre a janela e não consulta descritores
após tree-nested ou omplower. As cópias GIMPLE gerais continuam stale.

Guardas de endereço foram compartilhadas entre os emissores de leitura/escrita:
primeiro verificar validade estrutural/de faixa, depois formar o endereço lógico
da referência GIMPLE e compará-lo com objeto+offset por aritmética inteira de
largura suficiente. A validação anterior garante ausência de overflow.
Índices, endereços e argumentos já avaliados não são executados novamente.

## Valor escrito, armazenamento e status

Para uma escrita registrada, copiar o RHS GIMPLE atual para um valor privado
tipado e daí para um buffer privado endereçável. Isso serve tanto para escalares
quanto para agregados. O RHS não é reconstruído a partir do plano diagnóstico.
Uma leitura preexistente no RHS já foi checada na primeira caminhada; cópias
privadas restantes não voltam a ler memória comum.

O buffer não se sobrepõe ao objeto destino, a bindings ou a armazenamento do
runtime e permanece vivo até a chamada retornar. O runtime copia seus bytes;
não conserva um ponteiro para o temporário da função. Buffers distintos também
resolvem o caso em que origem e destino representam o mesmo objeto registrado.

| Classificação | Emissão |
|---|---|
| REGISTERED | write(ctx, objeto, offset, buffer, tamanho), seguida de verificação de status. Registro já alcançado continua obrigatório. |
| PRIVATE | Store direto após validações, sem registro ou chamada de escrita STM. |
| DYNAMIC | Validar a tag e escolher store privado ou chamada da ABI. |
| REJECTED | Recusar, sem acesso comum alternativo. |

Qualquer status não OK segue ao epílogo comum. Ele retém a causa efetiva via
fail e retorna, antes de executar operações seguintes ou publicar a saída da
função. INVALID domina conflito. Os clones continuam recebendo contexto
emprestado: não confirmam, descartam ou consomem o slot externo em falha.

O store original registrado é removido e seus metadados são retirados
imediatamente. Stores criados pelo próprio compilador para buffers/resultados
ABI não são operações do programa e não recebem instrumentação recursiva.
Stores diretos do caminho DYNAMIC privado dependem da tag validada.

Não há inicialização fictícia de resultados. Uma operação como `(*p)++` só
entrega o valor anterior ao chamador se a escrita tiver sucesso. Em falha,
saídas escalares/agregadas permanecem preservadas. Locais de trabalho privados
podem ter sido modificados dentro da tentativa; o controle externo deve
descartar a tentativa inteira, sem consumir seus resultados.

## Interação com leituras, ponteiros e publicação

Leituras seguintes observam as escritas privadas no mesmo contexto, incluindo
aninhamento. Uma atribuição a ponteiro registrado modifica seu valor privado;
o próximo acesso obtém esse valor pela leitura instrumentada e valida o endereço
lógico contra sua identidade. Objeto ponteiro e apontado conservam canais
distintos. As restrições existentes de identidade, tipos, vida útil e escape de
endereços privados permanecem vigentes.

Antes da confirmação, as escritas registradas não alteram a memória original.
Descarte remove todas as alterações privadas, inclusive as de operações que
sucederam antes de uma falha posterior. Confirmação publica pelo runtime somente
após suas validações; o compilador não publica parcialmente ao executar stores.
Essas garantias referem-se aos acessos preparados instrumentados e ao protocolo
de confirmação existente, não à ativação automática do pragma.

Os testes de preparação voltaram a incluir atribuição ao ponteiro capturado
seguida de leitura do apontado e read-modify-write de outro objeto. Verificam
valores pela ABI antes de confirmar e só então esperam alterações na memória
original. A regressão de leitura também verifica sua escrita derivada pelo
serviço emitido, pois o descritor WRITE já foi consumido na segunda caminhada.

## Validação permanente

`make -C transaction_test check-escritas` usa plugin exclusivo de teste para
chamar clones dormentes e verificar chamadas emitidas. A matriz cobre C/C++,
O0/O2, ligação dinâmica/archive e LTO, debug/checking e GC forçado, incluindo:

- escrita escalar, leitura após escrita e confirmação/descarte explícitos;
- objetos privados, referências C++ privadas e ausência de chamadas STM nesses caminhos;
- incremento com resultado preservado em falha, índice incrementado uma vez e laços;
- stores derivados de chamadas e de leituras agregadas;
- agregados C, subcampos, origem já modificada na visão privada e alias origem/destino;
- falha na segunda escrita, erros permanentes e status desconhecido sem publicação parcial;
- ausência de registro, tag desconhecida, limites negativos e escape privado recusado;
- destino original PROT_NONE: escrita sem leitura/publicação antecipada, seguida de descarte ou confirmação;
- conflito real de promoção READ para WRITE após um prefixo já modificado privadamente.

O teste de conflito descarta pela fronteira pública, encerra a concorrente e
invoca novamente o corpo inteiro. Confere reexecução do prefixo e resultado
final. Essa repetição é explícita no teste, sem alegar retry gerado pela 5.5.
A sonda exige que operações vinculadas remanescentes tenham armazenamento
PRIVATE provado e observa serviços WRITE para os dois tipos de stores derivados.
Isso não substitui a verificação de cobertura integral da etapa 6.

Resultados finais: GCC recompilado; escritas 56 verificações/12 execuções,
preparação 37/8, leituras 52/12 e clones 209/48, todos aprovados. Passaram também
ocorrências 63/63, descritores 36/36, subconjunto 201 e ABI 88. Os três casos
novos GCC foram compilados diretamente e seus dumps conferidos; DejaGNU não
foi executado por essa matriz. Não houve ampliação para outros targets ou offload.
