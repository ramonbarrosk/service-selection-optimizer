#!/usr/bin/env python3
"""
Tabela colorida: Mergulho inviável (IGrAl) x Hibrido x GLS promovida.
As duas primeiras variantes isolam o efeito do best-fit + oscilacao; a terceira
acrescenta GLS adaptativa/persistente com GFLS.
Cores por GAP%: verde <=5% | amarelo <=20% | vermelho >20%.

Uso (da raiz do projeto):
  python3 scripts/table_dive_vs_hybrid.py
"""
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from pathlib import Path

GREEN, YELLOW, RED, HEADER = "#d5f5e3", "#fef9e7", "#fdecea", "#2c3e50"


def load(path):
    d = {}
    for line in open(path):
        f = line.split()
        if line.startswith("Instance_10_10") and len(f) >= 5:
            d[int(f[0].split('_')[-1])] = {"opt": float(f[1]), "best": float(f[4])}
    return d


def gap(v, o):
    return (v - o) / o * 100.0 if o else 0.0


def gap_color(g):
    return GREEN if g <= 5 else YELLOW if g <= 20 else RED


def main():
    A = load("data/dive_A_infeasible.txt")   # mergulho inviavel (OSC=0)
    B = load("data/dive_B_hybrid.txt")        # hibrido (OSC=1)
    C = load("data/gls_winner_difficult5.txt") # configuracao GLS promovida
    keys = sorted(set(A) & set(B) & set(C))

    header = ["Instancia", "Otimo",
              "Mergulho", "GAP",
              "Hibrido", "GAP",
              "GLS vencedora", "GAP", "Ganho GLS"]
    rows, colors = [], []
    for k in keys:
        o = A[k]["opt"]
        a, b, c = A[k]["best"], B[k]["best"], C[k]["best"]
        ga, gb, gc = gap(a, o), gap(b, o), gap(c, o)
        rows.append([f"#{k}", f"{o:.0f}",
                     f"{a:.0f}", f"{ga:.0f}%",
                     f"{b:.0f}", f"{gb:.0f}%",
                     f"{c:.0f}", f"{gc:.0f}%",
                     f"+{b-c:.0f}"])
        colors.append(["white", "#eef2f5",
                       "white", gap_color(ga),
                       "white", gap_color(gb),
                       "white", gap_color(gc),
                       "#eaf7ef"])

    mean_a = np.mean([gap(A[k]["best"], A[k]["opt"]) for k in keys])
    mean_b = np.mean([gap(B[k]["best"], B[k]["opt"]) for k in keys])
    mean_c = np.mean([gap(C[k]["best"], C[k]["opt"]) for k in keys])
    # linha de media
    rows.append(["MEDIA", "", "", f"{mean_a:.1f}%",
                 "", f"{mean_b:.1f}%", "", f"{mean_c:.1f}%",
                 f"+{mean_b-mean_c:.1f}pp"])
    colors.append(["#dfe6ea"] * 9)

    fig, ax = plt.subplots(figsize=(16, 3.5))
    ax.axis("off")
    fig.suptitle(
        "Mergulho inviavel  x  Hibrido best-fit+oscilacao  x  GLS vencedora\n"
        "Melhor custo em 3 execucoes | 5 instancias dificeis | GLS alpha=0.3 + 30/60/120 + persistencia + GFLS",
        fontweight="bold", fontsize=11.5, y=1.13,
    )
    t = ax.table(cellText=rows, colLabels=header, cellColours=colors,
                 cellLoc="center", loc="center")
    t.auto_set_font_size(False)
    t.set_fontsize(9.5)
    t.scale(1, 1.7)
    for (r, c), cell in t.get_celld().items():
        cell.set_edgecolor("#c9d2da")
        if r == 0:
            cell.set_facecolor(HEADER)
            cell.set_text_props(color="white", fontweight="bold")
        elif r == len(rows):   # linha MEDIA
            cell.set_text_props(fontweight="bold")

    legend = [
        mpatches.Patch(color=GREEN, label="GAP <= 5%"),
        mpatches.Patch(color=YELLOW, label="GAP <= 20%"),
        mpatches.Patch(color=RED, label="GAP > 20%"),
    ]
    fig.legend(handles=legend, loc="lower center", ncol=3, fontsize=9,
               frameon=True, bbox_to_anchor=(0.5, -0.06))

    out = Path("data/charts_dive_vs_hybrid")
    out.mkdir(parents=True, exist_ok=True)
    path = out / "table_dive_vs_hybrid.png"
    fig.savefig(path, bbox_inches="tight", dpi=150)
    plt.close(fig)
    print(f"Salvo: {path}")


if __name__ == "__main__":
    main()
