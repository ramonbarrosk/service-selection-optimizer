"""Teste de Mann-Whitney (bilateral, aproximação normal com correção de empates e de
continuidade) entre Java e C++ na validação com o mesmo número de iterações.

    python3 experiments/14_ils_cpp/scripts/validacao_mw.py [sufixo]   # ex.: _i1s1
"""
import csv
import math
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

DIR = Path(__file__).resolve().parents[1] / "results/validacao"


def mann_whitney(x, y):
    values = sorted((v, g) for g, s in ((0, x), (1, y)) for v in s)
    ranks, i = [], 0
    ties = 0.0
    while i < len(values):
        j = i
        while j < len(values) and values[j][0] == values[i][0]:
            j += 1
        r = (i + j + 1) / 2
        ranks += [(r, values[k][1]) for k in range(i, j)]
        t = j - i
        ties += t ** 3 - t
        i = j
    n1, n2 = len(x), len(y)
    u = sum(r for r, g in ranks if g == 0) - n1 * (n1 + 1) / 2
    n = n1 + n2
    var = n1 * n2 / 12 * ((n + 1) - ties / (n * (n - 1)))
    if var <= 0:
        return 1.0
    z = (abs(u - n1 * n2 / 2) - 0.5) / math.sqrt(var)
    return min(1.0, math.erfc(max(z, 0) / math.sqrt(2)))


def load(name):
    d = defaultdict(list)
    for r in csv.DictReader(open(DIR / name)):
        d[int(r["instance"])].append((float(r["cost"]), float(r["elapsed_ms"]) / 1000, r["feasible"]))
    return d


tag = sys.argv[1] if len(sys.argv) > 1 else ""
j, c = load(f"java{tag}.csv"), load(f"cpp{tag}.csv")
print("inst | Java custo | C++ custo | p | Java t | C++ t | razão | inviáveis")
for i in sorted(j):
    jc, cc = [x[0] for x in j[i]], [x[0] for x in c[i]]
    jt, ct = mean(x[1] for x in j[i]), mean(x[1] for x in c[i])
    inf = sum(x[2] not in ("1", "1.0") for x in j[i] + c[i])
    print(f"{i} | {mean(jc):.2f} | {mean(cc):.2f} | {mann_whitney(jc, cc):.2f} | "
          f"{jt:.2f} | {ct:.2f} | {jt / ct:.1f} | {inf}")
