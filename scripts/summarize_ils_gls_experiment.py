#!/usr/bin/env python3
"""Gera um relatório Markdown para o experimento ILS x GLS x GLS+oscilação."""

import csv
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path


METHODS = {
    "ils": "ILS original",
    "gls": "GLS sem ILS",
    "gls_osc": "GLS + oscilação sem ILS",
}


def mean(values):
    return statistics.fmean(values) if values else float("nan")


def sign_test_p_value(wins, losses):
    """Teste binomial bilateral, desconsiderando empates."""
    n = wins + losses
    if n == 0:
        return 1.0
    smaller = min(wins, losses)
    tail = sum(math.comb(n, k) for k in range(smaller + 1)) / (2**n)
    return min(1.0, 2.0 * tail)


def fmt(value, digits=2):
    return f"{value:.{digits}f}"


def duration(milliseconds):
    total_seconds = milliseconds / 1000.0
    minutes = int(total_seconds // 60)
    seconds = total_seconds - minutes * 60
    return f"{minutes}min {seconds:.2f}s" if minutes else f"{seconds:.2f}s"


def main():
    input_path = Path(sys.argv[1] if len(sys.argv) > 1
                      else "data/experiments/ils_gls_comparison.csv")
    output_path = Path(sys.argv[2] if len(sys.argv) > 2
                       else "data/experiments/ils_gls_summary.md")

    with input_path.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise SystemExit("O CSV do experimento está vazio.")

    by_instance = defaultdict(list)
    for row in rows:
        by_instance[int(row["instance"])].append(row)

    instance_stats = {}
    for instance, group in sorted(by_instance.items()):
        optimum = int(group[0]["optimum"])
        stats = {"optimum": optimum}
        for key in METHODS:
            costs = [float(row[f"{key}_cost"]) for row in group]
            gaps = [float(row[f"{key}_gap_pct"]) for row in group]
            times = [float(row[f"{key}_time_ms"]) for row in group]
            stats[key] = {
                "mean_cost": mean(costs),
                "best_cost": min(costs),
                "mean_gap": mean(gaps),
                "mean_time_ms": mean(times),
                "std_cost": statistics.stdev(costs) if len(costs) > 1 else 0.0,
                "optimal_runs": sum(cost <= optimum for cost in costs) if optimum > 0 else 0,
            }
        instance_stats[instance] = stats

    summary = {}
    for key in METHODS:
        values = [stats[key] for stats in instance_stats.values()]
        summary[key] = {
            "mean_cost": mean([item["mean_cost"] for item in values]),
            "mean_gap": mean([item["mean_gap"] for item in values]),
            "mean_time_ms": mean([item["mean_time_ms"] for item in values]),
            "total_time_ms": sum(float(row[f"{key}_time_ms"]) for row in rows),
            "optimal_instances": sum(item["best_cost"] <= instance_stats[instance]["optimum"]
                                     for instance, item in
                                     ((i, instance_stats[i][key]) for i in instance_stats)
                                     if instance_stats[instance]["optimum"] > 0),
        }

    comparisons = []
    keys = list(METHODS)
    for first_index, first in enumerate(keys):
        for second in keys[first_index + 1:]:
            wins = ties = losses = 0
            for stats in instance_stats.values():
                first_cost = stats[first]["mean_cost"]
                second_cost = stats[second]["mean_cost"]
                if first_cost < second_cost - 1e-9:
                    wins += 1
                elif second_cost < first_cost - 1e-9:
                    losses += 1
                else:
                    ties += 1
            comparisons.append((first, second, wins, ties, losses,
                                sign_test_p_value(wins, losses)))

    repetitions = len(next(iter(by_instance.values())))
    budgets = [float(row["budget_seconds"]) for row in rows]
    unique_budgets = sorted(set(budgets))
    if len(unique_budgets) == 1:
        budget_description = f"{fmt(unique_budgets[0], 3)} s (fixo)"
    else:
        budget_description = (
            "adaptativo por instância, entre "
            f"{fmt(min(budgets), 3)} s e {fmt(max(budgets), 3)} s "
            f"(média {fmt(mean(budgets), 3)} s)"
        )
    lines = [
        "# Experimento: ILS original × GLS × GLS com oscilação",
        "",
        "## Protocolo",
        "",
        f"- Instâncias: {len(instance_stats)}",
        f"- Repetições por instância: {repetitions}",
        f"- Orçamento por método em cada repetição: {budget_description}",
        "- A mesma solução inicial é fornecida aos três métodos em cada repetição.",
        "- ILS original: perturbação MOVE + busca local SWAP, sem GLS e sem oscilação.",
        "- GLS: GLS/GFLS sem o laço de perturbação do ILS e sem oscilação.",
        "- GLS + oscilação: GLS/GFLS alternada com oscilação, sem o laço do ILS.",
        "- A probabilidade não integra o objetivo da GLS; a SLA permanece restrição dura.",
        "",
        "## Resultado agregado",
        "",
        "| Método | Custo médio | GAP médio | Tempo médio por execução | Tempo total do algoritmo | Instâncias ótimas |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for key, name in METHODS.items():
        item = summary[key]
        lines.append(
            f"| {name} | {fmt(item['mean_cost'])} | {fmt(item['mean_gap'])}% "
            f"| {fmt(item['mean_time_ms'] / 1000.0, 3)} s | "
            f"{duration(item['total_time_ms'])} | "
            f"{item['optimal_instances']}/{len(instance_stats)} |"
        )

    gls_gain = summary["ils"]["mean_gap"] - summary["gls"]["mean_gap"]
    oscillation_change = summary["gls_osc"]["mean_gap"] - summary["gls"]["mean_gap"]
    lines.extend([
        "",
        "### Leitura do resultado",
        "",
        f"- A GLS sem ILS reduziu o GAP médio em {fmt(gls_gain)} pontos percentuais "
        "em relação ao ILS original.",
        f"- Acrescentar a oscilação à GLS alterou o GAP médio em "
        f"{fmt(oscillation_change)} ponto(s) percentual(is); valor positivo significa piora.",
        "- Tempos médios inferiores ao orçamento ocorrem quando o ótimo conhecido é "
        "atingido e a execução termina antecipadamente.",
        "- Essa conclusão vale para este protocolo, orçamento, conjunto de instâncias e "
        "sementes; outros orçamentos devem ser avaliados separadamente.",
    ])

    lines.extend([
        "",
        "## Comparações pareadas por instância",
        "",
        "Vitória significa menor custo médio na instância. O teste de sinais bilateral "
        "usa somente vitórias e derrotas; empates são desconsiderados.",
        "",
        "| Primeiro método | Segundo método | Vitórias | Empates | Derrotas | p-valor |",
        "|---|---|---:|---:|---:|---:|",
    ])
    for first, second, wins, ties, losses, p_value in comparisons:
        lines.append(
            f"| {METHODS[first]} | {METHODS[second]} | {wins} | {ties} | "
            f"{losses} | {p_value:.4f} |"
        )

    lines.extend([
        "",
        "## Resultado por instância",
        "",
        "| Instância | Ótimo | ILS média | ILS GAP | ILS tempo | GLS média | GLS GAP | "
        "GLS tempo | GLS+OSC média | GLS+OSC GAP | GLS+OSC tempo |",
        "|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ])
    for instance, stats in instance_stats.items():
        lines.append(
            f"| {instance} | {stats['optimum']} | "
            f"{fmt(stats['ils']['mean_cost'])} | {fmt(stats['ils']['mean_gap'])}% | "
            f"{fmt(stats['ils']['mean_time_ms'] / 1000.0, 3)} s | "
            f"{fmt(stats['gls']['mean_cost'])} | {fmt(stats['gls']['mean_gap'])}% | "
            f"{fmt(stats['gls']['mean_time_ms'] / 1000.0, 3)} s | "
            f"{fmt(stats['gls_osc']['mean_cost'])} | "
            f"{fmt(stats['gls_osc']['mean_gap'])}% | "
            f"{fmt(stats['gls_osc']['mean_time_ms'] / 1000.0, 3)} s |"
        )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Relatório salvo em {output_path}")


if __name__ == "__main__":
    main()
