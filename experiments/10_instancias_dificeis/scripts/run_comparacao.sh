#!/usr/bin/env bash
# Executa ILS#3 (Java) e GLS (C++) nas instâncias de data/instances_hard, com uma
# linha por repetição. Os dois lados param antes do orçamento ao alcançar o ótimo do
# método exato (data/Log_hard, gerado por run_exato.sh). Instâncias sem ótimo provado
# usam o limite inferior trivial como critério de parada.
# Os dois lados rodam em sequência para não disputarem CPU.
# Execute a partir da raiz do repositório, depois de generate_instances.sh e run_exato.sh:
#   bash experiments/10_instancias_dificeis/scripts/run_comparacao.sh

set -euo pipefail

JAVA_PROJECT="/home/ramon/MyProjects/Service-Selection-Under-Uncertainty/Service Selection Under Uncertainty"
INSTANCES="$(pwd)/data/instances_hard"
LOGS="$(pwd)/data/Log_hard"
RESULTS="$(pwd)/experiments/10_instancias_dificeis/results"
REPETITIONS="${REPETITIONS:-3}"
TIME_SECONDS="${TIME_SECONDS:-10}"

mkdir -p "$RESULTS"

echo "[1/2] Java ILS#3: $REPETITIONS repetições de ${TIME_SECONDS}s por instância"
(cd "$JAVA_PROJECT" && java -Dsso.ils=3 -Dsso.time="$TIME_SECONDS" -Dsso.reps="$REPETITIONS" \
    -Dsso.dir="$INSTANCES" -Dsso.logdir="$LOGS" -Dsso.csv="$RESULTS/ils3_hard.csv" \
    -cp "bin:lib/commons-math3-3.6.1.jar" main.ArticleResult) > "$RESULTS/ils3_hard.log" 2>&1

echo "[2/2] C++ GLS: mesmas instâncias e repetições"
make
SSO_INSTANCE_DIR="$INSTANCES" SSO_LOG_DIR="$LOGS" \
    SSO_REPETITIONS="$REPETITIONS" SSO_TIME_SECONDS="$TIME_SECONDS" \
    SSO_CSV="$RESULTS/gls_hard.csv" ./build/service-selection-optimizer \
    > "$RESULTS/gls_hard.log" 2>&1

echo "Concluído. Agora execute: python3 experiments/10_instancias_dificeis/scripts/analyze_comparacao.py"
