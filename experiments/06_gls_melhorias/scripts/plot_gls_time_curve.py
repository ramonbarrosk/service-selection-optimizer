#!/usr/bin/env python3
"""Gera gráficos da curva tempo × GAP de uma configuração GLS/GFLS."""

import argparse
import csv
import os
import statistics
from collections import defaultdict
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


COLORS = ["#2563eb", "#dc2626", "#7c3aed", "#ea580c", "#0891b2"]


def mean(values):
    return statistics.fmean(values)


def load(path, variant):
    with Path(path).open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    rows = [row for row in rows if row["variant"] == variant]
    if not rows:
        raise ValueError(f"O CSV não contém a configuração '{variant}'")
    for row in rows:
        for field in ("budget_seconds", "cost", "gap_pct", "time_ms", "time_to_best_ms"):
            row[field] = float(row[field])
        for field in ("instance", "repetition", "optimum", "feasible"):
            row[field] = int(row[field])
    return rows


def aggregate(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["instance"], row["budget_seconds"])].append(row)
    stats = {}
    for key, group in grouped.items():
        stats[key] = {
            "cost": mean(row["cost"] for row in group),
            "best": min(row["cost"] for row in group),
            "gap": mean(row["gap_pct"] for row in group),
            "time_to_best": mean(row["time_to_best_ms"] for row in group) / 1000.0,
            "optimum": group[0]["optimum"],
        }
    budgets = sorted({row["budget_seconds"] for row in rows})
    return stats, budgets


def plot_curve(stats, budgets, instances, output):
    figure, axes = plt.subplots(1, 2, figsize=(17, 7))
    figure.patch.set_facecolor("#f8fafc")
    figure.suptitle("Curva de tempo da GLS/GFLS promovida",
                    fontsize=18, fontweight="bold", y=0.98)
    for index, instance in enumerate(instances):
        color = COLORS[index % len(COLORS)]
        gaps = [stats[(instance, budget)]["gap"] for budget in budgets]
        costs = [stats[(instance, budget)]["cost"] for budget in budgets]
        axes[0].plot(budgets, gaps, marker="o", linewidth=2.2,
                     color=color, label=f"Instância {instance}")
        axes[1].plot(budgets, costs, marker="o", linewidth=2.2,
                     color=color, label=f"Instância {instance}")
    for axis, title, ylabel in (
        (axes[0], "Redução do GAP com o aumento de tempo", "GAP (%)"),
        (axes[1], "Custo encontrado por orçamento", "Custo médio"),
    ):
        axis.set_title(title, fontweight="bold", pad=10)
        axis.set_xlabel("Orçamento por execução (s)")
        axis.set_ylabel(ylabel)
        axis.set_xscale("log")
        axis.set_xticks(budgets, [f"{budget:g}" for budget in budgets])
        axis.grid(True, color="#e2e8f0", linewidth=0.8)
        axis.spines[["top", "right"]].set_visible(False)
    axes[0].axhline(0, color="#16a34a", linestyle="--", linewidth=1.2)
    axes[1].legend(ncol=2, frameon=True)
    figure.tight_layout(rect=[0, 0, 1, 0.94])
    figure.savefig(output, dpi=180, bbox_inches="tight",
                   facecolor=figure.get_facecolor())
    plt.close(figure)


def plot_table(stats, budgets, instances, output):
    columns = ["Instância", "Ótimo"] + [f"{budget:g}s\nCusto / GAP" for budget in budgets]
    cells = []
    colors = []
    for instance in instances:
        optimum = stats[(instance, budgets[0])]["optimum"]
        row = [str(instance), str(optimum)]
        row_colors = ["#f8fafc", "#dbeafe"]
        best_cost = min(stats[(instance, budget)]["cost"] for budget in budgets)
        for budget in budgets:
            item = stats[(instance, budget)]
            row.append(f"{item['cost']:.2f} / {item['gap']:.2f}%")
            row_colors.append("#bbf7d0" if item["cost"] == best_cost else "#f1f5f9")
        cells.append(row)
        colors.append(row_colors)
    average = ["MÉDIA", "—"]
    average_colors = ["#1e293b", "#1e293b"]
    for budget in budgets:
        average.append(
            f"{mean(stats[(instance, budget)]['cost'] for instance in instances):.2f} / "
            f"{mean(stats[(instance, budget)]['gap'] for instance in instances):.2f}%"
        )
        average_colors.append("#c4b5fd")
    cells.append(average)
    colors.append(average_colors)

    figure, axis = plt.subplots(figsize=(17, 7.5))
    figure.patch.set_facecolor("#f8fafc")
    axis.axis("off")
    table = axis.table(cellText=cells, colLabels=columns, cellColours=colors,
                       cellLoc="center", loc="center")
    table.auto_set_font_size(False)
    table.set_fontsize(9.5)
    table.scale(1, 2.2)
    for column in range(len(columns)):
        table[0, column].set_facecolor("#1e293b")
        table[0, column].set_text_props(color="white", fontweight="bold")
        table[len(cells), column].set_text_props(
            color="white" if column < 2 else "#0f172a", fontweight="bold")
    figure.suptitle("Resultados da curva tempo × qualidade",
                    fontsize=18, fontweight="bold", y=0.95)
    figure.tight_layout(rect=[0.01, 0.03, 0.99, 0.91])
    figure.savefig(output, dpi=180, bbox_inches="tight",
                   facecolor=figure.get_facecolor())
    plt.close(figure)


def write_summary(stats, budgets, instances, rows, output):
    first = rows[0]
    configuration = (
        f"incremental={first.get('incremental_enabled', '?')}, "
        f"substituição={first.get('service_replace_enabled', '?')}, "
        f"lambda local={first.get('lambda_local_enabled', '?')}, "
        f"arrependimento={first.get('regret_enabled', '?')}"
    )
    lines = [
        "# Curva de tempo — GLS/GFLS promovida",
        "",
        f"- Instâncias: {', '.join(map(str, instances))}",
        f"- Repetições por orçamento: {len(set(row['repetition'] for row in rows))}",
        f"- Orçamentos: {', '.join(f'{budget:g}s' for budget in budgets)}",
        f"- Configuração: {configuration}.",
        "",
        "| Instância | Ótimo | " + " | ".join(f"Custo em {budget:g}s" for budget in budgets) + " |",
        "|---:|---:|" + "---:|" * len(budgets),
    ]
    for instance in instances:
        optimum = stats[(instance, budgets[0])]["optimum"]
        values = " | ".join(f"{stats[(instance, budget)]['cost']:.2f}"
                            for budget in budgets)
        lines.append(f"| {instance} | {optimum} | {values} |")
    lines.extend(["", "## GAP médio", ""])
    for budget in budgets:
        gap = mean(stats[(instance, budget)]["gap"] for instance in instances)
        lines.append(f"- {budget:g}s: {gap:.2f}%")
    solved = [instance for instance in instances
              if stats[(instance, budgets[-1])]["best"]
              <= stats[(instance, budgets[-1])]["optimum"]]
    lines.extend([
        "", "## Resultado", "",
        f"- Instâncias que atingiram o ótimo no maior orçamento: "
        f"{', '.join(map(str, solved)) if solved else 'nenhuma'}.",
        "- O tempo até a melhor solução está disponível no CSV para distinguir "
        "melhoria tardia de platô.",
        "",
    ])
    Path(output).write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", default="experiments/06_gls_melhorias/results/gls_time_curve_difficult5.csv")
    parser.add_argument("--output-dir", default="experiments/06_gls_melhorias/charts/gls_time_curve")
    parser.add_argument("--summary", default="experiments/06_gls_melhorias/results/gls_time_curve_difficult5_summary.md")
    parser.add_argument("--variant", default="regret")
    arguments = parser.parse_args()
    rows = load(arguments.csv, arguments.variant)
    stats, budgets = aggregate(rows)
    instances = sorted({row["instance"] for row in rows})
    output = Path(arguments.output_dir)
    output.mkdir(parents=True, exist_ok=True)
    plot_curve(stats, budgets, instances, output / "curva_tempo_gap.png")
    plot_table(stats, budgets, instances, output / "tabela_curva_tempo.png")
    write_summary(stats, budgets, instances, rows, arguments.summary)
    print(f"Gráficos salvos em {output}")
    print(f"Resumo salvo em {arguments.summary}")


if __name__ == "__main__":
    main()
