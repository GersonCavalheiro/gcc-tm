# Status no CFG remapeado — bloco 4 da 6.5.3

Atualização do bloco 7: a [composição atual do grafo](OPENMP_TRANSACTION_CURRENT_GRAPH.md)
publica aprovação agregada somente na nova API e na janela pré-SSA do frontend.
Este componente ou certificado histórico conserva seu contrato próprio e não
concede aprovação atual isoladamente. A autoridade agregada é revogada antes
de transformações. A [reconstrução LTO do bloco 8](OPENMP_TRANSACTION_LTO_GRAPH.md)
cria gerações independentes na mesma API; a aceitação integral da 6.6 permanece
pendente.


Bloco 4 concluído como componente do frontend no domínio descrito abaixo.
A [prova proprietária do bloco 5](OPENMP_TRANSACTION_CURRENT_OWNER.md) está
implementada como componente separado. O resultado conserva a dependência
OWNER até a composição do bloco 7 e não concede aprovação integral atual.

`newtm_status_verify_cfg` reconstrói operações e saídas no CFG atual das
funções preparadas. Reutiliza as análises independentes de status, propagação
de falhas e publicação, sem consultar descritores retirados. A entrada do
controlador é inspecionada por `newtm_status_verify_entry`, incluindo os
corpos extraídos por outlining.

As obrigações persistentes contêm declarações de status, papéis de chamadas,
predicados e associações de publicação. Nenhuma instrução ou receita de
execução é conservada. Cada nova inspeção reconstrói os estados atuais;
`newtm_status_get` apenas expõe seu último resultado, sem certificado reutilizável.

Cada definição de chamada tem sua própria fase e seus próprios canais de
valor e binding. Uma consulta STATUS diferente não autoriza seus resultados.
Cópias sem perda preservam a prova; sobrescritas e aliases a invalidam. A
descoberta de endereços ocorre nos operandos atuais, inclusive sem depender
de TREE_ADDRESSABLE. Construtores também consomem seus operandos.
Constantes de seletores globais são invalidadas por chamadas e escritores
possíveis; uma chamada comum anterior ao controlador não pode ocultar um
caminho por conservar artificialmente o valor antigo de um global.

Buffers internos usam o modo estrito: um store comum anterior à chamada não
produz um resultado autorizado. O braço PRIVATE exige a leitura real, sua
tupla correspondente à mesma operação READ, guardas e cadeia de cópias.
O domínio de disponibilidade acompanha objetos locais completos de buffer,
como os gerados pelo compilador atual. Um campo/subobjeto usado como saída
não se converte silenciosamente em objeto sem rastreamento: a forma é recusada
quando não há uma prova de disponibilidade para ela.

Publicação exige o STATUS final da própria instância, dominante, comprovado
como sucesso, e sem chamada posterior até os stores de saída. Sucesso entrega
os canais completos; falha preserva ambos. O retorno real e os cleanups do CFG
participam da prova. Estados são separados por fase de status e por máscara
de publicação, conservando seletores de continuação em cleanups compartilhados.

Os epílogos preparados precisam conservar a causa, usar FAIL e retornar seu
resultado efetivo. Conflito não vira erro permanente por substituição da causa;
validação não vira retry. Status desconhecidos são normalizados pela fronteira
FAIL da ABI, inclusive quando vierem de um serviço de runtime. A análise
conserva conflito e INVALID como resultados efetivos possíveis de FAIL quando
a causa é conflito, respeitando a precedência permanente prevista pela ABI.
Falhas não podem continuar chamadas normais, acessos de memória ou validações, nem circular no
epílogo. O slot não pertence ao clone.

O resultado é DEPENDENT com OWNER. O bloco 5 ainda precisa demonstrar o
protocolo completo de propriedade, limpeza e retry do controlador no CFG
atual. Cobertura residual, composição/aceitação atual e LTO permanecem nos
blocos 6–8. current_approved continua falso. ABI/runtime/layouts e o domínio
de funções e tipos foram preservados.

## Verificação

`make -C transaction_test check-status-atual` inclui positivos e mutações
após ompexp em C/C++, O0/O2, checking 0/2, GC forçado e DSO/archive.
São exercitados os gates do controlador, inclusive após parallel, e os
dois canais independentes. O resultado final foi de 449 verificações e
100 execuções. A matriz inclui seletores de cleanup compartilhado, consumo
antecipado por construtor, causa substituída, retorno desconhecido sem FAIL,
sobrescritas, aliases sem flag de endereço, falsa produção PRIVATE e saída
em subobjeto sem prova de disponibilidade. Cópias e inversões equivalentes
de predicado precisam continuar aceitas e executáveis.
As mutações do seletor GNU são realizadas em O0, onde esse cleanup está
presente nesta versão. Em O2, os positivos continuam executados; o GCC já
removeu o cleanup vazio antes de ompexp. As recusas não dependem de IR inválido.

O teste de seletor global usa uma chamada comum que altera seu valor antes
do controlador. Uma falha do END precisa impedir publicação global mesmo
quando todos os retornos da função original são não zero. Os positivos
confirmam o caminho efetivamente executado e o retorno esperado do programa.

O teste de corrupção tardia de `check-vida-util` exige agora que as inspeções
atuais de canais e status tenham passado antes de injetar a falha no início
de IPA. Isso testa a defesa executável do chamador; não certifica o corpo
corrompido, não fabrica aprovação e não substitui a reconstrução futura.

## Resultados — 2026-11-06

- `all-gcc`: concluído sem avisos ou erros; self-tests internos desabilitados.
- Status atual: 449 verificações e 100 execuções.
- Grafo transitivo: 317 verificações; canais: 241/44; memória: 281/32.
- Controlador: 225 verificações e 106 execuções.
- Integração: 1078 verificações e 930 execuções, com falhas por posição,
  retry repetido e integral, visões privadas, GC e LTO funcional.
- Vida útil: 126 verificações, 32 execuções e 1708 cenários sintéticos.
- Status anterior: 77/16/1680 cenários; falhas: 77/16/1856.
- Fluxo: 73/16/816; guardas: 102/24/1036; composição: 93/16/816.
- Propriedade anterior: 77/16/2356; ABI v1: 88; chamadas: 75/18.

Logs específicos em `transaction_test/.build/current-status/`. Resultados
nativos Linux x86_64. LTO funcional não é prova reconstruída em LTO.
Blocos 5–8 e aceitação integral da 6.6 continuam pendentes.
