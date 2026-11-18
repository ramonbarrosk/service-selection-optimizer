#!/usr/bin/env bash
# Fase 13: refaz todos os resultados do capítulo de resultados com
#   - GLS na configuração calibrada pelo irace (primeira linha de
#     11_calibracao_irace/results/best_configurations.csv), 3 execuções;
#   - ILS#3 Java (original e variantes da fase 12) com 20 execuções;
#   - construção da solução inicial da GLS dentro do orçamento e dos tempos.
#
# Os jobs vão para uma fila consumida por WORKERS processos, cada um preso a um
# núcleo (taskset), para que os tempos não sofram com disputa de CPU. A JVM usa o
# coletor serial. Os jobs de 0,55 s rodam as 94 instâncias numa mesma JVM (o
# aquecimento do JIT pesaria muito em orçamentos curtos); os de 10 s, lotes de
# 12 a 15 instâncias. Jobs com CSV já gravado são pulados, então o script pode ser
# reexecutado para retomar a bateria.
#
# Execute a partir da raiz do repositório:
#   WORKERS=4 bash experiments/13_calibrada_20exec/scripts/run_bateria.sh
#   python3 experiments/13_calibrada_20exec/scripts/merge_results.py
#
# Teste rápido: RAW=/tmp/x ONLY='ils_ref_10s_111|gls_hard|abl_0.55s_gls' TIME_OVERRIDE=0.2 \
#   ILS_REPS=1 GLS_REPS=1 bash ...

set -euo pipefail

ROOT="$(pwd)"
D="$ROOT/experiments/13_calibrada_20exec"
RAW="${RAW:-$D/results/raw}"
QUEUE="$RAW/queue.txt"
JAVA_PROJECT="/home/ramon/MyProjects/Service-Selection-Under-Uncertainty/Service Selection Under Uncertainty"
WORKERS="${WORKERS:-4}"
ILS_REPS="${ILS_REPS:-20}"
GLS_REPS="${GLS_REPS:-3}"
mkdir -p "$RAW"

make build/service-selection-optimizer experiment-gls-improvements
(cd "$JAVA_PROJECT" && javac -nowarn -encoding UTF-8 -d bin -cp lib/commons-math3-3.6.1.jar \
    $(find src -name '*.java')) 2>&1 | grep -v '^Note:' || true

TUNED_ENV="$(python3 - "$ROOT/experiments/11_calibracao_irace/results/best_configurations.csv" <<'PY'
import csv, sys
names = {"alpha": "SSO_GLS_ALPHA", "rounds": "SSO_GLS_ROUNDS", "gfls": "SSO_GLS_GFLS",
         "replacement": "SSO_GLS_REPLACEMENT", "regret": "SSO_GLS_REGRET",
         "calibrate": "SSO_GLS_CALIBRATE", "maxpen": "SSO_GLS_MAX_PENALIZED"}
best = next(csv.DictReader(open(sys.argv[1])))
print(" ".join(f"{names[k]}={best[k]}" for k in names))
PY
)"
# A ablação liga e desliga os componentes; dela só vêm α, rodadas e maxpen.
ABL_ENV="$(tr ' ' '\n' <<< "$TUNED_ENV" | grep -E 'ALPHA|ROUNDS|MAX_PENALIZED' | tr '\n' ' ')"
echo "GLS calibrada: $TUNED_ENV"

ids() { ls "$1" | sed 's/Instance_10_10_//' | sort -n; }
chunks() {  # <pasta> <tamanho>: imprime lotes separados por vírgula
    ids "$1" | paste -sd, - | tr ',' '\n' | awk -v n="$2" '
        { b = b (b == "" ? "" : ",") $0 } NR % n == 0 { print b; b = "" } END { if (b != "") print b }'
}
REF_ALL="$(ids "$ROOT/data/instances" | paste -sd, -)"

# Fila: nome|tipo|conjunto|tempo|variante|instâncias. As mais longas primeiro.
{
    for v in 000 110 111; do
        for p in 11 12 13 14 15; do
            inst="$(ids "$ROOT/data/instances_hard" | grep "^$p" | paste -sd, -)"
            echo "ils_hard_10s_$v|ils|hard|10|$v|$inst"
        done
    done
    for v in 000 110 111; do
        i=0; while read -r inst; do
            echo "ils_ref_10s_$v|ils|ref|10|$v|$inst"; i=$((i + 1))
        done < <(chunks "$ROOT/data/instances" 12)
    done
    for p in 11 12 13 14 15; do
        inst="$(ids "$ROOT/data/instances_hard" | grep "^$p" | paste -sd, -)"
        echo "gls_hard_10s|gls|hard|10|-|$inst"
    done
    for v in 000 001 100 010 110 111; do
        echo "ils_ref_0.55s_$v|ils|ref|0.55|$v|$REF_ALL"
    done
    echo "abl_0.55s_gls|abl|ref|0.55|gls|$REF_ALL"
    for s in 0 1; do for i in 0 1; do for l in 0 1; do for r in 0 1; do
        echo "abl_0.55s_i${i}_s${s}_l${l}_r${r}|abl|ref|0.55|i${i}_s${s}_l${l}_r${r}|$REF_ALL"
    done; done; done; done
    echo "gls_ref_10s|gls|ref|10|-|$REF_ALL"
    echo "gls_ref_0.55s|gls|ref|0.55|-|$REF_ALL"
} | { if [[ -n "${ONLY:-}" ]]; then grep -E "$ONLY"; else cat; fi; } \
  | { if [[ -n "${TIME_OVERRIDE:-}" ]]; then awk -F"|" -v OFS="|" -v t="$TIME_OVERRIDE" '{ $4 = t; print }'; else cat; fi; } > "$QUEUE"
echo "$(wc -l < "$QUEUE") jobs na fila"

pop() {
    (
        flock 9
        line="$(head -n1 "$QUEUE")"
        [[ -n "$line" ]] && sed -i 1d "$QUEUE"
        printf '%s' "$line"
    ) 9>"$QUEUE.lock"
}

run_job() {  # <núcleo> <linha>
    local core="$1" name kind set time variant inst
    IFS='|' read -r name kind set time variant inst <<< "$2"
    local tag; tag="$(cut -d, -f1 <<< "$inst")-$(awk -F, '{print $NF}' <<< "$inst")"
    local out="$RAW/$name/$tag.csv"
    [[ -f "$out" ]] && { echo "skip $name $tag"; return; }
    mkdir -p "$RAW/$name"
    local tmp="$out.tmp" log="$RAW/$name/$tag.log"
    local start=$SECONDS
    case "$kind" in
        ils)
            local dir_args=()
            [[ "$set" == hard ]] && dir_args=(-Dsso.dir="$ROOT/data/instances_hard" \
                                              -Dsso.logdir="$ROOT/data/Log_hard")
            (cd "$JAVA_PROJECT" && taskset -c "$core" java -XX:+UseSerialGC -Xmx1g \
                -Dsso.ils=3 -Dsso.time="$time" -Dsso.reps="$ILS_REPS" \
                -Dsso.incremental="${variant:0:1}" -Dsso.replacement="${variant:1:1}" \
                -Dsso.keepbest="${variant:2:1}" -Dsso.instances="$inst" "${dir_args[@]}" \
                -Dsso.csv="$tmp" -cp "bin:lib/commons-math3-3.6.1.jar" main.ArticleResult) \
                > "$log" 2>&1 ;;
        gls)
            local dirs=(SSO_INSTANCE_DIR=data/instances SSO_LOG_DIR=data/Log)
            [[ "$set" == hard ]] && dirs=(SSO_INSTANCE_DIR=data/instances_hard SSO_LOG_DIR=data/Log_hard)
            # shellcheck disable=SC2086
            env $TUNED_ENV "${dirs[@]}" SSO_INSTANCES="$inst" SSO_REPETITIONS="$GLS_REPS" \
                SSO_TIME_SECONDS="$time" SSO_CSV="$tmp" \
                taskset -c "$core" ./build/service-selection-optimizer > "$log" 2>&1 ;;
        abl)
            local factorial=1; [[ "$variant" == gls ]] && factorial=0
            # shellcheck disable=SC2086
            env $ABL_ENV SSO_FACTORIAL="$factorial" SSO_TIME_SECONDS="$time" \
                SSO_INSTANCE_SET=all SSO_REPETITIONS="$GLS_REPS" SSO_VARIANTS="$variant" \
                SSO_EXPERIMENT_OUTPUT="$tmp" \
                taskset -c "$core" ./build/compare-gls-improvements > "$log" 2>&1 ;;
    esac
    mv "$tmp" "$out"
    echo "[núcleo $core] $name $tag em $((SECONDS - start)) s"
}

worker() {
    local core="$1" line
    while line="$(pop)"; [[ -n "$line" ]]; do
        run_job "$core" "$line" || echo "[núcleo $core] FALHOU: $line"
    done
}

for core in $(seq 0 $((WORKERS - 1))); do
    worker "$core" &
done
wait
echo "Bateria concluída. Agora execute: python3 experiments/13_calibrada_20exec/scripts/merge_results.py"
