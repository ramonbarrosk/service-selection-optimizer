#!/usr/bin/env bash
# Valida a configuração calibrada (primeira linha de results/best_configurations.csv)
# contra a configuração atual, nos dois conjuntos de teste e no protocolo da fase 07:
# 3 repetições de 10 s, parando antes ao alcançar o ótimo conhecido.
#   original: as 94 instâncias do artigo (data/instances, ótimo em data/Log);
#   hard: as 75 instâncias da fase 10 (data/instances_hard, ótimo em data/Log_hard).
# Nenhuma instância de teste foi usada na calibração. As duas configurações rodam em
# paralelo, nas mesmas condições de carga. Execute a partir da raiz do repositório:
#   bash experiments/11_calibracao_irace/scripts/run_validacao.sh
set -euo pipefail

D=experiments/11_calibracao_irace
RESULTS="$D/results/validacao"
REPETITIONS="${REPETITIONS:-3}"
TIME_SECONDS="${TIME_SECONDS:-10}"
mkdir -p "$RESULTS"
make build/service-selection-optimizer

# Converte a melhor configuração do irace em variáveis SSO_GLS_*.
TUNED_ENV="$(python3 - "$D/results/best_configurations.csv" <<'PY'
import csv, sys
names = {"alpha": "SSO_GLS_ALPHA", "rounds": "SSO_GLS_ROUNDS", "gfls": "SSO_GLS_GFLS",
         "replacement": "SSO_GLS_REPLACEMENT", "regret": "SSO_GLS_REGRET",
         "calibrate": "SSO_GLS_CALIBRATE", "maxpen": "SSO_GLS_MAX_PENALIZED"}
best = next(csv.DictReader(open(sys.argv[1])))
print(" ".join(f"{names[k]}={best[k]}" for k in names))
PY
)"
echo "Calibrada: $TUNED_ENV"

run() {  # <config> <conjunto> <pasta de instâncias> <pasta de logs> [variáveis...]
    local config="$1" set="$2" instances="$3" logs="$4"
    shift 4
    env "$@" SSO_INSTANCE_DIR="$instances" SSO_LOG_DIR="$logs" \
        SSO_REPETITIONS="$REPETITIONS" SSO_TIME_SECONDS="$TIME_SECONDS" \
        SSO_CSV="$RESULTS/${config}_${set}.csv" ./build/service-selection-optimizer \
        > "$RESULTS/${config}_${set}.log" 2>&1
}

for spec in "original:data/instances:data/Log" "hard:data/instances_hard:data/Log_hard"; do
    IFS=: read -r set instances logs <<< "$spec"
    echo "Conjunto $set: atual e calibrada em paralelo"
    run atual "$set" "$instances" "$logs" &
    # shellcheck disable=SC2086
    run calibrada "$set" "$instances" "$logs" $TUNED_ENV &
    wait
done

echo "Concluído. Agora execute: python3 $D/scripts/analyze_validacao.py"
