#!/usr/bin/env python3
"""Gera gráficos da ablação incremental da GLS/GFLS nas cinco difíceis."""

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
import numpy as np


VARIANTS = [
    ("gls", "GLS"),
    ("gfls", "+GFLS"),
    ("incremental", "+Avaliação\nincremental"),
    ("service_replace", "+Substituição\nde serviço"),
    ("lambda_local", "+Lambda\nlocal"),
    ("regret", "+Utilidade por\narrependimento"),
    ("top1", "+Penalização\ntop-1"),
    ("osc_cond", "+Oscilação\ncondicional"),
]
INSTANCES = [28, 100, 128, 129, 147]
COLORS = ["#64748b", "#2563eb", "#0891b2", "#0f766e", "#7c3aed", "#db2777", "#ea580c", "#16a34a"]


def mean(values):
    return statistics.fmean(values)


def load(path):
    with Path(path).open(newline="", encoding="utf-8") as source:
        raw = list(csv.DictReader(source))
    if not raw:
        raise ValueError(f"CSV vazio: {path}")
    rows = []
    numeric_float = {
        "budget_seconds", "initial_cost", "cost", "gap_pct", "time_ms", "lambda"
    }
    numeric_int = {
        "instance", "repetition", "seed", "optimum", "feasible", "candidates",
        "rounds", "moves", "swaps", "rejected_capacity", "rejected_sla",
        "rejected_smax", "service_replacements", "oscillation_calls",
        "successful_oscillations",
    }
    for item in raw:
        for field in numeric_float:
            item[field] = float(item[field])
        for field in numeric_int:
            item[field] = int(item[field])
        rows.append(item)
    return rows


def aggregate(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[(row["instance"], row["variant"])].append(row)

    result = {}
    for key, group in grouped.items():
        result[key] = {
            "cost": mean(row["cost"] for row in group),
            "best": min(row["cost"] for row in group),
            "gap": mean(row["gap_pct"] for row in group),
            "time": mean(row["time_ms"] for row in group),
            "candidates_per_second": mean(
                row["candidates"] / max(row["time_ms"] / 1000.0, 1e-9)
                for row in group
            ),
            "rounds_per_second": mean(
                row["rounds"] / max(row["time_ms"] / 1000.0, 1e-9)
                for row in group
            ),
            "oscillations": sum(row["oscillation_calls"] for row in group),
            "successful_oscillations": sum(
                row["successful_oscillations"] for row in group
            ),
            "rejected_capacity": mean(row["rejected_capacity"] for row in group),
            "rejected_sla": mean(row["rejected_sla"] for row in group),
            "rejected_smax": mean(row["rejected_smax"] for row in group),
        }
    return result


def style(axis, title, ylabel):
    axis.set_title(title, fontsize=13, fontweight="bold", pad=12)
    axis.set_ylabel(ylabel)
    axis.grid(axis="y", color="#e2e8f0", linewidth=0.8)
    axis.set_axisbelow(True)
    axis.spines[["top", "right"]].set_visible(False)


def plot_cost_and_contribution(stats, output):
    keys = [key for key, _ in VARIANTS]
    labels = [label for _, label in VARIANTS]
    average_costs = [mean(stats[(instance, key)]["cost"] for instance in INSTANCES)
                     for key in keys]
    average_gaps = [mean(stats[(instance, key)]["gap"] for instance in INSTANCES)
                    for key in keys]
    contributions = [0.0] + [average_costs[index - 1] - average_costs[index]
                             for index in range(1, len(keys))]

    figure, axes = plt.subplots(1, 2, figsize=(18, 7.5))
    figure.patch.set_facecolor("#f8fafc")
    figure.suptitle(
        "Contribuição incremental das melhorias da GLS/GFLS",
        fontsize=18, fontweight="bold", y=0.98,
    )

    bars = axes[0].bar(range(len(keys)), average_costs, color=COLORS, width=0.7)
    style(axes[0], "Custo médio nas cinco instâncias", "Custo médio")
    axes[0].set_xticks(range(len(keys)), labels, fontsize=8.5)
    lower = min(average_costs) - 3
    axes[0].set_ylim(max(0, lower), max(average_costs) + 5)
    for bar, cost, gap in zip(bars, average_costs, average_gaps):
        axes[0].text(bar.get_x() + bar.get_width() / 2, cost + 0.35,
                     f"{cost:.2f}\nGAP {gap:.2f}%", ha="center", va="bottom",
                     fontsize=8.5, fontweight="bold")

    contribution_colors = ["#94a3b8"] + [
        "#16a34a" if value > 0 else "#dc2626" if value < 0 else "#94a3b8"
        for value in contributions[1:]
    ]
    contribution_bars = axes[1].bar(
        range(1, len(keys)), contributions[1:],
        color=contribution_colors[1:], width=0.68,
    )
    style(axes[1], "Contribuição marginal de cada etapa", "Redução de custo médio")
    axes[1].axhline(0, color="#334155", linewidth=1)
    axes[1].set_xticks(range(1, len(keys)), labels[1:], fontsize=8.5)
    maximum = max([abs(value) for value in contributions[1:]] + [1.0])
    axes[1].set_ylim(-maximum * 1.35, maximum * 1.35)
    for bar, value in zip(contribution_bars, contributions[1:]):
        vertical = 0.08 * maximum if value >= 0 else -0.08 * maximum
        axes[1].text(
            bar.get_x() + bar.get_width() / 2, value + vertical,
            f"{value:+.2f}", ha="center",
            va="bottom" if value >= 0 else "top", fontweight="bold",
        )

    figure.text(
        0.5, 0.02,
        "Barras verdes reduzem o custo; barras vermelhas indicam regressão. "
        "Todas as etapas usam o mesmo orçamento por execução.",
        ha="center", fontsize=10, color="#475569",
    )
    figure.tight_layout(rect=[0, 0.05, 1, 0.94])
    figure.savefig(output, dpi=180, bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def plot_per_instance(stats, output):
    keys = [key for key, _ in VARIANTS]
    labels = [label.replace("\n", " ") for _, label in VARIANTS]
    figure, axis = plt.subplots(figsize=(16, 8))
    figure.patch.set_facecolor("#f8fafc")
    markers = ["o", "s", "^", "D", "P"]
    colors = ["#2563eb", "#dc2626", "#7c3aed", "#ea580c", "#0891b2"]
    for instance, marker, color in zip(INSTANCES, markers, colors):
        gaps = [stats[(instance, key)]["gap"] for key in keys]
        axis.plot(range(len(keys)), gaps, marker=marker, markersize=7,
                  linewidth=2.2, color=color, label=f"Instância {instance}")
    style(axis, "Evolução do GAP em cada instância difícil", "GAP médio (%)")
    axis.set_xticks(range(len(keys)), labels, rotation=12, ha="right")
    axis.legend(ncol=3, frameon=True)
    axis.axhline(0, color="#16a34a", linewidth=1.2, linestyle="--")
    figure.tight_layout()
    figure.savefig(output, dpi=180, bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def plot_efficiency(stats, output):
    keys = [key for key, _ in VARIANTS]
    labels = [label for _, label in VARIANTS]
    candidates = [mean(stats[(instance, key)]["candidates_per_second"]
                       for instance in INSTANCES) for key in keys]
    rounds = [mean(stats[(instance, key)]["rounds_per_second"]
                   for instance in INSTANCES) for key in keys]

    figure, axes = plt.subplots(1, 2, figsize=(17, 6.8))
    figure.patch.set_facecolor("#f8fafc")
    figure.suptitle("Efeito das melhorias na capacidade de busca",
                    fontsize=18, fontweight="bold", y=0.98)
    first = axes[0].bar(range(len(keys)), np.array(candidates) / 1_000_000,
                        color=COLORS)
    style(axes[0], "Candidatos avaliados por segundo", "Milhões de candidatos/s")
    axes[0].set_xticks(range(len(keys)), labels, fontsize=8.2)
    for bar in first:
        axes[0].text(bar.get_x() + bar.get_width()/2, bar.get_height(),
                     f"{bar.get_height():.1f}", ha="center", va="bottom",
                     fontsize=8, fontweight="bold")

    second = axes[1].bar(range(len(keys)), rounds, color=COLORS)
    style(axes[1], "Rodadas de penalização por segundo", "Rodadas/s")
    axes[1].set_xticks(range(len(keys)), labels, fontsize=8.2)
    for bar in second:
        axes[1].text(bar.get_x() + bar.get_width()/2, bar.get_height(),
                     f"{bar.get_height():.0f}", ha="center", va="bottom",
                     fontsize=8, fontweight="bold")
    figure.tight_layout(rect=[0, 0, 1, 0.93])
    figure.savefig(output, dpi=180, bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def plot_blockers(stats, output):
    key = "incremental"
    capacity = []
    sla = []
    smax = []
    for instance in INSTANCES:
        item = stats[(instance, key)]
        total = (item["rejected_capacity"] + item["rejected_sla"]
                 + item["rejected_smax"])
        divisor = max(total, 1.0)
        capacity.append(100 * item["rejected_capacity"] / divisor)
        sla.append(100 * item["rejected_sla"] / divisor)
        smax.append(100 * item["rejected_smax"] / divisor)

    figure, axis = plt.subplots(figsize=(12.5, 7))
    figure.patch.set_facecolor("#f8fafc")
    positions = np.arange(len(INSTANCES))
    axis.bar(positions, capacity, color="#f59e0b", label="Capacidade")
    axis.bar(positions, smax, bottom=capacity, color="#7c3aed", label="Smax")
    axis.bar(positions, sla, bottom=np.array(capacity) + np.array(smax),
             color="#0891b2", label="SLA")
    style(axis, "Restrições que bloquearam movimentos promissores",
          "Participação nos bloqueios (%)")
    axis.set_xticks(positions, [f"Instância {instance}" for instance in INSTANCES])
    axis.set_ylim(0, 100)
    axis.legend(ncol=3, loc="upper center", bbox_to_anchor=(0.5, 1.10))
    figure.text(
        0.5, 0.025,
        "Medição da configuração promovida: GFLS com avaliação incremental. "
        "A SLA não bloqueou candidatos promissores neste protocolo.",
        ha="center", fontsize=9.5, color="#475569",
    )
    figure.tight_layout(rect=[0, 0.05, 1, 0.94])
    figure.savefig(output, dpi=180, bbox_inches="tight",
                   facecolor=figure.get_facecolor())
    plt.close(figure)


def make_table(stats, repetitions, budget, output):
    keys = [key for key, _ in VARIANTS]
    short_labels = [label.replace("\n", " ") for _, label in VARIANTS]
    columns = ["Inst.", "Ótimo"] + short_labels
    cells = []
    colors = []
    optimums = {}
    for instance in INSTANCES:
        optimum = round(stats[(instance, keys[0])]["cost"] /
                        (1 + stats[(instance, keys[0])]["gap"] / 100))
        optimums[instance] = optimum
        row = [str(instance), str(optimum)]
        row_colors = ["#f8fafc", "#dbeafe"]
        best = min(stats[(instance, key)]["cost"] for key in keys)
        for key, color in zip(keys, COLORS):
            value = stats[(instance, key)]
            row.append(f"{value['cost']:.2f}\n({value['gap']:.2f}%)")
            row_colors.append("#bbf7d0" if abs(value["cost"] - best) < 1e-9 else color + "33")
        cells.append(row)
        colors.append(row_colors)

    average = ["MÉDIA", "—"]
    average_colors = ["#1e293b", "#1e293b"]
    for key, color in zip(keys, COLORS):
        average.append(
            f"{mean(stats[(instance, key)]['cost'] for instance in INSTANCES):.2f}\n"
            f"({mean(stats[(instance, key)]['gap'] for instance in INSTANCES):.2f}%)"
        )
        average_colors.append(color + "55")
    cells.append(average)
    colors.append(average_colors)

    figure, axis = plt.subplots(figsize=(20, 8.3))
    figure.patch.set_facecolor("#f8fafc")
    axis.axis("off")
    table = axis.table(cellText=cells, colLabels=columns, cellColours=colors,
                       cellLoc="center", loc="center")
    table.auto_set_font_size(False)
    table.set_fontsize(8.4)
    table.scale(1, 2.2)
    for column in range(len(columns)):
        table[0, column].set_facecolor("#1e293b")
        table[0, column].set_text_props(color="white", fontweight="bold")
        table[len(cells), column].set_text_props(
            color="white" if column < 2 else "#0f172a", fontweight="bold"
        )
    figure.suptitle("Ablação da GLS/GFLS nas cinco instâncias mais difíceis",
                    fontsize=18, fontweight="bold", y=0.96)
    figure.text(0.5, 0.89,
                f"{repetitions} repetições × {budget:.3f}s por variante — célula: custo médio (GAP)",
                ha="center", fontsize=11, color="#475569")
    figure.tight_layout(rect=[0.01, 0.03, 0.99, 0.91])
    figure.savefig(output, dpi=180, bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def write_summary(stats, rows, output):
    keys = [key for key, _ in VARIANTS]
    labels = [label.replace("\n", " ") for _, label in VARIANTS]
    lines = [
        "# Ablação das melhorias GLS/GFLS — cinco instâncias difíceis",
        "",
        f"- Instâncias: {', '.join(map(str, INSTANCES))}",
        f"- Repetições: {len(set(row['repetition'] for row in rows))}",
        f"- Orçamento por variante: {rows[0]['budget_seconds']:.3f} s",
        "- SLA, capacidade e Smax permaneceram restrições duras.",
        "",
        "| Etapa | Custo médio | GAP médio | Candidatos/s | Rodadas/s |",
        "|---|---:|---:|---:|---:|",
    ]
    average_costs = []
    for key, label in zip(keys, labels):
        cost = mean(stats[(instance, key)]["cost"] for instance in INSTANCES)
        average_costs.append(cost)
        gap = mean(stats[(instance, key)]["gap"] for instance in INSTANCES)
        candidates = mean(stats[(instance, key)]["candidates_per_second"]
                          for instance in INSTANCES)
        rounds = mean(stats[(instance, key)]["rounds_per_second"]
                      for instance in INSTANCES)
        lines.append(f"| {label} | {cost:.2f} | {gap:.2f}% | {candidates:.0f} | {rounds:.1f} |")
    lines.extend(["", "## Contribuição marginal", ""])
    for index in range(1, len(keys)):
        contribution = average_costs[index - 1] - average_costs[index]
        lines.append(f"- {labels[index]}: {contribution:+.2f} unidade(s) de custo médio.")
    calls = sum(row["oscillation_calls"] for row in rows if row["variant"] == "osc_cond")
    successes = sum(row["successful_oscillations"] for row in rows
                    if row["variant"] == "osc_cond")
    lines.extend([
        "", "## Oscilação condicional", "",
        f"- Chamadas: {calls}",
        f"- Chamadas que melhoraram a solução corrente: {successes}",
        "- O gatilho não disparou porque capacidade não representou 60% dos "
        "bloqueios; Smax foi o bloqueador dominante.",
        "- A SLA não bloqueou movimentos promissores nas cinco instâncias deste protocolo.",
        "",
        "## Decisão para a configuração padrão", "",
        "- Promovidos provisoriamente: GFLS, avaliação incremental, substituição "
        "composta de serviço, lambda local e utilidade por arrependimento.",
        "- A utilidade por arrependimento só apresentou ganho forte depois da "
        "inclusão da vizinhança composta, indicando interação entre as melhorias.",
        "- Mantidos somente como opções experimentais: penalização top-1 e "
        "oscilação condicional, pois não melhoraram o resultado agregado.",
        "- A configuração promovida ainda precisa ser validada nas 94 instâncias.",
        "- As repetições medem variação de tempo; a trajetória atual é majoritariamente "
        "determinística nestas cinco instâncias.",
        "",
    ])
    Path(output).write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", default="data/experiments/gls_improvements_difficult5.csv")
    parser.add_argument("--output-dir", default="data/charts_gls_improvements")
    parser.add_argument("--summary", default="data/experiments/gls_improvements_difficult5_summary.md")
    arguments = parser.parse_args()
    rows = load(arguments.csv)
    stats = aggregate(rows)
    output = Path(arguments.output_dir)
    output.mkdir(parents=True, exist_ok=True)
    repetitions = len(set(row["repetition"] for row in rows))
    budget = rows[0]["budget_seconds"]
    plot_cost_and_contribution(stats, output / "contribuicao_melhorias.png")
    plot_per_instance(stats, output / "evolucao_gap_por_instancia.png")
    plot_efficiency(stats, output / "eficiencia_busca.png")
    plot_blockers(stats, output / "bloqueios_restricoes.png")
    make_table(stats, repetitions, budget, output / "tabela_comparativa.png")
    write_summary(stats, rows, arguments.summary)
    print(f"Gráficos salvos em {output}")
    print(f"Resumo salvo em {arguments.summary}")


if __name__ == "__main__":
    main()
