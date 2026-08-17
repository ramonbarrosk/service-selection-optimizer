#!/usr/bin/env python3
"""Comparação geral e hard5 do path relinking sem deadline."""

from pathlib import Path
import matplotlib.pyplot as plt

CONFIGS = [
    ("Java", Path("data/comparison_java_normal.txt"), "3:01,18", "—"),
    ("Best-fit + oscilação\nsem GLS", Path("data/comparison_bestfit_normal_no_gls.txt"), "15:21,67", "14:35,39"),
    ("GLS anterior\nsem deadline", Path("data/gls_all94_stage_trace_no_deadline.txt"), "12:49,88", "12:18,03"),
    ("GLS + path relinking\nsem deadline", Path("data/path_relinking_sem_deadline.txt"), "11:42,36", "11:00,20"),
]
HARD5 = [100, 128, 129, 147, 28]
OUTPUT = Path("data/charts_gls_reform/path_relinking_no_deadline_comparison.png")

def load(path):
    rows = {}
    for line in path.read_text().splitlines():
        f = line.split()
        if len(f) >= 5 and f[0].startswith("Instance_10_10_"):
            rows[int(f[0].rsplit("_", 1)[1])] = {
                "opt": float(f[1]), "mean": float(f[3]), "best": float(f[4])
            }
    return rows

def gap(cost, optimum):
    return 100 * (cost - optimum) / optimum

def main():
    configs = [(name, load(path), external, internal)
               for name, path, external, internal in CONFIGS]
    summary = []
    for name, data, external, internal in configs:
        optimums = sum(row["best"] == row["opt"] for row in data.values())
        avg_best = sum(row["best"] for row in data.values()) / len(data)
        best_gap = sum(gap(row["best"], row["opt"]) for row in data.values()) / len(data)
        mean_gap = sum(gap(row["mean"], row["opt"]) for row in data.values()) / len(data)
        summary.append([name, f"{optimums}/94", f"{avg_best:.3f}",
                        f"{best_gap:.3f}%", f"{mean_gap:.3f}%",
                        external, internal])

    hard = []
    for instance in HARD5:
        optimum = configs[0][1][instance]["opt"]
        row = [str(instance), f"{optimum:.0f}"]
        for _, data, _, _ in configs:
            best = data[instance]["best"]
            row.extend([f"{best:.0f}", f"{gap(best, optimum):.1f}%"])
        hard.append(row)

    fig, axes = plt.subplots(2, 1, figsize=(18, 11),
                             gridspec_kw={"hspace": .55, "height_ratios": [1, 1.5]})
    for ax in axes: ax.axis("off")
    top = axes[0].table(
        cellText=summary,
        colLabels=["Configuração", "Ótimos", "Custo médio\ndos melhores",
                   "GAP melhor", "GAP das médias", "Tempo externo", "Tempo interno"],
        cellLoc="center", loc="center")
    headers = ["Inst.", "Ótimo"]
    for name, _, _, _ in configs:
        headers.extend([f"{name}\nMelhor", "GAP"])
    bottom = axes[1].table(cellText=hard, colLabels=headers,
                           cellLoc="center", loc="center")
    for table, size, scale in ((top, 9.5, 1.75), (bottom, 8.0, 1.65)):
        table.auto_set_font_size(False); table.set_fontsize(size); table.scale(1, scale)
        for (row, _), cell in table.get_celld().items():
            cell.set_edgecolor("#c7d1d9")
            if row == 0:
                cell.set_facecolor("#263746")
                cell.set_text_props(color="white", fontweight="bold")
    for (row, col), cell in bottom.get_celld().items():
        if row == 0:
            cell.set_height(.16)
        elif col in (8, 9):
            cell.set_facecolor("#dff3e5")
    fig.suptitle(
        "PATH RELINKING SEM DEADLINE — COMPARAÇÃO NAS 94 INSTÂNCIAS\n"
        "3 repetições | resultados históricos têm sementes e orçamentos distintos",
        fontsize=15, fontweight="bold")
    axes[1].text(.5, -.08,
        "O path relinking terminou em 11:00,20 internamente: 82 ótimos, GAP global 0,780% e GAP 11,87% nas cinco difíceis.",
        ha="center", transform=axes[1].transAxes, fontsize=10)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT, dpi=170, bbox_inches="tight")
    plt.close(fig)
    print(f"Salvo: {OUTPUT}")

if __name__ == "__main__":
    main()
