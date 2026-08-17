#!/usr/bin/env python3
"""Tabela da triagem do GLS estrutural nas cinco instâncias difíceis."""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


CONFIGS = [
    ("Sem GLS", Path("data/gls_reform_hard5_no_gls.txt"), "2min13,67s"),
    ("GLS atual\nα=0,3", Path("data/gls_reform_hard5_control.txt"), "2min13,82s"),
    ("GLS estrutural\nα=1,0", Path("data/gls_reform_hard5_alpha1.txt"), "2min12,25s"),
    ("GLS estrutural\nα=2,0", Path("data/gls_reform_hard5_alpha2.txt"), "2min13,94s"),
]
HARD5 = [100, 129, 147, 28, 128]
OUTPUT = Path("data/charts_gls_reform/gls_structural_hard5.png")


def load(path):
    result = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if line.startswith("Instance_10_10_"):
            instance = int(fields[0].rsplit("_", 1)[1])
            result[instance] = {
                "optimum": float(fields[1]),
                "mean": float(fields[3]),
                "best": float(fields[4]),
            }
    return result


def gap(cost, optimum):
    return 100 * (cost - optimum) / optimum


def main():
    configs = [(name, load(path), elapsed) for name, path, elapsed in CONFIGS]
    rows, colors = [], []
    for instance in HARD5:
        optimum = configs[0][1][instance]["optimum"]
        best_values = [data[instance]["best"] for _, data, _ in configs]
        winner = int(np.argmin(best_values))
        row = [str(instance), f"{optimum:.0f}"]
        color = ["white", "#eef2f5"]
        for index, (_, data, _) in enumerate(configs):
            value = data[instance]["best"]
            row.extend([f"{value:.0f}", f"{gap(value, optimum):.1f}%"])
            shade = "#d5f5e3" if index == winner else "#fdecea"
            color.extend([shade, shade])
        rows.append(row)
        colors.append(color)

    summary = ["MÉDIA", ""]
    for _, data, _ in configs:
        best = np.mean([data[i]["best"] for i in HARD5])
        best_gap = np.mean([gap(data[i]["best"], data[i]["optimum"]) for i in HARD5])
        summary.extend([f"{best:.1f}", f"{best_gap:.2f}%"])
    rows.append(summary)
    colors.append(["#dfe6ea"] * 10)

    headers = ["Inst.", "Ótimo"]
    for name, _, _ in configs:
        headers.extend([f"{name}\nMelhor", "GAP"])

    fig, ax = plt.subplots(figsize=(19, 5.3))
    ax.axis("off")
    table = ax.table(cellText=rows, colLabels=headers, cellColours=colors,
                     cellLoc="center", loc="center")
    table.auto_set_font_size(False)
    table.set_fontsize(8.8)
    table.scale(1, 1.9)
    for (row, _), cell in table.get_celld().items():
        cell.set_edgecolor("#c7d1d9")
        if row == 0:
            cell.set_facecolor("#263746")
            cell.set_text_props(color="white", fontweight="bold")
        elif row == len(rows):
            cell.set_text_props(fontweight="bold")

    fig.suptitle(
        "TRIAGEM DO GLS ESTRUTURAL — CINCO INSTÂNCIAS DIFÍCEIS\n"
        "3 execuções, timeScale=0,80 | utilidade estrutural: capacidade=2,0, arrependimento=0,5\n"
        "Tempos: sem GLS 2:13,67 | atual 2:13,82 | α=1 2:12,25 | α=2 2:13,94",
        fontsize=13, fontweight="bold", y=1.08,
    )
    ax.text(
        0.5, -0.10,
        "Verde indica o menor custo por instância. α=1 melhora o melhor de três, "
        "mas α=2 diversifica agressivamente demais.",
        ha="center", transform=ax.transAxes, fontsize=9,
    )
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=170, bbox_inches="tight")
    plt.close(fig)
    print(f"Salvo: {OUTPUT}")


if __name__ == "__main__":
    main()
