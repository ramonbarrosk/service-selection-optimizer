#!/usr/bin/env bash
# Validação do porte C++ do ILS#3: uma única chamada de 10.000 iterações, sem limite
# de tempo, nas duas linguagens. Mesmo número de iterações isola o algoritmo da
# velocidade da linguagem. Java no núcleo 0, C++ no núcleo 1.
# INCREMENTAL=1 REPLACEMENT=1 valida a ILS com avaliação incremental e substituição
# de serviço (saída java_is.csv e cpp_is.csv).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
JAVA_PROJECT="${JAVA_PROJECT:-$ROOT/../Service-Selection-Under-Uncertainty/Service Selection Under Uncertainty}"
OUT="$ROOT/experiments/14_ils_cpp/results/validacao"
INST="${INST:-11,28,100,128,129,5,42,77,110,143}"
REPS="${REPS:-20}"
INCREMENTAL="${INCREMENTAL:-0}"
REPLACEMENT="${REPLACEMENT:-0}"
TAG=""
[[ "$INCREMENTAL$REPLACEMENT" != "00" ]] && TAG="_i${INCREMENTAL}s${REPLACEMENT}"
mkdir -p "$OUT"
make -C "$ROOT" build/ils-run >/dev/null
(cd "$JAVA_PROJECT" && taskset -c 0 java -XX:+UseSerialGC -Xmx1g -Dsso.ils=3 -Dsso.time=10 \
    -Dsso.reps="$REPS" -Dsso.singlecall=1 -Dsso.instances="$INST" -Dsso.csv="$OUT/java$TAG.csv" \
    -Dsso.incremental="$INCREMENTAL" -Dsso.replacement="$REPLACEMENT" \
    -cp "bin:lib/commons-math3-3.6.1.jar" main.ArticleResult > "$OUT/java$TAG.log" 2>&1) &
(cd "$ROOT" && SSO_INSTANCES="$INST" SSO_REPETITIONS="$REPS" SSO_ILS_SINGLE_CALL=1 \
    SSO_ILS_INCREMENTAL="$INCREMENTAL" SSO_ILS_REPLACEMENT="$REPLACEMENT" \
    SSO_CSV="$OUT/cpp$TAG.csv" taskset -c 1 ./build/ils-run > "$OUT/cpp$TAG.log" 2>&1) &
wait
echo "validação concluída"
