"""Resume a varredura de alpha da GLS promovida nas 94 instâncias.

Uso: python3 scripts/summarize_alpha_sweep.py [diretório]
Lê alpha_<valor>.csv e escreve summary.md no mesmo diretório.
"""
import csv
import sys
from collections import defaultdict
from pathlib import Path

directory = Path(sys.argv[1] if len(sys.argv) > 1 else "data/experiments/alpha_sweep_all94")
REFERENCE = "0.3"

# results[budget][alpha][instance] = (cost, optimum, time_to_best_s)
results = defaultdict(lambda: defaultdict(dict))
for path in sorted(directory.glob("alpha_*.csv")):
    alpha = path.stem.removeprefix("alpha_")
    with path.open() as file:
        for row in csv.DictReader(file):
            budget = float(row["budget_seconds"])
            results[budget][alpha][int(row["instance"])] = (
                float(row["cost"]), int(row["optimum"]),
                float(row["time_to_best_ms"]) / 1000.0)

lines = ["# Varredura de alpha — GLS promovida (I1 S1 L0 R1), 94 instâncias", "",
         "GLS determinística: 1 execução por instância, alpha e orçamento. "
         f"Comparações contra o alpha promovido ({REFERENCE}).", ""]
for budget in sorted(results):
    by_alpha = results[budget]
    alphas = sorted(by_alpha, key=float)
    lines += [f"## Orçamento {budget:g} s", "",
              "| alpha | ótimos | GAP médio (%) | GAP máx (%) | soma dos custos | "
              f"melhor que {REFERENCE} | pior que {REFERENCE} |",
              "|---|---|---|---|---|---|---|"]
    reference = by_alpha.get(REFERENCE, {})
    for alpha in alphas:
        data = by_alpha[alpha]
        gaps = [100.0 * (cost - opt) / opt for cost, opt, _ in data.values()]
        better = sum(1 for i, (c, _, _) in data.items() if i in reference and c < reference[i][0])
        worse = sum(1 for i, (c, _, _) in data.items() if i in reference and c > reference[i][0])
        hits = sum(1 for cost, opt, _ in data.values() if cost <= opt)
        lines.append(
            f"| {alpha} | {hits}/{len(data)} | {sum(gaps) / len(gaps):.3f} | {max(gaps):.2f} | "
            f"{sum(c for c, _, _ in data.values()):.0f} | {better} | {worse} |")
    # Instâncias em que algum alpha não chega ao ótimo.
    instances = sorted({i for data in by_alpha.values() for i, (c, o, _) in data.items() if c > o})
    if instances:
        lines += ["", "Instâncias em que algum alpha não chega ao ótimo (custo):", "",
                  "| instância | ótimo | " + " | ".join(alphas) + " |",
                  "|---|---|" + "---|" * len(alphas)]
        for i in instances:
            optimum = next(data[i][1] for data in by_alpha.values() if i in data)
            cells = []
            for alpha in alphas:
                cost = by_alpha[alpha].get(i, (float("nan"),))[0]
                best = min(by_alpha[a][i][0] for a in alphas if i in by_alpha[a])
                text = f"{cost:.0f}"
                cells.append(f"**{text}**" if cost == best else text)
            lines.append(f"| {i} | {optimum} | " + " | ".join(cells) + " |")
    lines.append("")

(directory / "summary.md").write_text("\n".join(lines))
print("\n".join(lines))
