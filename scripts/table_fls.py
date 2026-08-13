#!/usr/bin/env python3
"""
Tabela colorida comparando FLS=0 (varredura completa) x FLS=1 (sub-vizinhancas com
bit de ativacao), mesma config ILS#1 de producao (SWAP + FIRST_IMPROVEMENT, 94
instancias, 3 execucoes). Cada celula de custo e colorida pelo GAP% em relacao ao
otimo do CPLEX: verde <=5% | amarelo <=20% | vermelho >20%.

Tambem imprime no stdout um resumo do tempo-medio-ate-o-melhor (TtB) por versao,
que e o sinal principal esperado do FLS (busca local mais rapida -> mais
iteracoes/restarts no mesmo orcamento de parede -> TtB menor e/ou custo menor).

Uso (da raiz do projeto):
  python3 scripts/table_fls.py
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
        if line.startswith("Instance_10_10") and len(f) >= 6:
            n = int(f[0].split('_')[-1])
            d[n] = {"opt": float(f[1]), "best": float(f[4]), "ttb": float(f[5])}
    return d


def gap(v, o):
    return (v - o) / o * 100.0 if o else 0.0


def gap_color(g):
    return GREEN if g <= 5 else YELLOW if g <= 20 else RED


def draw(ax, keys, OFF, ON, title):
    ax.axis("off")
    header = ["Inst.", "Otimo", "FLS=0", "gap", "TtB(s)", "FLS=1", "gap", "TtB(s)"]
    rows, colors = [], []
    for k in keys:
        o = OFF[k]["opt"]
        ob, nb = OFF[k]["best"], ON[k]["best"]
        go, gn = gap(ob, o), gap(nb, o)
        rows.append([str(k), f"{o:.0f}",
                     f"{ob:.0f}", f"{go:.0f}%", f"{OFF[k]['ttb']:.1f}",
                     f"{nb:.0f}", f"{gn:.0f}%", f"{ON[k]['ttb']:.1f}"])
        colors.append(["white", "#eef2f5",
                       "white", gap_color(go), "white",
                       "white", gap_color(gn), "white"])
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
    OFF = load("data/report_cpp_fls_off.txt")
    ON = load("data/report_cpp_fls_on.txt")
    keys = sorted(set(OFF) & set(ON))

    def stats(D):
        gs = [gap(D[k]["best"], D[k]["opt"]) for k in keys]
        opt = sum(1 for k in keys if D[k]["best"] <= D[k]["opt"] + 1e-9)
        meanTtb = np.mean([D[k]["ttb"] for k in keys])
        return np.mean(gs), opt, meanTtb

    go, oo, tto = stats(OFF)
    gn, on_, ttn = stats(ON)
    n = len(keys)

    print(f"FLS=0: gap medio {go:.2f}%, {oo}/{n} otimos, TtB medio {tto:.2f}s")
    print(f"FLS=1: gap medio {gn:.2f}%, {on_}/{n} otimos, TtB medio {ttn:.2f}s")
    print(f"Delta TtB: {ttn - tto:+.2f}s ({(ttn - tto) / tto * 100 if tto else 0:+.1f}%)")

    print("\nInstancias com gap FLS=1 pior que FLS=0 em mais de 1 ponto percentual:")
    regressed = []
    for k in keys:
        d = gap(ON[k]["best"], ON[k]["opt"]) - gap(OFF[k]["best"], OFF[k]["opt"])
        if d > 1.0:
            regressed.append((k, d))
    if regressed:
        for k, d in sorted(regressed, key=lambda x: -x[1]):
            print(f"  instancia {k}: gap piora {d:+.1f}pp")
    else:
        print("  nenhuma")

    half = (n + 1) // 2
    left, right = keys[:half], keys[half:]

    fig, axes = plt.subplots(1, 2, figsize=(16, 20))
    fig.suptitle(
        "FLS=0 (varredura completa) x FLS=1 (sub-vizinhancas ativas) | config ILS#1 | 94 instancias\n"
        f"FLS=0: gap {go:.1f}%, {oo}/{n} otimos, TtB {tto:.1f}s   |   "
        f"FLS=1: gap {gn:.1f}%, {on_}/{n} otimos, TtB {ttn:.1f}s",
        fontweight="bold", fontsize=13, y=1.005,
    )
    draw(axes[0], left, OFF, ON, f"Instancias 1-{half}")
    draw(axes[1], right, OFF, ON, f"Instancias {half+1}-{n}")

    legend = [
        mpatches.Patch(color=GREEN, label="GAP <= 5%"),
        mpatches.Patch(color=YELLOW, label="GAP <= 20%"),
        mpatches.Patch(color=RED, label="GAP > 20%"),
    ]
    fig.legend(handles=legend, loc="lower center", ncol=3, fontsize=10,
               frameon=True, bbox_to_anchor=(0.5, -0.005))
    fig.tight_layout()
    out = Path("data/charts_fls")
    out.mkdir(parents=True, exist_ok=True)
    path = out / "table_fls.png"
    fig.savefig(path, bbox_inches="tight", dpi=130)
    plt.close(fig)
    print(f"\nSalvo: {path}")


if __name__ == "__main__":
    main()
