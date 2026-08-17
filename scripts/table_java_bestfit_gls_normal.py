#!/usr/bin/env python3
"""Compara Java, best-fit sem GLS e GLS no run normal."""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


FILES = {
    "Java original": Path("data/comparison_java_normal.txt"),
    "Best-fit sem GLS": Path("data/comparison_bestfit_java_budget.txt"),
    "GLS/GFLS": Path("data/comparison_gls_java_budget.txt"),
}
TIMES = {
    "Java original": "3min01,18s",
    "Best-fit sem GLS": "12min06,63s",
    "GLS/GFLS": "13min18,32s",
}
HARD5 = [100, 129, 147, 28, 128]
OUT = Path("data/charts_java_bestfit_gls_normal")


def load(path):
    result = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if not line.startswith("Instance_10_10_") or len(fields) < 5:
            continue
        instance = int(fields[0].rsplit("_", 1)[1])
        result[instance] = {
            "optimum": float(fields[1]),
            "mean": float(fields[3]),
            "best": float(fields[4]),
        }
    return result


def gap(cost, optimum):
    return 100 * (cost - optimum) / optimum


def fmt(value):
    return f"{value:.0f}" if float(value).is_integer() else f"{value:.1f}"


def stats(data):
    keys = sorted(data)
    return {
        "best_cost": float(np.mean([data[k]["best"] for k in keys])),
        "best_gap": float(np.mean([gap(data[k]["best"], data[k]["optimum"]) for k in keys])),
        "reached": sum(data[k]["best"] <= data[k]["optimum"] + 1e-9 for k in keys),
    }


def style(table, summary_row=None):
    table.auto_set_font_size(False)
    for (row, _), cell in table.get_celld().items():
        cell.set_edgecolor("#c7d1d9")
        if row == 0:
            cell.set_facecolor("#263746")
            cell.set_text_props(color="white", fontweight="bold")
        elif summary_row is not None and row == summary_row:
            cell.set_facecolor("#dfe6ea")
            cell.set_text_props(fontweight="bold")


def summary_table(all_data):
    rows = []
    colors = []
    all_stats = {}
    for name, data in all_data.items():
        s = stats(data)
        all_stats[name] = s
        rows.append([
            name, f'{s["best_cost"]:.2f}', f'{s["best_gap"]:.2f}%',
            f'{s["reached"]}/94', TIMES[name],
        ])
        base = "#fdecea" if name == "Java original" else "#fef9e7" if "Best" in name else "#d5f5e3"
        colors.append([base] * 5)

    fig, ax = plt.subplots(figsize=(16, 3.7))
    ax.axis("off")
    headers = [
        "Solução", "Média dos melhores\ncustos por instância",
        "GAP médio\ndos melhores", "Atingiu ótimo\n≥1 de 3", "Tempo final",
    ]
    table = ax.table(cellText=rows, colLabels=headers, cellColours=colors,
                     cellLoc="center", loc="center")
    table.set_fontsize(10)
    table.scale(1, 1.9)
    style(table)
    fig.suptitle(
        "COMPARAÇÃO DAS SOLUÇÕES — 94 INSTÂNCIAS, 3 EXECUÇÕES, ILS#1\n"
        "α=0,4 | first-improvement | perturbação MOVE | vizinhança SWAP | "
        "10.000/2.000 iterações | chamadas completas enquanto há orçamento",
        fontsize=13, fontweight="bold", y=1.05,
    )
    ax.text(
        0.5, -0.13,
        "Métricas baseadas no melhor custo das 3 execuções. Os C++ replicam a política "
        "temporal Java: verificam o orçamento somente entre chamadas ILS completas.",
        ha="center", transform=ax.transAxes, fontsize=9,
    )
    OUT.mkdir(parents=True, exist_ok=True)
    output = OUT / "summary_all94.png"
    fig.savefig(output, dpi=170, bbox_inches="tight")
    plt.close(fig)
    return all_stats, output


def hard5_table(all_data):
    rows, colors = [], []
    for instance in HARD5:
        optimum = next(iter(all_data.values()))[instance]["optimum"]
        row = [str(instance), f"{optimum:.0f}"]
        color = ["white", "#eef2f5"]
        bests = []
        for name, data in all_data.items():
            value = data[instance]
            best_gap = gap(value["best"], optimum)
            row.extend([fmt(value["best"]), f"{best_gap:.1f}%"])
            bests.append(value["best"])
        winner = int(np.argmin(bests))
        for index in range(3):
            shade = "#d5f5e3" if index == winner else "#fdecea"
            color.extend([shade] * 2)
        rows.append(row)
        colors.append(color)

    mean_row = ["MÉDIA", ""]
    for _, data in all_data.items():
        best_cost = float(np.mean([data[i]["best"] for i in HARD5]))
        best_gap = float(np.mean([gap(data[i]["best"], data[i]["optimum"]) for i in HARD5]))
        mean_row.extend([f"{best_cost:.1f}", f"{best_gap:.1f}%"])
    rows.append(mean_row)
    colors.append(["#dfe6ea"] * 8)

    headers = ["Inst.", "Ótimo"]
    for short in ("Java", "Best-fit", "GLS"):
        headers.extend([f"{short}\nMelhor", f"{short}\nGAP"])
    fig, ax = plt.subplots(figsize=(15, 4.8))
    ax.axis("off")
    table = ax.table(cellText=rows, colLabels=headers, cellColours=colors,
                     cellLoc="center", loc="center")
    table.set_fontsize(9.2)
    table.scale(1, 1.8)
    style(table, len(rows))
    fig.suptitle(
        "COMPARAÇÃO NAS CINCO INSTÂNCIAS MAIS DIFÍCEIS\n"
        "Java original × best-fit + FLS + oscilação × best-fit + FLS + oscilação + GLS/GFLS\n"
        "Melhor custo de 3 execuções e seu GAP",
        fontsize=13, fontweight="bold", y=1.08,
    )
    output = OUT / "hard5_comparison.png"
    fig.savefig(output, dpi=170, bbox_inches="tight")
    plt.close(fig)
    return output


def all94_table(all_data):
    instances = sorted(next(iter(all_data.values())))
    half = (len(instances) + 1) // 2
    fig, axes = plt.subplots(1, 2, figsize=(19, 21))
    headers = ["Inst.", "Ót.", "Java", "GAP", "Best-fit", "GAP", "GLS", "GAP"]
    for ax, subset in zip(axes, (instances[:half], instances[half:])):
        ax.axis("off")
        cells, colors = [], []
        for instance in subset:
            optimum = next(iter(all_data.values()))[instance]["optimum"]
            values = [all_data[name][instance]["best"] for name in all_data]
            winner = int(np.argmin(values))
            row = [str(instance), f"{optimum:.0f}"]
            color = ["#fff3f3" if instance in HARD5 else "white", "#eef2f5"]
            for index, value in enumerate(values):
                row.extend([fmt(value), f"{gap(value, optimum):.1f}%"])
                shade = "#d5f5e3" if index == winner else "white"
                color.extend([shade, shade])
            cells.append(row)
            colors.append(color)
        table = ax.table(cellText=cells, colLabels=headers, cellColours=colors,
                         cellLoc="center", loc="center")
        table.set_fontsize(7.5)
        table.scale(1, 1.25)
        style(table)
    fig.suptitle(
        "MELHOR RESULTADO DAS 3 EXECUÇÕES — TODAS AS 94 INSTÂNCIAS\n"
        "Java original × best-fit sem GLS × GLS/GFLS | identificação rosada = cinco difíceis",
        fontsize=14, fontweight="bold", y=0.995,
    )
    output = OUT / "all94_best_results.png"
    fig.savefig(output, dpi=150, bbox_inches="tight")
    plt.close(fig)
    return output


def main():
    all_data = {name: load(path) for name, path in FILES.items()}
    for name, data in all_data.items():
        if len(data) != 94:
            raise RuntimeError(f"{name}: esperadas 94 instâncias, recebidas {len(data)}")
    all_stats, summary_output = summary_table(all_data)
    hard_output = hard5_table(all_data)
    all_output = all94_table(all_data)
    print("Salvo:", summary_output)
    print("Salvo:", hard_output)
    print("Salvo:", all_output)
    for name, values in all_stats.items():
        print(name, values, "tempo", TIMES[name])


if __name__ == "__main__":
    main()
