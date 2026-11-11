# Composição das provas locais — subetapa 6.4.7

A inspeção consolida, por obrigação, os requisitos V01–V08 aplicáveis e as
provas locais executadas. Cada resultado identifica escopo, geração e revisão;
seu resumo registra as declarações preparadas de que depende. A publicação é
atômica: classificação, fluxo, guardas, status, vida útil, protocolo e
reconciliação devem terminar antes de qualquer aprovação local.

A aprovação local não autoriza ativação de grafo nem reutilização após
remapeamento. O estado COBERTA continua reservado; dependências transitivas,
ciclos e invalidação entre funções pertencem à 6.5. Um escopo COPY sem contexto
preparado registra apenas as regras aplicáveis à sua inspeção, sem prova V07.
Os resumos históricos preservam declarações e fatos escalares, não descritores
ou operandos GIMPLE. O fechamento da janela marca o resultado como histórico.

A reconciliação considera inclusive obrigações sem ocorrência e retiradas,
filhas, produtores e ancestrais. Associa explicitamente o achatamento de
controle às operações filhas e o marcador de publicação ao STATUS e às saídas
verificadas. Entradas e saídas de origem preservam a classe de serviço esperada.
Marcas do emissor indicam candidatos; a inspeção real e as provas independentes
continuam necessárias. Cópias e evidências inválidas não herdam aprovação.

Alterações nas evidências revogam a aprovação do escopo inteiro. Uma nova
inspeção cria revisão própria; uma recusa impede recuperação silenciosa na
mesma janela. Stubs recusados continuam recusados mesmo quando seu corpo de
falha é estruturalmente válido. O diagnóstico conserva a primeira causa, regra,
função, categoria, localização e identificadores; quando aplicável, informa o
ancestral da derivação. Não contém valores de execução.

`make -C transaction_test check-composicao`: 93 verificações, 16 execuções e
816 cenários sintéticos. Inclui diagnósticos V01/V05/V08 e ancestral, checking
0/2, C/C++, O0/O2, LTO, DSO/archive e GC forçado. Também passaram as matrizes
anteriores de verificadores e a integração (1303 verificações e 1036 execuções,
incluindo controlador), além de clones, preparação, leituras, escritas,
chamadas, ocorrências, descritores, subconjunto e ABI v1. O build all-gcc
passou; self-tests internos estão desabilitados nesta configuração. Resultados
limitados ao ambiente Linux x86_64; não equivalem ao testsuite completo do GCC.

Logs e JSON: `transaction_test/.build/composition/`. A ABI, o runtime e os
layouts compartilhados foram preservados. A próxima etapa é a 6.5.
