#!/usr/bin/env python3
"""Validação da GLS calibrada pelo irace contra a configuração atual.

Lê os CSVs de run_validacao.sh (uma linha por repetição) e, para cada conjunto de
teste, resume: instâncias com o ótimo alcançado, GAP médio, tempo médio até a melhor
solução, vitórias por custo médio e o Wilcoxon pareado sobre o custo médio por
instância (o mesmo da fase 09). O GAP é o do CSV: contra o ótimo do método exato ou,
sem ele, contra o limite inferior trivial.

    python3 experiments/11_calibracao_irace/scripts/analyze_validacao.py
"""

import csv
import sys
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experiments/09_teste_estatistico/scripts"))
from analyze_teste_estatistico import br, fmt_p, read_runs, wilcoxon  # noqa: E402

PHASE = ROOT / "experiments/11_calibracao_irace"
RESULTS = PHASE / "results/validacao"
BEST = PHASE / "results/best_configurations.csv"
SUMMARY = RESULTS / "summary.md"
SETS = [("original", "94 instâncias do artigo"), ("hard", "75 instâncias difíceis (fase 10)")]


def summarize(runs):
    return {
        "hits": sum(any(r["optimal"] for r in reps) for reps in runs.values()),
        "gap": mean(mean(r["gap"] for r in reps) for reps in runs.values()),
        "time": mean(mean(r["time_to_best_ms"] for r in reps) for reps in runs.values()) / 1000,
    }


def main():
    best = next(csv.DictReader(BEST.open()))
    lines = [
        "# Validação da GLS calibrada pelo irace",
        "",
        "Configuração calibrada: " + ", ".join(
            f"{k} = {v}" for k, v in best.items() if k != ".ID.") + ".",
        "Configuração atual: alpha = 0.3, rounds = 30, gfls = 1, replacement = 1, "
        "regret = 1, calibrate = 0, maxpen = 0.",
        "",
        "Protocolo da fase 07: 3 repetições de 10 s, parando ao alcançar o ótimo "
        "conhecido. Wilcoxon pareado, exato e bilateral, sobre o custo médio por instância.",
        "",
        "| Conjunto | Config. | Ótimo alcançado | GAP médio | Tempo até a melhor "
        "| Melhor que a outra | p-valor |",
        "|---|---|---:|---:|---:|---:|---:|",
    ]
    for name, label in SETS:
        current, _ = read_runs(RESULTS / f"atual_{name}.csv")
        tuned, _ = read_runs(RESULTS / f"calibrada_{name}.csv")
        ids = sorted(set(current) & set(tuned))
        current = {i: current[i] for i in ids}
        tuned = {i: tuned[i] for i in ids}
        diffs = [mean(r["cost"] for r in current[i]) - mean(r["cost"] for r in tuned[i])
                 for i in ids]
        p = wilcoxon(diffs)[0]
        for config, runs, wins in [("atual", current, sum(d < 0 for d in diffs)),
                                   ("calibrada", tuned, sum(d > 0 for d in diffs))]:
            s = summarize(runs)
            lines.append(
                f"| {label} | {config} | {s['hits']}/{len(ids)} | {br(s['gap'])}% "
                f"| {br(s['time'], 2)} s | {wins} | {fmt_p(p) if config == 'atual' else ''} |")
    SUMMARY.write_text("\n".join(lines) + "\n")
    print(SUMMARY.read_text())


if __name__ == "__main__":
    main()
