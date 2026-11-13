#!/usr/bin/env python3
"""Comparação ILS#3 (Java) x GLS (C++) nas instâncias geradas em data/instances_hard.

A referência de cada instância é, nesta ordem:
  - o ótimo do método exato (log com "Status = Optimal" em data/Log_hard);
  - o limite inferior trivial (LB), quando algum método o alcança: o ótimo está provado;
  - senão, a melhor solução conhecida (BKS) entre as repetições dos dois métodos e a
    solução do CPLEX interrompida pelo limite de tempo (data/Log_hard_timeout).
O GAP de cada método é medido contra essa referência.

Também resume o CPLEX (cplex_log.py): status, tempo total, tempo até a última
solução e gap final entre a solução e o limite inferior.

Produz, por perfil e no total: instâncias com ótimo provado por método, GAP médio,
vitórias por custo médio e o teste de Wilcoxon pareado sobre o custo médio por
instância (o mesmo da fase 09). Grava um resumo em Markdown com a tabela por instância.

    python3 experiments/10_instancias_dificeis/scripts/analyze_comparacao.py
"""

import csv
import re
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experiments/09_teste_estatistico/scripts"))
from analyze_teste_estatistico import br, fmt_p, read_runs, wilcoxon  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cplex_log import load_cplex  # noqa: E402

RESULTS = ROOT / "experiments/10_instancias_dificeis/results"
EXACT_LOGS = ROOT / "data/Log_hard"
TIMEOUT_LOGS = ROOT / "data/Log_hard_timeout"
MANIFEST = RESULTS / "manifest.csv"
ILS_CSV = RESULTS / "ils3_hard.csv"
GLS_CSV = RESULTS / "gls_hard.csv"
SUMMARY = RESULTS / "summary.md"


def read_manifest():
    with MANIFEST.open() as handle:
        return {int(row["id"]): row for row in csv.DictReader(handle)}


def log_cost(folder, iid):
    """Custo na última linha do log do CPLEX ("... Cost = <custo>"), ou None."""
    path = folder / f"Instance_10_10_{iid}"
    if not path.exists():
        return None
    lines = [line for line in path.read_text(errors="ignore").splitlines() if line.strip()]
    match = re.search(r"Cost = (\d+)", lines[-1]) if lines else None
    return int(match.group(1)) if match else None


def trivial_bound(iid):
    """Soma do menor custo de cada tarefa, lida do arquivo da instância."""
    costs = defaultdict(list)
    path = ROOT / f"data/instances_hard/Instance_10_10_{iid}"
    for line in path.read_text().splitlines():
        tokens = line.split()
        if tokens and tokens[0] == "c":
            costs[tokens[2]].append(int(tokens[3]))
    return sum(min(values) for values in costs.values())


def analyze():
    manifest = read_manifest()
    ils, _ = read_runs(ILS_CSV)
    gls, _ = read_runs(GLS_CSV)
    rows = []
    for iid in sorted(set(ils) & set(gls)):
        ils_costs = [r["cost"] for r in ils[iid]]
        gls_costs = [r["cost"] for r in gls[iid]]
        lb = trivial_bound(iid)
        exact = log_cost(EXACT_LOGS, iid)
        upper = log_cost(TIMEOUT_LOGS, iid)
        best = min(ils_costs + gls_costs + ([upper] if upper is not None else []))
        if exact is not None:
            reference, source = exact, "exato"
        elif best <= lb:
            reference, source = lb, "LB"
        else:
            reference, source = best, "BKS"
        rows.append({
            "id": iid,
            "profile": manifest[iid]["profile"],
            "lb": lb,
            "reference": reference,
            "proven": source != "BKS",
            "source": source,
            "gls_mean": mean(gls_costs),
            "ils_mean": mean(ils_costs),
            "gls_gap": mean(100 * (c - reference) / reference for c in gls_costs),
            "ils_gap": mean(100 * (c - reference) / reference for c in ils_costs),
            "gls_hits": sum(c <= reference for c in gls_costs),
            "ils_hits": sum(c <= reference for c in ils_costs),
            "reps": len(gls_costs),
            "cplex": load_cplex(iid),
        })
    return rows


def group_summary(rows):
    test = wilcoxon([r["ils_mean"] - r["gls_mean"] for r in rows])
    return {
        "n": len(rows),
        "proven": sum(r["proven"] for r in rows),
        "gls_proven": sum(r["proven"] and r["gls_hits"] > 0 for r in rows),
        "ils_proven": sum(r["proven"] and r["ils_hits"] > 0 for r in rows),
        "gls_gap": mean(r["gls_gap"] for r in rows),
        "ils_gap": mean(r["ils_gap"] for r in rows),
        "gls_wins": sum(r["gls_mean"] < r["ils_mean"] for r in rows),
        "ils_wins": sum(r["gls_mean"] > r["ils_mean"] for r in rows),
        "p": test[0],
        "cplex_optimal": sum(r["cplex"]["status"] == "Optimal" for r in rows),
        "cplex_time": mean(r["cplex"]["total_s"] for r in rows),
        "cplex_max_time": max(r["cplex"]["total_s"] for r in rows),
    }


def write_summary(rows):
    profiles = list(dict.fromkeys(r["profile"] for r in rows))
    groups = [(p, group_summary([r for r in rows if r["profile"] == p])) for p in profiles]
    groups.append(("Total", group_summary(rows)))
    reps = rows[0]["reps"]

    lines = [
        "# ILS#3 x GLS nas instâncias difíceis geradas",
        "",
        f"Instâncias: {len(rows)}; repetições por método: {reps}. Referência: ótimo do método "
        "exato (CPLEX, 15 min); sem ele, o limite inferior trivial quando algum método o "
        "alcança, senão a melhor solução "
        "conhecida entre os dois métodos e o CPLEX interrompido. Ótimo provado: pelo método "
        "exato ou por algum método alcançar o limite inferior. Wilcoxon pareado, exato e "
        "bilateral, sobre o "
        "custo médio por instância.",
        "",
        "| Perfil | Instâncias | CPLEX provou | Tempo médio CPLEX | Ótimo provado "
        "| GLS no ótimo provado | ILS#3 no ótimo provado "
        "| GAP GLS | GAP ILS#3 | GLS melhor | ILS#3 melhor | p-valor |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for name, g in groups:
        lines.append(
            f"| {name} | {g['n']} | {g['cplex_optimal']} | {br(g['cplex_time'], 1)} s "
            f"| {g['proven']} | {g['gls_proven']} | {g['ils_proven']} "
            f"| {br(g['gls_gap'])}% | {br(g['ils_gap'])}% | {g['gls_wins']} | {g['ils_wins']} "
            f"| {fmt_p(g['p'])} |")
    lines += [
        "",
        "## Por instância",
        "",
        "| Instância | Perfil | LB | Referência | Fonte | CPLEX status | CPLEX custo "
        "| CPLEX limite inf. | CPLEX gap | CPLEX tempo total | CPLEX última solução "
        "| GLS | ILS#3 | Acertos GLS | Acertos ILS#3 |",
        "|---:|---|---:|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for r in rows:
        reference = f"**{r['reference']:.0f}**" if r["proven"] else f"{r['reference']:.0f}"
        c = r["cplex"]
        lines.append(
            f"| {r['id']} | {r['profile']} | {r['lb']} | {reference} | {r['source']} "
            f"| {c['status']} | {c['cost']:.0f} | {br(c['bound'], 1)} | {br(c['gap_pct'])}% "
            f"| {br(c['total_s'], 1)} s | ≤ {br(c['last_incumbent_s'], 1)} s "
            f"| {br(r['gls_mean'])} "
            f"| {br(r['ils_mean'])} | {r['gls_hits']}/{reps} | {r['ils_hits']}/{reps} |")
    lines += [
        "",
        "Referência em negrito: ótimo provado. Fonte: exato (CPLEX), LB (limite inferior "
        "alcançado) ou BKS (melhor solução conhecida). GLS e ILS#3: custo "
        "médio das repetições. Acertos: repetições que alcançaram a referência.",
        "",
        "CPLEX: Optimal = ótimo provado (limite inferior igual ao custo da solução); "
        "Feasible = parou nos 15 min com uma solução viável cuja otimalidade não foi provada. "
        "Gap = (custo − limite inferior) / custo. Última solução: instante da última melhoria "
        "da solução, estimado pela linha \"Elapsed time\" seguinte do log (limite superior; "
        "o CPLEX não registra o tempo exato de cada solução).",
    ]
    SUMMARY.write_text("\n".join(lines) + "\n")


def main():
    rows = analyze()
    write_summary(rows)
    print(SUMMARY.read_text())


if __name__ == "__main__":
    main()
