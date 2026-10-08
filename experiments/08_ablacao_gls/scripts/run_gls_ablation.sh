#!/usr/bin/env bash
# Ablação das melhorias da GLS com orçamento fixo por execução:
# fatorial 2^4 (I, S, L, R, todas com GFLS) + GLS pura como baseline,
# 94 instâncias, 3 repetições. Cada variante roda em um processo próprio e
# grava seu CSV em experiments/08_ablacao_gls/results/ablation_<tempo>s/. Variantes com CSV já
# gravado são puladas, então o script pode ser reexecutado para retomar um lote.
#
#   SSO_ABLATION_TIME=0.55 SSO_PARALLEL=3 bash experiments/08_ablacao_gls/scripts/run_gls_ablation.sh
#   SSO_ABLATION_TIME=10   SSO_PARALLEL=3 bash experiments/08_ablacao_gls/scripts/run_gls_ablation.sh

set -euo pipefail
cd "$(dirname "$0")/../../.."

make experiment-gls-improvements
budget="${SSO_ABLATION_TIME:-10}"
out_dir="experiments/08_ablacao_gls/results/ablation_${budget}s"
mkdir -p "$out_dir"
parallel="${SSO_PARALLEL:-3}"

run_variant() {
    local variant="$1" out_dir="$2" budget="$3"
    if [[ -f "$out_dir/$variant.csv" ]]; then
        echo "skip $variant"
        return
    fi
    local factorial=1
    [[ "$variant" == "gls" ]] && factorial=0
    SSO_FACTORIAL="$factorial" SSO_TIME_SECONDS="$budget" SSO_INSTANCE_SET=all \
    SSO_REPETITIONS=3 SSO_VARIANTS="$variant" \
    SSO_EXPERIMENT_OUTPUT="$out_dir/$variant.csv" \
        ./build/compare-gls-improvements > "$out_dir/$variant.log" 2>&1
    echo "done $variant"
}
export -f run_variant

# As variantes mais lentas (sem substituição de serviço) vão primeiro para equilibrar a fila.
variants=(gls)
for s in 0 1; do for i in 0 1; do for l in 0 1; do for r in 0 1; do
    variants+=("i${i}_s${s}_l${l}_r${r}")
done; done; done; done

printf '%s\n' "${variants[@]}" \
    | xargs -P "$parallel" -I{} bash -c 'run_variant "$1" "$2" "$3"' _ {} "$out_dir" "$budget"
