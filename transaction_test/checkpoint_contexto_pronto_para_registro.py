"""Checkpoint: contexto pronto para implementar registro STM.

Valida, pelos oito alvos existentes executados em processos separados:
* As nove primitivas da hash: identidade, referencias, colisões, metadados,
  contagem, limites/overflow e os cenarios concorrentes exercitados pela suite.
* Admissao/encerramento do owner e preservacao de participacoes durante limpeza.
* Criacao do contexto, transferencia da lease, aninhamento, causa permanente
  dominante, rejeicao de slot copiado/consumo alheio e descarte sem publicacao.
* Limpeza de buffers e listas preparadas manualmente, leitores compartilhados,
  escritor e diagnosticos fatais previstos pelos testes existentes.
* Integracao de rastreamento: exemplos e carga nas proporcoes 1:9, 1:1 e 9:1.
* Preservacao dos logs e ocultacao dos simbolos internos verificados pela suite.

Limites: nao valida registro STM funcional, concessao de reservas, leitura/escrita
privada, isolamento, commit, retry ou os filtros futuros de linguagem. A carga
mantem critical auxiliar; registros de limpeza sao preparados por acesso privado.
Nao acrescenta os testes ainda ausentes de OOM do contexto, matriz completa de
estados ou descarte simultaneo de contextos. Nao executa sanitizadores.

Algoritmo: recompilar os testes (-B) contra o build selecionado; executar cada
alvo sequencialmente com timeout; salvar log individual e relatorio JSON; parar
na primeira falha e retornar erro. Nunca inferir aprovacao apenas dos printf.
A libgomp precisa ter sido reconstruida antes: este checkpoint nao recompila o
runtime nem atesta que um archive antigo corresponde aos fontes atuais.
"""

import argparse
from datetime import datetime, timezone
import json
import os
import signal
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
NAME = "contexto_pronto_para_registro"
TARGETS = (
    "check-hash-create-destroy",
    "check-hash-acquire",
    "check-hash-entry",
    "check-hash-count",
    "check-hash-diagnostics",
    "check-hash-owner",
    "check-lifecycle",
    "check",  # Inclui check-carga com as tres proporcoes e os exemplos.
)


def run_target(command, log, timeout):
    """Executar um alvo; em interrupcao/timeout, encerrar tambem seus filhos."""
    with log.open("w", encoding="utf-8") as stream:
        with subprocess.Popen(command, cwd=ROOT, stdout=stream,
                              stderr=subprocess.STDOUT,
                              start_new_session=True) as process:
            try:
                return process.wait(timeout=timeout)
            except BaseException:
                # Um make interrompido nao deve deixar testes vivos em paralelo
                # com uma nova execucao do checkpoint.
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()
                raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--make", default="make")
    parser.add_argument("--timeout", type=int, default=180,
                        help="limite em segundos por alvo (padrao: 180)")
    parser.add_argument("--var", action="append", default=[],
                        help="atribuicao NAME=VALUE repassada ao make")
    args = parser.parse_args()
    if args.timeout <= 0 or any("=" not in value for value in args.var):
        parser.error("timeout deve ser positivo; --var requer NAME=VALUE")
    output = ROOT / ".build" / ("checkpoint_" + NAME)
    output.mkdir(parents=True, exist_ok=True)
    report_path = output / "resultado.json"
    report = {
        "checkpoint": NAME,
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "status": "running",
        "variables": args.var,
        "steps": [],
    }

    def save():
        report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2)
                               + "\n", encoding="utf-8")

    save()  # Invalida imediatamente a aprovacao de uma execucao anterior.
    for target in TARGETS:
        command = [args.make, "--no-print-directory", "--jobs=1", "-B",
                   target, *args.var]
        print(f"[checkpoint {NAME}] executando {target}", flush=True)
        log = output / (target + ".log")
        step = {"target": target, "command": command, "log": str(log)}
        try:
            # Direct file output retains diagnostics even on timeout/interruption.
            returncode = run_target(command, log, args.timeout)
            step["returncode"] = returncode
            step["status"] = "passed" if returncode == 0 else "failed"
        except (subprocess.TimeoutExpired, OSError) as error:
            step["status"] = "failed"
            step["error"] = str(error)
        report["steps"].append(step)
        if step["status"] != "passed":
            report["status"] = "failed"
            save()
            print(f"FALHOU: {target}; consulte {log}", file=sys.stderr)
            print(f"Relatorio: {report_path}", file=sys.stderr)
            return 1
        save()
        print(f"OK: {target}", flush=True)
    report["status"] = "passed"
    report["finished_utc"] = datetime.now(timezone.utc).isoformat()
    save()
    print(f"CHECKPOINT APROVADO: {NAME} ({len(TARGETS)} alvos)")
    print(f"Relatorio: {report_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
