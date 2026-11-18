#!/usr/bin/env python3
"""Números do capítulo de resultados a partir da bateria da fase 13.

GLS calibrada pelo irace (3 execuções) e ILS#3 com 20 execuções, todos com a
construção inicial dentro do tempo. Lê results/<job>.csv (merge_results.py) e grava
results/summary.md, com uma seção por tabela do capítulo.

Referência nas instâncias geradas (a mesma para todos os métodos e tabelas):
ótimo do CPLEX; senão o LB trivial, se algum método o alcançou; senão a melhor
solução conhecida entre todas as execuções da bateria e a solução do CPLEX
interrompida.

    python3 experiments/13_calibrada_20exec/scripts/analyze.py
"""

import csv
import os
import re
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[3]
RESULTS = Path(os.environ.get("SSO_RESULTS", ROOT / "experiments/13_calibrada_20exec/results"))
sys.path.insert(0, str(ROOT / "experiments/09_teste_estatistico/scripts"))
sys.path.insert(0, str(ROOT / "experiments/10_instancias_dificeis/scripts"))
from analyze_teste_estatistico import fmt_p, wilcoxon  # noqa: E402
from analyze_comparacao import TIMEOUT_LOGS, log_cost, trivial_bound  # noqa: E402
from cplex_log import load_cplex  # noqa: E402

PROFILES = {"11": "T97", "12": "T98", "13": "T99", "14": "T100", "15": "C50"}
ILS_NAMES = {"000": "ILS original", "001": "ILS + C", "100": "ILS + I", "010": "ILS + S",
             "110": "ILS + I + S", "111": "ILS + I + S + C"}
OUT = []
# A GLS é determinística: o capítulo usa só a 1ª execução (SSO_GLS_RUNS muda isso).
GLS_RUNS = int(os.environ.get("SSO_GLS_RUNS", "1"))


def emit(line=""):
    OUT.append(line)


def br(x, d=2):
    return f"{x:.{d}f}".replace(".", ",")


def read(name):
    path = RESULTS / f"{name}.csv"
    if not path.exists():
        return None
    runs = defaultdict(list)
    with path.open() as handle:
        for row in csv.DictReader(handle):
            if (name.startswith(("gls_", "abl_")) and "repetition" in row
                    and int(row["repetition"]) > GLS_RUNS):
                continue
            optimum = float(row["optimum"])
            cost = float(row["cost"])
            elapsed = float(row.get("elapsed_ms") or row.get("time_ms") or 0) / 1000
            ttb = float(row.get("time_to_best_ms") or 0) / 1000
            runs[int(row["instance"])].append({
                "cost": cost, "optimum": optimum, "elapsed": elapsed,
                # Sem ótimo, o tempo até a melhor é o tempo total (protocolo do artigo).
                "time": ttb if cost <= optimum else elapsed,
                "feasible": row.get("feasible", "1") in ("1", "1.0", "true"),
                "candidates": float(row.get("candidates") or 0),
                "rounds": float(row.get("rounds") or 0),
                "rej_cap": float(row.get("rejected_capacity") or 0),
                "rej_sla": float(row.get("rejected_sla") or 0),
                "rej_smax": float(row.get("rejected_smax") or 0),
            })
    return dict(runs)


def mcost(runs, i):
    return mean(r["cost"] for r in runs[i])


def ref_exec_time(iid):
    for line in (ROOT / f"data/Log/Instance_10_10_{iid}").read_text(errors="ignore").splitlines():
        if line.startswith("Total") and "=" in line:
            return float(line.split("=")[1].split("sec")[0])
    return None


def compare(a, b, ids):
    """Diferenças de custo médio a - b; (melhor, igual, pior) de a e Wilcoxon."""
    diffs = [mcost(a, i) - mcost(b, i) for i in ids]
    diffs = [0.0 if abs(d) < 1e-9 else d for d in diffs]
    p, n, wp, wm = wilcoxon(diffs)
    return (sum(d < 0 for d in diffs), sum(d == 0 for d in diffs), sum(d > 0 for d in diffs),
            p, n, wp, wm)


def cmp_text(a, b, ids):
    better, equal, worse, p, n, wp, wm = compare(a, b, ids)
    return (f"melhor {better} / igual {equal} / pior {worse}; Wilcoxon n={n}, "
            f"W+={wp:g}, W-={wm:g}, p={fmt_p(p) if p is not None else '–'}")


def stats(runs, ids, refs):
    proven = [i for i in ids if refs[i][1]]
    hits = sum(1 for i in proven if any(r["cost"] <= refs[i][0] for r in runs[i]))
    gap = mean(mean(100 * (r["cost"] - refs[i][0]) / refs[i][0] for r in runs[i]) for i in ids)
    cost = mean(mcost(runs, i) for i in ids)
    ttb = mean(mean(r["time"] for r in runs[i]) for i in ids)
    hit_runs = sum(r["cost"] <= refs[i][0] for i in proven for r in runs[i])
    infeasible = sum(not r["feasible"] for i in ids for r in runs[i])
    total = sum(len(runs[i]) for i in ids)
    return {"hits": hits, "proven": len(proven), "gap": gap, "cost": cost, "ttb": ttb,
            "hit_runs": hit_runs, "runs": total, "infeasible": infeasible}


def row(name, s):
    return (f"| {name} | {s['hits']}/{s['proven']} | {br(s['cost'])} | {br(s['gap'])} | "
            f"{br(s['ttb'])} | {s['hit_runs']}/{s['runs']} | {s['infeasible']} |")


HEADER = ("| Método | Ótimos | Custo médio | Gap (%) | t (s) | Execuções no ótimo | Inviáveis |\n"
          "|---|---:|---:|---:|---:|---:|---:|")


def hard_refs(methods):
    ids = sorted(set.intersection(*(set(m) for m in methods.values())))
    refs = {}
    for i in ids:
        exact = log_cost(ROOT / "data/Log_hard", i)
        if exact is not None:
            refs[i] = (exact, True)
            continue
        lb = trivial_bound(i)
        costs = [r["cost"] for m in methods.values() for r in m[i] if r["feasible"]]
        upper = log_cost(TIMEOUT_LOGS, i)
        best = min(costs + ([upper] if upper is not None else []))
        refs[i] = (lb, True) if best <= lb else (best, False)
    return ids, refs


def section_reference(gls, ils):
    emit("## 7.1 Comparação nas 94 instâncias de referência (Tabela 7.1)")
    for budget in ("0.55s", "10s"):
        g, o = gls[budget], ils[budget]["000"]
        ids = sorted(set(g) & set(o))
        refs = {i: (g[i][0]["optimum"], True) for i in ids}
        emit(f"\n### {budget}\n\n{HEADER}")
        emit(row("ILS original (20 exec.)", stats(o, ids, refs)))
        emit(row("GLS calibrada (3 exec.)", stats(g, ids, refs)))
        emit(f"\nGLS contra ILS: {cmp_text(g, o, ids)}")
    g55, o10 = gls["0.55s"], ils["10s"]["000"]
    ids = sorted(set(g55) & set(o10))
    emit(f"\nCruzada GLS 0,55 s contra ILS 10 s: {cmp_text(g55, o10, ids)}")

    g, o = gls["10s"], ils["10s"]["000"]
    slow = [i for i in ids if ref_exec_time(i) > 2]
    fast = [i for i in ids if i not in slow]
    refs = {i: (g[i][0]["optimum"], True) for i in ids}
    emit("\n### Instâncias demoradas, 10 s (Tabela 7.2)\n")
    emit("| Inst. | Ótimo | Exato (s) | ILS custo | ILS t (s) | ILS exec. no ótimo | GLS custo | GLS t (s) |")
    emit("|---:|---:|---:|---:|---:|---:|---:|---:|")
    for i in slow:
        opt = refs[i][0]
        emit(f"| {i} | {opt:g} | {br(ref_exec_time(i))} | {br(mcost(o, i))} | "
             f"{br(mean(r['time'] for r in o[i]))} | {sum(r['cost'] <= opt for r in o[i])}/{len(o[i])} | "
             f"{br(mcost(g, i))} | {br(mean(r['time'] for r in g[i]))} |")
    for label, sub in (("demoradas", slow), ("rápidas", fast)):
        so, sg = stats(o, sub, refs), stats(g, sub, refs)
        emit(f"\n{label} ({len(sub)}): ILS {so['hits']} ótimos, gap {br(so['gap'])}%, "
             f"t {br(so['ttb'])} s | GLS {sg['hits']} ótimos, gap {br(sg['gap'])}%, t {br(sg['ttb'])} s")
    gls_hit_t = [mean(r["time"] for r in g[i]) for i in slow if mcost(g, i) <= refs[i][0]]
    if gls_hit_t:
        emit(f"GLS nas demoradas em que alcança o ótimo: t médio {br(mean(gls_hit_t))} s; "
             f"sempre abaixo do exato: {all(mean(r['time'] for r in g[i]) < ref_exec_time(i) for i in slow if mcost(g, i) <= refs[i][0])}")
    exhausted = sum(1 for i in slow if any(r["cost"] > refs[i][0] for r in o[i]))
    emit(f"Demoradas em que alguma execução do ILS esgotou o orçamento: {exhausted}")

    emit("\n## 7.3 Instâncias de referência com diferença, 10 s (substitui a Tabela 7.6)\n")
    emit("| Inst. | GLS | ILS (20 exec.) | Diferença | ILS exec. com custo da GLS |")
    emit("|---:|---:|---:|---:|---:|")
    for i in ids:
        d = mcost(o, i) - mcost(g, i)
        if abs(d) > 1e-9:
            emit(f"| {i} | {br(mcost(g, i))} | {br(mcost(o, i))} | {br(d)} | "
                 f"{sum(r['cost'] <= mcost(g, i) for r in o[i])}/{len(o[i])} |")


def section_hard(gls, ils):
    methods = {"GLS": gls}
    methods.update({ILS_NAMES[v]: runs for v, runs in ils.items()})
    ids, refs = hard_refs(methods)
    o = ils["000"]
    emit("\n## 7.2 Instâncias geradas, 10 s (Tabela 7.3)\n")
    emit("| Perfil | Ótimo provado | GLS ótimos | ILS ótimos | GLS gap | ILS gap | GLS vitórias | ILS vitórias | p |")
    emit("|---|---:|---:|---:|---:|---:|---:|---:|---:|")
    for code, prof in list(PROFILES.items()) + [("", "Total")]:
        sub = [i for i in ids if str(i).startswith(code)]
        sg, so = stats(gls, sub, refs), stats(o, sub, refs)
        better, _, worse, p, *_ = compare(gls, o, sub)
        emit(f"| {prof} | {sg['proven']} | {sg['hits']} | {so['hits']} | {br(sg['gap'])} | "
             f"{br(so['gap'])} | {better} | {worse} | {fmt_p(p)} |")
    tight = [i for i in ids if not str(i).startswith("15")]
    lb_eq = sum(1 for i in tight if refs[i][1] and refs[i][0] == trivial_bound(i))
    emit(f"\nPerfis T: ótimo igual ao LB em {lb_eq} de {len(tight)}.")
    miss = [(i, mcost(gls, i) - refs[i][0]) for i in tight if mcost(gls, i) > refs[i][0]]
    emit(f"GLS fora do ótimo nos perfis T: {len(miss)} instâncias, distância "
         f"{min(d for _, d in miss) if miss else 0:g} a {max(d for _, d in miss) if miss else 0:g}.")
    big = [i for i in tight if mcost(o, i) > 250]
    emit(f"ILS original com custo médio > 250 nos perfis T: {big}")

    c50 = [i for i in ids if str(i).startswith("15")]
    proven = [i for i in c50 if refs[i][1]]
    emit(f"\nC50 com ótimo provado ({len(proven)}): GLS {stats(gls, proven, refs)['hits']} ótimos, "
         f"gap {br(stats(gls, proven, refs)['gap'])}% | ILS {stats(o, proven, refs)['hits']} ótimos, "
         f"gap {br(stats(o, proven, refs)['gap'])}%")
    emit("\n### C50 sem ótimo provado (Tabela 7.4)\n")
    emit("| Inst. | LB | Limite inf. CPLEX | CPLEX | ILS original | GLS | Referência (BKS) | GLS acima do lim. CPLEX |")
    emit("|---:|---:|---:|---:|---:|---:|---:|---:|")
    for i in c50:
        if refs[i][1]:
            continue
        cp = load_cplex(i) or {}
        bound = cp.get("bound")
        above = f"{br(100 * (mcost(gls, i) - bound) / bound, 1)}%" if bound else "–"
        emit(f"| {i} | {trivial_bound(i)} | {br(bound, 1) if bound else '–'} | "
             f"{log_cost(TIMEOUT_LOGS, i)} | {br(mcost(o, i))} | {br(mcost(gls, i))} | "
             f"{refs[i][0]:g} | {above} |")
    return ids, refs


def section_ablation(abl):
    emit("\n## 7.4 Ablação, 0,55 s (Tabelas 7.7 e 7.8)\n")
    emit("| Variante | Ótimos | Gap (%) | t (s) | Candidatos/s | Rodadas | Rejeições por Smax |")
    emit("|---|---:|---:|---:|---:|---:|---:|")
    order = ["gls", "i0_s0_l0_r0", "i1_s0_l0_r0", "i1_s1_l0_r0", "i1_s1_l0_r1", "i0_s1_l0_r1",
             "i1_s0_l0_r1", "i1_s1_l1_r1", "i1_s0_l1_r0"]
    order += sorted(k for k in abl if k not in order)
    for key in order:
        runs = abl.get(key)
        if not runs:
            continue
        ids = sorted(runs)
        refs = {i: (runs[i][0]["optimum"], True) for i in ids}
        s = stats(runs, ids, refs)
        all_runs = [r for i in ids for r in runs[i]]
        rate = sum(r["candidates"] for r in all_runs) / max(sum(r["elapsed"] for r in all_runs), 1e-9)
        rej = sum(r["rej_cap"] + r["rej_sla"] + r["rej_smax"] for r in all_runs)
        smax = 100 * sum(r["rej_smax"] for r in all_runs) / rej if rej else 0
        emit(f"| {key} | {s['hits']}/94 | {br(s['gap'])} | {br(s['ttb'])} | {rate:.2e} | "
             f"{mean(r['rounds'] for r in all_runs):.0f} | {br(smax, 0)}% |")


def section_ils(gls, ils, hard_ids, hard_refs_):
    emit("\n## 7.5 Componentes na ILS, 94 de referência, 0,55 s (Tabela 7.9)\n")
    g = gls["0.55s"]
    ids = sorted(g)
    refs = {i: (g[i][0]["optimum"], True) for i in ids}
    emit("| Método | Ótimos | Gap (%) | Pior / igual / melhor que a GLS | p |\n|---|---:|---:|---|---:|")
    for v in ("000", "001", "100", "010", "110", "111"):
        runs = ils["0.55s"].get(v)
        if not runs:
            continue
        s = stats(runs, ids, refs)
        better, equal, worse, p, *_ = compare(runs, g, ids)
        emit(f"| {ILS_NAMES[v]} | {s['hits']}/94 | {br(s['gap'])} | {worse} / {equal} / {better} | {fmt_p(p)} |")
    s = stats(g, ids, refs)
    emit(f"| GLS | {s['hits']}/94 | {br(s['gap'])} | – | – |")
    i55 = ils["0.55s"]
    if "001" in i55:
        emit(f"\nILS + C contra original: {cmp_text(i55['001'], i55['000'], ids)}")
    if "111" in i55 and "110" in i55:
        emit(f"ILS + I + S + C contra ILS + I + S: {cmp_text(i55['111'], i55['110'], ids)}")

    emit("\n## 7.5 Comparação com 10 s (Tabela 7.10)\n")
    for set_name, gruns, iruns, sids, srefs in (
            ("94 de referência", gls["10s"], ils["10s"], sorted(gls["10s"]),
             {i: (gls["10s"][i][0]["optimum"], True) for i in gls["10s"]}),
            ("75 geradas", gls["hard"], ils["hard"], hard_ids, hard_refs_)):
        emit(f"\n### {set_name}\n\n| Método | Ótimos | Gap (%) | t (s) | Pior / igual / melhor que a GLS | p | Inviáveis |")
        emit("|---|---:|---:|---:|---|---:|---:|")
        for v in ("000", "110", "111"):
            runs = iruns[v]
            s = stats(runs, sids, srefs)
            better, equal, worse, p, *_ = compare(runs, gruns, sids)
            emit(f"| {ILS_NAMES[v]} | {s['hits']}/{s['proven']} | {br(s['gap'])} | {br(s['ttb'])} | "
                 f"{worse} / {equal} / {better} | {fmt_p(p)} | {s['infeasible']}/{s['runs']} |")
        s = stats(gruns, sids, srefs)
        emit(f"| GLS | {s['hits']}/{s['proven']} | {br(s['gap'])} | {br(s['ttb'])} | – | – | 0 |")
        emit(f"\nI + S contra original: {cmp_text(iruns['110'], iruns['000'], sids)}")
        emit(f"I + S + C contra I + S: {cmp_text(iruns['111'], iruns['110'], sids)}")

    emit("\n### Por perfil, 10 s (Figura gap_por_perfil)\n")
    emit("| Perfil | GLS | ILS original | ILS + I + S | ILS + I + S + C | C vs I+S (melhor/pior, p) | I+S+C vs GLS (melhor/pior, p) |")
    emit("|---|---:|---:|---:|---:|---|---|")
    for code, prof in PROFILES.items():
        sub = [i for i in hard_ids if str(i).startswith(code)]
        gaps = [br(stats(m, sub, hard_refs_)["gap"]) for m in
                (gls["hard"], ils["hard"]["000"], ils["hard"]["110"], ils["hard"]["111"])]
        b1, _, w1, p1, *_ = compare(ils["hard"]["111"], ils["hard"]["110"], sub)
        b2, _, w2, p2, *_ = compare(ils["hard"]["111"], gls["hard"], sub)
        emit(f"| {prof} | " + " | ".join(gaps) + f" | {b1}/{w1}, {fmt_p(p1)} | {b2}/{w2}, {fmt_p(p2)} |")

    emit("\n### C50 por instância (Tabela 7.11)\n")
    emit("| Inst. | Referência | GLS | ILS+I+S+C médio | ILS+I+S+C melhor | ILS+I+S médio |")
    emit("|---:|---:|---:|---:|---:|---:|")
    for i in hard_ids:
        if not str(i).startswith("15"):
            continue
        ref, ok = hard_refs_[i]
        k1, k0 = ils["hard"]["111"], ils["hard"]["110"]
        emit(f"| {i}{'' if ok else '*'} | {ref:g} | {br(mcost(gls['hard'], i))} | {br(mcost(k1, i))} | "
             f"{min(r['cost'] for r in k1[i]):g} | {br(mcost(k0, i))} |")


def main():
    gls = {"0.55s": read("gls_ref_0.55s"), "10s": read("gls_ref_10s"), "hard": read("gls_hard_10s")}
    ils = {"0.55s": {}, "10s": {}, "hard": {}}
    for path in RESULTS.glob("ils_*.csv"):
        m = re.fullmatch(r"ils_(ref|hard)_([\d.]+s)_(\d{3})\.csv", path.name)
        if m:
            key = "hard" if m.group(1) == "hard" else m.group(2)
            ils[key][m.group(3)] = read(path.stem)
    abl = {}
    for path in RESULTS.glob("abl_0.55s_*.csv"):
        abl[path.stem.replace("abl_0.55s_", "")] = read(path.stem)

    emit("# Fase 13: GLS calibrada (3 exec.) × ILS (20 exec.), construção inicial no tempo\n")
    if gls["0.55s"] and gls["10s"] and "000" in ils["0.55s"] and "000" in ils["10s"]:
        section_reference(gls, ils)
    hard_ids = hard_r = None
    if gls["hard"] and all(v in ils["hard"] for v in ("000", "110", "111")):
        hard_ids, hard_r = section_hard(gls["hard"], ils["hard"])
    if abl:
        section_ablation(abl)
    if hard_ids and gls["0.55s"] and gls["10s"] and all(v in ils["10s"] for v in ("000", "110", "111")):
        section_ils(gls, ils, hard_ids, hard_r)
    text = "\n".join(OUT) + "\n"
    (RESULTS / "summary.md").write_text(text)
    print(text)


if __name__ == "__main__":
    main()
