"""Gera as tabelas completas por instância do apêndice do TCC.

- 94 instâncias de referência, 0,55 s e 10 s;
- 75 instâncias geradas, 10 s.

Duas famílias de tabelas, com 20 execuções da ILS e 1 da GLS calibrada:
- ILS original em Java (fase 13) × ILS original em C++ (fase 14) × GLS;
- ILS + I + S em Java (fase 13) × ILS + I + S em C++ (fase 14) × GLS. A referência das geradas é a do capítulo, sem as
variantes com correções (BKS entre GLS, ILS Java, ILS Java + I + S, ILS C++ e CPLEX).
Grava docs/tcc/apendice_tabela_completa.tex (longtable).
"""

import sys
from statistics import mean

from analyze import ROOT, br, hard_refs, read, ref_exec_time

sys.path.insert(0, str(ROOT / "experiments/14_ils_cpp/scripts"))
from analyze_sem_correcoes import read14  # noqa: E402

OUT = ROOT / "docs/tcc/apendice_tabela_completa.tex"
PROFILES = {"11": "T97", "12": "T98", "13": "T99", "14": "T100", "15": "C50"}


def cost_cell(value, ref):
    text = br(value)
    return f"\\textbf{{{text}}}" if abs(value - ref) < 1e-9 else text


def ils_cells(runs, ref):
    # Melhor: menor custo entre as execuções com solução viável; médio: todas.
    feasible = [r["cost"] for r in runs if r["feasible"]]
    return [cost_cell(min(feasible), ref), cost_cell(mean(r["cost"] for r in runs), ref),
            br(mean(r["time"] for r in runs))]


def gls_cells(runs, ref):
    # A GLS é determinística e roda uma vez (analyze.GLS_RUNS).
    return [cost_cell(mean(r["cost"] for r in runs), ref), br(mean(r["time"] for r in runs))]


def gls_note(gls):
    return "custo e tempo até a melhor solução da única execução"


def longtable(label, caption, head, rows):
    return rf"""{{\small
\setlength{{\tabcolsep}}{{3.5pt}}
\begin{{longtable}}{{rrrrrrrrrrr}}
\caption{{{caption}}}
\label{{{label}}} \\
\toprule
{head}
\midrule
\endfirsthead
\multicolumn{{11}}{{l}}{{\footnotesize Tabela~\ref{{{label}}} -- continuação}} \\
\toprule
{head}
\midrule
\endhead
\midrule
\multicolumn{{11}}{{r}}{{\footnotesize continua na próxima página}} \\
\endfoot
\bottomrule
\endlastfoot
""" + "\n".join(rows) + "\n\\end{longtable}\n}\n"


FAMILIES = {
    # chave: (sufixo do CSV Java da fase 13, prefixo do CSV C++ da fase 14, nome, rótulos)
    "orig": ("000", "cpp", "ILS original", ("a", "b", "c")),
    "is": ("110", "cppis", "ILS + I + S", ("d", "e", "f")),
}


def groups(name):
    return (rf"\multicolumn{{3}}{{c}}{{{name}, Java}} & \multicolumn{{3}}{{c}}{{{name}, C++}}"
            r" & \multicolumn{2}{c}{GLS} \\" "\n"
            r"\cmidrule(lr){4-6}\cmidrule(lr){7-9}\cmidrule(lr){10-11}" "\n")
METRICS = r"Melhor & Médio & $t$ (s) & Melhor & Médio & $t$ (s) & Custo & $t$ (s) \\"


def reference_table(family, budget, label_budget, suffix):
    jkey, ckey, name, _ = FAMILIES[family]
    gls, java, cpp = read(f"gls_ref_{budget}"), read(f"ils_ref_{budget}_{jkey}"), read14(f"{ckey}_ref_{budget}")
    rows = []
    for i in sorted(gls):
        opt = gls[i][0]["optimum"]
        exact = ref_exec_time(i)
        rows.append(" & ".join([str(i), f"{opt:g}", br(exact) if exact is not None else "--"]
                               + ils_cells(java[i], opt) + ils_cells(cpp[i], opt)
                               + gls_cells(gls[i], opt)) + r" \\")
    over = (" Com 0,55~s, o $t$ da ILS pode passar do orçamento, porque o prazo só é verificado"
            " depois da construção inicial e ao fim de cada iteração da busca."
            if budget == "0.55s" else "")
    caption = (f"{name} em Java e em C++ e GLS nas 94 instâncias de referência, orçamento de "
               f"{label_budget}. ILS: menor custo e custo médio das 20 execuções. GLS: "
               f"{gls_note(gls)}. Em negrito, valores iguais ao ótimo. $t$: tempo médio até a "
               f"melhor solução, incluindo a construção inicial. Exato: tempo do método exato."
               + over)
    head = " & & & " + groups(name) + "Inst. & Ótimo & Exato (s) & " + METRICS
    return longtable(f"tab:apendice-completa-{suffix}", caption, head, rows)


def generated_table(family, suffix):
    jkey, ckey, name, _ = FAMILIES[family]
    gls = read("gls_hard_10s")
    java, cpp = read(f"ils_hard_10s_{jkey}"), read14(f"{ckey}_hard_10s")
    # Mesma referência do capítulo: BKS entre todos os métodos apresentados.
    ids, refs = hard_refs({"GLS": gls, "ILS": read("ils_hard_10s_000"),
                           "ILS+I+S": read("ils_hard_10s_110"), "C++": read14("cpp_hard_10s"),
                           "C++I+S": read14("cppis_hard_10s")})
    rows = []
    for i in ids:
        ref, proven = refs[i]
        rows.append(" & ".join([str(i), PROFILES[str(i)[:2]], f"{ref:g}{'' if proven else '*'}"]
                               + ils_cells(java[i], ref) + ils_cells(cpp[i], ref)
                               + gls_cells(gls[i], ref)) + r" \\")
    caption = (f"{name} em Java e em C++ e GLS nas 75 instâncias geradas, orçamento de 10~s. "
               "Referência: ótimo provado ou, nas instâncias marcadas com *, melhor solução "
               "conhecida (Seção~\\ref{sec:instancias-geradas}). ILS: menor custo entre as "
               "execuções com solução viável e custo médio de todas as 20 execuções. GLS: "
               f"{gls_note(gls)}. Em negrito, valores iguais à referência. $t$: tempo médio até "
               "a melhor solução, incluindo a construção inicial.")
    head = " & & & " + groups(name) + "Inst. & Perfil & Ref. & " + METRICS
    return longtable(f"tab:apendice-completa-{suffix}", caption, head, rows)


def main():
    parts = ["% Gerado por experiments/13_calibrada_20exec/scripts/make_apendice.py -- não editar à mão.\n"]
    for family, (*_, labels) in FAMILIES.items():
        parts += [reference_table(family, "0.55s", "0,55~s", labels[0]),
                  reference_table(family, "10s", "10~s", labels[1]),
                  generated_table(family, labels[2])]
    OUT.write_text("\n".join(parts))
    print(f"-> {OUT}")


if __name__ == "__main__":
    main()
