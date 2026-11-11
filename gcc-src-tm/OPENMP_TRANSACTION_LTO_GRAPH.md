# Bloco 8 da 6.5.3 — reconstrução do grafo em LTO

O bloco acrescenta uma fronteira de reconstrução em WPA, LTRANS e LTO
monolítico/incremental. O certificado do frontend não é serializado nem
reaproveitado. Cada processo precisa carregar os corpos, reconstruir as
identidades e reexecutar as provas antes de publicar sua própria geração.
A validação final desta implementação é registrada no checkpoint de retomada.

## Contratos transportados

`newtm-lto.{cc,h}` transporta um envelope versionado por corpo protegido:
versão, manifesto de raízes de origem, fatos declarativos de clones, contratos
de memória/canais/status/propriedade/cobertura, inventário de preparação e
identidades dos temporários que já eram SSA antes da construção formal de SSA.

Não transporta APPROVED, gerações, resultados consolidados dos componentes,
GIMPLE, SSA_NAME ou ponteiros de blocos básicos. Tampouco transporta os mapas
operacionais por instrução ou `newtm_region_sources`: este último associa
operações GENERIC e só pertence à janela de preparação. Os fatos consumidos
na reconstrução são declarações, tipos, constantes e termos escalares de prova;
não são descritores nem planos para emitir acessos.

O leitor confere versão, dimensões das seções, estrutura dos mapas, registros
de identidade e opções preservadas. Uma função com builtins STM carregados
precisa possuir seus contratos. Dados ausentes, incompatíveis ou incompletos
produzem diagnóstico e impedem a emissão. As provas existentes continuam
responsáveis por confrontar o conteúdo dos contratos com as operações reais.

A inclusão do envelope muda o formato dos corpos: esta árvore usa LTO **16.1**.
Objetos anteriores, 16.0, precisam ser recompilados; são recusados pelo controle
de versão do LTO antes da interpretação dos corpos. Isso não muda a versão da
ABI de runtime, seus símbolos ou seus layouts.

## Identidade após leitura e combinação de unidades

Uma declaração local recebe um ID escalar no namespace de uma unidade de
contratos que o LTO não mescla com outras unidades. Assim, IDs numéricos iguais
em arquivos diferentes não identificam o mesmo objeto. Após carregar os corpos,
a reconstrução registra primeiro as declarações dos operandos e parâmetros
reais; depois reconcilia as cópias locais que vieram em streams distintos dos
contratos. A declaração operacional atual prevalece sobre a cópia do fato.

Temporários que já eram SSA durante a captura conservam uma origem declarativa
própria e sua versão de origem escalar. A versão renumerada pelo LTO não se
passa por essa identidade. Os demais temporários criados pela renomeação SSA
conservam suas declarações; temporários anônimos recebem locais exclusivos da
inspeção, sem inserir declarações ou operações executáveis no programa.

Os tipos ABI são adotados das assinaturas carregadas somente depois de conferir
contexto opaco incompleto, incoming const, famílias de ponteiros, tamanhos,
alinhamentos, offsets, empacotamento e ordem de armazenamento. Isso evita criar
novos campos de ABI e tratá-los como se fossem as mesmas declarações dos campos
remapeados. `newtm_binding_v1`, `newtm_clone_binding_v1`, incoming/result e os
canais de status permanecem com seus layouts anteriores.

## Reconstrução de SSA

As provas dos blocos 1–7 usam um CFG de declarações. Para reexecutá-las, o
bloco 8 constrói uma visão temporária cuja correspondência com SSA é verificada:

- Cada uso escalar precisa consumir a versão alcançável da sua declaração;
  uma versão antiga após outra definição não é aceita por compartilhar o nome.
- Cada PHI precisa selecionar a versão correta da mesma declaração na sua
  própria aresta predecessora. Constantes ou escolhas sem essa correspondência
  não são assumidas equivalentes.
- Definições padrão e junções são analisadas até ponto fixo, incluindo ciclos;
  limites ou formas desconhecidas são recusados conservadoramente.
- Só depois dessas verificações os operandos são apresentados como declarações
  e os PHIs são ocultados da visão de inspeção. Nenhuma atribuição ou aresta
  executável é sintetizada para justificar a prova.
- Ao terminar, inclusive em falha, operandos e PHIs são restaurados. A publicação
  exige igualdade da fotografia do SSA real antes e depois da inspeção.

A fotografia inclui versões e argumentos de PHI, SSA de memória, CFG, operações,
assinaturas, layouts, opções e relações EH. São bytes escalares comparados
integralmente, sem reaproveitar instruções anteriores ou depender de hash.

Duas normalizações específicas foram necessárias: comparações de tipos
booleanos remapeados podem virar `cond ? 1 : 0`, cuja forma booleana exata é
reconhecida pelo analisador de guardas; e SSA marca clobbers de vida útil como
volatile para preservá-los. A marca isolada de um clobber não é tomada por
acesso volatile, mas seu destino continua sujeito à classificação de memória
e à recusa de declarações, referências ou tipos volatile reais.

## Raízes, fechamento e publicação

As raízes ativas são reconstruídas das chamadas atuais de controladores para
funções preparadas e precisam constar do manifesto de origem. O manifesto não
concede autoridade ao corpo. Regiões de um controlador ainda vivo permanecem
obrigatórias mesmo se sua invocação for removida; admissões e invocações são
confrontadas para que uma região corrompida não desapareça da seleção de raízes.
No modo funcional opcional, as arestas de entrada do cgraph também precisam
vir de chamadores protegidos; um chamador sem CFG inspecionado não pode escapar
da checagem de instruções. O envelope registra esse modo de origem. Os probes
anteriores com controle externo manual da ABI, fora desse modo, conservam seus
fatos de corpo, mas não podem publicar raízes nem recebem aprovação de grafo.
Essa distinção preserva a ABI executável sem certificar seu driver manual.
Cada raiz executa novamente a composição do bloco 7: assinaturas, contexto,
registros, identidade/intervalos, canais e produtores de saídas, status,
propriedade, cobertura, dependências, ciclos e produtores de outlining.

O GCC também preserva auxiliares preparados que não possuem controlador externo
ativo, por exemplo a versão externa não utilizada de uma região aninhada. Eles
transportam contratos para sua materialização e emissão, mas não são promovidos
a raízes nem recebem um controlador ou certificado fictício. Um programa sem
entrada ativa não recebe aprovação de grafo por conter esses auxiliares.

`newtm_current_get` só expõe APPROVED depois da composição completa, com
required/proved 31 e dependências zero. Os componentes conservam DEPENDENT; o
getter histórico continua com current_approved=false. A geração de cada
processo é nova, e a restauração de IR não ressuscita uma geração invalidada.
Revalidação completa produz outro identificador, como no frontend.

WPA materializa os membros protegidos para reconstruir a prova antes das
transformações. LTRANS usa `get_untransformed_body`, verifica os corpos
carregados antes dos planos IPA e retira a autoridade antes de aplicá-los.
LTO monolítico e links incrementais usam a mesma fronteira de reconstrução.
O callback ALL_IPA_PASSES_START observa a janela; em seguida ela é fechada.
As consultas posteriores não concedem aprovação anterior às transformações.

## Domínio conservador e custo de otimização

Este primeiro domínio preserva a correspondência de escalares durante a
geração de LTO e WPA: as funções da unidade de prova usam temporariamente O0 e
noipa/noinline/noclone/no_icf. Inclui corpos preparados, funções contendo os
controladores e produtores de capturas. O bloco não pretende reconstruir
arbitrariamente qualquer SSA já otimizado.

Depois da reconstrução final em LTRANS ou LTO monolítico, as opções, atributos
e a condição de inlining originais são restabelecidos. O caminho nativo de um
objeto fat também restaura as opções depois de escrever LTO. Um link incremental
que produz outro objeto LTO mantém os contratos e a preservação até a fase
final; não transporta aprovação junto com o objeto intermediário.

Os membros protegidos são colocados conjuntamente em LTRANS, inclusive entre
unidades, mantendo as políticas de particionamento para o código comum.
Isso evita substituir uma prova de fechamento por uma promessa sobre uma
função ausente da partição. A escolha pode aumentar tamanho dos objetos,
materialização/memória em WPA e tamanho da partição protegida, além de limitar
as otimizações anteriores à fronteira final. Não altera a semântica da ABI.

Continuam as restrições do preflight e do frontend: não são produzidos clones
remotos para uma função sem definição admitida na unidade de preparação. Os
testes entre unidades ligam chamadas comuns a funções com controladores próprios.
Agregados compartilhados do fixture de integração continuam restritos a C;
não houve ampliação do domínio C++. Alternativa B continua otimização futura.

## Validação

`make -C transaction_test check-grafo-lto` usa processos reais do driver, WPA e
LTRANS, além de LTO monolítico e incremental. O teste contém:

- C/C++, O0/O2, checking 0/2, GC forçado e DSO/archive;
- raízes com recursão compartilhada, reconstrução em nova geração e revogação
  por mutação de operandos, sem ressurreição após restauração;
- contratos com versão, modo de origem, envelope, mapas, raiz ou componente
  inválidos/ausentes;
- mutações serializadas de layout, memória residual, guarda, versão SSA antiga,
  PHI, contexto, causa de falha, proprietário, canal de resultado e remoção
  da própria invocação do auxiliar;
- GNU, parallel, EH externo de C++, memória dinâmica e transações aninhadas;
- múltiplas unidades, chamadas comuns entre elas e partições balanced/max/1to1;
- controlador C++ COMDAT em partições máximas, auxiliares inativos sem aprovação,
  objeto fat nativo/LTO e recusa explícita do formato antigo;
- opções de otimização restauradas e autoridade retirada após a janela.

O plugin altera SSA válido depois da captura dos contratos e antes do streaming;
a recusa no processo LTO não depende de uma mutação do certificado histórico.
O caso de versão SSA antiga permanece SSA válido, mas não corresponde à
projeção por declarações e deve ser recusado por essa regra específica.

A matriz integral final passou com **426 verificações e 122 execuções**,
incluindo remoção da invocação e alteração indevida do modo de origem.
Passaram também grafo atual (543/212), histórico (317), cobertura (695/156),
canais (241/44), memória (281/32), guardas (102/24/1036), propriedade (553/88),
status (449/100), controlador (225/106), integração (1078/930), ABI (88) e
chamadas (75/18). Build all-gcc e higiene aprovados. Resultados e logs no
[checkpoint](OPENMP_TRANSACTION_RETOMADA.md).
A conclusão do bloco 8 fecha a 6.5.3 no domínio documentado, sem substituir a
etapa 6.6 de aceitação integral. Os resultados são nativos Linux x86_64;
self-tests internos do GCC estão desabilitados neste build.
