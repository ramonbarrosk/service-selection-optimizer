#!/usr/bin/env bash
# Compara ILS#1 (config de producao: SWAP + FIRST_IMPROVEMENT, OSC=1 por padrao) com
# FLS desligado (comportamento atual, varredura completa) e ligado (sub-vizinhancas
# com bit de ativacao, Voudouris/Tsang). Roda SEQUENCIALMENTE (o do-while do main.cpp
# e por tempo de parede; paralelo falsearia a comparacao).
# Saidas em data/report_cpp_fls_off.txt e data/report_cpp_fls_on.txt (mesmo formato
# de scripts/run_3way_report.sh: nome optCost optTime meanBest best TtB).
set -e
cd "$(dirname "$0")/.."

echo "==================== [1/2] FLS=0 (comportamento atual) ===================="
make clean >/dev/null && make >/dev/null 2>&1
./build/service-selection-optimizer > data/report_cpp_fls_off.txt 2>&1
echo "  FLS=0 OK -> data/report_cpp_fls_off.txt"

echo "==================== [2/2] FLS=1 (sub-vizinhancas ativas) ===================="
make clean >/dev/null && make FLS=1 >/dev/null 2>&1
./build/service-selection-optimizer > data/report_cpp_fls_on.txt 2>&1
echo "  FLS=1 OK -> data/report_cpp_fls_on.txt"

echo "==================== CONCLUIDO ===================="
grep -H "MEAN BEST\|REACHED OPTIMAL" data/report_cpp_fls_off.txt data/report_cpp_fls_on.txt 2>/dev/null || true
