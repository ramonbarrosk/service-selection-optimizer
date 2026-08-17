#!/usr/bin/env python3
"""Visualiza a contribuição observada de cada etapa nas cinco difíceis."""

from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


INPUT = Path("data/gls_hard5_stage_trace.txt")
OUTPUT = Path("data/charts_gls_stage_power/gls_stage_power_hard5.png")
CSV_OUTPUT = Path("data/gls_hard5_stage_power.csv")
HARD5 = [100, 129, 147, 28, 128]


def load_best_trajectory_per_repetition():
    grouped = defaultdict(list)
    numeric = {
        "optimum", "bestFit", "initialLocal", "initialOsc", "final",
        "normalLocalGain", "normalOscGain", "glsDirectGain",
        "glsLocalPolishGain", "glsOscPolishGain", "glsCalls",
    }
    for line in INPUT.read_text().splitlines():
        if not line.startswith("STAGE_TRACE "):
            continue
        row = {}
        for field in line.split()[1:]:
            key, value = field.split("=", 1)
            row[key] = float(value) if key in numeric else value
        instance = int(row["instance"].rsplit("_", 1)[1])
        repetition = int(row["repetition"])
        grouped[(instance, repetition)].append(row)

    selected = defaultdict(list)
    for (instance, _), candidates in grouped.items():
        selected[instance].append(min(candidates, key=lambda row: row["final"]))
    return selected


def mean(rows, key):
    return float(np.mean([row[key] for row in rows]))


def main():
    selected = load_best_trajectory_per_repetition()
    summary = []
    for instance in HARD5:
        rows = selected[instance]
        best_fit = mean(rows, "bestFit")
        # Ganhos de recorde são aditivos. O polimento local/OSC após uma chamada
        # GLS é creditado ao mecanismo que efetivamente reduziu o custo.
        local_gain = float(np.mean([
            row["bestFit"] - row["initialLocal"]
            + row["normalLocalGain"] + row["glsLocalPolishGain"]
            for row in rows
        ]))
        osc_gain = float(np.mean([
            row["initialLocal"] - row["initialOsc"]
            + row["normalOscGain"] + row["glsOscPolishGain"]
            for row in rows
        ]))
        gls_gain = mean(rows, "glsDirectGain")
        final = mean(rows, "final")
        optimum = mean(rows, "optimum")
        calls = mean(rows, "glsCalls")
        assert abs(best_fit - local_gain - osc_gain - gls_gain - final) < 1e-8
        summary.append({
            "instance": instance, "optimum": optimum, "best_fit": best_fit,
            "local_gain": local_gain, "osc_gain": osc_gain,
            "gls_gain": gls_gain, "final": final, "gls_calls": calls,
        })

    CSV_OUTPUT.write_text(
        "instance,optimum,best_fit,local_gain,oscillation_gain,"
        "direct_gls_gain,final_mean,mean_gls_calls\n"
        + "".join(
            f'{r["instance"]},{r["optimum"]:.0f},{r["best_fit"]:.3f},'
            f'{r["local_gain"]:.3f},{r["osc_gain"]:.3f},'
            f'{r["gls_gain"]:.3f},{r["final"]:.3f},{r["gls_calls"]:.3f}\n'
            for r in summary
        )
    )

    fig = plt.figure(figsize=(16, 9.5))
    grid = fig.add_gridspec(2, 1, height_ratios=[1.25, 1], hspace=0.30)
    ax = fig.add_subplot(grid[0])
    table_ax = fig.add_subplot(grid[1])

    y = np.arange(len(summary))
    final = np.array([r["final"] for r in summary])
    gls = np.array([r["gls_gain"] for r in summary])
    osc = np.array([r["osc_gain"] for r in summary])
    local = np.array([r["local_gain"] for r in summary])
    optimum = np.array([r["optimum"] for r in summary])

    ax.barh(y, final, color="#d9e1e8", edgecolor="white", label="Custo final")
    ax.barh(y, gls, left=final, color="#8e63ce", edgecolor="white",
            label="Redução direta GLS/GFLS")
    ax.barh(y, osc, left=final + gls, color="#f39c3d", edgecolor="white",
            label="Redução por oscilação")
    ax.barh(y, local, left=final + gls + osc, color="#3f86c5", edgecolor="white",
            label="Redução por busca local/FLS")
    ax.scatter(optimum, y, marker="D", s=45, color="#111111", zorder=5,
               label="Ótimo conhecido")
    for index, row in enumerate(summary):
        ax.text(row["final"] / 2, index, f'final {row["final"]:.1f}',
                ha="center", va="center", fontsize=9, fontweight="bold")
        ax.text(row["best_fit"] + 1.5, index, f'BF {row["best_fit"]:.0f}',
                ha="left", va="center", fontsize=9)
    ax.set_yticks(y, [str(r["instance"]) for r in summary])
    ax.invert_yaxis()
    ax.set_xlabel("Custo médio — menor é melhor")
    ax.set_ylabel("Instância")
    ax.set_title(
        "Decomposição do custo do Best-Fit: o trecho colorido mostra quem reduziu o custo",
        fontsize=12, fontweight="bold",
    )
    ax.grid(axis="x", alpha=0.18)
    ax.legend(loc="lower center", bbox_to_anchor=(0.5, 1.01), ncol=4,
              frameon=False, fontsize=9)

    headers = [
        "Inst.", "Ótimo", "Best-Fit", "Ganho local", "Ganho OSC",
        "Ganho GLS direto", "Final médio", "Redução total", "Chamadas GLS",
    ]
    cells = []
    for row in summary:
        cells.append([
            str(row["instance"]), f'{row["optimum"]:.0f}',
            f'{row["best_fit"]:.1f}', f'-{row["local_gain"]:.1f}',
            f'-{row["osc_gain"]:.1f}', f'-{row["gls_gain"]:.1f}',
            f'{row["final"]:.1f}', f'-{row["best_fit"] - row["final"]:.1f}',
            f'{row["gls_calls"]:.1f}',
        ])
    means = {key: float(np.mean([row[key] for row in summary])) for key in (
        "optimum", "best_fit", "local_gain", "osc_gain", "gls_gain",
        "final", "gls_calls",
    )}
    cells.append([
        "MÉDIA", f'{means["optimum"]:.1f}', f'{means["best_fit"]:.1f}',
        f'-{means["local_gain"]:.1f}', f'-{means["osc_gain"]:.1f}',
        f'-{means["gls_gain"]:.1f}', f'{means["final"]:.1f}',
        f'-{means["best_fit"] - means["final"]:.1f}',
        f'{means["gls_calls"]:.1f}',
    ])
    table_ax.axis("off")
    table = table_ax.table(cellText=cells, colLabels=headers, cellLoc="center", loc="center")
    table.auto_set_font_size(False)
    table.set_fontsize(9.2)
    table.scale(1, 1.7)
    for (row, _), cell in table.get_celld().items():
        cell.set_edgecolor("#c6d0d8")
        if row == 0:
            cell.set_facecolor("#263746")
            cell.set_text_props(color="white", fontweight="bold")
        elif row == len(cells):
            cell.set_facecolor("#dfe6ea")
            cell.set_text_props(fontweight="bold")

    fig.suptitle(
        "PODER DE CADA ETAPA — 5 INSTÂNCIAS MAIS DIFÍCEIS\n"
        "Best-fit + FLS + oscilação + ILS + GLS/GFLS | média de 3 execuções com deadlines iguais",
        fontsize=15, fontweight="bold", y=0.985,
    )
    fig.text(
        0.5, 0.015,
        "Ganhos = reduções que estabeleceram novo melhor custo. O GLS direto não inclui "
        "benefícios indiretos da região para onde ele levou o ILS.",
        ha="center", fontsize=9,
    )
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=160, bbox_inches="tight")
    plt.close(fig)
    print(f"Salvo: {OUTPUT}")
    print(f"Salvo: {CSV_OUTPUT}")
    print(means)


if __name__ == "__main__":
    main()
