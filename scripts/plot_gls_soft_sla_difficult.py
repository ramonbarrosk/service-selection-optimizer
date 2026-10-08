#!/usr/bin/env python3
"""Gera a tabela-imagem das cinco instâncias difíceis no experimento de SLA."""

import argparse
import csv
import os
import statistics
from collections import defaultdict
from pathlib import Path

# Impede que o Matplotlib grave configurações no diretório pessoal.
os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


DIFFICULT_INSTANCES = [28, 100, 128, 129, 147]
METHODS = [
    ("strict", "GLS estrita", "#dbeafe"),
    ("soft", "GLS sempre suave", "#dcfce7"),
    ("conditional", "GLS condicional", "#fef3c7"),
]


def mean(values):
    return statistics.fmean(values)


def load_and_aggregate(path):
    with Path(path).open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))

    grouped = defaultdict(list)
    for row in rows:
        instance = int(row["instance"])
        if instance in DIFFICULT_INSTANCES:
            grouped[instance].append(row)

    missing = [instance for instance in DIFFICULT_INSTANCES if instance not in grouped]
    if missing:
        raise ValueError(f"Instâncias ausentes no CSV: {missing}")

    result = []
    for instance in DIFFICULT_INSTANCES:
        group = grouped[instance]
        item = {
            "instance": instance,
            "optimum": int(group[0]["optimum"]),
            "repetitions": len(group),
        }
        for key, _, _ in METHODS:
            item[key] = {
                "cost": mean(float(row[f"{key}_cost"]) for row in group),
                "gap": mean(float(row[f"{key}_gap_pct"]) for row in group),
                "time": mean(float(row[f"{key}_time_ms"]) for row in group),
            }
        result.append(item)

    soft_crossings = sum(
        int(row["soft_accepted_infeasible"])
        for row in rows
        if int(row["instance"]) in DIFFICULT_INSTANCES
    )
    conditional_crossings = sum(
        int(row["conditional_accepted_infeasible"])
        for row in rows
        if int(row["instance"]) in DIFFICULT_INSTANCES
    )
    return result, soft_crossings, conditional_crossings


def create_image(instances, soft_crossings, conditional_crossings, output_path):
    columns = ["Instância", "Ótimo"]
    for _, label, _ in METHODS:
        columns.extend([f"{label}\nCusto", "GAP", "Tempo"])

    table_rows = []
    row_colors = []
    for item in instances:
        row = [str(item["instance"]), str(item["optimum"])]
        colors = ["#f8fafc", "#e0f2fe"]
        best_cost = min(item[key]["cost"] for key, _, _ in METHODS)
        for key, _, method_color in METHODS:
            stats = item[key]
            row.extend([
                f"{stats['cost']:.2f}",
                f"{stats['gap']:.2f}%",
                f"{stats['time'] / 1000.0:.3f}s",
            ])
            cost_color = "#bbf7d0" if abs(stats["cost"] - best_cost) < 1e-9 else method_color
            colors.extend([cost_color, method_color, "#f1f5f9"])
        table_rows.append(row)
        row_colors.append(colors)

    average_row = ["MÉDIA", "—"]
    average_colors = ["#1e293b", "#1e293b"]
    for key, _, method_color in METHODS:
        average_row.extend([
            f"{mean(item[key]['cost'] for item in instances):.2f}",
            f"{mean(item[key]['gap'] for item in instances):.2f}%",
            f"{mean(item[key]['time'] for item in instances) / 1000.0:.3f}s",
        ])
        average_colors.extend([method_color, method_color, "#e2e8f0"])
    table_rows.append(average_row)
    row_colors.append(average_colors)

    figure, axis = plt.subplots(figsize=(20, 7.8))
    figure.patch.set_facecolor("#f8fafc")
    axis.axis("off")
    figure.suptitle(
        "Comparação nas cinco instâncias mais difíceis",
        fontsize=19,
        fontweight="bold",
        color="#0f172a",
        y=0.97,
    )
    figure.text(
        0.5,
        0.91,
        "GLS estrita × GLS sempre suave × GLS suave condicional — 10 repetições por instância",
        ha="center",
        fontsize=11,
        color="#475569",
    )

    table = axis.table(
        cellText=table_rows,
        colLabels=columns,
        cellColours=row_colors,
        cellLoc="center",
        loc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(9.2)
    table.scale(1.0, 2.15)

    for column in range(len(columns)):
        header = table[0, column]
        header.set_facecolor("#1e293b")
        header.set_text_props(color="white", fontweight="bold")

    last_row = len(table_rows)
    for column in range(len(columns)):
        table[last_row, column].set_text_props(
            color="white" if column < 2 else "#0f172a",
            fontweight="bold",
        )

    figure.text(
        0.5,
        0.075,
        f"Travessias da SLA nas cinco difíceis: suave = {soft_crossings}; "
        f"condicional = {conditional_crossings}.",
        ha="center",
        fontsize=10.5,
        fontweight="bold",
        color="#334155",
    )
    figure.text(
        0.5,
        0.04,
        "Resultado: empate nas cinco instâncias; o ganho do relaxamento ocorreu na instância 143, fora deste conjunto.",
        ha="center",
        fontsize=10,
        color="#64748b",
    )

    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output, dpi=180, bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)
    print(f"Imagem salva em {output}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        default="data/experiments/gls_soft_sla_difficult_10runs.csv",
    )
    parser.add_argument(
        "--output",
        default="data/charts_gls_soft_sla/comparacao_5_dificeis_10runs.png",
    )
    arguments = parser.parse_args()
    instances, soft_crossings, conditional_crossings = load_and_aggregate(
        arguments.input
    )
    create_image(instances, soft_crossings, conditional_crossings, arguments.output)


if __name__ == "__main__":
    main()
