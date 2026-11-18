"""Números do capítulo sem as variantes com correções (C) da ILS.

Métodos: GLS, ILS Java original (000), ILS Java + I (100), + S (010), + I + S (110),
ILS C++ original (porte fiel) e ILS C++ + I + S (quando os CSVs cppis_* existem). A referência das instâncias geradas sem ótimo provado é a BKS
entre esses métodos e o CPLEX interrompido (as variantes com C ficam de fora).
"""
import sys
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experiments/13_calibrada_20exec/scripts"))
import analyze as A  # noqa: E402

P14 = ROOT / "experiments/14_ils_cpp/results"
PROFILES = A.PROFILES


def read14(name):
    old, A.RESULTS = A.RESULTS, P14
    try:
        return A.read(name)
    finally:
        A.RESULTS = old


def stats(runs, ids, refs):
    proven = [i for i in ids if refs[i][1]]
    hit = sum(r["cost"] <= refs[i][0] for i in proven for r in runs[i])
    tot = sum(len(runs[i]) for i in proven)
    return {
        "pct": 100 * hit / tot if tot else 0,
        "inst": sum(any(r["cost"] <= refs[i][0] for r in runs[i]) for i in proven),
        "all": sum(all(r["cost"] <= refs[i][0] for r in runs[i]) for i in proven),
        "gap": mean(mean(100 * (r["cost"] - refs[i][0]) / refs[i][0] for r in runs[i]) for i in ids),
        "cost": mean(A.mcost(runs, i) for i in ids),
        "t": mean(mean(r["time"] for r in runs[i]) for i in ids),
        "inf": sum(not r["feasible"] for i in ids for r in runs[i]),
        "proven": len(proven),
    }


def vs(a, b, ids):
    """a contra b: (a pior, igual, a melhor, p)."""
    better, equal, worse, p, *_ = A.compare(a, b, ids)
    return f"{worse}/{equal}/{better}", (A.fmt_p(p) if p is not None else "-")


def block(title, methods, ids, refs, base="GLS", pairs=()):
    print(f"\n## {title}")
    print("método | no ót. % | inst≥1 | inst todas | gap | custo | t | inviáveis | pior/igual/melhor vs GLS | p")
    for name, runs in methods.items():
        s = stats(runs, ids, refs)
        c = vs(runs, methods[base], ids) if name != base else ("-", "-")
        print(f"{name} | {A.br(s['pct'], 1)} | {s['inst']}/{s['proven']} | {s['all']} | {A.br(s['gap'])} | "
              f"{A.br(s['cost'])} | {A.br(s['t'])} | {s['inf']} | {c[0]} | {c[1]}")
    for a, b in pairs:
        if a not in methods or b not in methods:
            continue
        print(f"  {a} × {b}: {A.cmp_text(methods[a], methods[b], ids)}")


def main():
    # ---------------- referência
    ref = {}
    for b in ("0.55s", "10s"):
        m = {"GLS": A.read(f"gls_ref_{b}"), "ILS": A.read(f"ils_ref_{b}_000")}
        for k, n in (("100", "ILS+I"), ("010", "ILS+S"), ("110", "ILS+I+S")):
            r = A.read(f"ils_ref_{b}_{k}")
            if r:
                m[n] = r
        m["C++"] = read14(f"cpp_ref_{b}")
        cis = read14(f"cppis_ref_{b}")
        if cis:
            m["C++I+S"] = cis
        ref[b] = m
    ids = sorted(ref["10s"]["GLS"])
    refs = {i: (ref["10s"]["GLS"][i][0]["optimum"], True) for i in ids}
    block("Referência 0,55 s", ref["0.55s"], ids, refs,
          pairs=[("ILS+I", "ILS"), ("ILS+S", "ILS"), ("ILS+I+S", "ILS+I"), ("ILS+I+S", "ILS+S"),
                 ("C++", "ILS"), ("C++", "ILS+I+S"), ("C++I+S", "ILS+I+S"), ("C++I+S", "C++")])
    block("Referência 10 s", ref["10s"], ids, refs,
          pairs=[("ILS+I+S", "ILS"), ("C++", "ILS"), ("C++", "ILS+I+S"), ("C++I+S", "ILS+I+S"),
                 ("C++I+S", "C++")])
    print("  GLS 0,55 × C++ 10:", A.cmp_text(ref["0.55s"]["GLS"], ref["10s"]["C++"], ids))
    print("  GLS 0,55 × ILS+I+S 10:", A.cmp_text(ref["0.55s"]["GLS"], ref["10s"]["ILS+I+S"], ids))
    fast = [i for i in ids if A.ref_exec_time(i) < 2]
    slow = [i for i in ids if i not in fast]
    if "C++I+S" in ref["10s"]:
        print("  GLS 0,55 × C++I+S 10:", A.cmp_text(ref["0.55s"]["GLS"], ref["10s"]["C++I+S"], ids))
    for name in [k for k in ("ILS", "ILS+I+S", "C++", "C++I+S", "GLS") if k in ref["10s"]]:
        f, s = stats(ref["10s"][name], fast, refs), stats(ref["10s"][name], slow, refs)
        print(f"  {name}: rápidas {A.br(f['pct'], 1)}% t {A.br(f['t'])} | demoradas inst≥1 {s['inst']} "
              f"todas {s['all']} gap {A.br(s['gap'])} t {A.br(s['t'])}")

    # ---------------- geradas
    hard = {"GLS": A.read("gls_hard_10s"), "ILS": A.read("ils_hard_10s_000"),
            "ILS+I+S": A.read("ils_hard_10s_110"), "C++": read14("cpp_hard_10s")}
    if read14("cppis_hard_10s"):
        hard["C++I+S"] = read14("cppis_hard_10s")
    hids, hrefs = A.hard_refs(hard)
    _, old_refs = A.hard_refs({**hard, "C": A.read("ils_hard_10s_111")})
    _, refs_sem_cis = A.hard_refs({k: v for k, v in hard.items() if k != "C++I+S"})
    print("\n## Referências das geradas sem prova")
    for i in hids:
        if not hrefs[i][1]:
            src = [n for n, m in hard.items()
                   if any(r["cost"] == hrefs[i][0] and r["feasible"] for r in m[i])]
            print(f"  {i}: nova {hrefs[i][0]:g} (sem C++I+S {refs_sem_cis[i][0]:g}; com C {old_refs[i][0]:g})"
                  f" fonte {src or ['CPLEX']}")
    block("Geradas 10 s", hard, hids, hrefs,
          pairs=[("ILS+I+S", "ILS"), ("C++", "ILS"), ("C++", "ILS+I+S"), ("C++I+S", "ILS+I+S"),
                 ("C++I+S", "C++")])
    for p, label in PROFILES.items():
        pids = [i for i in hids if str(i).startswith(p)]
        cells = []
        for name, runs in hard.items():
            s = stats(runs, pids, hrefs)
            c = vs(runs, hard["GLS"], pids) if name != "GLS" else ("-", "-")
            cells.append(f"{name} {A.br(s['pct'], 1)}% gap {A.br(s['gap'])} ({c[0]}, p={c[1]})")
        print(f"  {label}: " + " | ".join(cells))
        if label != "C50":
            continue
        proven = [i for i in pids if hrefs[i][1]]
        for name in hard:
            print(f"    C50 provadas {name}: gap {A.br(stats(hard[name], proven, hrefs)['gap'])}")
        for i in pids:
            row = [f"{i}{'' if hrefs[i][1] else '*'} ref {hrefs[i][0]:g}"]
            for name, runs in hard.items():
                feas = [r["cost"] for r in runs[i] if r["feasible"]]
                row.append(f"{name} {A.br(A.mcost(runs, i))} (melhor {min(feas):g})")
            print("    " + " | ".join(row))
    tight = [i for i in hids if not str(i).startswith("15")]
    for name in hard:
        s = stats(hard[name], tight, hrefs)
        print(f"  T97-T100 {name}: inst≥1 {s['inst']} todas {s['all']} pct {A.br(s['pct'], 1)}")


if __name__ == "__main__":
    main()
