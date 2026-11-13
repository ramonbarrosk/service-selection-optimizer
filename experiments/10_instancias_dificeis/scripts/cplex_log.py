"""Leitura dos logs do método exato (CPLEX) gerados por run_exato.sh.

Para cada log extrai:
  - status: "Optimal" (ótimo provado: limite inferior = melhor solução) ou "Feasible"
    (parou nos 15 min com uma solução viável, sem provar que é ótima);
  - cost: custo da melhor solução viável encontrada (incumbente);
  - bound: melhor limite inferior (Best Bound) no fim da execução;
  - gap_pct: (cost - bound) / cost, a mesma definição do CPLEX; 0 quando ótimo;
  - total_s: tempo total (raiz + branch-and-cut);
  - last_incumbent_s: tempo até a última melhoria da solução. O CPLEX não imprime o
    tempo nas linhas de incumbente (as marcadas com "*"), só nas linhas periódicas
    "Elapsed time". O valor é a primeira "Elapsed time" depois da última "*", ou o
    tempo total se não houver nenhuma depois: um limite superior para o instante real;
    last_incumbent_lo_s é a "Elapsed time" anterior, o limite inferior;
  - incumbents: quantas vezes a solução melhorou;
  - violation_cuts: cortes de SLA adicionados pelo callback lazy.

    python3 experiments/10_instancias_dificeis/scripts/cplex_log.py   # grava cplex_hard.csv
"""

import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
EXACT_LOGS = ROOT / "data/Log_hard"
TIMEOUT_LOGS = ROOT / "data/Log_hard_timeout"
OUTPUT = ROOT / "experiments/10_instancias_dificeis/results/cplex_hard.csv"

ANSI = re.compile(r"\x1b\[[0-9;]*m")
NODE_LINE = re.compile(r"^\*?\s*\d+\+?\s+\d+\s")
ELAPSED = re.compile(r"^Elapsed time = ([\d.]+) sec")


def node_bound(line):
    """Best Bound de uma linha do log de nós, ou None se a linha não tiver gap."""
    tokens = line.replace("*", " ").split()
    gap_index = next((i for i, t in enumerate(tokens) if t.endswith("%")), None)
    if gap_index is None:
        return None
    # Incumbentes achados por heurística ("0+") não têm a coluna ItCnt.
    offset = 1 if tokens[0].endswith("+") else 2
    return float(tokens[gap_index - offset])


def parse_log(path):
    lines = [ANSI.sub("", line) for line in path.read_text(errors="ignore").splitlines()]
    text = "\n".join(lines)
    status = re.search(r"^Status = (\w+)", text, re.M).group(1)
    total = float(re.search(r"Total \(root\+branch&cut\) =\s+([\d.]+) sec", text).group(1))
    cost = float(re.search(r"^Objective Value = ([\d.]+)", text, re.M).group(1))
    cuts = re.search(r"# of violation cuts (\d+)", text)

    bound = None
    last_incumbent = None
    incumbents = 0
    elapsed = []
    for index, line in enumerate(lines):
        match = ELAPSED.match(line)
        if match:
            elapsed.append((index, float(match.group(1))))
        elif NODE_LINE.match(line):
            value = node_bound(line)
            if value is not None:
                bound = value
            if line.startswith("*"):
                incumbents += 1
                last_incumbent = index

    before = [t for i, t in elapsed if i < last_incumbent]
    after = [t for i, t in elapsed if i > last_incumbent]
    if status == "Optimal":
        bound = cost
    return {
        "status": status,
        "cost": cost,
        "bound": bound,
        "gap_pct": 100 * (cost - bound) / cost,
        "total_s": total,
        "last_incumbent_lo_s": before[-1] if before else 0.0,
        "last_incumbent_s": after[0] if after else total,
        "incumbents": incumbents,
        "violation_cuts": int(cuts.group(1)) if cuts else 0,
    }


def load_cplex(iid):
    """Resultado do CPLEX para a instância, procurando nas duas pastas de logs."""
    for folder in (EXACT_LOGS, TIMEOUT_LOGS):
        path = folder / f"Instance_10_10_{iid}"
        if path.exists():
            return parse_log(path)
    return None


def main():
    paths = sorted(list(EXACT_LOGS.glob("Instance_10_10_*"))
                   + list(TIMEOUT_LOGS.glob("Instance_10_10_*")),
                   key=lambda p: int(p.name.split("_")[-1]))
    fields = ["instance", "status", "cost", "bound", "gap_pct", "total_s",
              "last_incumbent_lo_s", "last_incumbent_s", "incumbents", "violation_cuts"]
    with OUTPUT.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for path in paths:
            row = parse_log(path)
            writer.writerow({"instance": path.name.split("_")[-1], **row})
    print(OUTPUT.relative_to(ROOT))


if __name__ == "__main__":
    main()
