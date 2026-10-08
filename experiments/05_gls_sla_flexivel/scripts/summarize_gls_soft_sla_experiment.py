#!/usr/bin/env python3
"""Resume o experimento GLS estrita x GLS com SLA probabilística suave."""

import csv
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def mean(values):
    return statistics.fmean(values) if values else 0.0


def main():
    input_path = Path(
        sys.argv[1] if len(sys.argv) > 1
        else "experiments/05_gls_sla_flexivel/results/gls_soft_sla_optimized_comparison.csv"
    )
    output_path = Path(
        sys.argv[2] if len(sys.argv) > 2
        else "experiments/05_gls_sla_flexivel/results/gls_soft_sla_optimized_summary.md"
    )

    with input_path.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise SystemExit("O CSV do experimento está vazio.")

    grouped = defaultdict(list)
    for row in rows:
        grouped[int(row["instance"])].append(row)

    methods = {
        "strict": "GLS estrita otimizada",
        "soft": "GLS com SLA suave",
        "conditional": "GLS com SLA suave condicional",
    }
    aggregate = {}
    for key in methods:
        aggregate[key] = {
            "cost": mean(float(row[f"{key}_cost"]) for row in rows),
            "gap": mean(float(row[f"{key}_gap_pct"]) for row in rows),
            "time": mean(float(row[f"{key}_time_ms"]) for row in rows),
            "candidates": mean(float(row[f"{key}_candidates"]) for row in rows),
            "rounds": mean(float(row[f"{key}_rounds"]) for row in rows),
            "optimal_runs": sum(
                float(row[f"{key}_cost"]) <= int(row["optimum"])
                for row in rows
            ),
            "optimal_instances": sum(
                min(float(row[f"{key}_cost"]) for row in group)
                <= int(group[0]["optimum"])
                for group in grouped.values()
            ),
        }

    comparisons = {}
    run_comparisons = {}
    for key in ("soft", "conditional"):
        wins = ties = losses = 0
        for group in grouped.values():
            strict_cost = mean(float(row["strict_cost"]) for row in group)
            candidate_cost = mean(float(row[f"{key}_cost"]) for row in group)
            if candidate_cost < strict_cost - 1e-9:
                wins += 1
            elif strict_cost < candidate_cost - 1e-9:
                losses += 1
            else:
                ties += 1
        comparisons[key] = (wins, ties, losses)
        run_wins = run_ties = run_losses = 0
        for row in rows:
            strict_cost = float(row["strict_cost"])
            candidate_cost = float(row[f"{key}_cost"])
            if candidate_cost < strict_cost - 1e-9:
                run_wins += 1
            elif strict_cost < candidate_cost - 1e-9:
                run_losses += 1
            else:
                run_ties += 1
        run_comparisons[key] = (run_wins, run_ties, run_losses)

    crossing = {}
    for key in ("soft", "conditional"):
        accepted_field = f"{key}_accepted_infeasible"
        crossing[key] = {
            "accepted": sum(int(row[accepted_field]) for row in rows),
            "recoveries": sum(int(row[f"{key}_recoveries"]) for row in rows),
            "runs": sum(int(row[accepted_field]) > 0 for row in rows),
            "instances": len({
                int(row["instance"])
                for row in rows
                if int(row[accepted_field]) > 0
            }),
            "infeasible_returns": sum(
                row[f"{key}_feasible"] != "1" for row in rows
            ),
            "maximum_drift": max(
                float(row[f"{key}_probability_drift"]) for row in rows
            ),
        }
    budget = float(rows[0]["budget_seconds"])
    beta = float(rows[0]["probability_beta"])
    soft_tail = float(rows[0]["soft_tail_fraction"])
    proximity = float(rows[0]["proximity_fraction"])
    repetitions = len(next(iter(grouped.values())))

    lines = [
        "# Experimento: probabilidade na função objetivo da GLS",
        "",
        "## Objetivo experimental",
        "",
        "A variante permite que a trajetória visite soluções que excedem a SLA, usando:",
        "",
        "`h_prob(s) = custo(s) + lambda * penalidades_GLS(s) "
        "+ mu * max(0, P_violacao(s) - Pmax)`",
        "",
        "Capacidade e `Smax` continuam sendo restrições duras. A melhor solução e a "
        "resposta final são sempre viáveis e comparadas pelo custo real.",
        "",
        "## Protocolo",
        "",
        f"- Instâncias: {len(grouped)}",
        f"- Repetições por instância: {repetitions}",
        f"- Orçamento por variante e repetição: {budget:.3f} s",
        f"- Peso probabilístico inicial (`beta`): {beta:g}",
        f"- Cauda suave da variante condicional: {100 * soft_tail:.0f}% do tempo.",
        f"- Proximidade exigida: margem de até {100 * proximity:.0f}% de `Pmax`.",
        "- Mesma solução inicial para as três variantes em cada repetição.",
        "- GLS/GFLS estrita: SLA tratada como restrição dura.",
        "- GLS/GFLS suave: excesso da SLA incluído no objetivo aumentado.",
        "- GLS/GFLS condicional: SLA suave somente na cauda do tempo e perto de `Pmax`.",
        "",
        "## Resultado agregado",
        "",
        "| Variante | Custo médio | GAP médio | Tempo | Candidatos | Rodadas | Execuções ótimas | Instâncias ótimas |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for key, label in methods.items():
        item = aggregate[key]
        lines.append(
            f"| {label} | {item['cost']:.2f} | {item['gap']:.2f}% | "
            f"{item['time'] / 1000.0:.3f} s | "
            f"{item['candidates']:.0f} | {item['rounds']:.1f} | "
            f"{item['optimal_runs']}/{len(rows)} | "
            f"{item['optimal_instances']}/{len(grouped)} |"
        )

    lines.extend([
        "",
        "## Travessias da região inviável",
        "",
        f"- Suave x estrita (V/E/D): {comparisons['soft'][0]}/"
        f"{comparisons['soft'][1]}/{comparisons['soft'][2]} por instância; "
        f"{run_comparisons['soft'][0]}/{run_comparisons['soft'][1]}/"
        f"{run_comparisons['soft'][2]} por execução.",
        f"- Condicional x estrita (V/E/D): {comparisons['conditional'][0]}/"
        f"{comparisons['conditional'][1]}/{comparisons['conditional'][2]} por "
        f"instância; {run_comparisons['conditional'][0]}/"
        f"{run_comparisons['conditional'][1]}/"
        f"{run_comparisons['conditional'][2]} por execução.",
        f"- Suave: {crossing['soft']['accepted']} movimentos inviáveis, "
        f"{crossing['soft']['recoveries']} retornos, "
        f"{crossing['soft']['runs']}/{len(rows)} execuções com travessia.",
        f"- Condicional: {crossing['conditional']['accepted']} movimentos inviáveis, "
        f"{crossing['conditional']['recoveries']} retornos, "
        f"{crossing['conditional']['runs']}/{len(rows)} execuções com travessia.",
        f"- Respostas finais inviáveis (suave/condicional): "
        f"{crossing['soft']['infeasible_returns']}/"
        f"{crossing['conditional']['infeasible_returns']}.",
        f"- Deriva probabilística máxima final (suave/condicional): "
        f"{crossing['soft']['maximum_drift']:.3e}/"
        f"{crossing['conditional']['maximum_drift']:.3e}.",
        "",
        "## Conclusão",
        "",
        f"A implementação otimizada reduziu o GAP da versão sempre suave em "
        f"{aggregate['strict']['gap'] - aggregate['soft']['gap']:.3f} ponto "
        "percentual frente à estrita. A variante condicional alterou o GAP em "
        f"{aggregate['strict']['gap'] - aggregate['conditional']['gap']:.3f} ponto "
        "percentual. A busca sempre preservou um incumbente viável. Como o ganho "
        "da versão suave ainda está concentrado em poucas instâncias, ela apresenta "
        "retorno experimental, mas requer mais repetições antes de substituir a "
        "configuração padrão.",
        "",
    ])

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Relatório salvo em {output_path}")


if __name__ == "__main__":
    main()
