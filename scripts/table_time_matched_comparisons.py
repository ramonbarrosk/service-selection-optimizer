#!/usr/bin/env python3
"""Gera comparacoes OSC x GLS com orcamentos longos e curtos equivalentes."""

from pathlib import Path

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np


GREEN, YELLOW, RED = "#d5f5e3", "#fef9e7", "#fdecea"
HEADER, GAIN, LOSS, SUMMARY = "#2c3e50", "#eaf7ef", "#fbe9e7", "#dfe6ea"
HARD5 = [100, 129, 147, 28, 128]


def load(path):
    data = {}
    for line in Path(path).read_text().splitlines():
        fields = line.split()
        if line.startswith("Instance_10_10_") and len(fields) >= 5:
            key = int(fields[0].split("_")[-1])
            data[key] = {"optimal": float(fields[1]), "best": float(fields[4])}
    return data


def gap(cost, optimum):
    return (cost - optimum) / optimum * 100 if optimum else 0


def gap_color(value):
    return GREEN if value <= 5 else YELLOW if value <= 20 else RED


def metrics(first, second, keys):
    first_gap = np.mean([gap(first[k]["best"], first[k]["optimal"]) for k in keys])
    second_gap = np.mean([gap(second[k]["best"], second[k]["optimal"]) for k in keys])
    first_opt = sum(first[k]["best"] <= first[k]["optimal"] + 1e-9 for k in keys)
    second_opt = sum(second[k]["best"] <= second[k]["optimal"] + 1e-9 for k in keys)
    wins = sum(second[k]["best"] < first[k]["best"] for k in keys)
    ties = sum(second[k]["best"] == first[k]["best"] for k in keys)
    losses = len(keys) - wins - ties
    gain = np.mean([first[k]["best"] - second[k]["best"] for k in keys])
    return first_gap, second_gap, first_opt, second_opt, wins, ties, losses, gain


def rows_for(first, second, keys):
    rows, colors = [], []
    for key in keys:
        optimum = first[key]["optimal"]
        first_cost, second_cost = first[key]["best"], second[key]["best"]
        first_gap, second_gap = gap(first_cost, optimum), gap(second_cost, optimum)
        gain = first_cost - second_cost
        rows.append([
            str(key), f"{optimum:.0f}", f"{first_cost:.0f}", f"{first_gap:.0f}%",
            f"{second_cost:.0f}", f"{second_gap:.0f}%", f"{gain:+.0f}",
        ])
        colors.append([
            "white", "#eef2f5", "white", gap_color(first_gap),
            "white", gap_color(second_gap), GAIN if gain >= 0 else LOSS,
        ])
    return rows, colors


def style_table(table, summary_row=None):
    table.auto_set_font_size(False)
    for (row, _), cell in table.get_celld().items():
        cell.set_edgecolor("#c9d2da")
        if row == 0:
            cell.set_facecolor(HEADER)
            cell.set_text_props(color="white", fontweight="bold")
        elif summary_row is not None and row == summary_row:
            cell.set_text_props(fontweight="bold")


def legend(fig, y):
    items = [
        mpatches.Patch(color=GREEN, label="GAP <= 5%"),
        mpatches.Patch(color=YELLOW, label="GAP <= 20%"),
        mpatches.Patch(color=RED, label="GAP > 20%"),
        mpatches.Patch(color=GAIN, label="GLS melhor/igual"),
        mpatches.Patch(color=LOSS, label="GLS pior"),
    ]
    fig.legend(handles=items, loc="lower center", ncol=5, fontsize=9,
               frameon=True, bbox_to_anchor=(0.5, y))


def full_table(first, second, config, output):
    keys = sorted(set(first) & set(second))
    stats = metrics(first, second, keys)
    fg, sg, fo, so, wins, ties, losses, gain = stats
    half = (len(keys) + 1) // 2
    fig, axes = plt.subplots(1, 2, figsize=(18, 20))
    fig.suptitle(
        f'{config["title"]}\n{config["description"]}\n'
        f'Oscilacao: GAP {fg:.2f}%, {fo}/94 otimos  |  '
        f'GLS: GAP {sg:.2f}%, {so}/94 otimos  |  '
        f'GLS V/E/D={wins}/{ties}/{losses}, ganho medio {gain:+.2f}',
        fontweight="bold", fontsize=12, y=1.015,
    )
    headers = ["Inst.", "Otimo", "Oscilacao", "GAP", "GLS", "GAP", "Ganho"]
    for ax, subset, title in (
        (axes[0], keys[:half], f"Instancias 1-{half}"),
        (axes[1], keys[half:], f"Instancias {half + 1}-{len(keys)}"),
    ):
        ax.axis("off")
        rows, colors = rows_for(first, second, subset)
        table = ax.table(cellText=rows, colLabels=headers, cellColours=colors,
                         cellLoc="center", loc="center")
        table.set_fontsize(7.5)
        table.scale(1, 1.25)
        style_table(table)
        ax.set_title(title, fontweight="bold", fontsize=10, pad=8)
    legend(fig, -0.005)
    fig.tight_layout()
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight", dpi=130)
    plt.close(fig)
    print(f"Salvo: {output}")
    return stats


def hard5_table(first, second, config, output):
    fg, sg, fo, so, wins, ties, losses, gain = metrics(first, second, HARD5)
    rows, colors = rows_for(first, second, HARD5)
    rows.append(["MEDIA", "", "", f"{fg:.1f}%", "", f"{sg:.1f}%", f"{gain:+.1f}"])
    colors.append([SUMMARY] * 7)
    fig, ax = plt.subplots(figsize=(13.5, 3.8))
    ax.axis("off")
    fig.suptitle(
        f'{config["hard_title"]}\n{config["description"]}\n'
        f'GLS nas dificeis: {wins} vitorias, {ties} empates, {losses} derrotas',
        fontweight="bold", fontsize=11.5, y=1.12,
    )
    headers = ["Instancia", "Otimo", "Oscilacao", "GAP", "GLS", "GAP", "Ganho GLS"]
    table = ax.table(cellText=rows, colLabels=headers, cellColours=colors,
                     cellLoc="center", loc="center")
    table.set_fontsize(10)
    table.scale(1, 1.7)
    style_table(table, len(rows))
    legend(fig, -0.035)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight", dpi=150)
    plt.close(fig)
    print(f"Salvo: {output}")


def main():
    comparisons = [
        {
            "first": "data/osc_deadline_all94_equal_long_time.txt",
            "second": "data/gls_deadline_all94_equal_time.txt",
            "title": "COMPARACAO LONGA COM TEMPO IGUAL — best-fit + oscilacao x GLS + GFLS",
            "hard_title": "COMPARACAO LONGA ATUAL COM TEMPO IGUAL — cinco instancias dificeis",
            "description": (
                "Oscilacao atual: 8m12s externo / 7m59.88s interno | "
                "GLS atual: 8m08s externo / 7m59.14s interno | "
                "mesmos deadlines, timeScale=2.14, 94 instancias, 3 execucoes"
            ),
            "slug": "long_current_equal_8min",
        },
        {
            "first": "data/osc_deadline_all94_equal_short_time.txt",
            "second": "data/gls_deadline_all94.txt",
            "title": "COMPARACAO CURTA COM TEMPO IGUAL — best-fit + oscilacao x GLS + GFLS",
            "hard_title": "COMPARACAO CURTA COM TEMPO IGUAL — cinco instancias dificeis",
            "description": (
                "Oscilacao: 3m48s externo / 3m44.50s interno | "
                "GLS: 3m51s externo / 3m44.45s interno | mesmos deadlines, 94 instancias, 3 execucoes"
            ),
            "slug": "short_equal_3m44",
        },
    ]
    output_dir = Path("data/charts_time_matched")
    for config in comparisons:
        first, second = load(config["first"]), load(config["second"])
        stats = full_table(first, second, config, output_dir / f'{config["slug"]}_all94.png')
        hard5_table(first, second, config, output_dir / f'{config["slug"]}_hard5.png')
        print(config["slug"], stats)


if __name__ == "__main__":
    main()
