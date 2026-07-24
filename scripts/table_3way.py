#!/usr/bin/env python3
"""
Tabela colorida comparando 3 versoes lado a lado (mesma config ILS#1, 94 instancias):
  Java (referencia) | C++ baseline (OSC=0) | C++ hibrido (best-fit + oscilacao)

Cada celula de custo e colorida pelo GAP% em relacao ao otimo do CPLEX:
  verde <=5% | amarelo <=20% | vermelho >20%.

Uso (da raiz do projeto):
  python3 scripts/table_3way.py
"""
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from pathlib import Path

GREEN, YELLOW, RED = "#d5f5e3", "#fef9e7", "#fdecea"
HEADER = "#2c3e50"


def load(path):
    d = {}
    for line in open(path):
        f = line.split()
        if line.startswith("Instance_10_10") and len(f) >= 5:
            n = int(f[0].split('_')[-1])
            d[n] = {"opt": float(f[1]), "best": float(f[4])}
    return d


def gap(v, o):
    return (v - o) / o * 100.0 if o else 0.0


def gap_color(g):
    return GREEN if g <= 5 else YELLOW if g <= 20 else RED


def draw(ax, keys, J, B, H, title):
    ax.axis("off")
    header = ["Inst.", "Otimo", "Java", "gap", "Baseline", "gap", "Hibrido", "gap"]
    rows, colors = [], []
    for k in keys:
        o = J[k]["opt"]
        jb, bb, hb = J[k]["best"], B[k]["best"], H[k]["best"]
        gj, gb, gh = gap(jb, o), gap(bb, o), gap(hb, o)
        rows.append([str(k), f"{o:.0f}",
                     f"{jb:.0f}", f"{gj:.0f}%",
                     f"{bb:.0f}", f"{gb:.0f}%",
                     f"{hb:.0f}", f"{gh:.0f}%"])
        colors.append(["white", "#eef2f5",
                       "white", gap_color(gj),
                       "white", gap_color(gb),
                       "white", gap_color(gh)])
    t = ax.table(cellText=rows, colLabels=header, cellColours=colors,
                 cellLoc="center", loc="center")
    t.auto_set_font_size(False)
    t.set_fontsize(7.5)
    t.scale(1, 1.25)
    for (r, c), cell in t.get_celld().items():
        cell.set_edgecolor("#d0d7de")
        if r == 0:
            cell.set_facecolor(HEADER)
            cell.set_text_props(color="white", fontweight="bold")
    ax.set_title(title, fontweight="bold", fontsize=10, pad=8)


def main():
    J = load("data/report_java.txt")
    B = load("data/report_cpp_baseline.txt")
    H = load("data/report_cpp_hybrid.txt")
    keys = sorted(set(J) & set(B) & set(H))

    def stats(D):
        gs = [gap(D[k]["best"], D[k]["opt"]) for k in keys]
        opt = sum(1 for k in keys if D[k]["best"] <= D[k]["opt"] + 1e-9)
        return np.mean(gs), opt

    gj, oj = stats(J); gb, ob = stats(B); gh, oh = stats(H)
    n = len(keys)
    half = (n + 1) // 2
    left, right = keys[:half], keys[half:]

    fig, axes = plt.subplots(1, 2, figsize=(18, 20))
    fig.suptitle(
        "Comparacao 3 versoes (config ILS#1 dos autores | 94 instancias | 3 execucoes)\n"
        f"Java: gap {gj:.1f}%, {oj}/{n} otimos   |   "
        f"C++ baseline: gap {gb:.1f}%, {ob}/{n}   |   "
        f"C++ hibrido (best-fit): gap {gh:.1f}%, {oh}/{n}",
        fontweight="bold", fontsize=13, y=1.005,
    )
    draw(axes[0], left, J, B, H, f"Instancias 1-{half}")
    draw(axes[1], right, J, B, H, f"Instancias {half+1}-{n}")

    legend = [
        mpatches.Patch(color=GREEN, label="GAP <= 5%"),
        mpatches.Patch(color=YELLOW, label="GAP <= 20%"),
        mpatches.Patch(color=RED, label="GAP > 20%"),
    ]
    fig.legend(handles=legend, loc="lower center", ncol=3, fontsize=10,
               frameon=True, bbox_to_anchor=(0.5, -0.005))
    fig.tight_layout()
    out = Path("data/charts_3way"); out.mkdir(parents=True, exist_ok=True)
    path = out / "table_3way.png"
    fig.savefig(path, bbox_inches="tight", dpi=130)
    plt.close(fig)
    print(f"Salvo: {path}")


if __name__ == "__main__":
    main()
