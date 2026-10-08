#!/usr/bin/env python3
"""Evidencia de que a OSCILACAO melhora — e melhora cruzando a inviabilidade.
Le /tmp/osc_data.json (gerado da instrumentacao OSC_CALL/OSC_IMPROVE)."""
import json
import numpy as np
import matplotlib.pyplot as plt

data = json.load(open("/tmp/osc_data.json"))
insts = sorted(data, key=int)
calls   = [data[i]["calls"] for i in insts]
imps    = [data[i]["imps"] for i in insts]
totals  = [data[i]["total"] for i in insts]
avgs    = [data[i]["avg"] for i in insts]
rate    = [100 * data[i]["imps"] / data[i]["calls"] for i in insts]
all_deltas = [d for i in insts for d in data[i]["deltas"]]

tot_calls = sum(calls); tot_imps = sum(imps)
tot_cross = sum(data[i]["crossed"] for i in insts)
pct_cross = 100 * tot_cross / tot_imps

BLUE, GREEN = "#2e6da4", "#27ae60"

fig, axes = plt.subplots(1, 2, figsize=(15, 6))
fig.suptitle(
    "Evidencia: a oscilacao estrategica MELHORA a solucao — e 100% das melhorias "
    "vem de cruzar a inviabilidade\n"
    f"5 instancias dificeis | {tot_calls:,} chamadas | {tot_imps:,} melhorias "
    f"({100*tot_imps/tot_calls:.0f}%) | {pct_cross:.0f}% cruzaram a barreira de capacidade"
    .replace(",", "."),
    fontweight="bold", fontsize=12.5, y=1.02,
)

# --- Painel 1: por instancia (nº de melhorias + reducao total) ---
ax = axes[0]
x = np.arange(len(insts))
bars = ax.bar(x, imps, color=BLUE, width=0.6, zorder=3)
ax.set_xticks(x); ax.set_xticklabels([f"#{i}" for i in insts], fontsize=11)
ax.set_ylabel("Nº de melhorias da oscilacao", fontsize=11)
ax.set_title("Quantas vezes a oscilacao melhorou a solucao\n(rotulo = reducao total de custo)",
             fontsize=11, fontweight="bold")
ax.grid(axis="y", alpha=0.3, zorder=0)
for xi, b, tot, r in zip(x, bars, totals, rate):
    ax.text(xi, b.get_height() + max(imps)*0.01,
            f"-{tot:,.0f}\n({r:.0f}%)".replace(",", "."),
            ha="center", va="bottom", fontsize=9, fontweight="bold", color="#1a3a52")
ax.margins(y=0.18)

# --- Painel 2: distribuicao das magnitudes de melhoria ---
ax = axes[1]
arr = np.array(all_deltas)
ax.hist(arr, bins=range(0, int(arr.max()) + 2, 1), color=GREEN, alpha=0.85, zorder=3)
ax.axvline(arr.mean(), color="#c0392b", lw=2, ls="--", zorder=4,
           label=f"media = {arr.mean():.1f} de custo por melhoria")
ax.set_xlabel("Reducao de custo em cada melhoria (custo antes - depois)", fontsize=11)
ax.set_ylabel("Frequencia", fontsize=11)
ax.set_title("Magnitude das melhorias da oscilacao\n(cada uma obtida cruzando a inviabilidade)",
             fontsize=11, fontweight="bold")
ax.grid(axis="y", alpha=0.3, zorder=0)
ax.legend(fontsize=10)
ax.set_xlim(0, np.percentile(arr, 99.5))

fig.tight_layout()
from pathlib import Path
out = Path("data/charts_osc_evidence"); out.mkdir(parents=True, exist_ok=True)
p = out / "osc_evidence.png"
fig.savefig(p, bbox_inches="tight", dpi=140)
print(f"Salvo: {p}")
