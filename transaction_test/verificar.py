"""Verifica resultados e logs da ponte provisoria, sem testar semantica STM."""
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent


def executar(nome, threads):
    env = dict(os.environ, OMP_NUM_THREADS=str(threads), OMP_DYNAMIC="FALSE")
    resultado = subprocess.run(
        [str(ROOT / ".build" / nome)], env=env, text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    )
    (ROOT / ".build" / f"{nome}-{threads}.log").write_text(
        resultado.stdout + resultado.stderr
    )
    if resultado.stderr:
        raise AssertionError(resultado.stderr)
    return resultado.stdout


def contagem(log, operacao, esperado):
    obtido = log.count(f"[newtm] newtm_hash_{operacao}:")
    if obtido != esperado:
        raise AssertionError(f"{operacao}: {obtido}, esperado {esperado}\n{log}")


for threads in (1, 2, 4):
    log = executar("transaction_exemplo", threads)
    resultado = re.search(
        r"\[teste\] maximo OK: valor=2000 posicao=500 threads=(\d+)", log
    )
    if resultado is None:
        raise AssertionError(log)
    usadas = int(resultado.group(1))
    # O runtime pode limitar a equipe; verificar a quantidade efetivamente usada.
    for operacao in ("acquire", "release"):
        contagem(log, operacao, 4 * usadas)
    contagem(log, "create", 1)
    contagem(log, "destroy", 1)
    print(f"OK: maximo, solicitadas={threads}, usadas={usadas}")

log = executar("transaction_aninhamento", 1)
for operacao in ("acquire", "release"):
    contagem(log, operacao, 2)
contagem(log, "create", 1)
contagem(log, "destroy", 1)
marcadores = [
    "[newtm] newtm_hash_release:",
    "[teste] regiao externa encerrada",
    "[teste] aninhamento OK: saldo=17 total=8",
    "[newtm] newtm_hash_destroy:",
]
posicoes = [log.index(marcador) for marcador in marcadores]
if posicoes != sorted(posicoes):
    raise AssertionError(log)
if log.rindex("[newtm] newtm_hash_release:") > log.index(marcadores[1]):
    raise AssertionError(log)
if log.rindex("[newtm] newtm_hash_acquire:") > log.index(marcadores[0]):
    raise AssertionError(log)
print("OK: aninhamento lexico e por funcao; duplicatas e liberacao externa")
