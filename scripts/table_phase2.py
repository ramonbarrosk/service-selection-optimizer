#!/usr/bin/env python3
"""
Tabela completa colorida (todas as instancias) da abordagem Fase 2
(best-fit + oscilacao estrategica + ILS), reusando o estilo de table_results.py.

Cores por GAP%: verde (<=5%), amarelo (<=20%), vermelho (>20%).

Uso (da raiz do projeto):
  python scripts/table_phase2.py
  python scripts/table_phase2.py --cpp-results data/phase2_cpp_results.txt --output-dir data/charts_phase2
"""

import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from pathlib import Path

from table_results import parse_results, gap_pct, draw_table


def make_full_table(rows, algo_label, color_algo, out_path, runs_note):
    half = (len(rows) + 1) // 2
    left_rows, right_rows = rows[:half], rows[half:]

    fig, axes = plt.subplots(1, 2, figsize=(16, 18))
    fig.suptitle(
        f"Resultados por Instancia — {algo_label}  ({runs_note})",
        fontweight="bold", fontsize=13, y=1.005,
    )

    n = len(rows)
    mean_gap = np.mean([gap_pct(r["best"], r["cplex_cost"]) for r in rows])
    best_gap = min(gap_pct(r["best"], r["cplex_cost"]) for r in rows)
    reached  = sum(1 for r in rows if gap_pct(r["best"], r["cplex_cost"]) <= 0.0)

    draw_table(axes[0], left_rows,
               f"Instancias 1-{half}  |  GAP medio: {mean_gap:.1f}%  |  Melhor GAP: {best_gap:.1f}%",
               color_algo)
    draw_table(axes[1], right_rows,
               f"Instancias {half+1}-{n}  |  Chegou ao otimo (GAP=0): {reached}/{n}",
               color_algo)

    legend_patches = [
        mpatches.Patch(color="#d5f5e3", label="GAP <= 5%"),
        mpatches.Patch(color="#fef9e7", label="GAP <= 20%"),
        mpatches.Patch(color="#fdecea", label="GAP > 20%"),
    ]
    fig.legend(handles=legend_patches, loc="lower center", ncol=3,
               fontsize=9, frameon=True, bbox_to_anchor=(0.5, -0.01))

    fig.tight_layout()
    fig.savefig(out_path, bbox_inches="tight", dpi=130)
    plt.close(fig)
    print(f"  Salvo: {out_path}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cpp-results", default="data/phase2_cpp_results.txt")
    ap.add_argument("--output-dir", default="data/charts_phase2")
    ap.add_argument("--runs-note", default="best-fit + oscilacao + ILS")
    args = ap.parse_args()

    rows = parse_results(args.cpp_results)
    if not rows:
        print(f"[ERRO] Nenhuma linha valida em {args.cpp_results}")
        return
    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"{len(rows)} instancias carregadas de {args.cpp_results}")
    make_full_table(rows, "C++ ILS (Fase 2)", "#d6eaf8",
                    out_dir / "table_phase2.png", args.runs_note)
    print(f"\nPronto! Abra {out_dir}/table_phase2.png")


if __name__ == "__main__":
    main()
