#!/usr/bin/env bash
# Executa variantes do ILS#3 (Java) com as melhorias portadas da GLS, uma linha por
# repetição. Cada variante é "I S K": avaliação incremental, substituição de serviço
# e keepBest (perturba uma cópia da melhor solução e devolve a melhor encontrada).
# 000 é o ILS#3 original.
#
#   SET=ref  (94 instâncias de data/Instances do projeto Java, com os ótimos de data/Log)
#   SET=hard (75 instâncias de data/instances_hard, com os ótimos de data/Log_hard)
#   VARIANTS="111 110" TIME_SECONDS=10 REPETITIONS=3
#
# Execute a partir da raiz do projeto C++ (service-selection-optimizer):
#   SET=ref TIME_SECONDS=0.55 VARIANTS="000 100 010 110" bash experiments/12_ils_melhorado/scripts/run_ils_variants.sh

set -euo pipefail

JAVA_PROJECT="/home/ramon/MyProjects/Service-Selection-Under-Uncertainty/Service Selection Under Uncertainty"
RESULTS="$(pwd)/experiments/12_ils_melhorado/results"
SET="${SET:-ref}"
VARIANTS="${VARIANTS:-110}"
REPETITIONS="${REPETITIONS:-3}"
TIME_SECONDS="${TIME_SECONDS:-10}"

case "$SET" in
    ref)  DIR_ARGS=() ;;
    hard) DIR_ARGS=(-Dsso.dir="$(pwd)/data/instances_hard" -Dsso.logdir="$(pwd)/data/Log_hard") ;;
    *) echo "SET deve ser ref ou hard" >&2; exit 1 ;;
esac

mkdir -p "$RESULTS"
(cd "$JAVA_PROJECT" && javac -nowarn -encoding UTF-8 -d bin -cp lib/commons-math3-3.6.1.jar \
    $(find src -name '*.java')) 2>&1 | grep -v '^Note:' || true

for variant in $VARIANTS; do
    inc="${variant:0:1}"; rep="${variant:1:1}"; keep="${variant:2:1}"
    name="ils3_${SET}_${TIME_SECONDS}s_i${inc}_s${rep}_k${keep}"
    echo "[$name] $REPETITIONS repetições de ${TIME_SECONDS}s"
    (cd "$JAVA_PROJECT" && java -Dsso.ils=3 -Dsso.time="$TIME_SECONDS" -Dsso.reps="$REPETITIONS" \
        -Dsso.incremental="$inc" -Dsso.replacement="$rep" -Dsso.keepbest="$keep" \
        "${DIR_ARGS[@]}" -Dsso.csv="$RESULTS/$name.csv" \
        -cp "bin:lib/commons-math3-3.6.1.jar" main.ArticleResult) > "$RESULTS/$name.log" 2>&1
done
echo "Concluído. Resultados em $RESULTS"
