#!/usr/bin/env python3
"""Figura do capítulo: gap médio por perfil nas instâncias geradas (10 s) para a GLS,
a ILS em Java (original e com avaliação incremental e substituição de serviço) e a
ILS em C++ (original e com I + S). Reaproveita plot_figuras.py da fase 13; sem as variantes com correções.

    python3 experiments/14_ils_cpp/scripts/plot_perfil.py
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experiments/13_calibrada_20exec/scripts"))
import plot_figuras as P  # noqa: E402
from analyze_sem_correcoes import read14  # noqa: E402

P.CHARTS = ROOT / "experiments/14_ils_cpp/charts"
P.METHODS = [
    ("GLS", "GLS", "#2a78d6"),
    ("orig", "ILS original (Java)", "#eb6834"),
    ("110", "ILS + I + S (Java)", "#1baf7a"),
    ("cpp", "ILS original (C++)", "#eda100"),
    ("cppis", "ILS + I + S (C++)", "#7e57c2"),
]
P.SUFFIX = "_ils_cpp"
_load = P.load


def load(set_name, budget):
    methods = _load(set_name, budget)
    methods["cpp"] = read14(f"cpp_{set_name}_{budget}s")
    methods["cppis"] = read14(f"cppis_{set_name}_{budget}s")
    return methods


P.load = load

if __name__ == "__main__":
    P.CHARTS.mkdir(parents=True, exist_ok=True)
    P.gap_por_perfil()
    P.gap_por_variante()
    print(P.CHARTS)
