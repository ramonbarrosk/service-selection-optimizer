#!/usr/bin/env python3
"""Resume e representa a configuração GLS/GFLS promovida nas 94 instâncias."""

import argparse
import csv
import os
import statistics
from collections import defaultdict
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def mean(values):
    return statistics.fmean(values)


def load(csv_path):
    with csv_path.open(newline="", encoding="utf-8") as source:
        rows = list(csv.DictReader(source))
    if not rows:
        raise ValueError(f"CSV vazio: {csv_path}")
    for row in rows:
        for field in ("cost", "gap_pct", "time_ms", "time_to_best_ms"):
            row[field] = float(row[field])
        for field in ("instance", "repetition", "optimum", "feasible"):
            row[field] = int(row[field])
    return rows


def aggregate(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[row["instance"]].append(row)
    result = []
    for instance, group in grouped.items():
        result.append({
            "instance": instance,
            "optimum": group[0]["optimum"],
            "cost": mean(row["cost"] for row in group),
            "best": min(row["cost"] for row in group),
            "gap": mean(row["gap_pct"] for row in group),
            "best_gap": min(row["gap_pct"] for row in group),
            "time": mean(row["time_ms"] for row in group),
        })
    return sorted(result, key=lambda item: item["instance"])


def plot_gaps(stats, output_dir):
    ordered = sorted(stats, key=lambda item: item["gap"], reverse=True)
    figure, axis = plt.subplots(figsize=(18, 8))
    figure.patch.set_facecolor("#f8fafc")
    colors = ["#dc2626" if item["gap"] >= 5 else
              "#f59e0b" if item["gap"] > 0 else "#16a34a" for item in ordered]
    bars = axis.bar(range(len(ordered)), [item["gap"] for item in ordered],
                    color=colors, width=0.82)
    axis.set_title("GAP médio da GLS/GFLS promovida nas 94 instâncias",
                   fontsize=18, fontweight="bold", pad=14)
    axis.set_ylabel("GAP médio em três repetições (%)")
    axis.set_xlabel("Instâncias ordenadas do maior para o menor GAP")
    axis.set_xticks(range(len(ordered)), [item["instance"] for item in ordered],
                    rotation=90, fontsize=7)
    axis.grid(axis="y", color="#e2e8f0")
    axis.spines[["top", "right"]].set_visible(False)
    for bar, item in zip(bars[:10], ordered[:10]):
        axis.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.15,
                  f"{item['gap']:.1f}%", ha="center", va="bottom",
                  rotation=90, fontsize=7, fontweight="bold")
    figure.tight_layout()
    figure.savefig(output_dir / "gap_por_instancia.png", dpi=180,
                   bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def plot_distribution(stats, output_dir):
    gaps = [item["gap"] for item in stats]
    categories = [
        ("Ótimo\n(0%)", sum(gap <= 1e-9 for gap in gaps)),
        ("Até 1%", sum(1e-9 < gap <= 1 for gap in gaps)),
        ("1% a 5%", sum(1 < gap <= 5 for gap in gaps)),
        ("Acima de 5%", sum(gap > 5 for gap in gaps)),
    ]
    figure, axis = plt.subplots(figsize=(10, 6.5))
    figure.patch.set_facecolor("#f8fafc")
    bars = axis.bar([label for label, _ in categories],
                    [value for _, value in categories],
                    color=["#16a34a", "#22c55e", "#f59e0b", "#dc2626"])
    axis.set_title("Distribuição das instâncias por faixa de GAP",
                   fontsize=17, fontweight="bold", pad=14)
    axis.set_ylabel("Quantidade de instâncias")
    axis.grid(axis="y", color="#e2e8f0")
    axis.spines[["top", "right"]].set_visible(False)
    for bar in bars:
        axis.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.4,
                  str(int(bar.get_height())), ha="center", fontweight="bold")
    figure.tight_layout()
    figure.savefig(output_dir / "distribuicao_gap.png", dpi=180,
                   bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def plot_hardest_table(stats, output_dir):
    hardest = sorted(stats, key=lambda item: item["gap"], reverse=True)[:15]
    cells = [[item["instance"], item["optimum"], f"{item['cost']:.2f}",
              f"{item['best']:.0f}", f"{item['gap']:.2f}%"] for item in hardest]
    figure, axis = plt.subplots(figsize=(11, 8))
    figure.patch.set_facecolor("#f8fafc")
    axis.axis("off")
    table = axis.table(
        cellText=cells,
        colLabels=["Instância", "Ótimo", "Custo médio", "Melhor custo", "GAP médio"],
        cellLoc="center", loc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1, 1.75)
    for column in range(5):
        table[0, column].set_facecolor("#1e293b")
        table[0, column].set_text_props(color="white", fontweight="bold")
    for row in range(1, len(cells) + 1):
        color = "#fee2e2" if row <= 5 else "#f8fafc"
        for column in range(5):
            table[row, column].set_facecolor(color)
    figure.suptitle("15 instâncias com maior GAP médio",
                    fontsize=18, fontweight="bold", y=0.95)
    figure.tight_layout(rect=[0.01, 0.02, 0.99, 0.91])
    figure.savefig(output_dir / "tabela_15_maiores_gaps.png", dpi=180,
                   bbox_inches="tight", facecolor=figure.get_facecolor())
    plt.close(figure)


def write_summary(rows, stats, summary_path):
    hardest = sorted(stats, key=lambda item: item["gap"], reverse=True)
    optimum_mean = sum(item["gap"] <= 1e-9 for item in stats)
    optimum_best = sum(item["best_gap"] <= 1e-9 for item in stats)
    optimum_runs = sum(row["gap_pct"] <= 1e-9 for row in rows)
    lines = [
        "# GLS/GFLS promovida — 94 instâncias",
        "",
        f"- Configuração: I1 S1 L0 R1.",
        f"- Instâncias: {len(stats)}.",
        f"- Repetições por instância: {len(set(row['repetition'] for row in rows))}.",
        f"- Orçamento por execução: {rows[0]['budget_seconds']}s.",
        f"- Tempo interno total: {sum(row['time_ms'] for row in rows) / 1000:.2f}s.",
        f"- Resultados viáveis: {sum(row['feasible'] for row in rows)}/{len(rows)}.",
        "",
        "## Resultado agregado",
        "",
        f"- Custo médio: **{mean(item['cost'] for item in stats):.2f}**.",
        f"- GAP médio: **{mean(item['gap'] for item in stats):.2f}%**.",
        f"- Instâncias ótimas nas três repetições: **{optimum_mean}/{len(stats)}**.",
        f"- Instâncias com ótimo em pelo menos uma repetição: **{optimum_best}/{len(stats)}**.",
        f"- Execuções ótimas: **{optimum_runs}/{len(rows)}**.",
        "",
        "## Maiores GAPs",
        "",
        "| Instância | Ótimo | Custo médio | Melhor custo | GAP médio |",
        "|---:|---:|---:|---:|---:|",
    ]
    for item in hardest[:15]:
        lines.append(
            f"| {item['instance']} | {item['optimum']} | {item['cost']:.2f} | "
            f"{item['best']:.0f} | {item['gap']:.2f}% |"
        )
    summary_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", type=Path,
                        default=Path("experiments/06_gls_melhorias/results/gls_promoted_all94.csv"))
    parser.add_argument("--output-dir", type=Path,
                        default=Path("experiments/06_gls_melhorias/charts/gls_promoted_all94"))
    parser.add_argument("--summary", type=Path,
                        default=Path("experiments/06_gls_melhorias/results/gls_promoted_all94_summary.md"))
    arguments = parser.parse_args()
    rows = load(arguments.csv)
    stats = aggregate(rows)
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    plot_gaps(stats, arguments.output_dir)
    plot_distribution(stats, arguments.output_dir)
    plot_hardest_table(stats, arguments.output_dir)
    write_summary(rows, stats, arguments.summary)
    print(f"Gráficos salvos em {arguments.output_dir}")
    print(f"Resumo salvo em {arguments.summary}")


if __name__ == "__main__":
    main()
