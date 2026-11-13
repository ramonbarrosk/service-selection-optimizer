#!/usr/bin/env python3
"""Tabelas e gráficos da comparação ILS#3 x GLS nas instâncias difíceis geradas.

Usa a mesma análise de analyze_comparacao.py e grava em
experiments/10_instancias_dificeis/charts:
  - tabela_resumo_perfis.png: resumo por perfil;
  - tabela_por_instancia.png: as 75 instâncias, com o GAP colorido;
  - gap_por_perfil.png: GAP médio de cada método por perfil;
  - otimos_por_perfil.png: instâncias em que cada método atingiu o ótimo provado;
  - c50_cplex_interrompido.png: GLS e ILS#3 (10 s) contra o CPLEX parado em 15 min;
  - tabela_cplex_interrompido.png: as mesmas instâncias, com limite inferior, gap e
    tempos do CPLEX.

    python3 experiments/10_instancias_dificeis/scripts/plot_comparacao.py
"""

import os
import sys
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_comparacao import (  # noqa: E402
    ROOT, TIMEOUT_LOGS, analyze, br, fmt_p, group_summary, log_cost)

CHARTS = ROOT / "experiments/10_instancias_dificeis/charts"

# Mesmas cores da fase 09; o CPLEX usa o terceiro slot da paleta categórica.
GLS_COLOR = "#2a78d6"
ILS_COLOR = "#eb6834"
CPLEX_COLOR = "#1baf7a"
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
SURFACE = "#fcfcfb"
HEADER = "#172554"
ROW_ALT = "#eaf2ff"
PROVEN_TRACK = "#e4e3df"
STATUS_LABEL = {"Optimal": "Ótimo", "Feasible": "Viável"}
STATUS_COLOR = {"Optimal": "#bbf7d0", "Feasible": "#fef3c7"}


def incumbent_window(c):
    """Intervalo em que a última solução do CPLEX apareceu, entre duas "Elapsed time"."""
    if c["last_incumbent_lo_s"] == 0:
        return f"≤ {br(c['last_incumbent_s'], 1)} s"
    return f"{br(c['last_incumbent_lo_s'], 1)}–{br(c['last_incumbent_s'], 1)} s"


def style_axes(ax, grid_axis="y"):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right", "left"):
        ax.spines[side].set_visible(False)
    ax.spines["bottom"].set_color(GRID)
    ax.tick_params(colors=TEXT_SECONDARY, length=0)
    ax.grid(axis=grid_axis, color=GRID, linewidth=1)
    ax.set_axisbelow(True)


def legend(ax, handles, ncol):
    ax.legend(handles=handles, frameon=False, labelcolor=TEXT, loc="lower left",
              bbox_to_anchor=(0, 1.0), ncol=ncol, fontsize=9, borderaxespad=0.2)


def swatch(color, label):
    return plt.Rectangle((0, 0), 1, 1, color=color, label=label)


def profile_groups(rows):
    profiles = list(dict.fromkeys(r["profile"] for r in rows))
    return [(p, group_summary([r for r in rows if r["profile"] == p])) for p in profiles]


def style_table(table, n_rows, n_cols):
    table.auto_set_font_size(False)
    for col in range(n_cols):
        table[0, col].set_facecolor(HEADER)
        table[0, col].set_text_props(color="white", weight="bold")
    for row in range(1, n_rows + 1):
        for col in range(n_cols):
            table[row, col].set_facecolor("#ffffff" if row % 2 else ROW_ALT)
            table[row, col].set_edgecolor(GRID)


def gap_color(value):
    if value <= 1e-9:
        return "#bbf7d0"
    if value <= 5:
        return "#fef3c7"
    return "#fecaca"


def plot_summary_table(rows):
    groups = profile_groups(rows) + [("Total", group_summary(rows))]
    columns = ["Perfil", "Instâncias", "CPLEX\nprovou", "CPLEX\ntempo médio", "Ótimo\nprovado", "GLS no\nótimo",
               "ILS#3 no\nótimo", "GAP médio\nGLS", "GAP médio\nILS#3",
               "GLS\nmelhor", "ILS#3\nmelhor", "p-valor\n(Wilcoxon)"]
    cells = [[name, str(g["n"]), str(g["cplex_optimal"]), f"{br(g['cplex_time'], 1)} s",
              str(g["proven"]), str(g["gls_proven"]), str(g["ils_proven"]),
              f"{br(g['gls_gap'])}%", f"{br(g['ils_gap'])}%", str(g["gls_wins"]),
              str(g["ils_wins"]), fmt_p(g["p"])] for name, g in groups]

    fig, ax = plt.subplots(figsize=(15.5, 3.6), facecolor=SURFACE)
    ax.axis("off")
    table = ax.table(cellText=cells, colLabels=columns, cellLoc="center", loc="center")
    style_table(table, len(cells), len(columns))
    table.set_fontsize(10.5)
    table.scale(1, 2.3)
    for col in range(len(columns)):
        table[len(cells), col].set_text_props(weight="bold")
    for row in range(1, len(cells) + 1):
        table[row, 7].set_facecolor(gap_color(groups[row - 1][1]["gls_gap"]))
        table[row, 8].set_facecolor(gap_color(groups[row - 1][1]["ils_gap"]))

    reps = rows[0]["reps"]
    fig.suptitle("ILS#3 × GLS nas 75 instâncias difíceis geradas",
                 fontsize=15, weight="bold", color=TEXT, y=1.06)
    fig.text(0.5, 0.97,
             f"{reps} repetições de 10 s por método, parada ao atingir o ótimo | referência: "
             "ótimo do CPLEX (15 min) ou, sem ele, melhor solução conhecida",
             ha="center", fontsize=10, color=TEXT_SECONDARY)
    fig.text(0.5, 0.06,
             "T97–T100: capacidade ocupada de 97% a 100% | C50: custo correlacionado com "
             "confiabilidade (ρ = 0,5) | CPLEX provou: ótimo provado em até 15 min "
             "| GAP: verde = 0, amarelo ≤ 5%, vermelho > 5%",
             ha="center", fontsize=9, color=TEXT_SECONDARY)
    fig.savefig(CHARTS / "tabela_resumo_perfis.png", dpi=200, bbox_inches="tight",
                facecolor=SURFACE)
    plt.close(fig)


def plot_instance_table(rows):
    columns = ["Inst.", "Perfil", "Referência", "Fonte", "CPLEX", "CPLEX\ntempo total",
               "CPLEX\núltima solução", "CPLEX\ngap final", "GLS médio", "GLS GAP",
               "ILS#3 médio", "ILS#3 GAP", "Acertos GLS", "Acertos ILS#3"]
    reps = rows[0]["reps"]
    cells = [[str(r["id"]), r["profile"], f"{r['reference']:.0f}", r["source"],
              STATUS_LABEL[r["cplex"]["status"]], f"{br(r['cplex']['total_s'], 1)} s",
              incumbent_window(r["cplex"]), f"{br(r['cplex']['gap_pct'])}%",
              br(r["gls_mean"], 1), f"{br(r['gls_gap'])}%", br(r["ils_mean"], 1),
              f"{br(r['ils_gap'])}%", f"{r['gls_hits']}/{reps}", f"{r['ils_hits']}/{reps}"]
             for r in rows]

    fig, ax = plt.subplots(figsize=(19, 0.3 * len(rows) + 1.5), facecolor=SURFACE)
    ax.axis("off")
    table = ax.table(cellText=cells, colLabels=columns, cellLoc="center", loc="center")
    style_table(table, len(cells), len(columns))
    table.set_fontsize(8.5)
    table.scale(1, 1.42)
    table[0, 0].set_height(table[0, 0].get_height() * 1.6)
    for col in range(len(columns)):
        table[0, col].set_height(table[0, 0].get_height())
    for row, r in enumerate(rows, start=1):
        table[row, 4].set_facecolor(STATUS_COLOR[r["cplex"]["status"]])
        table[row, 9].set_facecolor(gap_color(r["gls_gap"]))
        table[row, 11].set_facecolor(gap_color(r["ils_gap"]))

    fig.suptitle("Comparação por instância — ILS#3 × GLS nas instâncias difíceis geradas",
                 fontsize=15, weight="bold", color=TEXT, y=0.995)
    fig.text(0.5, 0.985,
             f"{reps} repetições de 10 s | Fonte da referência: exato = ótimo do CPLEX; "
             "BKS = melhor solução conhecida (CPLEX parou em 15 min) | CPLEX: Ótimo = provado; "
             "Viável = solução viável sem prova | última solução: intervalo entre duas linhas "
             "\"Elapsed time\" do log",
             ha="center", va="top", fontsize=10, color=TEXT_SECONDARY)
    fig.tight_layout(rect=[0.01, 0.01, 0.99, 0.98])
    fig.savefig(CHARTS / "tabela_por_instancia.png", dpi=180, bbox_inches="tight",
                facecolor=SURFACE)
    plt.close(fig)


def plot_gap_by_profile(rows):
    groups = profile_groups(rows)
    x = np.arange(len(groups))
    width = 0.36
    fig, ax = plt.subplots(figsize=(9, 4.8), facecolor=SURFACE)
    style_axes(ax)
    for offset, key, color in ((-width / 2 - 0.01, "gls_gap", GLS_COLOR),
                               (width / 2 + 0.01, "ils_gap", ILS_COLOR)):
        values = [g[key] for _, g in groups]
        ax.bar(x + offset, values, width=width, color=color, edgecolor=SURFACE, linewidth=2)
        for pos, value in zip(x, values):
            ax.text(pos + offset, value + 0.6, f"{br(value)}%", ha="center", va="bottom",
                    fontsize=9, color=TEXT)
    ax.set_xticks(x)
    ax.set_xticklabels([name for name, _ in groups], color=TEXT)
    ax.set_ylabel("GAP médio para a referência (%)", color=TEXT_SECONDARY)
    ax.set_ylim(0, max(g["ils_gap"] for _, g in groups) * 1.15)
    ax.set_title("GAP médio por perfil", color=TEXT, loc="left", fontsize=12, pad=26)
    legend(ax, [swatch(GLS_COLOR, "GLS (C++)"), swatch(ILS_COLOR, "ILS#3 (Java)")], ncol=2)
    fig.tight_layout()
    fig.savefig(CHARTS / "gap_por_perfil.png", dpi=200, facecolor=SURFACE)
    plt.close(fig)


def plot_optimal_by_profile(rows):
    groups = profile_groups(rows)
    y = np.arange(len(groups))[::-1]
    height = 0.34
    fig, ax = plt.subplots(figsize=(9, 4.8), facecolor=SURFACE)
    style_axes(ax, grid_axis="x")
    for pos, (_, g) in zip(y, groups):
        # Trilho cinza: instâncias com ótimo provado, o máximo que cada método pode atingir.
        for offset in (height / 2 + 0.02, -height / 2 - 0.02):
            ax.barh(pos + offset, g["proven"], height=height, color=PROVEN_TRACK)
        ax.barh(pos + height / 2 + 0.02, g["gls_proven"], height=height, color=GLS_COLOR)
        ax.barh(pos - height / 2 - 0.02, g["ils_proven"], height=height, color=ILS_COLOR)
        ax.text(g["proven"] + 0.3, pos + height / 2 + 0.02, f"{g['gls_proven']}/{g['proven']}",
                va="center", fontsize=9, color=TEXT)
        ax.text(g["proven"] + 0.3, pos - height / 2 - 0.02, f"{g['ils_proven']}/{g['proven']}",
                va="center", fontsize=9, color=TEXT)
    ax.set_yticks(y)
    ax.set_yticklabels([name for name, _ in groups], color=TEXT)
    ax.set_xlim(0, 17.5)
    ax.set_xticks(range(0, 16, 3))
    ax.set_xlabel("Instâncias em que o método atingiu o ótimo provado", color=TEXT_SECONDARY)
    ax.set_title("Ótimos atingidos por perfil", color=TEXT, loc="left", fontsize=12, pad=26)
    legend(ax, [swatch(GLS_COLOR, "GLS (C++)"), swatch(ILS_COLOR, "ILS#3 (Java)"),
                swatch(PROVEN_TRACK, "Instâncias com ótimo provado")], ncol=3)
    fig.tight_layout()
    fig.savefig(CHARTS / "otimos_por_perfil.png", dpi=200, facecolor=SURFACE)
    plt.close(fig)


def plot_interrupted_cplex(rows):
    interrupted = [r for r in rows if log_cost(TIMEOUT_LOGS, r["id"]) is not None]
    if not interrupted:
        return
    series = [
        ("CPLEX, parado em 15 min", CPLEX_COLOR,
         [log_cost(TIMEOUT_LOGS, r["id"]) for r in interrupted]),
        ("ILS#3, 10 s (média)", ILS_COLOR, [r["ils_mean"] for r in interrupted]),
        ("GLS, 10 s (média)", GLS_COLOR, [r["gls_mean"] for r in interrupted]),
    ]
    x = np.arange(len(interrupted))
    width = 0.27
    fig, ax = plt.subplots(figsize=(10.5, 5), facecolor=SURFACE)
    style_axes(ax)
    for index, (_, color, values) in enumerate(series):
        offset = (index - 1) * (width + 0.01)
        ax.bar(x + offset, values, width=width, color=color, edgecolor=SURFACE, linewidth=2)
        for pos, value in zip(x, values):
            ax.text(pos + offset, value + 12, f"{value:.0f}", ha="center", va="bottom",
                    fontsize=8, color=TEXT)
    ax.set_xticks(x)
    ax.set_xticklabels([f"Inst. {r['id']}" for r in interrupted], color=TEXT)
    ax.set_ylabel("Custo da melhor solução", color=TEXT_SECONDARY)
    ax.set_ylim(0, max(max(values) for _, _, values in series) * 1.12)
    ax.set_title("C50: instâncias em que o CPLEX não provou o ótimo em 15 min",
                 color=TEXT, loc="left", fontsize=12, pad=26)
    legend(ax, [swatch(color, label) for label, color, _ in series], ncol=3)
    fig.tight_layout()
    fig.savefig(CHARTS / "c50_cplex_interrompido.png", dpi=200, facecolor=SURFACE)
    plt.close(fig)


def plot_interrupted_table(rows):
    interrupted = [r for r in rows if r["cplex"]["status"] != "Optimal"]
    if not interrupted:
        return
    columns = ["Inst.", "CPLEX\ncusto", "CPLEX limite\ninferior", "CPLEX\ngap final",
               "CPLEX\núltima solução", "CPLEX\ntempo total", "Cortes\nde SLA",
               "ILS#3 10 s\n(média)", "GLS 10 s\n(média)", "GLS: gap p/\nlimite inf."]
    cells = []
    for r in interrupted:
        c = r["cplex"]
        cells.append([
            str(r["id"]), f"{c['cost']:.0f}", br(c["bound"], 1), f"{br(c['gap_pct'])}%",
            incumbent_window(c), f"{br(c['total_s'], 1)} s",
            str(c["violation_cuts"]), br(r["ils_mean"], 1), br(r["gls_mean"], 1),
            f"{br(100 * (r['gls_mean'] - c['bound']) / r['gls_mean'])}%"])

    fig, ax = plt.subplots(figsize=(15.5, 4.2), facecolor=SURFACE)
    ax.axis("off")
    table = ax.table(cellText=cells, colLabels=columns, cellLoc="center", loc="center")
    style_table(table, len(cells), len(columns))
    table.set_fontsize(10.5)
    table.scale(1, 2.3)
    fig.suptitle("C50: instâncias em que o CPLEX parou em 15 min sem provar o ótimo",
                 fontsize=15, weight="bold", color=TEXT, y=1.02)
    fig.text(0.5, 0.94,
             "Gap = (custo − limite inferior) / custo. O ótimo está entre o limite inferior do "
             "CPLEX e a melhor solução conhecida.",
             ha="center", fontsize=10, color=TEXT_SECONDARY)
    fig.text(0.5, 0.0,
             "Última solução: intervalo em que o CPLEX achou sua melhor solução, entre duas "
             "linhas \"Elapsed time\" do log | Cortes de SLA: soluções descartadas "
             "pelo callback por violar a SLA",
             ha="center", fontsize=9, color=TEXT_SECONDARY)
    fig.savefig(CHARTS / "tabela_cplex_interrompido.png", dpi=200, bbox_inches="tight",
                facecolor=SURFACE)
    plt.close(fig)


def main():
    CHARTS.mkdir(parents=True, exist_ok=True)
    rows = analyze()
    plot_summary_table(rows)
    plot_instance_table(rows)
    plot_gap_by_profile(rows)
    plot_optimal_by_profile(rows)
    plot_interrupted_cplex(rows)
    plot_interrupted_table(rows)
    for path in sorted(CHARTS.glob("*.png")):
        print(path.relative_to(ROOT))


if __name__ == "__main__":
    main()
