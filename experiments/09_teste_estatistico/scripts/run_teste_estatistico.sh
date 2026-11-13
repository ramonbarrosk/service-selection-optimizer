#!/usr/bin/env bash
# Executa ILS#3 (Java) e GLS (C++) nas 15 instâncias em que os métodos diferiram
# no protocolo de 10 s, com uma linha por repetição para os testes estatísticos.
# Os dois lados rodam em sequência para não disputarem CPU.
# Execute a partir da raiz do projeto C++ (service-selection-optimizer):
#   bash experiments/09_teste_estatistico/scripts/run_teste_estatistico.sh

set -euo pipefail

JAVA_PROJECT="/home/ramon/MyProjects/Service-Selection-Under-Uncertainty/Service Selection Under Uncertainty"
RESULTS="$(pwd)/experiments/09_teste_estatistico/results"
INSTANCES="5,11,21,24,28,40,42,51,60,67,100,101,128,129,147"
REPETITIONS="${REPETITIONS:-20}"
TIME_SECONDS="${TIME_SECONDS:-10}"

mkdir -p "$RESULTS"

echo "[1/2] Java ILS#3: instâncias $INSTANCES, $REPETITIONS repetições de ${TIME_SECONDS}s"
(cd "$JAVA_PROJECT" && java -Dsso.ils=3 -Dsso.time="$TIME_SECONDS" -Dsso.reps="$REPETITIONS" \
    -Dsso.instances="$INSTANCES" -Dsso.csv="$RESULTS/ils3_dificeis.csv" \
    -cp bin main.ArticleResult) > "$RESULTS/ils3_dificeis.log" 2>&1

echo "[2/2] C++ GLS: mesmas instâncias e repetições"
make
SSO_INSTANCES="$INSTANCES" SSO_REPETITIONS="$REPETITIONS" SSO_TIME_SECONDS="$TIME_SECONDS" \
    SSO_CSV="$RESULTS/gls_dificeis.csv" ./build/service-selection-optimizer \
    > "$RESULTS/gls_dificeis.log" 2>&1

echo "Concluído. Resultados em $RESULTS"
