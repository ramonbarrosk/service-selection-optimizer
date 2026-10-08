#!/usr/bin/env python3
from pathlib import Path
import csv
import re
import os
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "experiments/07_protocolo_artigo/charts/article_comparison"
OUT.mkdir(parents=True, exist_ok=True)


def read_java():
    pattern = re.compile(r"^Instance_10_10_(\d+)\s+(\d+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)")
    rows = {}
    for line in (ROOT / "experiments/00_baseline_java/results/java_results_raw.txt").read_text().splitlines():
        m = pattern.match(line.strip())
        if m:
            iid, opt, exact, mean, best, time = m.groups()
            rows[int(iid)] = dict(opt=int(opt), exact=float(exact), java_mean=float(mean),
                                  java_best=float(best), java_time=float(time))
    return rows


def read_cpp():
    rows = []
    cpp_path = ROOT / os.environ.get(
        "ARTICLE_CPP_CSV", "experiments/06_gls_melhorias/results/gls_promoted_all94.csv")
    with cpp_path.open() as f:
        for row in csv.DictReader(f):
            rows.append(row)
    grouped = {}
    for row in rows:
        iid = int(row["instance"])
        grouped.setdefault(iid, []).append(row)
    result = {}
    for iid, values in grouped.items():
        optimum = float(values[0]["optimum"])
        costs = [float(v["cost"]) for v in values]
        gaps = [float(v["gap_pct"]) for v in values]
        times = [float(v["time_ms"]) / 1000 for v in values]
        result[iid] = dict(cpp_mean=sum(costs) / 3, cpp_best=min(costs),
                            cpp_gap=sum(gaps) / 3, cpp_time=sum(times) / 3,
                            opt=int(optimum))
    return result


java, cpp = read_java(), read_cpp()
ids = sorted(set(java) & set(cpp))
cells = []
for iid in ids:
    j, c = java[iid], cpp[iid]
    cells.append([
        str(iid), str(j["opt"]), f"{j['java_mean']:.1f}", f"{j['java_best']:.0f}",
        f"{j['java_time']:.2f}", f"{c['cpp_mean']:.1f}", f"{c['cpp_best']:.0f}",
        f"{c['cpp_gap']:.1f}%", f"{c['cpp_time']:.2f}",
    ])

fig, ax = plt.subplots(figsize=(18, 34))
fig.patch.set_facecolor("#f8fafc")
ax.axis("off")
table = ax.table(
    cellText=cells,
    colLabels=["Inst.", "Ótimo", "Java médio", "Java melhor", "Java t(s)",
               "C++ GLS médio", "C++ melhor", "C++ GAP", "C++ t(s)"],
    cellLoc="center", colLoc="center", loc="center",
)
table.auto_set_font_size(False)
table.set_fontsize(8.5)
table.scale(1, 1.42)
for col in range(9):
    table[0, col].set_facecolor("#172554")
    table[0, col].set_text_props(color="white", weight="bold")
for row in range(1, len(cells) + 1):
    for col in range(9):
        table[row, col].set_facecolor("#ffffff" if row % 2 else "#eaf2ff")
    optimum = float(cells[row - 1][1])
    java_mean_gap = 100 * (float(cells[row - 1][2]) - optimum) / optimum
    java_best_gap = 100 * (float(cells[row - 1][3]) - optimum) / optimum
    if java_mean_gap <= 1e-9:
        table[row, 2].set_facecolor("#bbf7d0")
    elif java_mean_gap <= 5:
        table[row, 2].set_facecolor("#fef3c7")
    else:
        table[row, 2].set_facecolor("#fecaca")
    if java_best_gap <= 1e-9:
        table[row, 3].set_facecolor("#bbf7d0")
    elif java_best_gap <= 5:
        table[row, 3].set_facecolor("#fef3c7")
    else:
        table[row, 3].set_facecolor("#fecaca")
    if float(cells[row - 1][7].strip("%")) <= 0:
        table[row, 7].set_facecolor("#bbf7d0")
    elif float(cells[row - 1][7].strip("%")) > 5:
        table[row, 7].set_facecolor("#fecaca")
fig.suptitle(
    os.environ.get(
        "ARTICLE_TITLE",
        "Comparação por instância — SSUU Java × C++ GLS promovida (0,55 s, rerun)"),
    fontsize=18, weight="bold", y=0.995,
)
fig.text(
    0.5, 0.985,
    os.environ.get(
        "ARTICLE_SUBTITLE",
        "C++: I1 S1 L0 R1 | 94 instâncias | 3 repetições | execução atual | ótimo conhecido"),
    ha="center", va="top", fontsize=11, color="#334155",
)
fig.tight_layout(rect=[0.01, 0.01, 0.99, 0.975])
filename = os.environ.get(
    "ARTICLE_OUTPUT", "tabela_comparativa_94_instancias_rerun.png")
fig.savefig(OUT / filename, dpi=180,
            bbox_inches="tight", facecolor=fig.get_facecolor())
print(OUT / filename)
