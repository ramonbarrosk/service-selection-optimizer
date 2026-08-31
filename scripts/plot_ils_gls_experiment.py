#!/usr/bin/env python3
"""Gera imagens estáticas do experimento ILS × GLS × GLS com oscilação.

Uso, a partir da raiz do projeto:
    python3 scripts/plot_ils_gls_experiment.py

Saída:
    data/charts_ils_gls/comparacao_geral.png
    data/charts_ils_gls/comparacao_5_instancias_dificeis.png
    data/charts_ils_gls/tabela_instancias_1.png
    data/charts_ils_gls/tabela_instancias_2.png
"""

import argparse
import csv
import os
import statistics
from collections import defaultdict
from pathlib import Path

# Evita que o Matplotlib tente gravar sua configuração no diretório do usuário.
os.environ.setdefault("MPLCONFIGDIR", "/tmp/sso-matplotlib")

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches


METHODS = {
    "ils": {"label": "ILS original", "color": "#94a3b8", "bar": "#64748b"},
    "gls": {"label": "GLS sem ILS", "color": "#bfdbfe", "bar": "#2563eb"},
    "gls_osc": {"label": "GLS + oscilação", "color": "#fde68a", "bar": "#f59e0b"},
}

DIFFICULT_INSTANCES = {28, 100, 128, 129, 147}


def mean(values):
    return statistics.fmean(values) if values else 0.0


def duration(milliseconds):
    seconds = milliseconds / 1000.0
    minutes = int(seconds // 60)
    remainder = seconds - minutes * 60
    return f"{minutes}min {remainder:.2f}s" if minutes else f"{remainder:.2f}s"


def load_rows(path):
    with Path(path).open(newline="", encoding="utf-8") as file:
        raw_rows = list(csv.DictReader(file))
    if not raw_rows:
        raise ValueError(f"O CSV está vazio: {path}")

    rows = []
    for raw in raw_rows:
        row = {
            "instance": int(raw["instance"]),
            "repetition": int(raw["repetition"]),
            "optimum": int(raw["optimum"]),
            "budget": float(raw["budget_seconds"]),
        }
        for method in METHODS:
            row[f"{method}_cost"] = float(raw[f"{method}_cost"])
            row[f"{method}_gap"] = float(raw[f"{method}_gap_pct"])
            row[f"{method}_time"] = float(raw[f"{method}_time_ms"])
        rows.append(row)
    return rows


def aggregate(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[row["instance"]].append(row)

    instances = []
    for instance_id, group in sorted(grouped.items()):
        item = {
            "instance": instance_id,
            "optimum": group[0]["optimum"],
            "budget": group[0]["budget"],
        }
        for method in METHODS:
            costs = [row[f"{method}_cost"] for row in group]
            item[f"{method}_cost"] = mean(costs)
            item[f"{method}_best"] = min(costs)
            item[f"{method}_gap"] = mean([row[f"{method}_gap"] for row in group])
            item[f"{method}_time"] = mean([row[f"{method}_time"] for row in group])
        instances.append(item)

    summary = {}
    for method in METHODS:
        summary[method] = {
            "cost": mean([item[f"{method}_cost"] for item in instances]),
            "gap": mean([item[f"{method}_gap"] for item in instances]),
            "time": mean([row[f"{method}_time"] for row in rows]),
            "total_time": sum(row[f"{method}_time"] for row in rows),
            "optimal": sum(
                item[f"{method}_best"] <= item["optimum"] for item in instances
            ),
        }
    return instances, summary


def style_axes(axis, title):
    axis.set_title(title, fontsize=12, fontweight="bold", pad=12)
    axis.grid(axis="y", color="#e2e8f0", linewidth=0.8)
    axis.set_axisbelow(True)
    axis.spines[["top", "right"]].set_visible(False)
    axis.spines[["left", "bottom"]].set_color("#cbd5e1")


def add_bar_labels(axis, bars, formatter):
    for bar in bars:
        axis.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height(),
            formatter(bar.get_height()),
            ha="center", va="bottom", fontsize=9, fontweight="bold",
        )


def make_summary_image(summary, number_of_instances, repetitions,
                       budget_note, output_path):
    method_keys = list(METHODS)
    labels = [METHODS[key]["label"] for key in method_keys]
    colors = [METHODS[key]["bar"] for key in method_keys]

    fig, axes = plt.subplots(1, 3, figsize=(18, 6.5), gridspec_kw={"width_ratios": [1.25, 1, 1]})
    fig.patch.set_facecolor("#f8fafc")
    fig.suptitle(
        f"Comparação das três versões — {number_of_instances} instâncias × "
        f"{repetitions} repetições",
        fontsize=16, fontweight="bold", y=0.98,
    )

    axes[0].axis("off")
    table_rows = []
    for key in method_keys:
        item = summary[key]
        table_rows.append([
            METHODS[key]["label"],
            f"{item['cost']:.2f}",
            f"{item['gap']:.2f}%",
            f"{item['time'] / 1000.0:.3f}s",
            duration(item["total_time"]),
            f"{item['optimal']}/{number_of_instances}",
        ])
    table = axes[0].table(
        cellText=table_rows,
        colLabels=["Método", "Custo", "GAP", "Tempo médio", "Tempo total", "Ótimos"],
        cellLoc="center", loc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(8.2)
    table.scale(1.1, 2.0)
    for column in range(6):
        table[0, column].set_facecolor("#1e293b")
        table[0, column].set_text_props(color="white", fontweight="bold")
    for row, key in enumerate(method_keys, start=1):
        table[row, 0].set_facecolor(METHODS[key]["color"])
        table[row, 0].set_text_props(fontweight="bold")
    axes[0].set_title("Resumo agregado", fontsize=12, fontweight="bold", pad=12)

    gaps = [summary[key]["gap"] for key in method_keys]
    gap_bars = axes[1].bar(labels, gaps, color=colors, width=0.65)
    style_axes(axes[1], "GAP médio")
    axes[1].set_ylabel("GAP (%)")
    axes[1].tick_params(axis="x", rotation=18)
    add_bar_labels(axes[1], gap_bars, lambda value: f"{value:.2f}%")

    total_seconds = [summary[key]["total_time"] / 1000.0 for key in method_keys]
    time_bars = axes[2].bar(labels, total_seconds, color=colors, width=0.65)
    style_axes(axes[2], "Tempo total do algoritmo")
    axes[2].set_ylabel("Tempo (segundos)")
    axes[2].tick_params(axis="x", rotation=18)
    add_bar_labels(axes[2], time_bars, lambda value: f"{value:.1f}s")

    fig.text(
        0.5, 0.015,
        f"Mesmo orçamento por método dentro de cada instância. {budget_note} "
        "Execuções terminam antes ao atingir o ótimo.",
        ha="center", color="#475569", fontsize=9,
    )
    fig.tight_layout(rect=[0, 0.04, 1, 0.94])
    fig.savefig(output_path, dpi=160, bbox_inches="tight", facecolor=fig.get_facecolor())
    plt.close(fig)
    print(f"Salvo: {output_path}")


def gap_color(gap):
    if gap <= 1.0:
        return "#dcfce7"
    if gap <= 5.0:
        return "#fef3c7"
    return "#fee2e2"


def make_instance_table(instances, output_path, page, total_pages):
    columns = ["Inst.", "Ótimo"]
    for key in METHODS:
        short = {"ils": "ILS", "gls": "GLS", "gls_osc": "GLS+OSC"}[key]
        columns.extend([f"{short}\nCusto", f"{short}\nGAP", f"{short}\nTempo"])

    text = []
    colors = []
    for item in instances:
        row = [str(item["instance"]), str(item["optimum"])]
        row_colors = ["#ffffff", "#e0f2fe"]
        for key in METHODS:
            row.extend([
                f"{item[f'{key}_cost']:.2f}",
                f"{item[f'{key}_gap']:.2f}%",
                f"{item[f'{key}_time'] / 1000.0:.3f}s",
            ])
            row_colors.extend([
                METHODS[key]["color"],
                gap_color(item[f"{key}_gap"]),
                "#f1f5f9",
            ])
        text.append(row)
        colors.append(row_colors)

    figure, axis = plt.subplots(figsize=(20, 17))
    figure.patch.set_facecolor("white")
    axis.axis("off")
    table = axis.table(
        cellText=text,
        colLabels=columns,
        cellColours=colors,
        cellLoc="center",
        loc="center",
        colWidths=[0.045, 0.055] + [0.068, 0.063, 0.063] * 3,
    )
    table.auto_set_font_size(False)
    table.set_fontsize(7.3)
    table.scale(1, 1.42)
    for column in range(len(columns)):
        table[0, column].set_facecolor("#1e293b")
        table[0, column].set_text_props(color="white", fontweight="bold")
        table[0, column].set_height(table[0, column].get_height() * 1.35)
    for cell in table.get_celld().values():
        cell.set_edgecolor("#cbd5e1")
        cell.set_linewidth(0.45)

    first = instances[0]["instance"]
    last = instances[-1]["instance"]
    axis.set_title(
        f"Resultados por instância — página {page}/{total_pages} "
        f"(identificadores {first} a {last})",
        fontsize=14, fontweight="bold", pad=18,
    )
    legend = [
        mpatches.Patch(color="#dcfce7", label="GAP ≤ 1%"),
        mpatches.Patch(color="#fef3c7", label="1% < GAP ≤ 5%"),
        mpatches.Patch(color="#fee2e2", label="GAP > 5%"),
    ]
    figure.legend(handles=legend, loc="lower center", ncol=3, frameon=True, fontsize=9)
    figure.tight_layout(rect=[0, 0.035, 1, 0.98])
    figure.savefig(output_path, dpi=150, bbox_inches="tight")
    plt.close(figure)
    print(f"Salvo: {output_path}")


def make_difficult_table(instances, repetitions, output_path):
    """Gera uma tabela dedicada às cinco instâncias mais difíceis."""
    difficult = [
        item for item in instances if item["instance"] in DIFFICULT_INSTANCES
    ]
    found = {item["instance"] for item in difficult}
    if found != DIFFICULT_INSTANCES:
        missing = sorted(DIFFICULT_INSTANCES - found)
        raise ValueError(f"Instâncias difíceis ausentes no CSV: {missing}")

    difficult_budgets = [item["budget"] for item in difficult]
    if len(set(difficult_budgets)) == 1:
        budget_note = f"Orçamento fixo de {difficult_budgets[0]:.3f}s."
    else:
        budget_note = (
            f"Orçamento adaptativo neste subconjunto: "
            f"{min(difficult_budgets):.3f}s a {max(difficult_budgets):.3f}s."
        )

    columns = ["Instância", "Ótimo"]
    for key in METHODS:
        short = {"ils": "ILS original", "gls": "GLS isolada",
                 "gls_osc": "GLS + oscilação"}[key]
        columns.extend([f"{short}\nCusto", "GAP", "Tempo"])

    text = []
    colors = []
    for item in difficult:
        row = [str(item["instance"]), str(item["optimum"])]
        row_colors = ["#ffffff", "#e0f2fe"]
        for key in METHODS:
            row.extend([
                f"{item[f'{key}_cost']:.2f}",
                f"{item[f'{key}_gap']:.2f}%",
                f"{item[f'{key}_time'] / 1000.0:.3f}s",
            ])
            row_colors.extend([
                METHODS[key]["color"],
                gap_color(item[f"{key}_gap"]),
                "#f1f5f9",
            ])
        text.append(row)
        colors.append(row_colors)

    averages = {}
    average_row = ["MÉDIA", "—"]
    average_colors = ["#1e293b", "#1e293b"]
    for key in METHODS:
        averages[key] = {
            "cost": mean([item[f"{key}_cost"] for item in difficult]),
            "gap": mean([item[f"{key}_gap"] for item in difficult]),
        }
        average_row.extend([
            f"{averages[key]['cost']:.2f}",
            f"{averages[key]['gap']:.2f}%",
            f"{mean([item[f'{key}_time'] for item in difficult]) / 1000.0:.3f}s",
        ])
        average_colors.extend([METHODS[key]["color"]] * 3)
    text.append(average_row)
    colors.append(average_colors)

    figure, axis = plt.subplots(figsize=(19, 8.5))
    figure.patch.set_facecolor("#f8fafc")
    axis.axis("off")
    table = axis.table(
        cellText=text,
        colLabels=columns,
        cellColours=colors,
        cellLoc="center",
        loc="center",
        colWidths=[0.075, 0.075] + [0.09, 0.075, 0.075] * 3,
    )
    table.auto_set_font_size(False)
    table.set_fontsize(9.5)
    table.scale(1, 2.15)

    for column in range(len(columns)):
        table[0, column].set_facecolor("#1e293b")
        table[0, column].set_text_props(color="white", fontweight="bold")
        table[0, column].set_height(table[0, column].get_height() * 1.35)
    last_row = len(text)
    for column in range(len(columns)):
        table[last_row, column].set_text_props(
            color="white" if column < 2 else "#0f172a", fontweight="bold"
        )
    for cell in table.get_celld().values():
        cell.set_edgecolor("#334155")
        cell.set_linewidth(0.75)

    figure.suptitle(
        "ILS original × GLS isolada × GLS com oscilação",
        fontsize=18, fontweight="bold", y=0.95,
    )
    axis.set_title(
        "Comparação nas cinco instâncias mais difíceis — "
        f"{repetitions} repetições por instância",
        fontsize=12, color="#475569", pad=18,
    )
    figure.text(
        0.5, 0.055,
        "Valores por instância: média das repetições. " + budget_note,
        ha="center", color="#475569", fontsize=10,
    )
    best_cost = min(averages[key]["cost"] for key in METHODS)
    best_keys = [
        key for key in METHODS
        if abs(averages[key]["cost"] - best_cost) <= 1e-9
    ]
    if len(best_keys) == 1:
        best_key = best_keys[0]
        result_note = (
            f"Melhor resultado médio do subconjunto: {METHODS[best_key]['label']} — "
            f"custo {averages[best_key]['cost']:.2f} e "
            f"GAP {averages[best_key]['gap']:.2f}%."
        )
    else:
        labels = " e ".join(METHODS[key]["label"] for key in best_keys)
        result_note = (
            f"Empate no custo médio: {labels} — custo {best_cost:.2f} "
            f"e GAP aproximado de {averages[best_keys[0]]['gap']:.2f}%."
        )
    figure.text(
        0.5, 0.025,
        result_note,
        ha="center", color="#1e3a5f", fontsize=10, fontweight="bold",
    )
    figure.tight_layout(rect=[0.02, 0.08, 0.98, 0.91])
    figure.savefig(output_path, dpi=160, bbox_inches="tight",
                   facecolor=figure.get_facecolor())
    plt.close(figure)
    print(f"Salvo: {output_path}")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", default="data/experiments/ils_gls_comparison.csv")
    parser.add_argument("--output-dir", default="data/charts_ils_gls")
    return parser.parse_args()


def main():
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    rows = load_rows(args.csv)
    instances, summary = aggregate(rows)
    repetitions = len({row["repetition"] for row in rows})
    budgets = [row["budget"] for row in rows]
    if len(set(budgets)) == 1:
        budget_note = f"Orçamento fixo de {budgets[0]:.3f}s."
    else:
        budget_note = (
            f"Orçamento adaptativo: {min(budgets):.3f}s a "
            f"{max(budgets):.3f}s por execução."
        )
    make_summary_image(
        summary, len(instances), repetitions, budget_note,
        output_dir / "comparacao_geral.png"
    )
    make_difficult_table(
        instances, repetitions,
        output_dir / "comparacao_5_instancias_dificeis.png"
    )

    page_size = 47
    pages = [instances[index:index + page_size]
             for index in range(0, len(instances), page_size)]
    for page_number, page_rows in enumerate(pages, start=1):
        make_instance_table(
            page_rows,
            output_dir / f"tabela_instancias_{page_number}.png",
            page_number,
            len(pages),
        )

    print(f"Pronto. Imagens disponíveis em {output_dir}/")


if __name__ == "__main__":
    main()
