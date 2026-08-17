#!/usr/bin/env python3
"""Compara best-fit + oscilacao com GLS + GFLS com deadline nas 94 instancias."""

from pathlib import Path

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np


GREEN, YELLOW, RED = "#d5f5e3", "#fef9e7", "#fdecea"
HEADER, GAIN, LOSS = "#2c3e50", "#eaf7ef", "#fbe9e7"


def load(path):
    results = {}
    for line in Path(path).read_text().splitlines():
        fields = line.split()
        if line.startswith("Instance_10_10_") and len(fields) >= 5:
            instance = int(fields[0].split("_")[-1])
            results[instance] = {
                "optimal": float(fields[1]),
                "best": float(fields[4]),
            }
    return results


def gap(cost, optimum):
    return (cost - optimum) / optimum * 100.0 if optimum else 0.0


def gap_color(value):
    return GREEN if value <= 5 else YELLOW if value <= 20 else RED


def statistics(results, keys):
    gaps = [gap(results[key]["best"], results[key]["optimal"]) for key in keys]
    optimums = sum(
        results[key]["best"] <= results[key]["optimal"] + 1e-9 for key in keys
    )
    return np.mean(gaps), optimums


def draw(ax, keys, oscillation, gls, title):
    ax.axis("off")
    headers = ["Inst.", "Otimo", "Oscilacao", "GAP", "GLS deadline", "GAP", "Ganho"]
    rows, colors = [], []
    for key in keys:
        optimum = oscillation[key]["optimal"]
        osc_cost = oscillation[key]["best"]
        gls_cost = gls[key]["best"]
        osc_gap = gap(osc_cost, optimum)
        gls_gap = gap(gls_cost, optimum)
        gain = osc_cost - gls_cost
        rows.append([
            str(key), f"{optimum:.0f}", f"{osc_cost:.0f}", f"{osc_gap:.0f}%",
            f"{gls_cost:.0f}", f"{gls_gap:.0f}%", f"{gain:+.0f}",
        ])
        colors.append([
            "white", "#eef2f5", "white", gap_color(osc_gap),
            "white", gap_color(gls_gap), GAIN if gain >= 0 else LOSS,
        ])

    table = ax.table(
        cellText=rows, colLabels=headers, cellColours=colors,
        cellLoc="center", loc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(7.5)
    table.scale(1, 1.25)
    for (row, _), cell in table.get_celld().items():
        cell.set_edgecolor("#d0d7de")
        if row == 0:
            cell.set_facecolor(HEADER)
            cell.set_text_props(color="white", fontweight="bold")
    ax.set_title(title, fontweight="bold", fontsize=10, pad=8)


def draw_hard5(oscillation, gls, keys, output):
    headers = [
        "Instancia", "Otimo", "Oscilacao", "GAP",
        "GLS deadline", "GAP", "Ganho GLS",
    ]
    rows, colors = [], []
    osc_gaps, gls_gaps, gains = [], [], []
    for key in keys:
        optimum = oscillation[key]["optimal"]
        osc_cost = oscillation[key]["best"]
        gls_cost = gls[key]["best"]
        osc_gap = gap(osc_cost, optimum)
        gls_gap = gap(gls_cost, optimum)
        gain = osc_cost - gls_cost
        osc_gaps.append(osc_gap)
        gls_gaps.append(gls_gap)
        gains.append(gain)
        rows.append([
            f"#{key}", f"{optimum:.0f}", f"{osc_cost:.0f}", f"{osc_gap:.1f}%",
            f"{gls_cost:.0f}", f"{gls_gap:.1f}%", f"{gain:+.0f}",
        ])
        colors.append([
            "white", "#eef2f5", "white", gap_color(osc_gap),
            "white", gap_color(gls_gap), GAIN if gain >= 0 else LOSS,
        ])

    rows.append([
        "MEDIA", "", "", f"{np.mean(osc_gaps):.1f}%", "",
        f"{np.mean(gls_gaps):.1f}%", f"{np.mean(gains):+.1f}",
    ])
    colors.append(["#dfe6ea"] * len(headers))

    fig, ax = plt.subplots(figsize=(13.5, 3.5))
    ax.axis("off")
    fig.suptitle(
        "Best-fit + oscilacao  x  GLS + GFLS com deadline — instancias dificeis\n"
        "Melhor custo em 3 execucoes | ganho positivo significa vantagem do GLS",
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
        mpatches.Patch(color=GAIN, label="GLS melhor/igual"),
        mpatches.Patch(color=LOSS, label="GLS pior"),
    ]
    fig.legend(
        handles=legend, loc="lower center", ncol=5, fontsize=9,
        frameon=True, bbox_to_anchor=(0.5, -0.04),
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight", dpi=150)
    plt.close(fig)
    print(f"Salvo: {output}")


def main():
    oscillation = load("data/report_cpp_hybrid.txt")
    gls = load("data/gls_deadline_all94.txt")
    keys = sorted(set(oscillation) & set(gls))
    if len(keys) != 94:
        raise RuntimeError(f"Esperadas 94 instancias comuns; encontradas {len(keys)}")

    osc_gap, osc_optimums = statistics(oscillation, keys)
    gls_gap, gls_optimums = statistics(gls, keys)
    wins = sum(gls[key]["best"] < oscillation[key]["best"] for key in keys)
    ties = sum(gls[key]["best"] == oscillation[key]["best"] for key in keys)
    losses = len(keys) - wins - ties
    mean_gain = np.mean([
        oscillation[key]["best"] - gls[key]["best"] for key in keys
    ])

    half = (len(keys) + 1) // 2
    fig, axes = plt.subplots(1, 2, figsize=(18, 20))
    fig.suptitle(
        "Best-fit + oscilacao  x  GLS + GFLS com deadline "
        "(94 instancias | 3 execucoes)\n"
        f"Oscilacao: GAP {osc_gap:.2f}%, {osc_optimums}/94 otimos   |   "
        f"GLS deadline: GAP {gls_gap:.2f}%, {gls_optimums}/94 otimos   |   "
        f"GLS: {wins} vitorias, {ties} empates, {losses} derrotas, "
        f"ganho medio {mean_gain:+.2f}",
        fontweight="bold", fontsize=12.5, y=1.005,
    )
    draw(axes[0], keys[:half], oscillation, gls, f"Instancias 1-{half}")
    draw(axes[1], keys[half:], oscillation, gls, f"Instancias {half + 1}-{len(keys)}")

    legend = [
        mpatches.Patch(color=GREEN, label="GAP <= 5%"),
        mpatches.Patch(color=YELLOW, label="GAP <= 20%"),
        mpatches.Patch(color=RED, label="GAP > 20%"),
        mpatches.Patch(color=GAIN, label="GLS melhor/igual"),
        mpatches.Patch(color=LOSS, label="GLS pior"),
    ]
    fig.legend(
        handles=legend, loc="lower center", ncol=5, fontsize=10,
        frameon=True, bbox_to_anchor=(0.5, -0.005),
    )
    fig.tight_layout()
    output = Path("data/charts_osc_vs_gls_deadline/table_osc_vs_gls_deadline.png")
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight", dpi=130)
    plt.close(fig)
    print(f"Salvo: {output}")
    draw_hard5(
        oscillation, gls, [100, 129, 147, 28, 128],
        output.parent / "table_osc_vs_gls_deadline_hard5.png",
    )
    print(
        f"Oscilacao GAP={osc_gap:.3f}% otimos={osc_optimums}/94 | "
        f"GLS GAP={gls_gap:.3f}% otimos={gls_optimums}/94 | "
        f"V/E/D={wins}/{ties}/{losses} ganho medio={mean_gain:+.3f}"
    )


if __name__ == "__main__":
    main()
