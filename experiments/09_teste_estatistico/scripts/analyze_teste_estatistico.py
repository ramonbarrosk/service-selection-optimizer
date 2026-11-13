#!/usr/bin/env python3
"""Testes estatísticos ILS#3 (Java) x GLS (C++) nas instâncias difíceis.

Lê uma linha por repetição de cada método e produz:
  - um único teste de Wilcoxon pareado (postos sinalizados), exato e bilateral, sobre o
    custo médio por instância (a instância é a unidade de observação);
  - gráficos de distribuição do GAP e de taxa de acerto do ótimo;
  - resumo em Markdown e tabela LaTeX (custos médios por instância, para o apêndice).

    python3 experiments/09_teste_estatistico/scripts/analyze_teste_estatistico.py
"""

import csv
import os
from collections import defaultdict
from pathlib import Path
from statistics import mean, median

os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
RESULTS = ROOT / "experiments/09_teste_estatistico/results"
CHARTS = ROOT / "experiments/09_teste_estatistico/charts"
ILS_CSV = RESULTS / "ils3_dificeis.csv"
GLS_CSV = RESULTS / "gls_dificeis.csv"
SUMMARY = RESULTS / "summary.md"
LATEX = RESULTS / "tabela_teste_estatistico.tex"

ALPHA = 0.05
GLS_COLOR = "#2a78d6"
ILS_COLOR = "#eb6834"
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
SURFACE = "#fcfcfb"


def read_runs(path):
    runs = defaultdict(list)
    optimum = {}
    with path.open() as handle:
        for row in csv.DictReader(handle):
            iid = int(row["instance"])
            runs[iid].append({
                "cost": float(row["cost"]),
                "gap": float(row["gap_pct"]),
                "optimal": row["reached_optimal"] == "1",
                "time_to_best_ms": float(row["time_to_best_ms"]),
            })
            optimum[iid] = int(row["optimum"])
    return runs, optimum


def midranks(values):
    """Postos médios (1-based) para valores com empates."""
    order = sorted(range(len(values)), key=lambda k: values[k])
    ranks = [0.0] * len(values)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and values[order[j + 1]] == values[order[i]]:
            j += 1
        for k in range(i, j + 1):
            ranks[order[k]] = (i + j) / 2 + 1
        i = j + 1
    return ranks


def wilcoxon(diffs):
    """Wilcoxon pareado exato e bilateral (zeros descartados, postos médios nos empates).

    Retorna (p, n, W+, W-); p é None quando todas as diferenças são nulas.
    """
    nonzero = [d for d in diffs if d != 0]
    if not nonzero:
        return None, 0, 0.0, 0.0
    ranks = midranks([abs(d) for d in nonzero])
    doubled = [int(round(2 * r)) for r in ranks]
    w_plus = sum(r for r, d in zip(doubled, nonzero) if d > 0)
    counts = [0] * (sum(doubled) + 1)
    counts[0] = 1
    for w in doubled:
        for s in range(len(counts) - 1, w - 1, -1):
            counts[s] += counts[s - w]
    total = sum(counts)
    upper = sum(counts[w_plus:]) / total
    lower = sum(counts[: w_plus + 1]) / total
    p = min(1.0, 2 * min(upper, lower))
    w_minus = sum(doubled) - w_plus
    return p, len(nonzero), w_plus / 2, w_minus / 2


def fmt_p(p):
    if p is None:
        return "–"
    text = f"{p:.1e}".replace("e-0", "e-") if p < 0.001 else f"{p:.3f}"
    return text.replace(".", ",")


def fmt_p_latex(p):
    if p is None:
        return "--"
    if p < 0.001:
        mantissa, exponent = f"{p:.1e}".split("e")
        return f"${mantissa.replace('.', '{,}')} \\times 10^{{{int(exponent)}}}$"

    return f"{p:.3f}".replace(".", "{,}")


def br(value, digits=2):
    return f"{value:.{digits}f}".replace(".", ",")


def analyze():
    ils, optimum = read_runs(ILS_CSV)
    gls, _ = read_runs(GLS_CSV)
    ids = sorted(set(ils) & set(gls))
    if ids != sorted(ils) or ids != sorted(gls):
        raise SystemExit(f"Instâncias divergentes: ILS={sorted(ils)} GLS={sorted(gls)}")

    rows = []
    for iid in ids:
        x = [r["cost"] for r in ils[iid]]
        y = [r["cost"] for r in gls[iid]]
        rows.append({
            "instance": iid,
            "optimum": optimum[iid],
            "n_ils": len(x),
            "n_gls": len(y),
            "ils_gaps": [r["gap"] for r in ils[iid]],
            "gls_gaps": [r["gap"] for r in gls[iid]],
            "ils_mean_cost": mean(x),
            "gls_mean_cost": mean(y),
            "ils_mean_gap": mean(r["gap"] for r in ils[iid]),
            "ils_median_gap": median(r["gap"] for r in ils[iid]),
            "gls_mean_gap": mean(r["gap"] for r in gls[iid]),
            "ils_hits": sum(r["optimal"] for r in ils[iid]),
            "gls_hits": sum(r["optimal"] for r in gls[iid]),
        })

    global_test = wilcoxon([r["ils_mean_cost"] - r["gls_mean_cost"] for r in rows])
    return rows, global_test


def style_axes(ax):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(GRID)
    ax.tick_params(colors=TEXT_SECONDARY, length=0)
    ax.grid(axis="x", color=GRID, linewidth=1)
    ax.set_axisbelow(True)


def legend(ax, handles, **kwargs):
    leg = ax.legend(handles=handles, frameon=False, labelcolor=TEXT, **kwargs)
    return leg


def plot_gap_distribution(rows, global_test):
    ordered = sorted(rows, key=lambda r: (r["ils_median_gap"], r["ils_mean_gap"]))
    fig, ax = plt.subplots(figsize=(9, 0.42 * len(ordered) + 1.6), facecolor=SURFACE)
    style_axes(ax)
    rng = np.random.default_rng(7)
    for pos, row in enumerate(ordered):
        gaps = np.array(row["ils_gaps"])
        jitter = rng.uniform(-0.16, 0.16, len(gaps))
        ax.scatter(gaps, pos + jitter, s=26, color=ILS_COLOR, alpha=0.75,
                   edgecolors=SURFACE, linewidths=1, zorder=3)
        ax.plot([np.median(gaps)] * 2, [pos - 0.3, pos + 0.3], color=TEXT, linewidth=2,
                solid_capstyle="round", zorder=4)
        gls_gap = row["gls_mean_gap"]
        ax.scatter([gls_gap], [pos], s=90, marker="D", color=GLS_COLOR,
                   edgecolors=SURFACE, linewidths=2, zorder=5)

    ax.set_xscale("symlog", linthresh=1)
    ax.set_xlim(left=-0.2)
    ax.set_yticks(range(len(ordered)))
    ax.set_yticklabels([f"Inst. {r['instance']}" for r in ordered], color=TEXT)
    ax.set_xlabel("GAP para o ótimo (%) - escala log simétrica", color=TEXT_SECONDARY)
    n_ils = ordered[0]["n_ils"]
    ax.set_title(f"GAP por repetição nas instâncias difíceis ({n_ils} execuções de 10 s por método)",
                 color=TEXT, loc="left", fontsize=12, pad=26)
    handles = [
        plt.Line2D([], [], marker="o", linestyle="", color=ILS_COLOR, markersize=6, label="ILS#3 (Java), cada execução"),
        plt.Line2D([], [], color=TEXT, linewidth=2, label="Mediana do ILS#3"),
        plt.Line2D([], [], marker="D", linestyle="", color=GLS_COLOR, markersize=7, label="GLS (C++), determinística"),
    ]
    legend(ax, handles, loc="lower left", bbox_to_anchor=(0, 1.0), ncol=3, fontsize=9,
           borderaxespad=0.2, handletextpad=0.4, columnspacing=1.2)
    p_global, n_nonzero, w_plus, w_minus = global_test
    fig.text(0.99, 0.01, f"Wilcoxon pareado sobre o custo médio por instância: n = {n_nonzero}, "
             f"W+ = {w_plus:g}, W- = {w_minus:g}, p = {fmt_p(p_global)}",
             ha="right", fontsize=8, color=TEXT_SECONDARY)
    fig.tight_layout()
    fig.savefig(CHARTS / "gap_por_instancia.png", dpi=200, facecolor=SURFACE)
    plt.close(fig)


def plot_hit_rate(rows):
    ordered = sorted(rows, key=lambda r: (r["ils_hits"] / r["n_ils"], r["instance"]))
    fig, ax = plt.subplots(figsize=(9, 0.42 * len(ordered) + 1.6), facecolor=SURFACE)
    style_axes(ax)
    height = 0.34
    for pos, row in enumerate(ordered):
        ils_rate = 100 * row["ils_hits"] / row["n_ils"]
        gls_rate = 100 * row["gls_hits"] / row["n_gls"]
        ax.barh(pos + height / 2 + 0.02, gls_rate, height=height, color=GLS_COLOR)
        ax.barh(pos - height / 2 - 0.02, ils_rate, height=height, color=ILS_COLOR)
        ax.text(gls_rate + 1, pos + height / 2 + 0.02, f"{row['gls_hits']}/{row['n_gls']}",
                va="center", fontsize=8, color=TEXT_SECONDARY)
        ax.text(ils_rate + 1, pos - height / 2 - 0.02, f"{row['ils_hits']}/{row['n_ils']}",
                va="center", fontsize=8, color=TEXT_SECONDARY)
    ax.set_xlim(0, 112)
    ax.set_xticks([0, 25, 50, 75, 100])
    ax.set_xticklabels(["0%", "25%", "50%", "75%", "100%"])
    ax.set_yticks(range(len(ordered)))
    ax.set_yticklabels([f"Inst. {r['instance']}" for r in ordered], color=TEXT)
    ax.set_xlabel("Execuções que atingiram o ótimo conhecido", color=TEXT_SECONDARY)
    ax.set_title("Taxa de acerto do ótimo por instância difícil", color=TEXT, loc="left",
                 fontsize=12, pad=26)
    handles = [
        plt.Rectangle((0, 0), 1, 1, color=GLS_COLOR, label="GLS (C++)"),
        plt.Rectangle((0, 0), 1, 1, color=ILS_COLOR, label="ILS#3 (Java)"),
    ]
    legend(ax, handles, loc="lower left", bbox_to_anchor=(0, 1.0), ncol=2, fontsize=9,
           borderaxespad=0.2)
    fig.tight_layout()
    fig.savefig(CHARTS / "taxa_acerto_otimo.png", dpi=200, facecolor=SURFACE)
    plt.close(fig)


def write_summary(rows, global_test):
    p_global, n_nonzero, w_plus, w_minus = global_test
    ties = [r["instance"] for r in rows if r["ils_mean_cost"] == r["gls_mean_cost"]]
    verdict = "há" if p_global is not None and p_global < ALPHA else "não há"
    lines = [
        "# Teste de Wilcoxon: ILS#3 x GLS nas instâncias difíceis",
        "",
        f"Instâncias: {len(rows)}; repetições por método: {rows[0]['n_ils']}; orçamento de 10 s por "
        "repetição, com parada ao atingir o ótimo.",
        "",
        "Um único teste de Wilcoxon pareado (postos sinalizados), exato e bilateral, sobre o custo "
        "médio por instância: cada instância é um par (GLS, ILS#3), e a hipótese nula é que a "
        "diferença entre os métodos é simétrica em torno de zero. Instâncias empatadas são descartadas.",
        "",
        f"**Resultado:** n = {n_nonzero} instâncias com diferença "
        f"(empates: {', '.join(map(str, ties)) or 'nenhum'}), W+ = {w_plus:g}, W- = {w_minus:g}, "
        f"p = {fmt_p(p_global)}. Com α = {br(ALPHA)}, {verdict} diferença estatisticamente significativa.",
        "",
        "W+ soma os postos das instâncias em que o ILS#3 teve custo maior (pior) que a GLS.",
        "",
        "| Instância | GLS | ILS#3 |",
        "|---:|---:|---:|",
    ]
    for r in rows:
        lines.append(f"| {r['instance']} | {br(r['gls_mean_cost'])} | {br(r['ils_mean_cost'])} |")
    lines += ["", "GLS e ILS#3: custo médio das repetições."]
    SUMMARY.write_text("\n".join(lines) + "\n")


def write_latex(rows, global_test):
    p_global, n_nonzero, w_plus, w_minus = global_test
    lines = [
        "\\begin{table}[htbp]",
        "\\centering",
        "\\caption{Custo médio da GLS e da ILS nas 15 instâncias de referência executadas "
        f"{rows[0]['n_ils']} vezes por método (10\\,s por execução). Teste de Wilcoxon pareado sobre "
        f"esses custos: $n = {n_nonzero}$, $W^{{+}} = {w_plus:g}$, $W^{{-}} = {w_minus:g}$, "
        f"$p$ = {fmt_p_latex(p_global)}.}}",
        "\\label{tab:wilcoxon}",
        "\\begin{tabular}{rrr}",
        "\\toprule",
        "Inst. & GLS & ILS \\\\",
        "\\midrule",
    ]
    for r in rows:
        lines.append(f"{r['instance']} & {br(r['gls_mean_cost'])} & {br(r['ils_mean_cost'])} \\\\")
    lines += ["\\bottomrule", "\\end{tabular}", "\\end{table}"]
    LATEX.write_text("\n".join(lines) + "\n")


def main():
    CHARTS.mkdir(parents=True, exist_ok=True)
    rows, global_test = analyze()
    plot_gap_distribution(rows, global_test)
    plot_hit_rate(rows)
    write_summary(rows, global_test)
    write_latex(rows, global_test)
    print(SUMMARY.read_text())


if __name__ == "__main__":
    main()
