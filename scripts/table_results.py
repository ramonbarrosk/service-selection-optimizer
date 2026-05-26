#!/usr/bin/env python3
"""
Gera tabelas comparativas por instância: CPLEX vs Java ILS vs C++ ILS.
Salva duas figuras separadas em data/charts/:
  - table_java.png  : instância | ótimo | Java best | GAP%
  - table_cpp.png   : instância | ótimo | C++ best  | GAP%

Uso (a partir da raiz do projeto):
  python scripts/table_results.py
"""

import re
import sys
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from pathlib import Path

CPP_FILE  = "data/cpp_results.txt"
JAVA_FILE = "data/java_results_raw.txt"
OUT_DIR   = Path("data/charts")

# ─── Parsers ──────────────────────────────────────────────────────────────────

def parse_results(filepath):
    pat = re.compile(
        r"^(Instance_10_10_(\d+))\s+([\d.]+)\s+([\d.eE+\-]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)"
    )
    rows = []
    with open(filepath) as f:
        for line in f:
            m = pat.match(line.strip())
            if m:
                rows.append({
                    "name":       m.group(1),
                    "num":        int(m.group(2)),
                    "cplex_cost": float(m.group(3)),
                    "mean":       float(m.group(5)),
                    "best":       float(m.group(6)),
                })
    return sorted(rows, key=lambda r: r["num"])


def gap_pct(cost, optimal):
    return (cost - optimal) / optimal * 100.0 if optimal > 0 else 0.0


# ─── Desenha uma tabela de 47 linhas por coluna ───────────────────────────────

def draw_table(ax, rows, title, color_algo):
    col_labels = ["Instância", "CPLEX Ótimo", "Melhor", "GAP%"]
    cell_text  = []
    cell_colors = []

    for r in rows:
        gap = gap_pct(r["best"], r["cplex_cost"])
        if gap <= 5:
            gap_color = "#d5f5e3"   # verde — muito próximo
        elif gap <= 20:
            gap_color = "#fef9e7"   # amarelo — razoável
        else:
            gap_color = "#fdecea"   # vermelho — longe

        cell_text.append([
            r["name"].replace("Instance_10_10_", "#"),
            f"{int(r['cplex_cost'])}",
            f"{r['best']:.0f}",
            f"{gap:.1f}%",
        ])
        cell_colors.append(["white", "#eaf4fb", color_algo, gap_color])

    tbl = ax.table(
        cellText=cell_text,
        colLabels=col_labels,
        cellColours=cell_colors,
        loc="center",
        cellLoc="center",
    )
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(7.5)
    tbl.scale(1, 1.38)

    for j in range(len(col_labels)):
        cell = tbl[0, j]
        cell.set_facecolor("#2c3e50")
        cell.set_text_props(color="white", fontweight="bold")

    ax.axis("off")
    ax.set_title(title, fontweight="bold", fontsize=11, pad=10)


# ─── Figura principal ─────────────────────────────────────────────────────────

def make_table_figure(rows, algo_label, color_algo, filename):
    half = (len(rows) + 1) // 2
    left_rows  = rows[:half]
    right_rows = rows[half:]

    fig, axes = plt.subplots(1, 2, figsize=(16, 18))
    fig.suptitle(
        f"Resultados por Instância — {algo_label}  (melhor de 3 execuções)",
        fontweight="bold", fontsize=13, y=1.005,
    )

    n  = len(rows)
    mean_gap = np.mean([gap_pct(r["best"], r["cplex_cost"]) for r in rows])
    best_gap = min(gap_pct(r["best"], r["cplex_cost"]) for r in rows)
    reached  = sum(1 for r in rows if gap_pct(r["best"], r["cplex_cost"]) <= 0.0)

    draw_table(axes[0], left_rows,
               f"Instâncias 1–{half}  |  GAP médio: {mean_gap:.1f}%  |  Melhor GAP: {best_gap:.1f}%",
               color_algo)
    draw_table(axes[1], right_rows,
               f"Instâncias {half+1}–{n}  |  Chegou ao ótimo (GAP=0): {reached}/{n}",
               color_algo)

    legend_patches = [
        mpatches.Patch(color="#d5f5e3", label="GAP ≤ 5%"),
        mpatches.Patch(color="#fef9e7", label="GAP ≤ 20%"),
        mpatches.Patch(color="#fdecea", label="GAP > 20%"),
    ]
    fig.legend(handles=legend_patches, loc="lower center", ncol=3,
               fontsize=9, frameon=True, bbox_to_anchor=(0.5, -0.01))

    fig.tight_layout()
    out = OUT_DIR / filename
    fig.savefig(out, bbox_inches="tight", dpi=130)
    plt.close(fig)
    print(f"  Salvo: {out}")


# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    for path in [CPP_FILE, JAVA_FILE]:
        if not Path(path).exists():
            print(f"[ERRO] Arquivo não encontrado: {path}")
            sys.exit(1)

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    cpp_rows  = parse_results(CPP_FILE)
    java_rows = parse_results(JAVA_FILE)

    # Alinha pelo nome
    java_map = {r["name"]: r for r in java_rows}
    cpp_map  = {r["name"]: r for r in cpp_rows}

    # Usa instâncias comuns a ambos
    common_names = sorted(
        set(java_map) & set(cpp_map),
        key=lambda n: int(n.split("_")[-1])
    )

    java_common = [java_map[n] for n in common_names]
    cpp_common  = [cpp_map[n]  for n in common_names]

    print(f"Instâncias em comum: {len(common_names)}")

    print("\nGerando tabela Java...")
    make_table_figure(java_common, "Java ILS", "#fde8d0", "table_java.png")

    print("Gerando tabela C++...")
    make_table_figure(cpp_common,  "C++ ILS",  "#d6eaf8", "table_cpp.png")

    print("\nPronto! Abra data/charts/table_java.png e data/charts/table_cpp.png")


if __name__ == "__main__":
    main()
