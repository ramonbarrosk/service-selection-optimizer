#!/usr/bin/env python3
"""Tabela visual do experimento Java x best-fit x GLS em aproximadamente 3 min."""

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


FILES = {
    "Java original": Path("data/comparison_java_normal.txt"),
    "Best-fit sem GLS": Path("data/comparison_bestfit_3min.txt"),
    "GLS/GFLS": Path("data/comparison_gls_3min.txt"),
}
TIMES = {
    "Java original": ("—", "3min01,18s"),
    "Best-fit sem GLS": ("2min59,64s", "3min07,34s"),
    "GLS/GFLS": ("2min59,66s", "3min07,42s"),
}
HARD5 = [100, 129, 147, 28, 128]
OUTPUT = Path("data/charts_java_bestfit_gls_3min/comparison_3min.png")


def load(path):
    data = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if line.startswith("Instance_10_10_") and len(fields) >= 5:
            instance = int(fields[0].rsplit("_", 1)[1])
            data[instance] = {
                "optimum": float(fields[1]),
                "mean": float(fields[3]),
                "best": float(fields[4]),
                "status": " ".join(fields[6:]),
            }
    return data


def gap(cost, optimum):
    return 100 * (cost - optimum) / optimum


def fmt(value):
    return f"{value:.0f}" if value.is_integer() else f"{value:.1f}"


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


def main():
    all_data = {name: load(path) for name, path in FILES.items()}
    fig = plt.figure(figsize=(17, 10))
    grid = fig.add_gridspec(2, 1, height_ratios=[0.8, 1.2], hspace=0.32)
    summary_ax = fig.add_subplot(grid[0])
    hard_ax = fig.add_subplot(grid[1])
    summary_ax.axis("off")
    hard_ax.axis("off")

    summary_rows, summary_colors = [], []
    for name, data in all_data.items():
        keys = sorted(data)
        best_cost = np.mean([data[k]["best"] for k in keys])
        best_gap = np.mean([gap(data[k]["best"], data[k]["optimum"]) for k in keys])
        reached = sum(data[k]["best"] <= data[k]["optimum"] + 1e-9 for k in keys)
        three_of_three = (
            "—" if name == "Java original"
            else str(sum("YES (3/3)" in data[k]["status"] for k in keys))
        )
        internal, external = TIMES[name]
        summary_rows.append([
            name, f"{best_cost:.2f}", f"{best_gap:.2f}%", f"{reached}/94",
            three_of_three if three_of_three == "—" else f"{three_of_three}/94",
            internal, external,
        ])
        shade = "#fdecea" if name == "Java original" else "#fef9e7" if "Best" in name else "#d5f5e3"
        summary_colors.append([shade] * 7)

    summary_headers = [
        "Solução", "Média dos\nmelhores", "GAP médio", "Ótimo\n≥1/3",
        "Ótimo\n3/3", "Tempo\nalgoritmo", "Tempo\nexterno",
    ]
    table = summary_ax.table(
        cellText=summary_rows, colLabels=summary_headers,
        cellColours=summary_colors, cellLoc="center", loc="center",
    )
    table.set_fontsize(9.5)
    table.scale(1, 1.8)
    style(table)
    summary_ax.set_title("Resumo das 94 instâncias", fontweight="bold", fontsize=12, pad=5)

    hard_rows, hard_colors = [], []
    for instance in HARD5:
        optimum = next(iter(all_data.values()))[instance]["optimum"]
        values = [data[instance]["best"] for data in all_data.values()]
        winner = int(np.argmin(values))
        row = [str(instance), f"{optimum:.0f}"]
        colors = ["white", "#eef2f5"]
        for index, value in enumerate(values):
            row.extend([fmt(value), f"{gap(value, optimum):.1f}%"])
            shade = "#d5f5e3" if index == winner else "#fdecea"
            colors.extend([shade, shade])
        hard_rows.append(row)
        hard_colors.append(colors)

    mean_row = ["MÉDIA", ""]
    for data in all_data.values():
        best = np.mean([data[i]["best"] for i in HARD5])
        mean_gap = np.mean([gap(data[i]["best"], data[i]["optimum"]) for i in HARD5])
        mean_row.extend([f"{best:.1f}", f"{mean_gap:.1f}%"])
    hard_rows.append(mean_row)
    hard_colors.append(["#dfe6ea"] * 8)

    hard_headers = ["Inst.", "Ótimo", "Java", "GAP", "Best-fit", "GAP", "GLS", "GAP"]
    table = hard_ax.table(
        cellText=hard_rows, colLabels=hard_headers, cellColours=hard_colors,
        cellLoc="center", loc="center",
    )
    table.set_fontsize(10)
    table.scale(1, 1.75)
    style(table, len(hard_rows))
    hard_ax.set_title("Cinco instâncias mais difíceis — melhor de 3 execuções",
                      fontweight="bold", fontsize=12, pad=5)

    fig.suptitle(
        "EXPERIMENTO COM LIMITE DE APROXIMADAMENTE 3 MINUTOS\n"
        "Java original × best-fit + FLS + oscilação × GLS/GFLS",
        fontsize=15, fontweight="bold", y=0.98,
    )
    fig.text(
        0.5, 0.025,
        "C++: deadline interno, timeScale=0,80. Tempo externo inclui carregamento das "
        "94 instâncias e impressão dos resultados.",
        ha="center", fontsize=9,
    )
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=170, bbox_inches="tight")
    plt.close(fig)
    print(f"Salvo: {OUTPUT}")


if __name__ == "__main__":
    main()
