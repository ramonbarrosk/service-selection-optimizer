#!/usr/bin/env python3
"""Gera a tabela da ablação da oscilação nas cinco instâncias difíceis."""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


WITH_OSC = Path("data/gls_hard5_with_osc.txt")
NO_OSC = Path("data/gls_hard5_no_osc.txt")
OUTPUT = Path("data/charts_gls_osc_ablation/table_gls_osc_ablation_hard5.png")
HARD5 = [100, 129, 147, 28, 128]
HEADER = "#263746"
GOOD = "#d5f5e3"
BAD = "#fdecea"
SUMMARY = "#dfe6ea"


def load(path):
    rows = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if line.startswith("Instance_10_10_") and len(fields) >= 7:
            instance = int(fields[0].rsplit("_", 1)[1])
            rows[instance] = {
                "optimum": float(fields[1]),
                "mean": float(fields[3]),
                "best": float(fields[4]),
            }
    return rows


def gap(cost, optimum):
    return 100.0 * (cost - optimum) / optimum


def fmt(value):
    return f"{value:.0f}" if value.is_integer() else f"{value:.1f}"


def main():
    with_osc = load(WITH_OSC)
    no_osc = load(NO_OSC)
    rows = []
    colors = []
    osc_gaps = []
    no_osc_gaps = []

    for instance in HARD5:
        osc = with_osc[instance]
        no = no_osc[instance]
        osc_gap = gap(osc["best"], osc["optimum"])
        no_gap = gap(no["best"], no["optimum"])
        osc_gaps.append(osc_gap)
        no_osc_gaps.append(no_gap)
        winner_osc = osc["best"] < no["best"]
        winner_no = no["best"] < osc["best"]
        rows.append([
            str(instance),
            f'{osc["optimum"]:.0f}',
            fmt(osc["mean"]),
            f'{osc["best"]:.0f}',
            f"{osc_gap:.1f}%",
            fmt(no["mean"]),
            f'{no["best"]:.0f}',
            f"{no_gap:.1f}%",
            f'{no["best"] - osc["best"]:+.0f}',
        ])
        colors.append([
            "white", "#eef2f5",
            GOOD if winner_osc else "white",
            GOOD if winner_osc else "white",
            GOOD if winner_osc else "white",
            GOOD if winner_no else BAD,
            GOOD if winner_no else BAD,
            GOOD if winner_no else BAD,
            BAD if no["best"] > osc["best"] else GOOD,
        ])

    osc_gap_mean = float(np.mean(osc_gaps))
    no_osc_gap_mean = float(np.mean(no_osc_gaps))
    osc_mean_cost = float(np.mean([with_osc[i]["mean"] for i in HARD5]))
    no_osc_mean_cost = float(np.mean([no_osc[i]["mean"] for i in HARD5]))
    best_penalty = float(np.mean([
        no_osc[i]["best"] - with_osc[i]["best"] for i in HARD5
    ]))
    rows.append([
        "MÉDIA", "", f"{osc_mean_cost:.1f}", "", f"{osc_gap_mean:.1f}%",
        f"{no_osc_mean_cost:.1f}", "", f"{no_osc_gap_mean:.1f}%", f"{best_penalty:+.1f}",
    ])
    colors.append([SUMMARY] * 9)

    fig, ax = plt.subplots(figsize=(16, 4.3))
    ax.axis("off")
    fig.suptitle(
        "ABLAÇÃO DA OSCILAÇÃO — Best-fit + FLS + ILS + GLS/GFLS\n"
        "Com OSC vs. sem OSC | 5 instâncias difíceis, 3 execuções, mesmos deadlines\n"
        f"Tempo externo: 2min44s em ambos | Com OSC venceu 5/5 | "
        f"GAP médio {osc_gap_mean:.1f}% → {no_osc_gap_mean:.1f}% sem OSC",
        fontsize=12,
        fontweight="bold",
        y=1.09,
    )
    headers = [
        "Instância", "Ótimo", "Com OSC\nMédia", "Com OSC\nMelhor", "GAP",
        "Sem OSC\nMédia", "Sem OSC\nMelhor", "GAP", "Perda\nsem OSC",
    ]
    table = ax.table(
        cellText=rows,
        colLabels=headers,
        cellColours=colors,
        cellLoc="center",
        loc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(9.5)
    table.scale(1, 1.75)
    for (row, _), cell in table.get_celld().items():
        cell.set_edgecolor("#c9d2da")
        if row == 0:
            cell.set_facecolor(HEADER)
            cell.set_text_props(color="white", fontweight="bold")
        elif row == len(rows):
            cell.set_text_props(fontweight="bold")

    ax.text(
        0.5,
        -0.08,
        "Perda sem OSC = melhor custo sem oscilação − melhor custo com oscilação; "
        "valores positivos indicam piora.",
        ha="center",
        va="top",
        transform=ax.transAxes,
        fontsize=9,
    )
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=160, bbox_inches="tight")
    plt.close(fig)
    print(f"Salvo: {OUTPUT}")
    print(
        f"GAP com OSC={osc_gap_mean:.3f}% sem OSC={no_osc_gap_mean:.3f}% "
        f"piora média do melhor custo={best_penalty:.3f}"
    )


if __name__ == "__main__":
    main()
