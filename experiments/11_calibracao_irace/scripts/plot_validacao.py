#!/usr/bin/env python3
"""Tabelas em imagem da calibração com o irace e da validação.

Grava em experiments/11_calibracao_irace/charts:
  - tabela_parametros.png: faixa de cada parâmetro, configuração atual e elites do irace;
  - tabela_validacao.png: atual x calibrada nas 94 do artigo e nas 75 difíceis, por perfil;
  - tabela_sem_otimo.png: as instâncias C50 sem ótimo provado, contra o limite inferior
    do CPLEX.
O melhor valor de cada par atual/calibrada fica destacado em verde.

    python3 experiments/11_calibracao_irace/scripts/plot_validacao.py
"""

import csv
import os
import sys
from pathlib import Path
from statistics import mean

os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experiments/09_teste_estatistico/scripts"))
sys.path.insert(0, str(ROOT / "experiments/10_instancias_dificeis/scripts"))
from analyze_teste_estatistico import br, fmt_p, read_runs, wilcoxon  # noqa: E402
from cplex_log import load_cplex  # noqa: E402

PHASE = ROOT / "experiments/11_calibracao_irace"
RESULTS = PHASE / "results"
VALIDATION = RESULTS / "validacao"
CHARTS = PHASE / "charts"
MANIFEST = ROOT / "experiments/10_instancias_dificeis/results/manifest.csv"

TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e4e3df"
SURFACE = "#fcfcfb"
HEADER = "#172554"
ROW_ALT = "#eaf2ff"
BETTER = "#bbf7d0"
CURRENT_ROW = "#fef3c7"

PARAMETERS = [  # coluna do CSV, rótulo, faixa, valor atual
    ("alpha", "alpha", "[0,01; 1] log", "0,3"),
    ("rounds", "rounds", "5 a 100", "30"),
    ("gfls", "gfls", "0/1", "1"),
    ("replacement", "replacement", "0/1", "1"),
    ("regret", "regret", "0/1", "1"),
    ("calibrate", "calibrate", "0/1", "0"),
    ("maxpen", "maxpen", "0 a 3", "0"),
]


def style_table(table, n_rows, n_cols, fontsize):
    table.auto_set_font_size(False)
    table.set_fontsize(fontsize)
    for col in range(n_cols):
        table[0, col].set_facecolor(HEADER)
        table[0, col].set_text_props(color="white", weight="bold")
    for row in range(1, n_rows + 1):
        for col in range(n_cols):
            table[row, col].set_facecolor("#ffffff" if row % 2 else ROW_ALT)
            table[row, col].set_edgecolor(GRID)


def mark_better(table, row, col_current, col_tuned, current, tuned, lower_is_better=True):
    """Destaca a célula com o melhor valor; empate não destaca nenhuma."""
    if current == tuned:
        return
    tuned_wins = (tuned < current) == lower_is_better
    cell = table[row, col_tuned if tuned_wins else col_current]
    cell.set_facecolor(BETTER)
    cell.set_text_props(weight="bold")


def titles(fig, title, subtitle, top=1.06, sub=0.97):
    fig.suptitle(title, fontsize=15, weight="bold", color=TEXT, y=top)
    fig.text(0.5, sub, subtitle, ha="center", fontsize=10, color=TEXT_SECONDARY)


def footer(fig, text, y):
    fig.text(0.5, y, text, ha="center", fontsize=9, color=TEXT_SECONDARY)


def plot_parameters():
    elites = list(csv.DictReader((RESULTS / "best_configurations.csv").open()))
    columns = ["Parâmetro", "Faixa", "Atual"] + [
        f"Elite {i + 1}\n(config. {e['.ID.']})" for i, e in enumerate(elites)]
    cells = []
    for key, label, domain, current in PARAMETERS:
        values = []
        for elite in elites:
            value = elite[key]
            values.append(br(float(value), 3) if key == "alpha" else value)
        cells.append([label, domain, current] + values)

    fig, ax = plt.subplots(figsize=(12, 3.9), facecolor=SURFACE)
    ax.axis("off")
    table = ax.table(cellText=cells, colLabels=columns, cellLoc="center", loc="center")
    style_table(table, len(cells), len(columns), 10.5)
    table.scale(1, 2.0)
    for row in range(1, len(cells) + 1):
        table[row, 2].set_facecolor(CURRENT_ROW)
        table[row, 3].set_facecolor(BETTER)
        table[row, 3].set_text_props(weight="bold")
        table[row, 0].set_text_props(weight="bold")

    titles(fig, "Calibração da GLS com o irace",
           "50 instâncias de treino (sementes ≥ 10001) | 3.000 execuções de 10 s | "
           "elites em ordem de classificação", top=1.07, sub=0.98)
    footer(fig, "Amarelo: configuração usada nas fases 06 a 10 | Verde: melhor configuração "
                "do irace | alpha ficou no limite superior da faixa", 0.04)
    fig.savefig(CHARTS / "tabela_parametros.png", dpi=200, bbox_inches="tight",
                facecolor=SURFACE)
    plt.close(fig)


def group_stats(current, tuned, ids):
    diffs = [mean(r["cost"] for r in current[i]) - mean(r["cost"] for r in tuned[i])
             for i in ids]

    def stats(runs):
        return {
            "hits": sum(any(r["optimal"] for r in runs[i]) for i in ids),
            "gap": mean(mean(r["gap"] for r in runs[i]) for i in ids),
            "time": mean(mean(r["time_to_best_ms"] for r in runs[i]) for i in ids) / 1000,
        }

    return {
        "n": len(ids),
        "current": stats(current),
        "tuned": stats(tuned),
        "current_wins": sum(d < 0 for d in diffs),
        "tuned_wins": sum(d > 0 for d in diffs),
        "p": wilcoxon(diffs)[0],
    }


def plot_validation():
    with MANIFEST.open() as handle:
        profile = {int(r["id"]): r["profile"] for r in csv.DictReader(handle)}
    groups = []
    for name, label in [("original", "94 do artigo"), ("hard", "75 difíceis")]:
        current, _ = read_runs(VALIDATION / f"atual_{name}.csv")
        tuned, _ = read_runs(VALIDATION / f"calibrada_{name}.csv")
        ids = sorted(set(current) & set(tuned))
        if name == "hard":
            for p in dict.fromkeys(profile[i] for i in ids):
                groups.append((f"  {p}", group_stats(
                    current, tuned, [i for i in ids if profile[i] == p]), False))
        groups.append((label, group_stats(current, tuned, ids), True))

    columns = ["Conjunto", "Inst.", "Ótimo\natual", "Ótimo\ncalibrada", "GAP\natual",
               "GAP\ncalibrada", "Tempo\natual", "Tempo\ncalibrada",
               "Atual\nmelhor", "Calibrada\nmelhor", "p-valor\n(Wilcoxon)"]
    cells = []
    for label, g, _ in groups:
        c, t = g["current"], g["tuned"]
        cells.append([label, str(g["n"]), str(c["hits"]), str(t["hits"]),
                      f"{br(c['gap'])}%", f"{br(t['gap'])}%",
                      f"{br(c['time'])} s", f"{br(t['time'])} s",
                      str(g["current_wins"]), str(g["tuned_wins"]), fmt_p(g["p"])])

    fig, ax = plt.subplots(figsize=(15, 4.6), facecolor=SURFACE)
    ax.axis("off")
    widths = [0.12] + [0.095] * (len(columns) - 1)
    table = ax.table(cellText=cells, colLabels=columns, cellLoc="center", loc="center",
                     colWidths=widths)
    style_table(table, len(cells), len(columns), 10.5)
    table.scale(1, 2.1)
    for row, (_, g, total) in enumerate(groups, start=1):
        c, t = g["current"], g["tuned"]
        mark_better(table, row, 2, 3, c["hits"], t["hits"], lower_is_better=False)
        mark_better(table, row, 4, 5, round(c["gap"], 2), round(t["gap"], 2))
        mark_better(table, row, 6, 7, round(c["time"], 2), round(t["time"], 2))
        mark_better(table, row, 8, 9, g["current_wins"], g["tuned_wins"],
                    lower_is_better=False)
        table[row, 0].set_text_props(ha="left")
        table[row, 0]._loc = "left"
        if total:
            for col in range(len(columns)):
                table[row, col].set_text_props(weight="bold")

    titles(fig, "GLS calibrada × configuração atual nos conjuntos de teste",
           "3 repetições de 10 s por configuração, parada ao atingir o ótimo conhecido | "
           "nenhuma instância de teste foi usada na calibração")
    footer(fig, "Atual: alpha 0,3, rounds 30, maxpen 0 | Calibrada: alpha 0,997, rounds 79, "
                "maxpen 3 | Tempo: médio até a melhor solução | GAP: médio, contra o ótimo do CPLEX "
                "ou, sem ele, o limite inferior trivial | Verde: melhor valor do par", 0.05)
    fig.savefig(CHARTS / "tabela_validacao.png", dpi=200, bbox_inches="tight",
                facecolor=SURFACE)
    plt.close(fig)


def plot_without_optimum():
    current, _ = read_runs(VALIDATION / "atual_hard.csv")
    tuned, _ = read_runs(VALIDATION / "calibrada_hard.csv")
    rows = []
    for iid in sorted(current):
        cplex = load_cplex(iid)
        if cplex["status"] != "Optimal":
            rows.append((iid, cplex, mean(r["cost"] for r in current[iid]),
                         mean(r["cost"] for r in tuned[iid])))

    def gap(cost, bound):
        return 100 * (cost - bound) / cost

    columns = ["Inst.", "Limite inf.\nCPLEX", "CPLEX\n15 min", "GLS atual\n10 s",
               "GLS calibrada\n10 s", "Gap p/ limite\natual", "Gap p/ limite\ncalibrada"]
    cells = [[str(iid), br(c["bound"], 1), f"{c['cost']:.0f}", f"{a:.0f}", f"{t:.0f}",
              f"{br(gap(a, c['bound']))}%", f"{br(gap(t, c['bound']))}%"]
             for iid, c, a, t in rows]

    fig, ax = plt.subplots(figsize=(11.5, 3.9), facecolor=SURFACE)
    ax.axis("off")
    table = ax.table(cellText=cells, colLabels=columns, cellLoc="center", loc="center")
    style_table(table, len(cells), len(columns), 10.5)
    table.scale(1, 2.0)
    for row, (_, c, a, t) in enumerate(rows, start=1):
        mark_better(table, row, 3, 4, a, t)
        mark_better(table, row, 5, 6, round(gap(a, c["bound"]), 2),
                    round(gap(t, c["bound"]), 2))

    titles(fig, "Instâncias C50 sem ótimo provado pelo CPLEX",
           "custo médio de 3 repetições de 10 s | o ótimo está entre o limite inferior e a "
           "melhor solução", top=1.08, sub=0.98)
    footer(fig, "Gap p/ limite = (custo − limite inferior) / custo: distância máxima "
                "garantida até o ótimo | Verde: melhor valor do par", 0.04)
    fig.savefig(CHARTS / "tabela_sem_otimo.png", dpi=200, bbox_inches="tight",
                facecolor=SURFACE)
    plt.close(fig)


def main():
    CHARTS.mkdir(parents=True, exist_ok=True)
    plot_parameters()
    plot_validation()
    plot_without_optimum()
    for path in sorted(CHARTS.glob("*.png")):
        print(path.relative_to(ROOT))


if __name__ == "__main__":
    main()
