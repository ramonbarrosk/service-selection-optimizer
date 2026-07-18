#!/usr/bin/env bash
# Roda a abordagem da Fase 2 (best-fit + oscilacao estrategica + ILS) em TODAS
# as instancias e imprime uma tabela-resumo, alem de salvar um CSV.
#
# Uso:  ./scripts/run_phase2_all.sh [N_REINICIOS] [ITERS_ILS]
#   N_REINICIOS  numero de reinicios ILS pareados por seed (default 10)
#   ITERS_ILS    iteracoes de ILS-oscilacao por reinicio    (default 50)

set -euo pipefail
cd "$(dirname "$0")/.."

N="${1:-10}"
ITERS="${2:-50}"

FLAGS="-std=c++17 -O3 -DNDEBUG -Isrc -Isrc/basic -Isrc/enum -Isrc/instance -Isrc/util -Isrc/validator -Isrc/search -Isrc/metaheuristic"
mkdir -p build data/charts
# (re)compila so se preciso
if [ ! -x build/phase2 ] || [ experiments/phase2_oscillate.cpp -nt build/phase2 ]; then
    echo "compilando build/phase2..."
    g++ $FLAGS -o build/phase2 experiments/phase2_oscillate.cpp
fi

OUT="data/phase2_results.csv"
# arquivo no formato do visualize.py: nome opt opt_time mean best time
CPPRES="data/phase2_cpp_results.txt"
echo "instance,optimal,best_fit,vnd,osc,ils_osc,gap_pct" > "$OUT"
: > "$CPPRES"

# instancias ordenadas numericamente
mapfile -t IDS < <(ls data/instances/ | sed -E 's/.*_([0-9]+)$/\1/' | sort -n)

printf "%-10s %8s %8s %8s %8s %10s %8s\n" inst opt best-fit VND osc ILS-osc gap
printf -- "------------------------------------------------------------------------\n"
for id in "${IDS[@]}"; do
    line=$(./build/phase2 "$id" "$N" "$ITERS" 2>/dev/null)
    opt=$(echo "$line"  | sed -nE 's/.*optimal=([0-9]+).*/\1/p')
    bf=$(echo  "$line"  | sed -nE 's/.*\(B\)[^:]*: *([0-9]+).*/\1/p')
    vnd=$(echo "$line"  | sed -nE 's/.*\(D\)[^:]*: *([0-9]+).*/\1/p')
    osc=$(echo "$line"  | sed -nE 's/.*\(F\)[^:]*: *([0-9]+).*/\1/p')
    ils=$(echo "$line"  | sed -nE 's/.*best=([0-9]+).*/\1/p')
    gap=$(echo "$line"  | sed -nE 's/.*best=[0-9]+ *gap=([0-9-]+)%.*/\1/p')
    printf "%-10s %8s %8s %8s %8s %10s %7s%%\n" "$id" "$opt" "$bf" "$vnd" "$osc" "$ils" "$gap"
    echo "$id,$opt,$bf,$vnd,$osc,$ils,$gap" >> "$OUT"
    # repassa a linha RESULT (sem o prefixo) pro arquivo do visualize.py
    echo "$line" | sed -nE 's/^RESULT (.*)/\1/p' >> "$CPPRES"
done

echo
echo "CSV salvo em $OUT"
echo "Resultados p/ visualize.py salvos em $CPPRES"
# media de gap
awk -F, 'NR>1 && $7!=""{s+=$7;n++} END{if(n)printf "gap medio (ILS-osc): %.1f%% em %d instancias\n", s/n, n}' "$OUT"
