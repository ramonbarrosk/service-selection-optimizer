#!/usr/bin/env python3
"""Compara as variantes do ILS#3 com melhorias contra a GLS e o ILS#3 original.

Para cada conjunto (ref ou hard) e orçamento, lê os CSVs das variantes em results/
(ils3_<set>_<tempo>s_i?_s?_k?.csv) e os da GLS das fases 07 e 10, e escreve
results/summary_<set>_<tempo>s.md com:
  - ótimos alcançados, GAP médio, tempo até a melhor solução e soluções inviáveis;
  - vitórias/empates/derrotas por instância contra a GLS e o Wilcoxon pareado
    (exato, bilateral) sobre o custo médio por instância.

    python3 experiments/12_ils_melhorado/scripts/analyze_ils_variants.py
"""

import csv
import re
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[3]
RESULTS = ROOT / "experiments/12_ils_melhorado/results"
sys.path.insert(0, str(ROOT / "experiments/09_teste_estatistico/scripts"))
sys.path.insert(0, str(ROOT / "experiments/10_instancias_dificeis/scripts"))
from analyze_teste_estatistico import fmt_p, wilcoxon  # noqa: E402
from analyze_comparacao import log_cost, trivial_bound, TIMEOUT_LOGS  # noqa: E402

GLS = {
    ("ref", "0.55"): ROOT / "experiments/08_ablacao_gls/results/ablation_0.55s/i1_s1_l0_r1.csv",
    ("ref", "10"): ROOT / "experiments/07_protocolo_artigo/results/gls_promoted_article_protocol.csv",
    ("hard", "10"): ROOT / "experiments/10_instancias_dificeis/results/gls_hard.csv",
}
ORIGINAL = {
    ("hard", "10"): ROOT / "experiments/10_instancias_dificeis/results/ils3_hard.csv",
}
NAMES = {"i": "incremental", "s": "substituição", "k": "keepBest"}
PATTERN = re.compile(r"ils3_(ref|hard)_([\d.]+)s_i(\d)_s(\d)_k(\d)\.csv")


def read(path):
    runs = defaultdict(list)
    with path.open() as handle:
        for row in csv.DictReader(handle):
            runs[int(row["instance"])].append({
                "cost": float(row["cost"]),
                "optimum": float(row["optimum"]),
                "time": float(row.get("time_to_best_ms") or 0) / 1000.0,
                "feasible": row.get("feasible", "1") in ("1", "1.0"),
            })
    return runs


def label(i, s, k):
    parts = [NAMES[n] for n, v in (("i", i), ("s", s), ("k", k)) if v == "1"]
    return "ILS#3 + " + " + ".join(parts) if parts else "ILS#3 original"


def references(set_name, methods):
    """Ótimo do exato; senão LB alcançado; senão melhor solução conhecida."""
    any_method = next(iter(methods.values()))
    refs = {}
    for iid, runs in any_method.items():
        optimum = runs[0]["optimum"]
        if set_name == "ref":
            refs[iid] = (optimum, True)
            continue
        exact = log_cost(ROOT / "data/Log_hard", iid)
        if exact is not None:
            refs[iid] = (exact, True)
            continue
        lb = trivial_bound(iid)
        costs = [r["cost"] for m in methods.values() for r in m.get(iid, []) if r["feasible"]]
        upper = log_cost(TIMEOUT_LOGS, iid)
        best = min(costs + ([upper] if upper is not None else []))
        refs[iid] = (lb, True) if best <= lb else (best, False)
    return refs


def summarize(set_name, budget, methods):
    refs = references(set_name, methods)
    gls = methods.get("GLS")
    lines = [f"# ILS#3 com melhorias × GLS — conjunto {set_name}, {budget} s", "",
             "Ótimos: instâncias com ótimo provado alcançado em ao menos uma execução. "
             "GAP: custo médio contra a referência (ótimo, LB alcançado ou melhor solução "
             "conhecida entre todos os métodos). Inviáveis: execuções cuja solução reportada "
             "viola alguma restrição. Vitórias: instâncias em que o método teve custo médio "
             "menor que a GLS. p: Wilcoxon pareado exato e bilateral sobre o custo médio por instância.",
             "",
             "| Método | Ótimos | GAP médio | Tempo até a melhor (s) | Inviáveis | Melhor que a GLS | Empates | Pior que a GLS | p |",
             "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    proven = sum(1 for ref, ok in refs.values() if ok)
    for name, runs in methods.items():
        ids = sorted(set(runs) & set(refs))
        hits = sum(1 for i in ids if refs[i][1] and any(r["cost"] <= refs[i][0] for r in runs[i]))
        gap = mean(mean(100 * (r["cost"] - refs[i][0]) / refs[i][0] for r in runs[i]) for i in ids)
        ttb = mean(r["time"] for i in ids for r in runs[i])
        infeasible = sum(not r["feasible"] for i in ids for r in runs[i])
        total = sum(len(runs[i]) for i in ids)
        if name == "GLS" or gls is None:
            cmp = "| – | – | – | – |"
        else:
            common = [i for i in ids if i in gls]
            diffs = [mean(r["cost"] for r in runs[i]) - mean(r["cost"] for r in gls[i]) for i in common]
            diffs = [0.0 if abs(d) < 1e-9 else d for d in diffs]
            p = wilcoxon(diffs)[0]
            cmp = (f"| {sum(d < 0 for d in diffs)} | {sum(d == 0 for d in diffs)} | "
                   f"{sum(d > 0 for d in diffs)} | {fmt_p(p)} |")
        lines.append(f"| {name} | {hits}/{proven} | {gap:.2f}% | {ttb:.2f} | {infeasible}/{total} {cmp}"
                     .replace(".", ","))
    return "\n".join(lines) + "\n"


def main():
    groups = defaultdict(dict)
    for path in sorted(RESULTS.glob("ils3_*.csv")):
        match = PATTERN.fullmatch(path.name)
        if match:
            set_name, budget, i, s, k = match.groups()
            groups[(set_name, budget)][label(i, s, k)] = read(path)
    for key, variants in sorted(groups.items()):
        methods = {}
        if key in GLS and GLS[key].exists():
            methods["GLS"] = read(GLS[key])
        if key in ORIGINAL and ORIGINAL[key].exists() and "ILS#3 original" not in variants:
            methods["ILS#3 original (fase anterior)"] = read(ORIGINAL[key])
        methods.update(variants)
        text = summarize(key[0], key[1], methods)
        out = RESULTS / f"summary_{key[0]}_{key[1]}s.md"
        out.write_text(text)
        print(text)


if __name__ == "__main__":
    main()
