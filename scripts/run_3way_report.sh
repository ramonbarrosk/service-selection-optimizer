#!/usr/bin/env bash
# Roda as 3 versoes SEQUENCIALMENTE (o do-while e por tempo; paralelo falsearia).
#   1) Java (referencia dos autores, ILS#1)
#   2) C++ baseline  (make OSC=0)  -> mesmo algoritmo do Java, sem hibrido
#   3) C++ hibrido   (make)        -> best-fit + oscilacao
# Saidas em data/report_*.txt (formato: nome optCost optTime meanBest best TtB)
set -e

CPP="/home/ramon/MyProjects/service-selection-optimizer"
JAVA="/home/ramon/MyProjects/Service-Selection-Under-Uncertainty/Service Selection Under Uncertainty"

echo "==================== [1/3] JAVA (referencia, ILS#1) ===================="
cd "$JAVA"
java -cp "bin:lib/commons-math3-3.6.1.jar" main.ArticleResult > "$CPP/data/report_java.txt" 2>&1
echo "  Java OK -> data/report_java.txt"

echo "==================== [2/3] C++ BASELINE (OSC=0) ===================="
cd "$CPP"
make clean >/dev/null && make OSC=0 >/dev/null 2>&1
./build/service-selection-optimizer > data/report_cpp_baseline.txt 2>&1
echo "  baseline OK -> data/report_cpp_baseline.txt"

echo "==================== [3/3] C++ HIBRIDO (best-fit + oscilacao) ===================="
make clean >/dev/null && make >/dev/null 2>&1
./build/service-selection-optimizer > data/report_cpp_hybrid.txt 2>&1
echo "  hibrido OK -> data/report_cpp_hybrid.txt"

echo "==================== TODOS CONCLUIDOS ===================="
grep -H "MEAN BEST\|REACHED OPTIMAL" data/report_java.txt data/report_cpp_baseline.txt data/report_cpp_hybrid.txt 2>/dev/null || true
