#!/usr/bin/env python3
"""Gera resumo e tabela completa da contribuição das etapas nas 94 instâncias."""

from collections import defaultdict
from pathlib import Path
import argparse

import matplotlib.pyplot as plt
import numpy as np


INPUT = Path("data/gls_all94_stage_trace.txt")
OUTPUT_DIR = Path("data/charts_gls_stage_power")
CSV_OUTPUT = Path("data/gls_all94_stage_power.csv")
OUTPUT_SUFFIX = "all94"
RUN_DESCRIPTION = "3 execuções, deadlines adaptativos"
HARD5 = {100, 129, 147, 28, 128}
BLUE, ORANGE, PURPLE, FINAL = "#3f86c5", "#f39c3d", "#8e63ce", "#d9e1e8"


def load_selected():
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


def avg(rows, key):
    return float(np.mean([row[key] for row in rows]))


def summarize(selected):
    summary = []
    for instance in sorted(selected):
        rows = selected[instance]
        best_fit = avg(rows, "bestFit")
        initial_local = avg(rows, "initialLocal")
        initial_osc = avg(rows, "initialOsc")
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
        gls_gain = avg(rows, "glsDirectGain")
        final = avg(rows, "final")
        optimum = avg(rows, "optimum")
        assert abs(best_fit - local_gain - osc_gain - gls_gain - final) < 1e-7
        summary.append({
            "instance": instance, "optimum": optimum, "best_fit": best_fit,
            "initial_local": initial_local, "initial_osc": initial_osc,
            "local_gain": local_gain, "osc_gain": osc_gain,
            "gls_gain": gls_gain, "final": final,
            "gap": 100 * (final - optimum) / optimum,
            "gls_calls": avg(rows, "glsCalls"),
        })
    return summary


def save_csv(summary):
    header = (
        "instance,optimum,best_fit,after_initial_local,after_initial_oscillation,"
        "local_gain,oscillation_gain,direct_gls_gain,final_mean,gap_percent,"
        "mean_gls_calls\n"
    )
    lines = [header]
    for row in summary:
        lines.append(
            f'{row["instance"]},{row["optimum"]:.0f},{row["best_fit"]:.3f},'
            f'{row["initial_local"]:.3f},{row["initial_osc"]:.3f},'
            f'{row["local_gain"]:.3f},{row["osc_gain"]:.3f},'
            f'{row["gls_gain"]:.3f},{row["final"]:.3f},{row["gap"]:.3f},'
            f'{row["gls_calls"]:.3f}\n'
        )
    CSV_OUTPUT.write_text("".join(lines))


def means(summary):
    keys = (
        "optimum", "best_fit", "initial_local", "initial_osc", "local_gain",
        "osc_gain", "gls_gain", "final", "gap", "gls_calls",
    )
    return {key: float(np.mean([row[key] for row in summary])) for key in keys}


def summary_plot(summary):
    stats = means(summary)
    total_gain = stats["local_gain"] + stats["osc_gain"] + stats["gls_gain"]
    shares = [
        100 * stats["local_gain"] / total_gain if total_gain else 0,
        100 * stats["osc_gain"] / total_gain if total_gain else 0,
        100 * stats["gls_gain"] / total_gain if total_gain else 0,
    ]

    fig, axes = plt.subplots(2, 1, figsize=(16, 9), gridspec_kw={"height_ratios": [1, 1.35]})
    ax = axes[0]
    left = stats["final"]
    ax.barh([0], [stats["final"]], color=FINAL, edgecolor="white", label="Custo final")
    for value, color, label in (
        (stats["gls_gain"], PURPLE, "Redução direta GLS/GFLS"),
        (stats["osc_gain"], ORANGE, "Redução por oscilação"),
        (stats["local_gain"], BLUE, "Redução por busca local/FLS"),
    ):
        ax.barh([0], [value], left=[left], color=color, edgecolor="white", label=label)
        if value >= 1:
            ax.text(left + value / 2, 0, f"-{value:.1f}", ha="center", va="center",
                    color="white", fontweight="bold")
        left += value
    ax.text(stats["final"] / 2, 0, f'final {stats["final"]:.1f}',
            ha="center", va="center", fontweight="bold")
    ax.text(stats["best_fit"] + 0.5, 0, f'best-fit {stats["best_fit"]:.1f}', va="center")
    ax.scatter([stats["optimum"]], [0], marker="D", color="#111111", s=55,
               zorder=5, label=f'Ótimo médio {stats["optimum"]:.1f}')
    ax.set_yticks([])
    ax.set_xlabel("Custo médio nas 94 instâncias — menor é melhor")
    ax.grid(axis="x", alpha=0.18)
    ax.legend(loc="lower center", bbox_to_anchor=(0.5, 1.01), ncol=5,
              frameon=False, fontsize=9)

    ax = axes[1]
    order = sorted(summary, key=lambda row: row["gap"], reverse=True)
    x = np.arange(len(order))
    local = np.array([row["local_gain"] for row in order])
    osc = np.array([row["osc_gain"] for row in order])
    gls = np.array([row["gls_gain"] for row in order])
    total = local + osc + gls
    local_pct = np.divide(100 * local, total, out=np.zeros_like(local), where=total > 0)
    osc_pct = np.divide(100 * osc, total, out=np.zeros_like(osc), where=total > 0)
    gls_pct = np.divide(100 * gls, total, out=np.zeros_like(gls), where=total > 0)
    ax.bar(x, local_pct, color=BLUE, width=0.88, label="Local/FLS")
    ax.bar(x, osc_pct, bottom=local_pct, color=ORANGE, width=0.88, label="Oscilação")
    ax.bar(x, gls_pct, bottom=local_pct + osc_pct, color=PURPLE, width=0.88,
           label="GLS direto")
    hard_positions = [i for i, row in enumerate(order) if row["instance"] in HARD5]
    ax.scatter(hard_positions, [103] * len(hard_positions), marker="v", color="#b21f2d",
               s=40, clip_on=False, label="Cinco difíceis")
    tick_positions = np.arange(0, len(order), 5)
    ax.set_xticks(tick_positions, [str(order[i]["instance"]) for i in tick_positions],
                  rotation=45)
    ax.set_ylim(0, 108)
    ax.set_ylabel("Participação na redução total (%)")
    ax.set_xlabel("Instâncias ordenadas pelo GAP final (maior → menor)")
    ax.grid(axis="y", alpha=0.18)
    ax.legend(ncol=4, loc="upper right", fontsize=9)
    ax.set_title(
        f"Participação global: local {shares[0]:.1f}% | oscilação {shares[1]:.1f}% | "
        f"GLS direto {shares[2]:.1f}%",
        fontweight="bold", fontsize=11,
    )

    solved_mean = sum(row["final"] <= row["optimum"] + 1e-9 for row in summary)
    fig.suptitle(
        "PODER DE CADA ETAPA — TODAS AS 94 INSTÂNCIAS\n"
        f"{RUN_DESCRIPTION} | GAP médio do custo final {stats['gap']:.2f}% | "
        f"ótimo na média das 3 execuções: {solved_mean}/94",
        fontsize=14, fontweight="bold", y=0.985,
    )
    fig.text(
        0.5, 0.01,
        "O ganho GLS direto não inclui melhorias posteriores possibilitadas pela região "
        "para a qual o GLS deslocou o ILS.",
        ha="center", fontsize=9,
    )
    fig.tight_layout(rect=[0, 0.03, 1, 0.94])
    output = OUTPUT_DIR / f"gls_stage_power_{OUTPUT_SUFFIX}_summary.png"
    fig.savefig(output, dpi=160, bbox_inches="tight")
    plt.close(fig)
    return stats, shares, solved_mean, output


def gap_color(gap):
    if gap <= 1e-9:
        return "#d5f5e3"
    if gap <= 5:
        return "#fef9e7"
    return "#fdecea"


def table_plot(summary):
    half = (len(summary) + 1) // 2
    fig, axes = plt.subplots(1, 2, figsize=(19, 21))
    headers = ["Inst.", "Ót.", "BF", "G. local", "G. OSC", "G. GLS", "Final", "GAP"]
    for ax, subset in zip(axes, (summary[:half], summary[half:])):
        ax.axis("off")
        cells, colors = [], []
        for row in subset:
            cells.append([
                str(row["instance"]), f'{row["optimum"]:.0f}', f'{row["best_fit"]:.1f}',
                f'-{row["local_gain"]:.1f}', f'-{row["osc_gain"]:.1f}',
                f'-{row["gls_gain"]:.1f}', f'{row["final"]:.1f}', f'{row["gap"]:.1f}%',
            ])
            base = "#fff3f3" if row["instance"] in HARD5 else "white"
            colors.append([
                base, "#eef2f5", base, "#e8f2fb", "#fff0df", "#f0e8fb",
                base, gap_color(row["gap"]),
            ])
        table = ax.table(cellText=cells, colLabels=headers, cellColours=colors,
                         cellLoc="center", loc="center")
        table.auto_set_font_size(False)
        table.set_fontsize(7.4)
        table.scale(1, 1.25)
        for (row, _), cell in table.get_celld().items():
            cell.set_edgecolor("#c9d2da")
            if row == 0:
                cell.set_facecolor("#263746")
                cell.set_text_props(color="white", fontweight="bold")
    fig.suptitle(
        "CONTRIBUIÇÃO DAS ETAPAS — TABELA COMPLETA DAS 94 INSTÂNCIAS\n"
        "Ganhos médios de 3 execuções | vermelho-claro na identificação = cinco difíceis",
        fontsize=14, fontweight="bold", y=0.995,
    )
    fig.text(
        0.5, 0.008,
        "Ganho negativo reduz o custo. GAP calculado sobre o custo final médio das três execuções.",
        ha="center", fontsize=9,
    )
    fig.tight_layout(rect=[0, 0.02, 1, 0.97])
    output = OUTPUT_DIR / f"gls_stage_power_{OUTPUT_SUFFIX}_table.png"
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    return output


def main():
    global INPUT, CSV_OUTPUT, OUTPUT_SUFFIX, RUN_DESCRIPTION
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=INPUT)
    parser.add_argument("--csv", type=Path, default=CSV_OUTPUT)
    parser.add_argument("--suffix", default=OUTPUT_SUFFIX)
    parser.add_argument("--description", default=RUN_DESCRIPTION)
    args = parser.parse_args()
    INPUT = args.input
    CSV_OUTPUT = args.csv
    OUTPUT_SUFFIX = args.suffix
    RUN_DESCRIPTION = args.description
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    summary = summarize(load_selected())
    if len(summary) != 94:
        raise RuntimeError(f"Esperadas 94 instâncias, recebidas {len(summary)}")
    save_csv(summary)
    stats, shares, solved, summary_output = summary_plot(summary)
    table_output = table_plot(summary)
    print(f"Salvo: {summary_output}")
    print(f"Salvo: {table_output}")
    print(f"Salvo: {CSV_OUTPUT}")
    print("MEDIAS", stats)
    print("PARTICIPACOES", shares, "OTIMO_MEDIO", solved)


if __name__ == "__main__":
    main()
