#!/usr/bin/env python3
"""
Visualização comparativa: CPLEX Ótimo vs Java ILS vs C++ ILS
Projeto: Service Selection Under Uncertainty

USO (a partir da raiz do projeto):
  # 1) Rodar C++ e salvar saída:
  #    bash scripts/run_optimizer.sh          → data/cpp_results.txt
  #
  # 2) Rodar Java e salvar saída:
  #    bash scripts/run_java.sh               → data/java_results_raw.txt
  #
  # 3) Gerar gráficos:
  #    python scripts/visualize.py --java-raw data/java_results_raw.txt
  #    python scripts/visualize.py --java-mean 165   # fallback só com média

Saída: data/charts/fig1_distributions.png
               data/charts/fig2_per_instance.png
               data/charts/fig3_summary.png
               data/charts/fig4_java_vs_cpp.png   (se java_results_raw.txt existir)
"""

import re
import sys
import argparse
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from pathlib import Path

# ─── Estilo global ────────────────────────────────────────────────────────────
plt.rcParams.update({
    "font.family":     "DejaVu Sans",
    "font.size":       11,
    "axes.titlesize":  13,
    "axes.labelsize":  11,
    "axes.spines.top":    False,
    "axes.spines.right":  False,
    "figure.dpi":      150,
})

COLORS = {"cplex": "#27ae60", "java": "#e67e22", "cpp": "#2980b9"}
LABELS = {"cplex": "CPLEX (Ótimo)", "java": "Java ILS", "cpp": "C++ ILS"}


# ─── Parsers ──────────────────────────────────────────────────────────────────

def load_cpp_output(filepath: str) -> list[dict]:
    """
    Lê stdout do binário C++.
    Linhas de dados: Instance_10_10_N cplex_cost cplex_time cpp_mean cpp_best cpp_time
    """
    records = []
    pat = re.compile(
        r"^(Instance_\S+)\s+([\d.]+)\s+([\d.eE+\-]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)"
    )
    with open(filepath) as f:
        for line in f:
            m = pat.match(line.strip())
            if m:
                records.append({
                    "name":       m.group(1),
                    "cplex_cost": float(m.group(2)),
                    "cplex_time": float(m.group(3)),
                    "cpp_mean":   float(m.group(4)),
                    "cpp_best":   float(m.group(5)),
                    "cpp_time":   float(m.group(6)),
                })
    return records


def load_java_raw(filepath: str) -> dict:
    """
    Lê stdout do ILS Java — formato idêntico ao C++:
      Instance_10_10_N cplex_cost cplex_time java_mean java_best java_time
    Retorna dict {instance_name: {mean, best}}.
    """
    records = load_cpp_output(filepath)   # mesmo parser, mesmo formato
    return {r["name"]: {"mean": r["cpp_mean"], "best": r["cpp_best"]} for r in records}


def load_java_csv(filepath: str) -> dict:
    """
    CSV manual com resultados Java por instância.
    Formato de cada linha: Instance_10_10_N,mean_cost[,best_cost]
    """
    java = {}
    with open(filepath) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.lower().startswith("instance,"):
                continue
            parts = [p.strip() for p in line.split(",")]
            if len(parts) >= 2 and parts[0].startswith("Instance_"):
                java[parts[0]] = {
                    "mean": float(parts[1]),
                    "best": float(parts[2]) if len(parts) > 2 else float(parts[1]),
                }
    return java


# ─── Helpers ──────────────────────────────────────────────────────────────────

def gap_pct(cost: float, optimal: float) -> float:
    return (cost - optimal) / optimal * 100.0 if optimal > 0 else 0.0


def _box_style(bp, color: str):
    bp["boxes"][0].set(facecolor=color, alpha=0.75)
    for el in ["whiskers", "caps"]:
        for item in bp[el]:
            item.set(color="#444", linewidth=1.2)
    bp["medians"][0].set(color="#111", linewidth=2)
    for flier in bp["fliers"]:
        flier.set(marker="o", markerfacecolor=color, alpha=0.5, markersize=4)


# ─── Figura 1: Distribuição de custos e GAP% ─────────────────────────────────

def fig1_distributions(records, java_map, java_mean_overall, out_dir):
    cplex = np.array([r["cplex_cost"] for r in records])
    cpp   = np.array([r["cpp_mean"]   for r in records])
    cpp_gaps = np.array([gap_pct(r["cpp_mean"], r["cplex_cost"]) for r in records])

    # Dados Java alinhados com os records disponíveis
    java_arr = None
    java_gaps = None
    if java_map:
        common = [r for r in records if r["name"] in java_map]
        java_arr  = np.array([java_map[r["name"]]["mean"] for r in common])
        java_gaps = np.array([gap_pct(java_map[r["name"]]["mean"], r["cplex_cost"]) for r in common])

    fig, axes = plt.subplots(1, 2, figsize=(12, 6))
    fig.suptitle("Distribuição de Custos: CPLEX vs Java ILS vs C++ ILS", fontweight="bold", y=1.01)

    # ── subplot esquerdo: custos absolutos ──
    ax = axes[0]
    positions, datasets, colors, tick_labels = [], [], [], []
    for pos, (key, arr) in enumerate(
        [("cplex", cplex)] +
        ([("java", java_arr)] if java_arr is not None else []) +
        [("cpp", cpp)],
        start=1,
    ):
        bp = ax.boxplot(arr, positions=[pos], widths=0.55, patch_artist=True,
                        notch=False, showfliers=True)
        _box_style(bp, COLORS[key])
        ax.text(pos, arr.max() + 0.3, f"μ={arr.mean():.1f}",
                ha="center", va="bottom", fontsize=9, color=COLORS[key], fontweight="bold")
        tick_labels.append(LABELS[key])
        positions.append(pos)

    ax.set_xticks(positions)
    ax.set_xticklabels(tick_labels, fontsize=10)
    ax.set_ylabel("Custo da solução")
    ax.set_title("Custos absolutos")
    ax.grid(axis="y", linestyle="--", alpha=0.4)

    # ── subplot direito: GAP% ──
    ax = axes[1]
    gap_datasets = []
    gap_keys = []
    if java_gaps is not None:
        gap_datasets.append(("java", java_gaps))
    gap_datasets.append(("cpp", cpp_gaps))

    positions2, tick_labels2 = [], []
    for pos, (key, arr) in enumerate(gap_datasets, start=1):
        bp = ax.boxplot(arr, positions=[pos], widths=0.55, patch_artist=True,
                        notch=False, showfliers=True)
        _box_style(bp, COLORS[key])
        ax.text(pos, arr.max() + 0.3, f"μ={arr.mean():.1f}%",
                ha="center", va="bottom", fontsize=9, color=COLORS[key], fontweight="bold")
        tick_labels2.append(LABELS[key])
        positions2.append(pos)

    ax.set_xticks(positions2)
    ax.set_xticklabels(tick_labels2, fontsize=10)
    ax.set_ylabel("GAP em relação ao CPLEX (%)")
    ax.set_title("GAP% em relação ao ótimo")
    ax.axhline(0, color=COLORS["cplex"], linestyle="--", alpha=0.6, linewidth=1.5, label="Ótimo = 0%")
    ax.legend(fontsize=9)
    ax.grid(axis="y", linestyle="--", alpha=0.4)

    # Se só tiver média Java, anotação textual
    if java_arr is None and java_mean_overall > 0:
        java_gap_overall = gap_pct(java_mean_overall, cplex.mean())
        ax.axhline(java_gap_overall, color=COLORS["java"], linestyle=":",
                   linewidth=2, label=f"Java ILS μ ≈ {java_gap_overall:.1f}%")
        ax.legend(fontsize=9)

    fig.tight_layout()
    out = Path(out_dir) / "fig1_distributions.png"
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  Salvo: {out}")


# ─── Figura 2: Comparativo por instância ──────────────────────────────────────

def fig2_per_instance(records, java_map, out_dir):
    records_sorted = sorted(records, key=lambda r: r["cplex_cost"])
    cplex = np.array([r["cplex_cost"] for r in records_sorted])
    cpp   = np.array([r["cpp_mean"]   for r in records_sorted])
    idx   = np.arange(len(records_sorted))

    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.suptitle("Análise por Instância", fontweight="bold", y=1.01)

    # ── subplot esquerdo: scatter C++ vs CPLEX ──
    ax = axes[0]
    gaps = np.array([gap_pct(r["cpp_mean"], r["cplex_cost"]) for r in records_sorted])
    sc = ax.scatter(cplex, cpp, c=gaps, cmap="RdYlGn_r", alpha=0.75, s=50, zorder=3)
    plt.colorbar(sc, ax=ax, label="GAP C++ (%)")

    lo = min(cplex.min(), cpp.min()) - 1
    hi = max(cplex.max(), cpp.max()) + 1
    ax.plot([lo, hi], [lo, hi], color=COLORS["cplex"], linestyle="--",
            linewidth=1.5, label="Ótimo (y = x)", zorder=2)

    # Região de melhoria
    ax.fill_between([lo, hi], [lo, hi], [hi, hi],
                    alpha=0.05, color=COLORS["cpp"], label="Acima do ótimo")

    ax.set_xlim(lo, hi)
    ax.set_ylim(lo, hi)
    ax.set_xlabel("Custo CPLEX (ótimo)")
    ax.set_ylabel("Custo médio C++ ILS")
    ax.set_title("C++ ILS vs Ótimo (por instância)")
    ax.legend(fontsize=9)
    ax.grid(linestyle="--", alpha=0.35)

    # ── subplot direito: perfil de custos (sorted) ──
    ax = axes[1]
    ax.plot(idx, cplex, color=COLORS["cplex"], linewidth=1.8,
            label=LABELS["cplex"], marker="", zorder=3)
    ax.plot(idx, cpp, color=COLORS["cpp"], linewidth=1.5,
            label=LABELS["cpp"], linestyle="--", zorder=3)

    if java_map:
        java_sorted = [java_map.get(r["name"], {}).get("mean", None) for r in records_sorted]
        java_filtered_idx = [i for i, v in enumerate(java_sorted) if v is not None]
        java_filtered_val = [java_sorted[i] for i in java_filtered_idx]
        ax.plot(java_filtered_idx, java_filtered_val, color=COLORS["java"], linewidth=1.5,
                label=LABELS["java"], linestyle=":", zorder=3)

    ax.fill_between(idx, cplex, cpp, alpha=0.12, color=COLORS["cpp"])
    ax.set_xlabel("Instâncias (ordenadas por custo ótimo crescente)")
    ax.set_ylabel("Custo")
    ax.set_title("Perfil de custos por instância")
    ax.legend(fontsize=9)
    ax.grid(linestyle="--", alpha=0.35)
    ax.set_xticks([])

    fig.tight_layout()
    out = Path(out_dir) / "fig2_per_instance.png"
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  Salvo: {out}")


# ─── Figura 3: Resumo com métricas e tabela ───────────────────────────────────

def fig3_summary(records, java_map, java_mean_overall, out_dir):
    cplex = np.array([r["cplex_cost"] for r in records])
    cpp   = np.array([r["cpp_mean"]   for r in records])

    # Monta colunas dinâmicas
    methods = ["cplex", "cpp"]
    arrays  = {"cplex": cplex, "cpp": cpp}

    java_arr = None
    if java_map:
        common = [r for r in records if r["name"] in java_map]
        java_arr = np.array([java_map[r["name"]]["mean"] for r in common])
        methods = ["cplex", "java", "cpp"]
        arrays["java"] = java_arr
    elif java_mean_overall > 0:
        methods = ["cplex", "java", "cpp"]
        arrays["java"] = np.array([java_mean_overall])  # só média

    fig = plt.figure(figsize=(13, 5))
    gs  = gridspec.GridSpec(1, 2, width_ratios=[1, 1.4], figure=fig)
    fig.suptitle("Resumo Comparativo de Desempenho", fontweight="bold", y=1.02)

    # ── subplot esquerdo: barras de média ± std ──
    ax = fig.add_subplot(gs[0])
    x = np.arange(len(methods))
    means = [arrays[m].mean() for m in methods]
    stds  = [arrays[m].std()  if len(arrays[m]) > 1 else 0 for m in methods]
    colors_list = [COLORS[m] for m in methods]

    bars = ax.bar(x, means, yerr=stds, color=colors_list, alpha=0.82,
                  capsize=6, width=0.55, ecolor="#333", error_kw={"linewidth": 1.5})

    for bar, mean, std in zip(bars, means, stds):
        label = f"{mean:.1f}" + (f"\n±{std:.1f}" if std > 0 else "")
        ax.text(bar.get_x() + bar.get_width() / 2,
                bar.get_height() + (std if std > 0 else 0) + 0.5,
                label, ha="center", va="bottom", fontsize=10, fontweight="bold")

    ax.set_xticks(x)
    ax.set_xticklabels([LABELS[m] for m in methods], fontsize=10)
    ax.set_ylabel("Custo médio")
    ax.set_title("Custo médio por método (± desvio padrão)")
    ax.set_ylim(0, max(means) * 1.20)
    ax.grid(axis="y", linestyle="--", alpha=0.4)

    # Seta de melhoria Java → C++
    if "java" in methods:
        j_mean = arrays["java"].mean()
        c_mean = arrays["cpp"].mean()
        j_x = x[methods.index("java")]
        c_x = x[methods.index("cpp")]
        improvement = j_mean - c_mean
        ax.annotate(
            f"−{improvement:.1f} ({improvement/j_mean*100:.1f}%)",
            xy=(c_x, c_mean), xytext=(j_x + 0.15, j_mean - improvement / 2),
            arrowprops=dict(arrowstyle="->", color="#c0392b", lw=1.5),
            fontsize=9, color="#c0392b", fontweight="bold",
        )

    # ── subplot direito: tabela de estatísticas ──
    ax2 = fig.add_subplot(gs[1])
    ax2.axis("off")

    def _fmt(arr, use_gap=False, ref=None):
        if len(arr) == 1:
            v = arr[0]
            g = gap_pct(v, ref.mean()) if use_gap and ref is not None else None
            return [f"{v:.2f}", "—", "—", "—", f"{g:.1f}%" if g else "—"]
        gaps = np.array([gap_pct(v, r) for v, r in zip(arr, (ref if ref is not None else arr))])
        return [
            f"{arr.mean():.2f}",
            f"{arr.min():.2f}",
            f"{arr.max():.2f}",
            f"{arr.std():.2f}",
            f"{gaps.mean():.1f}%" if use_gap else "—",
        ]

    row_headers = ["Média", "Mínimo", "Máximo", "Desvio padrão", "GAP% médio (vs ótimo)"]
    col_headers = ["Métrica"] + [LABELS[m] for m in methods]

    table_rows = []
    for i, rh in enumerate(row_headers):
        row = [rh]
        for m in methods:
            arr = arrays[m]
            is_gap_row = (i == 4)
            if is_gap_row:
                if m == "cplex":
                    row.append("—")
                else:
                    ref = cplex if len(arr) > 1 else None
                    if ref is not None:
                        common_len = min(len(arr), len(ref))
                        row.append(f"{np.mean([gap_pct(arr[k], ref[k]) for k in range(common_len)]):.1f}%")
                    else:
                        g = gap_pct(arr[0], cplex.mean())
                        row.append(f"{g:.1f}%")
            else:
                if len(arr) == 1:
                    row.append(f"{arr[0]:.2f}" if i == 0 else "—")
                else:
                    vals = [arr.mean(), arr.min(), arr.max(), arr.std()]
                    row.append(f"{vals[i]:.2f}")
        table_rows.append(row)

    # Linha extra: nº de instâncias
    n_row = ["Instâncias"]
    for m in methods:
        n_row.append(str(len(arrays[m])))
    table_rows.append(n_row)

    tbl = ax2.table(
        cellText=table_rows,
        colLabels=col_headers,
        loc="center",
        cellLoc="center",
    )
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(10)
    tbl.scale(1, 1.9)

    # Estilo header
    for j in range(len(col_headers)):
        cell = tbl[0, j]
        cell.set_facecolor("#2c3e50")
        cell.set_text_props(color="white", fontweight="bold")

    # Estilo linhas alternadas
    for i in range(1, len(table_rows) + 1):
        bg = "#eaf4fb" if i % 2 == 0 else "white"
        for j in range(len(col_headers)):
            tbl[i, j].set_facecolor(bg)

    ax2.set_title("Tabela de métricas por método", fontweight="bold", pad=16)

    fig.tight_layout()
    out = Path(out_dir) / "fig3_summary.png"
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  Salvo: {out}")


# ─── Figura 4: Scatter Java vs C++ (só se tiver CSV por instância) ────────────

def fig4_java_vs_cpp(records, java_map, out_dir):
    if not java_map:
        return

    common = [r for r in records if r["name"] in java_map]
    if not common:
        return

    java_costs  = np.array([java_map[r["name"]]["mean"] for r in common])
    cpp_costs   = np.array([r["cpp_mean"] for r in common])
    cplex_costs = np.array([r["cplex_cost"] for r in common])

    fig, ax = plt.subplots(figsize=(7, 6))

    sc = ax.scatter(java_costs, cpp_costs, c=cplex_costs, cmap="viridis",
                    alpha=0.75, s=55, zorder=3)
    plt.colorbar(sc, ax=ax, label="Custo CPLEX (ótimo)")

    lo = min(java_costs.min(), cpp_costs.min()) - 1
    hi = max(java_costs.max(), cpp_costs.max()) + 1
    ax.plot([lo, hi], [lo, hi], "r--", linewidth=1.5, label="Empate", zorder=2)
    ax.fill_between([lo, hi], [lo, hi], [lo, lo], alpha=0.06,
                    color=COLORS["cpp"], label="C++ melhor (abaixo da linha)")

    n_better = int(np.sum(cpp_costs < java_costs))
    ax.text(0.04, 0.96,
            f"C++ melhor em {n_better}/{len(common)} instâncias\n"
            f"Melhoria média: {(java_costs - cpp_costs).mean():.1f} unidades",
            transform=ax.transAxes, fontsize=10, va="top",
            bbox=dict(boxstyle="round,pad=0.4", facecolor="#d5f5e3", alpha=0.85))

    ax.set_xlim(lo, hi)
    ax.set_ylim(lo, hi)
    ax.set_xlabel("Custo Java ILS")
    ax.set_ylabel("Custo C++ ILS")
    ax.set_title("Java ILS vs C++ ILS por instância\n(abaixo da linha = C++ venceu)")
    ax.legend(fontsize=9)
    ax.grid(linestyle="--", alpha=0.35)

    fig.tight_layout()
    out = Path(out_dir) / "fig4_java_vs_cpp.png"
    fig.savefig(out, bbox_inches="tight")
    plt.close(fig)
    print(f"  Salvo: {out}")


# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Gera gráficos comparativos SSuS: CPLEX vs Java ILS vs C++ ILS"
    )
    parser.add_argument(
        "--cpp-results", default="data/cpp_results.txt",
        help="Saída capturada do binário C++ (default: data/cpp_results.txt)",
    )
    parser.add_argument(
        "--java-raw", default="data/java_results_raw.txt",
        help="Saída capturada do ILS Java (mesmo formato do C++, default: data/java_results_raw.txt)",
    )
    parser.add_argument(
        "--java-results", default="data/java_results.csv",
        help="CSV manual com resultados Java por instância (alternativa ao --java-raw)",
    )
    parser.add_argument(
        "--java-mean", type=float, default=165.0,
        help="Média geral do Java ILS (fallback se não tiver arquivo, default: 165)",
    )
    parser.add_argument(
        "--output-dir", default="data/charts",
        help="Diretório de saída das figuras (default: data/charts)",
    )
    args = parser.parse_args()

    # Validar entrada obrigatória
    if not Path(args.cpp_results).exists():
        print(f"[ERRO] Arquivo não encontrado: {args.cpp_results}")
        print()
        print("Execute o otimizador primeiro e salve a saída:")
        print(f"  ./build/service-selection-optimizer | tee {args.cpp_results}")
        sys.exit(1)

    # Carregar dados C++
    print(f"Carregando resultados C++ de '{args.cpp_results}'...")
    records = load_cpp_output(args.cpp_results)
    if not records:
        print("[ERRO] Nenhuma linha de resultado encontrada. Verifique o formato do arquivo.")
        sys.exit(1)
    print(f"  {len(records)} instâncias carregadas.")

    cplex_mean = np.mean([r["cplex_cost"] for r in records])
    cpp_mean   = np.mean([r["cpp_mean"]   for r in records])
    print(f"  Média CPLEX (ótimo): {cplex_mean:.2f}")
    print(f"  Média C++ ILS:       {cpp_mean:.2f}")
    print(f"  GAP C++ médio:       {gap_pct(cpp_mean, cplex_mean):.1f}%")

    # Carregar dados Java (opcional) — prioridade: --java-raw > --java-results > --java-mean
    java_map = {}
    java_mean_overall = args.java_mean

    if Path(args.java_raw).exists():
        print(f"\nCarregando resultados Java (raw) de '{args.java_raw}'...")
        java_map = load_java_raw(args.java_raw)
        if java_map:
            java_mean_overall = np.mean([v["mean"] for v in java_map.values()])
            print(f"  {len(java_map)} instâncias Java carregadas. Média: {java_mean_overall:.2f}")
    elif Path(args.java_results).exists():
        print(f"\nCarregando resultados Java (CSV) de '{args.java_results}'...")
        java_map = load_java_csv(args.java_results)
        if java_map:
            java_mean_overall = np.mean([v["mean"] for v in java_map.values()])
            print(f"  {len(java_map)} instâncias Java carregadas. Média: {java_mean_overall:.2f}")
    else:
        print(f"\n[INFO] Nenhum arquivo Java encontrado.")
        if java_mean_overall > 0:
            print(f"       Usando média Java = {java_mean_overall:.1f} como fallback (arg --java-mean).")

    # Criar diretório de saída
    Path(args.output_dir).mkdir(parents=True, exist_ok=True)
    print(f"\nGerando gráficos em '{args.output_dir}/'...")

    fig1_distributions(records, java_map, java_mean_overall, args.output_dir)
    fig2_per_instance(records, java_map, args.output_dir)
    fig3_summary(records, java_map, java_mean_overall, args.output_dir)
    if java_map:
        fig4_java_vs_cpp(records, java_map, args.output_dir)

    print("\nPronto! Abra os arquivos PNG em data/charts/")


if __name__ == "__main__":
    main()
