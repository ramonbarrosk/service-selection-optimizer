"""Fase 14: ILS#3 em C++ comparado ao ILS#3 em Java e à GLS (dados da fase 13).

Usa as mesmas métricas e a mesma referência das instâncias geradas do capítulo
(ótimo do CPLEX, LB alcançado ou BKS entre todos os métodos da fase 13). Escreve
experiments/14_ils_cpp/results/summary.md.
"""
import os
import sys
from pathlib import Path
from statistics import mean

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experiments/13_calibrada_20exec/scripts"))
import analyze as A  # noqa: E402

P14 = ROOT / "experiments/14_ils_cpp/results"


def read14(name):
    old = A.RESULTS
    A.RESULTS = P14
    try:
        return A.read(name)
    finally:
        A.RESULTS = old


def pct(s):
    return A.br(100 * s["hit_runs"] / s["runs"], 1)


def table(title, methods, ids, refs):
    A.emit(f"\n## {title}\n")
    A.emit("| Método | Ótimo em ≥1 exec. | Execuções no ótimo (%) | Custo médio | Gap (%) | t (s) | Inviáveis |")
    A.emit("|---|---:|---:|---:|---:|---:|---:|")
    for name, runs in methods.items():
        s = A.stats(runs, ids, refs)
        A.emit(f"| {name} | {s['hits']}/{s['proven']} | {pct(s)} | {A.br(s['cost'])} | "
               f"{A.br(s['gap'])} | {A.br(s['ttb'])} | {s['infeasible']} |")
    names = list(methods)
    A.emit("")
    for a, b in [("ILS C++", "ILS Java"), ("GLS", "ILS C++"), ("GLS", "ILS Java")]:
        if a in methods and b in methods:
            A.emit(f"- {a} × {b}: {A.cmp_text(methods[a], methods[b], ids)}")
    return names


def main():
    gls10, gls055 = A.read("gls_ref_10s"), A.read("gls_ref_0.55s")
    java10, java055 = A.read("ils_ref_10s_000"), A.read("ils_ref_0.55s_000")
    cpp10, cpp055 = read14("cpp_ref_10s"), read14("cpp_ref_0.55s")
    ref_ids = sorted(gls10)
    ref_refs = {i: (gls10[i][0]["optimum"], True) for i in ref_ids}
    A.emit("# Fase 14 — ILS#3 em C++\n")
    A.emit("Referência: 94 instâncias com ótimo provado; geradas: mesma referência do capítulo "
           "(fase 13), acrescida das soluções da ILS em C++ quando melhores.")
    table("Referência, 10 s", {"GLS": gls10, "ILS Java": java10, "ILS C++": cpp10}, ref_ids, ref_refs)
    table("Referência, 0,55 s", {"GLS": gls055, "ILS Java": java055, "ILS C++": cpp055},
          ref_ids, ref_refs)
    A.emit(f"\n- ILS C++ 10 s × ILS Java 10 s (idem acima); GLS 0,55 s × ILS C++ 10 s: "
           f"{A.cmp_text(gls055, cpp10, ref_ids)}")

    glsh, javah, cpph = A.read("gls_hard_10s"), A.read("ils_hard_10s_000"), read14("cpp_hard_10s")
    methods13 = {"GLS": glsh}
    methods13.update({A.ILS_NAMES[v]: A.read(f"ils_hard_10s_{v}") for v in ("000", "110", "111")})
    ids, refs13 = A.hard_refs(methods13)
    ids, refs = A.hard_refs({**methods13, "ILS C++": cpph})
    changed = [(i, refs13[i][0], refs[i][0]) for i in ids if refs[i][0] != refs13[i][0]]
    A.emit(f"\nGeradas: referências alteradas pela ILS C++: {changed or 'nenhuma'}")
    table("Geradas, 10 s", {"GLS": glsh, "ILS Java": javah, "ILS C++": cpph}, ids, refs)
    A.emit("\n| Perfil | GLS gap | ILS Java gap | ILS C++ gap | C++ melhor/igual/pior que Java | GLS melhor/igual/pior que C++ |")
    A.emit("|---|---:|---:|---:|---:|---:|")
    for code, prof in list(A.PROFILES.items()):
        sub = [i for i in ids if str(i).startswith(code)]
        g, j, c = (A.stats(m, sub, refs)["gap"] for m in (glsh, javah, cpph))
        cj = A.compare(cpph, javah, sub)
        gc = A.compare(glsh, cpph, sub)
        A.emit(f"| {prof} | {A.br(g)} | {A.br(j)} | {A.br(c)} | {cj[0]}/{cj[1]}/{cj[2]} | {gc[0]}/{gc[1]}/{gc[2]} |")

    # Velocidade: iterações não são registradas; o tempo até o ótimo nas instâncias em que
    # as duas versões o atingem em todas as execuções dá uma medida direta.
    both = [i for i in ref_ids if all(r["cost"] <= ref_refs[i][0] for r in java10[i])
            and all(r["cost"] <= ref_refs[i][0] for r in cpp10[i])]
    if both:
        tj = mean(mean(r["time"] for r in java10[i]) for i in both)
        tc = mean(mean(r["time"] for r in cpp10[i]) for i in both)
        A.emit(f"\nTempo médio até o ótimo nas {len(both)} instâncias de referência resolvidas em "
               f"todas as execuções pelas duas versões (10 s): Java {A.br(tj)} s, C++ {A.br(tc)} s "
               f"(razão {A.br(tj / tc, 1)}).")
    (P14 / "summary.md").write_text("\n".join(A.OUT) + "\n")
    print("\n".join(A.OUT))


if __name__ == "__main__":
    main()
