#!/usr/bin/env python3
"""Consolida a curva adaptativa das cinco instâncias que mantinham GAP."""

import csv
import os
import statistics
from collections import defaultdict
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


INSTANCES = [11, 28, 100, 128, 147]
BUDGETS = [0.55, 2.0, 5.0, 10.0, 30.0, 60.0, 120.0]
SOURCES = [
    Path("experiments/06_gls_melhorias/results/gls_promoted_all94.csv"),
    Path("experiments/06_gls_melhorias/results/gls_time_curve_remaining5_stage1.csv"),
    Path("experiments/06_gls_melhorias/results/gls_time_curve_instance100_stage2.csv"),
]
OUTPUT_DIR = Path("experiments/06_gls_melhorias/charts/gls_time_curve_remaining5")
SUMMARY = Path("experiments/06_gls_melhorias/results/gls_time_curve_remaining5_summary.md")
COLORS = ["#0891b2", "#2563eb", "#dc2626", "#7c3aed", "#ea580c"]


def mean(values):
    return statistics.fmean(values)


def load():
    rows = []
    for source in SOURCES:
        with source.open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                if int(row["instance"]) not in INSTANCES:
                    continue
                if row["variant"] != "i1_s1_l0_r1":
                    continue
                row["instance"] = int(row["instance"])
                row["repetition"] = int(row["repetition"])
                row["optimum"] = int(row["optimum"])
                for field in ("budget_seconds", "cost", "gap_pct", "time_ms",
                              "time_to_best_ms"):
                    row[field] = float(row[field])
                rows.append(row)
    return rows


def consolidate(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["instance"], row["budget_seconds"])].append(row)

    values = {}
    for key, group in grouped.items():
        values[key] = {
            "cost": mean(row["cost"] for row in group),
            "gap": mean(row["gap_pct"] for row in group),
            "optimum": group[0]["optimum"],
            "measured": True,
        }

    # Depois de atingir o ótimo, orçamentos maiores devolveriam a mesma solução
    # e terminariam antecipadamente; carregamos esse ótimo sem executar novamente.
    for instance in INSTANCES:
        optimum = next(row["optimum"] for row in rows if row["instance"] == instance)
        last = None
        for budget in BUDGETS:
            key = (instance, budget)
            if key in values:
                last = values[key]
            elif last is not None and last["cost"] <= optimum:
                values[key] = {
                    "cost": float(optimum), "gap": 0.0,
                    "optimum": optimum, "measured": False,
                }
                last = values[key]
    return values


def plot_curve(values):
    figure, axis = plt.subplots(figsize=(13, 7.5))
    figure.patch.set_facecolor("#f8fafc")
    for instance, color in zip(INSTANCES, COLORS):
        gaps = [values[(instance, budget)]["gap"] for budget in BUDGETS]
        axis.plot(BUDGETS, gaps, marker="o", linewidth=2.4, color=color,
                  label=f"Instância {instance}")
    axis.set_title("Curva adaptativa das cinco instâncias com GAP",
                   fontsize=18, fontweight="bold", pad=14)
    axis.set_xlabel("Orçamento máximo por execução (s)")
    axis.set_ylabel("GAP (%)")
    axis.set_xscale("log")
    axis.set_xticks(BUDGETS, [f"{budget:g}" for budget in BUDGETS])
    axis.axhline(0, color="#16a34a", linestyle="--", linewidth=1.2)
    axis.grid(True, color="#e2e8f0")
    axis.spines[["top", "right"]].set_visible(False)
    axis.legend(ncol=2)
    figure.text(
        0.5, 0.015,
        "Após atingir o ótimo, a instância é encerrada e não recebe os orçamentos maiores.",
        ha="center", color="#475569",
    )
    figure.tight_layout(rect=[0, 0.04, 1, 1])
    figure.savefig(OUTPUT_DIR / "curva_gap_ate_120s.png", dpi=180,
                   bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def plot_table(values):
    columns = ["Instância", "Ótimo"] + [f"{budget:g}s" for budget in BUDGETS]
    cells = []
    cell_colors = []
    for instance in INSTANCES:
        optimum = values[(instance, BUDGETS[0])]["optimum"]
        row = [str(instance), str(optimum)]
        colors = ["#f8fafc", "#dbeafe"]
        for budget in BUDGETS:
            item = values[(instance, budget)]
            row.append(f"{item['cost']:.2f}\n{item['gap']:.2f}%")
            colors.append("#bbf7d0" if item["gap"] <= 1e-9 else "#fee2e2")
        cells.append(row)
        cell_colors.append(colors)
    average = ["MÉDIA", "—"]
    average_colors = ["#1e293b", "#1e293b"]
    for budget in BUDGETS:
        average.append(
            f"{mean(values[(instance, budget)]['cost'] for instance in INSTANCES):.2f}\n"
            f"{mean(values[(instance, budget)]['gap'] for instance in INSTANCES):.2f}%"
        )
        average_colors.append("#c4b5fd")
    cells.append(average)
    cell_colors.append(average_colors)

    figure, axis = plt.subplots(figsize=(17, 7.5))
    figure.patch.set_facecolor("#f8fafc")
    axis.axis("off")
    table = axis.table(cellText=cells, colLabels=columns, cellColours=cell_colors,
                       cellLoc="center", loc="center")
    table.auto_set_font_size(False)
    table.set_fontsize(9.5)
    table.scale(1, 2.15)
    for column in range(len(columns)):
        table[0, column].set_facecolor("#1e293b")
        table[0, column].set_text_props(color="white", fontweight="bold")
        table[len(cells), column].set_text_props(
            color="white" if column < 2 else "#0f172a", fontweight="bold")
    figure.suptitle("Custo e GAP com orçamento de até 120 segundos",
                    fontsize=18, fontweight="bold", y=0.95)
    figure.tight_layout(rect=[0.01, 0.03, 0.99, 0.91])
    figure.savefig(OUTPUT_DIR / "tabela_curva_ate_120s.png", dpi=180,
                   bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def write_summary(rows, values):
    stage_rows = [row for row in rows if row["budget_seconds"] >= 2]
    lines = [
        "# Curva adaptativa das cinco instâncias restantes",
        "",
        "- Configuração: I1 S1 L0 R1.",
        "- Uma repetição por orçamento.",
        "- Primeira etapa: 2s, 5s, 10s e 30s nas cinco instâncias.",
        "- Segunda etapa: 60s e 120s somente na instância 100.",
        f"- Tempo interno adicional do experimento: "
        f"{sum(row['time_ms'] for row in stage_rows) / 1000:.2f}s.",
        "",
        "| Instância | Ótimo | " + " | ".join(f"{budget:g}s" for budget in BUDGETS) + " |",
        "|---:|---:|" + "---:|" * len(BUDGETS),
    ]
    for instance in INSTANCES:
        optimum = values[(instance, BUDGETS[0])]["optimum"]
        costs = " | ".join(f"{values[(instance, budget)]['cost']:.2f}"
                           for budget in BUDGETS)
        lines.append(f"| {instance} | {optimum} | {costs} |")
    lines.extend(["", "## GAP médio", ""])
    for budget in BUDGETS:
        gap = mean(values[(instance, budget)]["gap"] for instance in INSTANCES)
        lines.append(f"- {budget:g}s: {gap:.2f}%")
    lines.extend([
        "", "## Conclusão", "",
        "- As instâncias 11 e 147 atingiram o ótimo com até 2 segundos.",
        "- As instâncias 28 e 128 atingiram o ótimo com até 5 segundos.",
        "- A instância 100 melhorou para custo 104 em 30 segundos, mas permaneceu "
        "em 104 também com 60 e 120 segundos; o ótimo conhecido é 101.",
        "- Portanto, aumentar apenas o tempo zerou quatro dos cinco GAPs, mas não "
        "superou o platô estrutural da instância 100.",
        "",
    ])
    SUMMARY.write_text("\n".join(lines), encoding="utf-8")


def main():
    rows = load()
    values = consolidate(rows)
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    plot_curve(values)
    plot_table(values)
    write_summary(rows, values)
    print(f"Gráficos salvos em {OUTPUT_DIR}")
    print(f"Resumo salvo em {SUMMARY}")


if __name__ == "__main__":
    main()
