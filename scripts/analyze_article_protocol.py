#!/usr/bin/env python3
"""Build the article-protocol comparison table from the two raw runs."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
JAVA = ROOT / "data/java_results_raw.txt"
CPP = ROOT / "data/article_protocol_current_gls.txt"
OUT = ROOT / "data/experiments/article_protocol_comparison.md"

row_re = re.compile(
    r"^Instance_10_10_(\d+)\s+(\d+)\s+([0-9.]+)\s+"
    r"([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)"
)


def read_rows(path):
    rows = {}
    for line in path.read_text().splitlines():
        match = row_re.match(line.strip())
        if match:
            iid, optimum, exact_time, mean_cost, best_cost, mean_time = match.groups()
            rows[int(iid)] = {
                "optimum": int(optimum),
                "exact_time": float(exact_time),
                "mean_cost": float(mean_cost),
                "best_cost": float(best_cost),
                "mean_time": float(mean_time),
            }
    return rows


java = read_rows(JAVA)
cpp = read_rows(CPP)
ids = sorted(java)
if len(java) != 94 or len(cpp) != 94 or ids != sorted(cpp):
    raise SystemExit(f"Expected 94 rows in both outputs: Java={len(java)}, C++={len(cpp)}")


def avg(key, source):
    return sum(source[i][key] for i in ids) / len(ids)


def gap(cost, optimum):
    return 100.0 * (cost - optimum) / optimum


def reached(source):
    return sum(source[i]["best_cost"] <= source[i]["optimum"] for i in ids)


lines = [
    "# Comparação sob o protocolo do artigo SSUU",
    "",
    "Execução reproduzida em 19/09/2026. Instâncias: 94; repetições: 3; "
    "alpha=0,4; Ps; 2.000/10.000 iterações conforme o artigo; "
    "orçamento por repetição: tempo exato/20 para ótimos < 2 s e tempo exato/10 nos demais.",
    "",
    "| Método | custo médio | GAP médio | melhor custo médio por instância | instâncias que atingiram o ótimo | tempo médio até melhor (s) |",
    "|---|---:|---:|---:|---:|---:|",
]
for label, source in (("Java — Service-Selection-Under-Uncertainty", java),
                      ("C++ — versão atual + GLS", cpp)):
    mean_cost = avg("mean_cost", source)
    mean_gap = sum(gap(source[i]["mean_cost"], source[i]["optimum"]) for i in ids) / len(ids)
    lines.append(f"| {label} | {mean_cost:.2f} | {mean_gap:.2f}% | {avg('best_cost', source):.2f} | {reached(source)}/94 | {avg('mean_time', source):.3f} |")

lines += [
    "",
    "## Tabela por instância",
    "",
    "| Instância | ótimo | tempo exato (s) | Java médio | Java melhor | Java t(s) | C++ médio | C++ melhor | C++ t(s) |",
    "|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
]
for iid in ids:
    j, c = java[iid], cpp[iid]
    lines.append(
        f"| {iid} | {j['optimum']} | {j['exact_time']:.2f} | {j['mean_cost']:.2f} | {j['best_cost']:.0f} | {j['mean_time']:.3f} | "
        f"{c['mean_cost']:.2f} | {c['best_cost']:.0f} | {c['mean_time']:.3f} |"
    )

OUT.write_text("\n".join(lines) + "\n")
print(f"Wrote {OUT}")
print(f"Java: mean={avg('mean_cost', java):.2f}, gap={sum(gap(java[i]['mean_cost'], java[i]['optimum']) for i in ids)/94:.2f}%, reached={reached(java)}/94, time={avg('mean_time', java):.3f}s")
print(f"C++ : mean={avg('mean_cost', cpp):.2f}, gap={sum(gap(cpp[i]['mean_cost'], cpp[i]['optimum']) for i in ids)/94:.2f}%, reached={reached(cpp)}/94, time={avg('mean_time', cpp):.3f}s")

