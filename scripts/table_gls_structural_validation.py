#!/usr/bin/env python3
"""Resume a validacao de 3 minutos do GLS estrutural."""

from pathlib import Path

import matplotlib.pyplot as plt


FILES = [
    ("Best-fit + oscilacao\nsem GLS", Path("data/comparison_bestfit_3min.txt"), "3:07,34"),
    ("GLS atual\nalpha=0,3", Path("data/comparison_gls_3min.txt"), "3:07,42"),
    ("GLS estrutural\nalpha=1,0", Path("data/comparison_gls_struct_alpha1_3min.txt"), "3:06,77"),
    ("GLS estrutural +\ncadeia de escape", Path("data/comparison_gls_ejection_escape_3min.txt"), "3:04,83"),
]
HARD5 = [100, 128, 129, 147, 28]
OUTPUT = Path("data/charts_gls_reform/gls_ejection_chain_all94_validation.png")


def load(path):
    rows = {}
    for line in path.read_text().splitlines():
        fields = line.split()
        if len(fields) >= 5 and fields[0].startswith("Instance_10_10_"):
            instance = int(fields[0].rsplit("_", 1)[1])
            rows[instance] = {
                "optimum": float(fields[1]),
                "mean": float(fields[3]),
                "best": float(fields[4]),
            }
    return rows


def gap(cost, optimum):
    return 100.0 * (cost - optimum) / optimum


def main():
    configs = [(name, load(path), elapsed) for name, path, elapsed in FILES]
    summary = []
    for name, data, elapsed in configs:
        optimums = sum(row["best"] == row["optimum"] for row in data.values())
        best_gap = sum(gap(row["best"], row["optimum"]) for row in data.values()) / len(data)
        mean_gap = sum(gap(row["mean"], row["optimum"]) for row in data.values()) / len(data)
        summary.append([name, str(optimums), f"{best_gap:.3f}%", f"{mean_gap:.3f}%", elapsed])

    difficult = []
    for instance in HARD5:
        optimum = configs[0][1][instance]["optimum"]
        row = [str(instance), f"{optimum:.0f}"]
        for _, data, _ in configs:
            best = data[instance]["best"]
            row.extend([f"{best:.0f}", f"{gap(best, optimum):.1f}%"])
        difficult.append(row)

    fig, axes = plt.subplots(
        2, 1, figsize=(14, 10.5),
        gridspec_kw={"height_ratios": [1, 2.4], "hspace": 0.6},
    )
    for ax in axes:
        ax.axis("off")

    top = axes[0].table(
        cellText=summary,
        colLabels=["Configuracao", "Otimos / 94", "GAP melhor", "GAP medio", "Tempo externo"],
        cellLoc="center", loc="center",
    )
    top.auto_set_font_size(False)
    top.set_fontsize(10)
    top.scale(1, 1.9)

    headers = ["Inst.", "Otimo"]
    for name, _, _ in configs:
        headers.extend([f"{name}\nMelhor", "GAP"])
    bottom = axes[1].table(cellText=difficult, colLabels=headers, cellLoc="center", loc="center")
    bottom.auto_set_font_size(False)
    bottom.set_fontsize(8.5)
    bottom.scale(1, 1.75)

    for table in (top, bottom):
        for (row, _), cell in table.get_celld().items():
            cell.set_edgecolor("#c7d1d9")
            if row == 0:
                cell.set_facecolor("#263746")
                cell.set_text_props(color="white", fontweight="bold")
    for (row, _), cell in bottom.get_celld().items():
        if row == 0:
            cell.set_height(0.16)

    fig.suptitle(
        "VALIDACAO DO GLS E CADEIA DE REALOCACOES — ORCAMENTO INTERNO DE 3 MINUTOS\n"
        "94 instancias, 3 repeticoes | capacidade=2,0; arrependimento=0,5",
        fontsize=14, fontweight="bold",
    )
    axes[1].text(
        0.5, -0.05,
        "A cadeia melhora levemente a media, mas reduz a quantidade de otimos; "
        "o GLS estrutural sem cadeia continua sendo o melhor compromisso.",
        ha="center", transform=axes[1].transAxes, fontsize=9,
    )
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=170, bbox_inches="tight")
    plt.close(fig)
    print(f"Salvo: {OUTPUT}")


if __name__ == "__main__":
    main()
