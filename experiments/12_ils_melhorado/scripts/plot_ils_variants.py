#!/usr/bin/env python3
"""Gráficos da comparação ILS#3 com melhorias × GLS (fase 12).

Grava em experiments/12_ils_melhorado/charts:
  - gap_por_variante.png: GAP médio de cada método nos três cenários
    (referência 0,55 s, referência 10 s e geradas 10 s);
  - gap_por_perfil.png: GAP médio por perfil das instâncias geradas;
  - c50_diferenca_por_instancia.png: diferença de custo médio para a GLS em cada
    instância C50, para as duas variantes melhoradas do ILS;
  - gap_por_variante_sem_correcoes.png e gap_por_perfil_sem_correcoes.png: as mesmas
    figuras só com GLS, ILS#3 original e ILS#3 + incremental + substituição.

As referências do GAP são as mesmas de analyze_ils_variants.py.

    python3 experiments/12_ils_melhorado/scripts/plot_ils_variants.py
"""

import os
import sys
from pathlib import Path
from statistics import mean

os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_ils_variants import GLS, ORIGINAL, RESULTS, ROOT, read, references  # noqa: E402

CHARTS = ROOT / "experiments/12_ils_melhorado/charts"

# Slots 1 a 4 da paleta categórica, na ordem fixa (validados com validate_palette.js).
# GLS e ILS#3 original mantêm as cores das fases 09 e 10.
METHODS = [
    ("GLS", "GLS", "#2a78d6"),
    ("orig", "ILS#3 original", "#eb6834"),
    ("110", "ILS#3 + incremental + substituição", "#1baf7a"),
    ("111", "ILS#3 + incremental + substituição + correções", "#eda100"),
]
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
SURFACE = "#fcfcfb"
SUFFIX = ""
PROFILES = [("T97", 1100), ("T98", 1200), ("T99", 1300), ("T100", 1400), ("C50", 1500)]


def br(value, digits=2):
    return f"{value:.{digits}f}".replace(".", ",")


def load(set_name, budget):
    """Métodos do cenário, na ordem de METHODS."""
    methods = {"GLS": read(GLS[(set_name, budget)])}
    original = RESULTS / f"ils3_{set_name}_{budget}s_i0_s0_k0.csv"
    if original.exists():
        methods["orig"] = read(original)
    else:
        methods["orig"] = read(ORIGINAL[(set_name, budget)])
    for key in [m[0] for m in METHODS if m[0] in ("110", "111")]:
        methods[key] = read(RESULTS / f"ils3_{set_name}_{budget}s_i{key[0]}_s{key[1]}_k{key[2]}.csv")
    return methods


def gaps(methods, refs, ids=None):
    result = {}
    for key, runs in methods.items():
        chosen = sorted(set(runs) & set(refs)) if ids is None else ids
        result[key] = mean(mean(100 * (r["cost"] - refs[i][0]) / refs[i][0] for r in runs[i])
                           for i in chosen)
    return result


def style_axes(ax, grid_axis):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(GRID)
    ax.tick_params(colors=TEXT_SECONDARY, length=0)
    ax.grid(axis=grid_axis, color=GRID, linewidth=1)
    ax.set_axisbelow(True)
    decimal = FuncFormatter(lambda v, _: f"{v:g}".replace(".", ",").replace("-", "−"))
    # Só no eixo de valores: o outro eixo tem rótulos categóricos.
    (ax.xaxis if grid_axis == "x" else ax.yaxis).set_major_formatter(decimal)


def legend(fig, handles):
    fig.legend(handles=handles, frameon=False, labelcolor=TEXT, loc="upper left",
               bbox_to_anchor=(0.01, 0.92), ncol=2, fontsize=9)


def swatch(color, label):
    return plt.Rectangle((0, 0), 1, 1, color=color, label=label)


def gap_por_variante():
    scenarios = [("ref", "0.55", "94 de referência, 0,55 s"),
                 ("ref", "10", "94 de referência, 10 s"),
                 ("hard", "10", "75 geradas, 10 s")]
    fig, axes = plt.subplots(1, 3, figsize=(12, 4.2), facecolor=SURFACE)
    for ax, (set_name, budget, title) in zip(axes, scenarios):
        methods = load(set_name, budget)
        values = gaps(methods, references(set_name, methods))
        keys = [m[0] for m in METHODS]
        y = np.arange(len(keys))[::-1]
        bars = ax.barh(y, [values[k] for k in keys], height=0.62 if len(keys) == 4 else 0.55,
                       color=[m[2] for m in METHODS], edgecolor=SURFACE, linewidth=2)
        top = max(values.values())
        for bar, key in zip(bars, keys):  # noqa: B007
            ax.text(bar.get_width() + top * 0.02, bar.get_y() + bar.get_height() / 2,
                    f"{br(values[key])}%", va="center", fontsize=9, color=TEXT)
        ax.set_xlim(0, top * 1.25)
        ax.set_yticks([])
        ax.set_title(title, loc="left", fontsize=10, color=TEXT, pad=8)
        ax.set_xlabel("GAP médio (%)", fontsize=9, color=TEXT_SECONDARY)
        style_axes(ax, "x")
    fig.suptitle("GAP médio da GLS e das variantes do ILS#3 (escala própria em cada painel)",
                 x=0.01, ha="left", fontsize=12, color=TEXT)
    legend(fig, [swatch(c, l) for _, l, c in METHODS])
    fig.tight_layout(rect=(0, 0, 1, 0.84))
    fig.savefig(CHARTS / f"gap_por_variante{SUFFIX}.png", dpi=180, facecolor=SURFACE)
    plt.close(fig)


def gap_por_perfil():
    methods = load("hard", "10")
    refs = references("hard", methods)
    fig, ax = plt.subplots(figsize=(11, 4.6), facecolor=SURFACE)
    width = 0.2
    x = np.arange(len(PROFILES))
    top = 0
    for offset, (key, _, color) in enumerate(METHODS):
        values = []
        for _, base in PROFILES:
            ids = [i for i in refs if base < i < base + 100]
            values.append(gaps({key: methods[key]}, refs, ids)[key])
        top = max(top, max(values))
        positions = x + (offset - (len(METHODS) - 1) / 2) * width
        ax.bar(positions, values, width=width, color=color, edgecolor=SURFACE, linewidth=2)
        for pos, value in zip(positions, values):
            ax.text(pos, value + 0.6, br(value, 1), ha="center", va="bottom",
                    fontsize=8, color=TEXT)
    ax.set_xticks(x)
    ax.set_xticklabels([p for p, _ in PROFILES], fontsize=10, color=TEXT)
    ax.set_ylim(0, top * 1.15)
    ax.set_ylabel("GAP médio (%)", fontsize=9, color=TEXT_SECONDARY)
    style_axes(ax, "y")
    fig.suptitle("GAP médio por perfil nas 75 instâncias geradas (10 s, 3 execuções)",
                 x=0.01, ha="left", fontsize=12, color=TEXT)
    legend(fig, [swatch(c, l) for _, l, c in METHODS])
    fig.tight_layout(rect=(0, 0, 1, 0.84))
    fig.savefig(CHARTS / f"gap_por_perfil{SUFFIX}.png", dpi=180, facecolor=SURFACE)
    plt.close(fig)


def c50_diferenca():
    methods = load("hard", "10")
    refs = references("hard", methods)
    ids = sorted(i for i in refs if 1500 < i < 1600)
    gls = {i: mean(r["cost"] for r in methods["GLS"][i]) for i in ids}
    fig, ax = plt.subplots(figsize=(9, 6.2), facecolor=SURFACE)
    y = np.arange(len(ids))[::-1]
    for key, label, color in METHODS[2:]:
        diffs = [100 * (mean(r["cost"] for r in methods[key][i]) - gls[i]) / gls[i] for i in ids]
        ax.scatter(diffs, y, s=64, color=color, edgecolor=SURFACE, linewidth=2,
                   label=label, zorder=3)
    ax.axvline(0, color=TEXT_SECONDARY, linewidth=1, zorder=2)
    style_axes(ax, "x")
    ax.set_yticks(y)
    ax.set_yticklabels([f"{i}{'' if refs[i][1] else ' *'}" for i in ids],
                       fontsize=9, color=TEXT)
    ax.set_xlabel("Custo médio do ILS em relação à GLS (%): à esquerda de 0, o ILS é melhor",
                  fontsize=9, color=TEXT_SECONDARY)
    ax.grid(axis="y", color=GRID, linewidth=0.5)
    fig.suptitle("Perfil C50: diferença de custo para a GLS por instância (10 s)",
                 x=0.01, ha="left", fontsize=12, color=TEXT)
    fig.legend(handles=[plt.Line2D([], [], marker="o", linestyle="", markersize=8,
                                   color=c, label=l) for _, l, c in METHODS[2:]],
               frameon=False, labelcolor=TEXT, loc="upper left",
               bbox_to_anchor=(0.01, 0.94), ncol=1, fontsize=9)
    fig.text(0.01, 0.01, "* sem ótimo provado pelo CPLEX em 15 min", fontsize=8,
             color=TEXT_SECONDARY)
    fig.tight_layout(rect=(0, 0.03, 1, 0.88))
    fig.savefig(CHARTS / "c50_diferenca_por_instancia.png", dpi=180, facecolor=SURFACE)
    plt.close(fig)


def main():
    global METHODS, SUFFIX
    CHARTS.mkdir(parents=True, exist_ok=True)
    gap_por_variante()
    gap_por_perfil()
    c50_diferenca()
    # Versão sem a variante com as correções do ILS: só avaliação incremental e
    # substituição de serviço. As referências do GAP também deixam de incluir as
    # soluções dessa variante.
    METHODS, SUFFIX = METHODS[:3], "_sem_correcoes"
    gap_por_variante()
    gap_por_perfil()
    print(f"Gráficos em {CHARTS}")


if __name__ == "__main__":
    main()
