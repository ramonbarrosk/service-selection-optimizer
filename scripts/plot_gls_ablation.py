#!/usr/bin/env python3
"""Ablação das melhorias da GLS: 94 instâncias, 3 repetições, orçamento fixo por execução.

    python3 scripts/plot_gls_ablation.py 0.55
    python3 scripts/plot_gls_ablation.py 10
"""

import csv
import os
import sys
from collections import defaultdict
from itertools import combinations
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
BUDGET = 10.0
BUDGET_LABEL = "10 s"
DATA = ROOT / "data/experiments/ablation_10s"
OUT = ROOT / "data/charts_gls_ablation_10s"
SUMMARY = DATA / "summary.md"


def configure(budget_arg):
    global BUDGET, BUDGET_LABEL, DATA, OUT, SUMMARY
    BUDGET = float(budget_arg)
    BUDGET_LABEL = budget_arg.replace(".", ",") + " s"
    DATA = ROOT / f"data/experiments/ablation_{budget_arg}s"
    OUT = ROOT / f"data/charts_gls_ablation_{budget_arg}s"
    SUMMARY = DATA / "summary.md"

FACTORS = [
    ("incremental_enabled", "Avaliação incremental", "I"),
    ("service_replace_enabled", "Substituição de serviço", "S"),
    ("lambda_local_enabled", "Lambda local", "L"),
    ("regret_enabled", "Utilidade por arrependimento", "R"),
]
PROMOTED = "i1_s1_l0_r1"
PATH = [
    ("gls", "GLS pura"),
    ("i0_s0_l0_r0", "+ GFLS"),
    ("i1_s0_l0_r0", "+ Avaliação\nincremental"),
    ("i1_s1_l0_r0", "+ Substituição\nde serviço"),
    ("i1_s1_l0_r1", "+ Arrependimento\n(promovida)"),
]
LEAVE_ONE_OUT = [
    ("i0_s1_l0_r1", "Sem avaliação incremental"),
    ("i1_s0_l0_r1", "Sem substituição de serviço"),
    ("i1_s1_l0_r0", "Sem utilidade por arrependimento"),
    ("i1_s1_l1_r1", "Com lambda local (rejeitado)"),
]

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_2 = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"
BLUE = "#2a78d6"
RED = "#e34948"
GOOD, WARNING, CRITICAL = "#0ca30c", "#fab219", "#d03b3b"


def load():
    rows = []
    for path in sorted(DATA.glob("*.csv")):
        with path.open(newline="", encoding="utf-8") as source:
            rows.extend(csv.DictReader(source))
    for row in rows:
        for field in ("cost", "gap_pct", "time_ms", "optimum"):
            row[field] = float(row[field])
        row["instance"] = int(row["instance"])
        for field, _, _ in FACTORS:
            row[field] = int(row[field])
    return rows


def stats(rows):
    by_variant = defaultdict(list)
    for row in rows:
        by_variant[row["variant"]].append(row)
    result = {}
    for variant, values in by_variant.items():
        per_instance = defaultdict(list)
        for row in values:
            per_instance[row["instance"]].append(row)
        n = len(values)
        result[variant] = {
            "label": values[0]["label"],
            "runs": n,
            "instances": len(per_instance),
            "optimal_runs": sum(r["gap_pct"] <= 1e-9 for r in values),
            "optimal_instances": sum(
                any(r["gap_pct"] <= 1e-9 for r in rs) for rs in per_instance.values()),
            "mean_cost": sum(r["cost"] for r in values) / n,
            "mean_gap": sum(r["gap_pct"] for r in values) / n,
            "mean_time": sum(r["time_ms"] for r in values) / n / 1000,
            "instance_gap": {i: sum(r["gap_pct"] for r in rs) / len(rs)
                             for i, rs in per_instance.items()},
            "flags": {f: values[0][f] for f, _, _ in FACTORS},
        }
    return result


def config_name(variant, s):
    if variant == "gls":
        return "GLS pura (sem GFLS)"
    return " ".join(f"{short}{s['flags'][field]}" for field, _, short in FACTORS)


def fmt(value, digits=2):
    return f"{value:.{digits}f}".replace(".", ",")


def style_axis(ax):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)
    ax.tick_params(colors=INK_2, labelsize=9)
    ax.grid(axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def bar_panel(ax, labels, values, title, value_fmt, ylim=None, log=False):
    style_axis(ax)
    xs = range(len(values))
    ax.bar(xs, values, width=0.6, color=BLUE, edgecolor=SURFACE, linewidth=2)
    ax.set_xticks(list(xs))
    ax.set_xticklabels(labels, fontsize=8.5, color=INK_2)
    ax.set_title(title, loc="left", fontsize=11, color=INK, weight="bold")
    if log:
        ax.set_yscale("log")
    if ylim:
        ax.set_ylim(*ylim)
    for x, v in zip(xs, values):
        ax.annotate(value_fmt(v), (x, v), textcoords="offset points", xytext=(0, 4),
                    ha="center", fontsize=9, color=INK)


def plot_path(s):
    present = [(k, l) for k, l in PATH if k in s]
    labels = [l for _, l in present]
    fig, axes = plt.subplots(3, 1, figsize=(11, 12), sharex=True)
    fig.patch.set_facecolor(SURFACE)
    bar_panel(axes[0], labels, [s[k]["optimal_instances"] for k, _ in present],
              "Instâncias com ótimo (de 94)", lambda v: f"{v}", ylim=(0, 100))
    axes[0].axhline(94, color=MUTED, linewidth=1, linestyle="--")
    bar_panel(axes[1], labels, [s[k]["mean_gap"] for k, _ in present],
              "GAP médio (%)", lambda v: fmt(v) + "%",
              ylim=(0, max(s[k]["mean_gap"] for k, _ in present) * 1.18 + 0.1))
    times = [s[k]["mean_time"] for k, _ in present]
    bar_panel(axes[2], labels, times,
              "Tempo médio por execução (s, escala log)", lambda v: fmt(v, 3) + " s",
              ylim=(min(times) / 2, BUDGET * 2), log=True)
    axes[2].axhline(BUDGET, color=MUTED, linewidth=1, linestyle="--")
    axes[2].annotate(f"orçamento {BUDGET_LABEL}", (len(present) - 0.5, BUDGET), textcoords="offset points",
                     xytext=(0, 4), ha="right", fontsize=8.5, color=MUTED)
    axes[2].tick_params(axis="x", labelsize=10)
    fig.suptitle(f"Construção acumulada até a GLS promovida — 94 instâncias, 3 repetições, {BUDGET_LABEL}",
                 x=0.01, ha="left", fontsize=14, weight="bold", color=INK)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    fig.savefig(OUT / "construcao_acumulada.png", dpi=170, facecolor=SURFACE)
    plt.close(fig)


def plot_leave_one_out(s):
    base = s[PROMOTED]
    present = [(k, l) for k, l in LEAVE_ONE_OUT if k in s]
    labels = [l for _, l in present]
    d_inst = [s[k]["optimal_instances"] - base["optimal_instances"] for k, _ in present]
    d_gap = [s[k]["mean_gap"] - base["mean_gap"] for k, _ in present]
    fig, axes = plt.subplots(1, 2, figsize=(15, 4.6))
    fig.patch.set_facecolor(SURFACE)
    panels = [
        (axes[0], d_inst, "Δ instâncias com ótimo (vs promovida)", lambda v: f"{v:+d}", True),
        (axes[1], d_gap, "Δ GAP médio (pontos percentuais, vs promovida)",
         lambda v: ("+" if v >= 0 else "") + fmt(v) + " pp", False),
    ]
    for ax, values, title, value_fmt, higher_is_better in panels:
        style_axis(ax)
        ax.grid(axis="y", visible=False)
        ax.grid(axis="x", color=GRID, linewidth=0.8)
        ys = list(range(len(values)))[::-1]
        colors = []
        for v in values:
            better = v > 0 if higher_is_better else v < 0
            colors.append(BLUE if better else RED if v != 0 else MUTED)
        ax.barh(ys, values, height=0.55, color=colors, edgecolor=SURFACE, linewidth=2)
        ax.axvline(0, color=AXIS, linewidth=1)
        ax.set_yticks(ys)
        ax.set_yticklabels(labels, fontsize=9.5, color=INK)
        span = max(abs(v) for v in values) or 1
        ax.set_xlim(-span * 1.7, span * 1.7)
        for y, v in zip(ys, values):
            better = v > 0 if higher_is_better else v < 0
            word = "melhora" if better else "piora" if v != 0 else "igual"
            ax.annotate(f"{value_fmt(v)} ({word})", (v, y), textcoords="offset points",
                        xytext=(6 if v >= 0 else -6, 0), va="center",
                        ha="left" if v >= 0 else "right", fontsize=9, color=INK)
        ax.set_title(title, loc="left", fontsize=11, color=INK, weight="bold")
    fig.suptitle("Remover/adicionar uma melhoria na configuração promovida (I1 S1 L0 R1)",
                 x=0.01, ha="left", fontsize=14, weight="bold", color=INK)
    fig.text(0.01, 0.015, "Azul = melhora, vermelho = piora em relação à promovida.",
             fontsize=9, color=INK_2)
    fig.tight_layout(rect=[0, 0.04, 1, 0.92])
    fig.savefig(OUT / "remocao_de_cada_melhoria.png", dpi=170, facecolor=SURFACE)
    plt.close(fig)


def factorial_effects(s):
    factorial = {k: v for k, v in s.items() if k != "gls"}
    main = []
    for field, name, _ in FACTORS:
        on = [v["mean_gap"] for v in factorial.values() if v["flags"][field] == 1]
        off = [v["mean_gap"] for v in factorial.values() if v["flags"][field] == 0]
        main.append((name, sum(off) / len(off), sum(on) / len(on)))
    inter = []
    for (fa, na, _), (fb, nb, _) in combinations(FACTORS, 2):
        cell = defaultdict(list)
        for v in factorial.values():
            cell[(v["flags"][fa], v["flags"][fb])].append(v["mean_gap"])
        m = {key: sum(vals) / len(vals) for key, vals in cell.items()}
        # Sinergia > 0: ligar os dois reduz o GAP mais que a soma dos efeitos isolados.
        synergy = (m[(0, 1)] + m[(1, 0)]) - (m[(1, 1)] + m[(0, 0)])
        inter.append((f"{na} × {nb}", synergy / 2))
    return main, inter


def plot_main_effects(main):
    fig, ax = plt.subplots(figsize=(10, 4.2))
    fig.patch.set_facecolor(SURFACE)
    style_axis(ax)
    ax.grid(axis="y", visible=False)
    ax.grid(axis="x", color=GRID, linewidth=0.8)
    names = [m[0] for m in main]
    effects = [m[1] - m[2] for m in main]
    ys = list(range(len(effects)))[::-1]
    ax.barh(ys, effects, height=0.55, color=[BLUE if e > 0 else RED for e in effects],
            edgecolor=SURFACE, linewidth=2)
    ax.axvline(0, color=AXIS, linewidth=1)
    ax.set_yticks(ys)
    ax.set_yticklabels(names, fontsize=10, color=INK)
    span = max(abs(e) for e in effects) or 1
    ax.set_xlim(-span * 1.4, span * 1.4)
    for y, e in zip(ys, effects):
        word = "reduz GAP" if e > 0 else "aumenta GAP"
        ax.annotate(f"{fmt(e)} pp ({word})", (e, y), textcoords="offset points",
                    xytext=(6 if e >= 0 else -6, 0), va="center",
                    ha="left" if e >= 0 else "right", fontsize=9, color=INK)
    ax.set_title("Efeito principal de cada melhoria no fatorial 2⁴ (redução média do GAP, pp)",
                 loc="left", fontsize=12, color=INK, weight="bold")
    fig.tight_layout()
    fig.savefig(OUT / "efeitos_principais.png", dpi=170, facecolor=SURFACE)
    plt.close(fig)


BLUE_RAMP = ["#ffffff", "#cde2fb", "#b7d3f6", "#9ec5f4", "#86b6ef", "#6da7ec", "#5598e7"]


SR_CELLS = [
    ("i1_s0_l0_r0", "i1_s0_l0_r1"),
    ("i1_s1_l0_r0", "i1_s1_l0_r1"),
]


def plot_sr_interaction(s):
    if not all(k in s for row in SR_CELLS for k in row):
        return
    fig, ax = plt.subplots(figsize=(8.5, 4.2))
    fig.patch.set_facecolor(SURFACE)
    ax.axis("off")
    cells = [[f"{s[k]['optimal_instances']}/94 instâncias\nGAP {fmt(s[k]['mean_gap'])}%"
              for k in row] for row in SR_CELLS]
    table = ax.table(
        cellText=cells,
        rowLabels=["Sem substituição\nde serviço (S0)", "Com substituição\nde serviço (S1)"],
        colLabels=["Sem arrependimento (R0)", "Com arrependimento (R1)"],
        cellLoc="center", rowLoc="center", colLoc="center", bbox=[0.28, 0, 0.72, 1],
    )
    table.auto_set_font_size(False)
    table.set_fontsize(11)
    max_gap = max(s[k]["mean_gap"] for row in SR_CELLS for k in row)
    for (r, c), cell in table.get_celld().items():
        cell.set_edgecolor(GRID)
        if r == 0 or c == -1:
            cell.set_facecolor("#172554")
            cell.set_text_props(color="white", weight="bold")
        else:
            cell.set_facecolor(gap_color(s[SR_CELLS[r - 1][c]]["mean_gap"], max_gap))
    table[2, 1].set_text_props(weight="bold")
    fig.suptitle("Interação substituição de serviço × arrependimento (I1, L0)",
                 x=0.01, ha="left", fontsize=13, weight="bold", color=INK)
    fig.text(0.01, 0.01, "Cada uma isolada não ajuda ou piora; juntas formam a configuração "
             "promovida. Azul mais escuro = GAP maior.", fontsize=9, color=INK_2)
    fig.tight_layout(rect=[0, 0.05, 1, 0.92])
    fig.savefig(OUT / "interacao_substituicao_arrependimento.png", dpi=170, facecolor=SURFACE)
    plt.close(fig)


def gap_color(gap, max_gap):
    if max_gap <= 0:
        return BLUE_RAMP[0]
    step = round(gap / max_gap * (len(BLUE_RAMP) - 1))
    return BLUE_RAMP[step]


def plot_ranking_table(s, ranking):
    cells = []
    for pos, (k, v) in enumerate(ranking, 1):
        cells.append([
            str(pos), config_name(k, v) + ("  ★" if k == PROMOTED else ""),
            f"{v['optimal_instances']}/{v['instances']}",
            f"{v['optimal_runs']}/{v['runs']}",
            fmt(v["mean_cost"]), fmt(v["mean_gap"]) + "%", fmt(v["mean_time"], 3) + " s",
        ])
    fig, ax = plt.subplots(figsize=(13, 0.42 * len(cells) + 1.6))
    fig.patch.set_facecolor(SURFACE)
    ax.axis("off")
    table = ax.table(
        cellText=cells,
        colLabels=["#", "Configuração", "Instâncias no ótimo", "Execuções ótimas",
                   "Custo médio", "GAP médio", "Tempo médio"],
        cellLoc="center", colLoc="center", bbox=[0, 0, 1, 1],
        colWidths=[0.05, 0.25, 0.15, 0.15, 0.12, 0.12, 0.13],
    )
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    for col in range(7):
        table[0, col].set_facecolor("#172554")
        table[0, col].set_text_props(color="white", weight="bold")
    max_gap = max(v["mean_gap"] for _, v in ranking)
    for row, (k, v) in enumerate(ranking, 1):
        for col in range(7):
            table[row, col].set_facecolor("#ffffff" if row % 2 else "#f7f7f5")
            table[row, col].set_edgecolor(GRID)
        table[row, 5].set_facecolor(gap_color(v["mean_gap"], max_gap))
        if k == PROMOTED:
            for col in range(7):
                table[row, col].set_text_props(weight="bold")
    fig.suptitle("Ablação GLS — 17 configurações ordenadas por GAP médio",
                 fontsize=14, weight="bold", color=INK, y=0.985)
    fig.text(0.5, 0.945, f"94 instâncias × 3 repetições, {BUDGET_LABEL} fixos por execução | "
             "I = avaliação incremental, S = substituição de serviço, L = lambda local, "
             "R = utilidade por arrependimento (todas com GFLS) | ★ = promovida | "
             "azul mais escuro = GAP maior",
             ha="center", fontsize=8.5, color=INK_2)
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    fig.savefig(OUT / "ranking_configuracoes.png", dpi=170, facecolor=SURFACE,
                bbox_inches="tight")
    plt.close(fig)


def paired_counts(s, variant):
    base = s[PROMOTED]["instance_gap"]
    other = s[variant]["instance_gap"]
    worse = sum(other[i] > base[i] + 1e-9 for i in base)
    better = sum(other[i] < base[i] - 1e-9 for i in base)
    return worse, better, len(base) - worse - better


def write_summary(s, ranking, main, inter):
    lines = [
        f"# Ablação das melhorias da GLS — {BUDGET_LABEL} fixos por execução",
        "",
        "- Instâncias: 94; repetições: 3 (mesma solução inicial por instância, sementes distintas).",
        f"- Orçamento: {BUDGET_LABEL} fixos por execução; encerra antes se atinge o ótimo conhecido.",
        "- Fatores (todos sobre GFLS): I = avaliação incremental, S = substituição composta de serviço, "
        "L = lambda local, R = utilidade por arrependimento. Baseline extra: GLS pura (sem GFLS).",
        f"- Tempo médio = tempo até parar (ótimo encontrado ou {BUDGET_LABEL} esgotados), média das 282 execuções.",
        "",
        "## Construção acumulada até a configuração promovida",
        "",
        "| Etapa | Configuração | Instâncias no ótimo | Execuções ótimas | GAP médio | Tempo médio |",
        "|---|---|---:|---:|---:|---:|",
    ]
    for k, label in PATH:
        if k in s:
            v = s[k]
            lines.append(f"| {label.replace(chr(10), ' ')} | {config_name(k, v)} | "
                         f"{v['optimal_instances']}/94 | {v['optimal_runs']}/{v['runs']} | "
                         f"{fmt(v['mean_gap'])}% | {fmt(v['mean_time'], 3)} s |")
    lines += [
        "",
        "## Remoção de cada melhoria a partir da promovida (I1 S1 L0 R1)",
        "",
        "| Variação | Instâncias no ótimo | Δ vs promovida | GAP médio | Δ GAP (pp) | Tempo médio | "
        "Instâncias piores / melhores / iguais |",
        "|---|---:|---:|---:|---:|---:|---|",
    ]
    base = s[PROMOTED]
    for k, label in LEAVE_ONE_OUT:
        if k in s:
            v = s[k]
            w, b, e = paired_counts(s, k)
            lines.append(
                f"| {label} ({config_name(k, v)}) | {v['optimal_instances']}/94 | "
                f"{v['optimal_instances'] - base['optimal_instances']:+d} | {fmt(v['mean_gap'])}% | "
                f"{v['mean_gap'] - base['mean_gap']:+.2f} | {fmt(v['mean_time'], 3)} s | {w} / {b} / {e} |")
    if all(k in s for row in SR_CELLS for k in row):
        lines += [
            "",
            "## Interação substituição de serviço × arrependimento (I1, L0)",
            "",
            "| | Sem arrependimento (R0) | Com arrependimento (R1) |",
            "|---|---:|---:|",
        ]
        for name, row in zip(("Sem substituição (S0)", "Com substituição (S1)"), SR_CELLS):
            lines.append(f"| {name} | " + " | ".join(
                f"{s[k]['optimal_instances']}/94, GAP {fmt(s[k]['mean_gap'])}%" for k in row) + " |")
    lines += [
        "",
        "## Ranking das 17 configurações",
        "",
        "| # | Configuração | Instâncias no ótimo | Execuções ótimas | Custo médio | GAP médio | Tempo médio |",
        "|---:|---|---:|---:|---:|---:|---:|",
    ]
    for pos, (k, v) in enumerate(ranking, 1):
        star = " ★" if k == PROMOTED else ""
        lines.append(f"| {pos} | {config_name(k, v)}{star} | {v['optimal_instances']}/94 | "
                     f"{v['optimal_runs']}/{v['runs']} | {fmt(v['mean_cost'])} | "
                     f"{fmt(v['mean_gap'])}% | {fmt(v['mean_time'], 3)} s |")
    lines += [
        "",
        "## Efeitos principais (fatorial 2⁴, média do GAP)",
        "",
        "| Fator | GAP desligado | GAP ligado | Redução (pp) |",
        "|---|---:|---:|---:|",
    ]
    for name, off, on in main:
        lines.append(f"| {name} | {fmt(off)}% | {fmt(on)}% | {off - on:+.2f} |")
    lines += [
        "",
        "## Interações pareadas (sinergia, pp de GAP)",
        "",
        "Positivo = ligar os dois juntos reduz o GAP mais do que a soma dos efeitos isolados.",
        "",
        "| Par | Sinergia |",
        "|---|---:|",
    ]
    for name, syn in inter:
        lines.append(f"| {name} | {syn:+.2f} |")
    SUMMARY.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    if len(sys.argv) > 1:
        configure(sys.argv[1])
    OUT.mkdir(parents=True, exist_ok=True)
    s = stats(load())
    missing = [k for k, _ in PATH + LEAVE_ONE_OUT if k not in s]
    if missing:
        print("Aviso: variantes ausentes:", ", ".join(missing))
    ranking = sorted(s.items(), key=lambda kv: (kv[1]["mean_gap"], kv[1]["mean_time"]))
    main_eff, inter = factorial_effects(s) if len(s) >= 17 else ([], [])
    plot_path(s)
    plot_leave_one_out(s)
    if main_eff:
        plot_main_effects(main_eff)
    plot_ranking_table(s, ranking)
    plot_sr_interaction(s)
    write_summary(s, ranking, main_eff, inter)
    for k, v in ranking:
        print(f"{config_name(k, v):22s} otimo={v['optimal_instances']:2d}/94 "
              f"runs={v['optimal_runs']:3d}/{v['runs']} gap={v['mean_gap']:6.2f}% "
              f"t={v['mean_time']:5.2f}s")
    print(f"Resumo: {SUMMARY}")
    print(f"Gráficos: {OUT}")


if __name__ == "__main__":
    main()
