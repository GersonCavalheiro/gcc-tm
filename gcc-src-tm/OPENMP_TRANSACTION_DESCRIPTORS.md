# Etapa 2: registros e identidade em High GIMPLE

Implementação em `gcc/newtm-descriptor.{cc,h}`, depois da gimplificação do corpo.
Cada ocorrência possui entradas por papel (registro, leitura, escrita, formação
de endereço, argumento ponteiro/referência, retorno ou testemunho de origem).
Elas complementam os [vínculos de ocorrências](OPENMP_TRANSACTION_OCCURRENCES.md).
A ABI pública GOMP permanece de rastreamento; esta etapa não ativa acessos STM.

## Campos e identidade

O consumidor consulta `newtm_descriptor_entries` ou `newtm_descriptor_find`.
O vetor é indexado pelo enum `newtm_descriptor_field`, não por ABI C pública:

| Campo | Contrato |
| --- | --- |
| OBJECT | Temporário `void *` com a chave lógica original efetivamente registrada; nunca endereço do buffer privado. |
| EXTENT | Temporário `size_t` com a extensão registrada em bytes. |
| OFFSET | Temporário `size_t` com o deslocamento relativo à chave registrada. |
| KIND | Temporário `uint32_t`: escalar 0, agregado 1. Capturado do tipo listado antes da gimplificação, não inferido pelo tamanho ou pelo agregado que contém um elemento. |
| SIZE | Tamanho do acesso; zero para registro, passagem de identidade ou formação de endereço. |
| VALID | Booleano estrutural/de faixa calculado antes da operação. Não prova registro no contexto, reserva, vida útil ou permissão de escrita. |
| STATE | registered, private, unregistered, incoming, result, unknown, source-only ou stale. |
| ORIGIN | Objeto físico raiz, usado para correlacionar campos/elementos e aliases. |
| CHANNEL | Canal formal para parâmetro ponteiro/referência, ou resultado de chamada. |
| KIND_VALUE / EXTENT_VALUE | Classificação e extensão antes da cópia para temporários. |
| LOGICAL | Referência/valor GIMPLE atual do programa; proveniência para o consumidor, nunca expressão para reavaliar. |
| LOCAL_VALID | Validade dos cálculos relativos já conhecidos, inclusive quando o binding completo ainda precisa atravessar uma chamada. |

Campos/elementos seguem a raiz física e acumulam deslocamentos. A seleção de
registro preserva a chave e extensão realmente listadas. Uma referência a um
`int` dentro de um array registra somente esse `int`, com kind escalar e offset
relativo zero. Registros iguais não criam escolhas concorrentes no mapeamento;
registros distintos continuam sujeitos à rejeição de sobreposição já existente
na análise/runtime. A hash não substitui essa prova.

O rastreamento considera as atribuições de ponteiros anteriores ao pragma,
operandos já avaliados e definições auxiliares de escala em bytes. Não executa
receitas GENERIC nem troca os ponteiros do programa por ponteiros privados.
Objetos automáticos criados na tentativa e parâmetros por valor de candidatos
preparados recebem classificação privada. Uma atribuição indireta/global capaz
de alterar ponteiros invalida a proveniência mutável conservadoramente.

## Limites sem overflow

Índices de arrays são verificados no tipo original antes de conversão a size_t,
inclusive sinal e largura. Cada dimensão é verificada separadamente. A formação
de endereço admite one-past; um acesso de tamanho não zero exige faixa contida.
Escalares exigem offset zero e tamanho integral.

Multiplicação, adição e subtração usam resultados e flags de overflow das
IFN_*_OVERFLOW. Deltas assinados de ponteiros são preservados: avançar para trás
a partir de um elemento intermediário é válido se o resultado couber no objeto.
Valores extremos não se tornam offsets válidos por wrap. A faixa usa
`offset <= extent` e `size <= extent - min(offset, extent)`, sem subtração
negativa. A extensão do endereço é verificada com
`extent - 1 <= UINTPTR_MAX - address`, com clamp para extensão zero, sem formar
`object + extent` ou um ponteiro de subacesso.

Os cálculos são instruções GIMPLE comuns sobre operandos avaliados; não leem os
valores dos objetos para registrar. Temporários possuem BIND dentro da região
OpenMP pertinente, permitindo remapeamento correto durante outlining.

## Chamadas, cópias e limites deliberados

Um parâmetro ponteiro/referência não fornece a extensão do apontado. No corpo da
função selecionada, incoming identifica o canal formal e conserva apenas os
cálculos relativos conhecidos. Argumentos do chamador recebem descritor concreto
quando demonstrável. Resultados ponteiro usam canal result; a ligação efetiva
com o binding devolvido dependerá dos clones preparados. Não preencher object,
extent, kind ou valid com valores fictícios nesses estados.

Fluxos cujo binding não foi demonstrado ficam explicitamente unknown, incluindo
seleção ambígua de subobjetos registrados e deltas modulares cuja origem assinada
não pode ser recuperada. A análise de fluxo atual é conservadora, não uma prova
CFG/SSA de todos os aliases. O consumidor futuro deve resolver ou rejeitar esses
casos; não pode acessá-los pela memória comum. Source-only não tem valor avaliado.

A janela dos descritores termina **na entrada de omplower**, antes de remapear
objetos/endereços. Os vínculos de ocorrências duram até a entrada de lower. Uma
cópia recebe estado stale e nenhum campo operacional antigo: o consumidor precisa
reconstruir o descritor no corpo remapeado antes de usá-lo. As tabelas são raízes
GC; tipos de registro são capturados nos sites, que guardam tokens estáveis em
vez de nós auxiliares GENERIC explicitamente liberados pelo gimplifier.

A validação calculada ainda não é consumida pelo pragma de produção. Portanto
esta etapa, isoladamente, não impede um acesso inválido do programa nem fornece
isolamento transacional. A próxima geração de acessos precisa ramificar em
INVALID antes de formar o endereço/acessar memória e propagar status sem resultado.
Nenhuma assinatura pública ou integração funcional parcial foi ativada.

## Validação reproduzível

`make -C transaction_test check-descritores` constrói um plugin consumidor
exclusivo dos testes. Ele verifica os tipos dos campos, GC forçado, invalidação
por cópia e estados de parâmetros/resultados/privados/source-only. Outra sonda
passa os campos realmente calculados a um observador e retorna antes do acesso
original quando inválido. Um oráculo independente usa inteiros Python sem wrap.
O retorno antecipado pertence somente ao teste, não à ponte GOMP do produto.

A matriz executa C/C++ em O0/O2: arrays assinados/não assinados, aritmética de
ponteiros, deslocamentos negativos a partir do meio do objeto, índices extremos,
wrap de multiplicação, i++, campos C, matrizes e referência C++ a elemento.
Classes C++ continuam rejeitadas. Testes de esquema verificam chamadas, retorno,
identidade lógica e retirada dos descritores antes de omplower. Cobertura nativa
obtida em Linux x86_64 LP64; 32 bits/outros targets continuam extensões de teste.

Rodada concluída em 2026-10-29: build GCC; 36/36 testes de descritores com
`-fchecking=2`; 63/63 de ocorrências; 201/201 do subconjunto; 41/41 casos
compilados diretamente da árvore de regressões; `make -C transaction_test check`
aprovado. A execução direta não interpreta dg-final; DejaGNU não foi executado.
`git diff --check` sem erros. A proteção dos acessos inválidos foi verificada
pela sonda de teste; o código de produção continua com a ponte de rastreamento.
