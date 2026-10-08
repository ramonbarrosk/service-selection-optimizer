#!/usr/bin/env python3
"""Tabela comparativa: Java ILS#1 x Java ILS#3 x C++ GLS promovida, orçamento fixo por execução.

    python3 scripts/plot_ils3_comparison.py                      # 10 s
    python3 scripts/plot_ils3_comparison.py --budget 0.55 \\
        --ils1 data/experiments/java_ils1_0.55s_raw.txt \\
        --ils3 data/experiments/java_ils3_0.55s_raw.txt \\
        --gls data/experiments/ablation_0.55s/i1_s1_l0_r1.csv
"""
import argparse
from pathlib import Path
import csv
import re
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "data/charts_article_comparison"
OUT.mkdir(parents=True, exist_ok=True)

ROW_RE = re.compile(
    r"^Instance_10_10_(\d+)\s+(\d+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)")


def read_java(path):
    rows = {}
    for line in path.read_text().splitlines():
        m = ROW_RE.match(line.strip())
        if m:
            iid, opt, exact, mean, best, t = m.groups()
            rows[int(iid)] = dict(opt=int(opt), mean=float(mean), best=float(best), t=float(t))
    return rows


def read_cpp(path):
    rows = []
    with path.open() as f:
        for row in csv.DictReader(f):
            rows.append(row)
    grouped = {}
    for row in rows:
        grouped.setdefault(int(row["instance"]), []).append(row)
    result = {}
    for iid, values in grouped.items():
        optimum = float(values[0]["optimum"])
        costs = [float(v["cost"]) for v in values]
        times = [float(v["time_ms"]) / 1000 for v in values]
        result[iid] = dict(opt=int(optimum), mean=sum(costs) / len(costs),
                            best=min(costs), t=sum(times) / len(times))
    return result


parser = argparse.ArgumentParser()
parser.add_argument("--budget", default="10")
parser.add_argument("--ils1", default="data/java_results_raw.txt")
parser.add_argument("--ils3", default="data/experiments/java_ils3_10s_raw.txt")
parser.add_argument("--gls", default="data/experiments/gls_promoted_article_protocol.csv")
args = parser.parse_args()
budget_label = args.budget.replace(".", ",") + "s"

ils1 = read_java(ROOT / args.ils1)
ils3 = read_java(ROOT / args.ils3)
gls = read_cpp(ROOT / args.gls)

ids = sorted(set(ils1) & set(ils3) & set(gls))


def gap(mean, opt):
    return 100.0 * (mean - opt) / opt


def summary(rows):
    n = len(ids)
    mean_cost = sum(rows[i]["mean"] for i in ids) / n
    mean_gap = sum(gap(rows[i]["mean"], rows[i]["opt"]) for i in ids) / n
    reached = sum(1 for i in ids if rows[i]["best"] <= rows[i]["opt"])
    mean_t = sum(rows[i]["t"] for i in ids) / n
    return mean_cost, mean_gap, reached, mean_t

for label, rows in (("Java ILS#1 (First Improvement, SWAP, a=0.4)", ils1),
                     ("Java ILS#3 (Best Improvement, MOVE, a=0.2)", ils3),
                     ("C++ GLS promovida (I1 S1 L0 R1)", gls)):
    mc, mg, r, mt = summary(rows)
    print(f"{label}: custo medio={mc:.2f} gap={mg:.2f}% otimo={r}/{len(ids)} t={mt:.2f}s")

cells = []
for iid in ids:
    j1, j3, c = ils1[iid], ils3[iid], gls[iid]
    cells.append([
        str(iid), str(j1["opt"]),
        f"{j1['mean']:.1f}", f"{gap(j1['mean'], j1['opt']):.1f}%",
        f"{j3['mean']:.1f}", f"{gap(j3['mean'], j3['opt']):.1f}%",
        f"{c['mean']:.1f}", f"{gap(c['mean'], c['opt']):.1f}%", f"{c['t']:.3f}",
    ])

fig, ax = plt.subplots(figsize=(16, 34))
fig.patch.set_facecolor("#f8fafc")
ax.axis("off")
table = ax.table(
    cellText=cells,
    colLabels=["Inst.", "Ótimo", "ILS#1 médio", "ILS#1 GAP", "ILS#3 médio",
               "ILS#3 GAP", "GLS médio", "GLS GAP", "GLS t(s)"],
    cellLoc="center", colLoc="center", loc="center",
)
table.auto_set_font_size(False)
table.set_fontsize(8.5)
table.scale(1, 1.42)
for col in range(9):
    table[0, col].set_facecolor("#172554")
    table[0, col].set_text_props(color="white", weight="bold")


def gap_color(value):
    if value <= 1e-9:
        return "#bbf7d0"
    elif value <= 5:
        return "#fef3c7"
    return "#fecaca"


for row in range(1, len(cells) + 1):
    for col in range(9):
        table[row, col].set_facecolor("#ffffff" if row % 2 else "#eaf2ff")
    table[row, 3].set_facecolor(gap_color(float(cells[row - 1][3].strip("%"))))
    table[row, 5].set_facecolor(gap_color(float(cells[row - 1][5].strip("%"))))
    table[row, 7].set_facecolor(gap_color(float(cells[row - 1][7].strip("%"))))

fig.suptitle(
    "Comparação por instância — Java ILS#1 × Java ILS#3 × C++ GLS promovida",
    fontsize=18, weight="bold", y=0.995,
)
fig.text(
    0.5, 0.985,
    f"Orçamento fixo de {budget_label} por execução | 94 instâncias | 3 repetições | ótimo conhecido",
    ha="center", va="top", fontsize=11, color="#334155",
)
fig.tight_layout(rect=[0.01, 0.01, 0.99, 0.975])
filename = f"tabela_comparativa_ils1_ils3_gls_{args.budget}s.png"
fig.savefig(OUT / filename, dpi=180, bbox_inches="tight", facecolor=fig.get_facecolor())
print(OUT / filename)
