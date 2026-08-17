#!/usr/bin/env python3
"""Tabela visual do A/B pareado do path relinking."""

from pathlib import Path
import matplotlib.pyplot as plt

FILES = [
    ("GLS estrutural", Path("data/comparison_gls_structural_seed20260813_3min.txt"), "3:10,03"),
    ("GLS + path relinking", Path("data/comparison_gls_path_gap5_seed20260813_3min.txt"), "3:12,02"),
]
HARD5 = [100, 128, 129, 147, 28]
OUTPUT = Path("data/charts_gls_reform/gls_path_relinking_paired.png")

def load(path):
    rows = {}
    for line in path.read_text().splitlines():
        f = line.split()
        if len(f) >= 5 and f[0].startswith("Instance_10_10_"):
            rows[int(f[0].rsplit("_", 1)[1])] = tuple(map(float, (f[1], f[3], f[4])))
    return rows

def gap(cost, optimum):
    return 100 * (cost - optimum) / optimum

def main():
    configs = [(name, load(path), elapsed) for name, path, elapsed in FILES]
    summary = []
    for name, data, elapsed in configs:
        optimums = sum(best == opt for opt, _, best in data.values())
        best_gap = sum(gap(best, opt) for opt, _, best in data.values()) / len(data)
        mean_gap = sum(gap(mean, opt) for opt, mean, _ in data.values()) / len(data)
        summary.append([name, optimums, f"{best_gap:.3f}%", f"{mean_gap:.3f}%", elapsed])

    hard = []
    for instance in HARD5:
        opt = configs[0][1][instance][0]
        row = [instance, f"{opt:.0f}"]
        for _, data, _ in configs:
            best = data[instance][2]
            row.extend([f"{best:.0f}", f"{gap(best, opt):.1f}%"])
        hard.append(row)

    fig, axes = plt.subplots(2, 1, figsize=(12, 8.5), gridspec_kw={"hspace": .5})
    for ax in axes: ax.axis("off")
    tables = [
        axes[0].table(cellText=summary, colLabels=["Configuração", "Ótimos / 94", "GAP melhor", "GAP médio", "Tempo"], cellLoc="center", loc="center"),
        axes[1].table(cellText=hard, colLabels=["Inst.", "Ótimo", "GLS melhor", "GAP", "Path melhor", "GAP"], cellLoc="center", loc="center"),
    ]
    for table in tables:
        table.auto_set_font_size(False); table.set_fontsize(10); table.scale(1, 1.7)
        for (row, _), cell in table.get_celld().items():
            cell.set_edgecolor("#c7d1d9")
            if row == 0:
                cell.set_facecolor("#263746"); cell.set_text_props(color="white", fontweight="bold")
    fig.suptitle("PATH RELINKING — A/B PAREADO NAS 94 INSTÂNCIAS\n3 repetições, seed=20260813, orçamento interno de 3 minutos", fontsize=14, fontweight="bold")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=170, bbox_inches="tight")
    plt.close(fig)
    print(f"Salvo: {OUTPUT}")

if __name__ == "__main__":
    main()
