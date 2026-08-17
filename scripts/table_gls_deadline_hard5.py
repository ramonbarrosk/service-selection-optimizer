#!/usr/bin/env python3
"""Gera a tabela das cinco instancias dificeis para o GLS com deadline."""

from pathlib import Path

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np


GREEN, YELLOW, RED = "#d5f5e3", "#fef9e7", "#fdecea"
HEADER, SUMMARY = "#2c3e50", "#dfe6ea"


def gap(cost, optimum):
    return (cost - optimum) / optimum * 100.0


def gap_color(value):
    return GREEN if value <= 5 else YELLOW if value <= 20 else RED


def load(path):
    results = []
    for line in Path(path).read_text().splitlines():
        fields = line.split()
        if line.startswith("Instance_10_10_") and len(fields) >= 7:
            results.append({
                "instance": int(fields[0].split("_")[-1]),
                "optimal": float(fields[1]),
                "mean": float(fields[3]),
                "best": float(fields[4]),
                "time": float(fields[5]),
            })
    return results


def main():
    results = load("data/gls_deadline_hard5.txt")
    rows, colors = [], []
    for result in results:
        best_gap = gap(result["best"], result["optimal"])
        mean_gap = gap(result["mean"], result["optimal"])
        rows.append([
            f'#{result["instance"]}',
            f'{result["optimal"]:.0f}',
            f'{result["mean"]:.2f}',
            f'{mean_gap:.1f}%',
            f'{result["best"]:.0f}',
            f'{best_gap:.1f}%',
            f'{result["time"]:.2f}s',
        ])
        colors.append([
            "white", "#eef2f5", "white", gap_color(mean_gap),
            "white", gap_color(best_gap), "white",
        ])

    mean_cost = np.mean([result["best"] for result in results])
    mean_gap = np.mean([
        gap(result["best"], result["optimal"]) for result in results
    ])
    mean_time = np.mean([result["time"] for result in results])
    rows.append([
        "MEDIA", "", "", "", f"{mean_cost:.1f}", f"{mean_gap:.1f}%",
        f"{mean_time:.2f}s",
    ])
    colors.append([SUMMARY] * 7)

    headers = [
        "Instancia", "Otimo", "Custo medio", "GAP medio",
        "Melhor custo", "GAP melhor", "Tempo medio",
    ]
    fig, ax = plt.subplots(figsize=(13.5, 3.5))
    ax.axis("off")
    fig.suptitle(
        "GLS + GFLS com deadline — cinco instancias dificeis\n"
        "alpha=0.3 | rodadas 30/60/120 adaptadas ao tempo | 3 execucoes",
        fontweight="bold", fontsize=12, y=1.08,
    )
    table = ax.table(
        cellText=rows, colLabels=headers, cellColours=colors,
        cellLoc="center", loc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1, 1.75)
    for (row, _), cell in table.get_celld().items():
        cell.set_edgecolor("#c9d2da")
        if row == 0:
            cell.set_facecolor(HEADER)
            cell.set_text_props(color="white", fontweight="bold")
        elif row == len(rows):
            cell.set_text_props(fontweight="bold")

    legend = [
        mpatches.Patch(color=GREEN, label="GAP <= 5%"),
        mpatches.Patch(color=YELLOW, label="GAP <= 20%"),
        mpatches.Patch(color=RED, label="GAP > 20%"),
    ]
    fig.legend(
        handles=legend, loc="lower center", ncol=3, fontsize=9,
        frameon=True, bbox_to_anchor=(0.5, -0.04),
    )
    output = Path("data/charts_gls_deadline_hard5/table_gls_deadline_hard5.png")
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight", dpi=150)
    plt.close(fig)
    print(f"Salvo: {output}")


if __name__ == "__main__":
    main()
