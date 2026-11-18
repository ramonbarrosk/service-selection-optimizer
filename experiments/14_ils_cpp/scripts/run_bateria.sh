#!/usr/bin/env bash
# Fase 14: ILS#3 original portado para C++ (tools/ils_run.cpp), no mesmo protocolo
# da fase 13 (20 execuções; 94 de referência com 10 s e 0,55 s; 75 geradas com 10 s).
# Responde à pergunta do orientador: quanto da diferença ILS × GLS vem da linguagem?
#
# Fila consumida por WORKERS processos presos a núcleos (taskset). Jobs com CSV já
# gravado são pulados, então o script retoma a bateria se for interrompido.
#   WORKERS=2 bash experiments/14_ils_cpp/scripts/run_bateria.sh
# INCREMENTAL=1 REPLACEMENT=1 roda a ILS em C++ com avaliação incremental e substituição
# de serviço; os jobs passam a se chamar cppis_* em vez de cpp_*.
set -euo pipefail

ROOT="$(pwd)"
D="$ROOT/experiments/14_ils_cpp"
RAW="${RAW:-$D/results/raw}"
QUEUE="$RAW/queue.txt"
WORKERS="${WORKERS:-2}"
REPS="${REPS:-20}"
INCREMENTAL="${INCREMENTAL:-0}"
REPLACEMENT="${REPLACEMENT:-0}"
PREFIX=cpp
[[ "$INCREMENTAL$REPLACEMENT" == "11" ]] && PREFIX=cppis
[[ "$INCREMENTAL$REPLACEMENT" == "10" ]] && PREFIX=cppi
[[ "$INCREMENTAL$REPLACEMENT" == "01" ]] && PREFIX=cpps
mkdir -p "$RAW"
make build/ils-run >/dev/null

ids() { ls "$1" | sed 's/Instance_10_10_//' | sort -n; }
chunks() {
    ids "$1" | awk -v n="$2" '
        { b = b (b == "" ? "" : ",") $0 } NR % n == 0 { print b; b = "" } END { if (b != "") print b }'
}

{
    for p in 11 12 13 14 15; do
        echo "${PREFIX}_hard_10s|hard|10|$(ids "$ROOT/data/instances_hard" | grep "^$p" | paste -sd, -)"
    done
    while read -r inst; do echo "${PREFIX}_ref_10s|ref|10|$inst"; done < <(chunks "$ROOT/data/instances" 12)
    while read -r inst; do echo "${PREFIX}_ref_0.55s|ref|0.55|$inst"; done < <(chunks "$ROOT/data/instances" 47)
} | { if [[ -n "${ONLY:-}" ]]; then grep -E "$ONLY"; else cat; fi; } \
  | { if [[ -n "${TIME_OVERRIDE:-}" ]]; then awk -F"|" -v OFS="|" -v t="$TIME_OVERRIDE" '{ $3 = t; print }'; else cat; fi; } > "$QUEUE"
echo "$(wc -l < "$QUEUE") jobs na fila"

pop() {
    ( flock 9
      line="$(head -n1 "$QUEUE")"
      [[ -n "$line" ]] && sed -i 1d "$QUEUE"
      printf '%s' "$line" ) 9>"$QUEUE.lock"
}

run_job() {
    local core="$1" name set time inst
    IFS='|' read -r name set time inst <<< "$2"
    local tag; tag="$(cut -d, -f1 <<< "$inst")-$(awk -F, '{print $NF}' <<< "$inst")"
    local out="$RAW/$name/$tag.csv"
    [[ -f "$out" ]] && { echo "skip $name $tag"; return; }
    mkdir -p "$RAW/$name"
    local dirs=(SSO_INSTANCE_DIR=data/instances SSO_LOG_DIR=data/Log)
    [[ "$set" == hard ]] && dirs=(SSO_INSTANCE_DIR=data/instances_hard SSO_LOG_DIR=data/Log_hard)
    local start=$SECONDS
    env "${dirs[@]}" SSO_ILS_INCREMENTAL="$INCREMENTAL" SSO_ILS_REPLACEMENT="$REPLACEMENT" SSO_INSTANCES="$inst" SSO_REPETITIONS="$REPS" SSO_TIME_SECONDS="$time" \
        SSO_CSV="$out.tmp" taskset -c "$core" ./build/ils-run > "$RAW/$name/$tag.log" 2>&1
    mv "$out.tmp" "$out"
    echo "[núcleo $core] $name $tag em $((SECONDS - start)) s"
}

worker() {
    local core="$1" line
    while line="$(pop)"; [[ -n "$line" ]]; do
        run_job "$core" "$line" || echo "[núcleo $core] FALHOU: $line"
    done
}
for core in $(seq 0 $((WORKERS - 1))); do worker "$core" & done
wait
# Junta os lotes em results/<job>.csv
for dir in "$RAW"/"$PREFIX"_*/; do
    name="$(basename "$dir")"
    files=("$dir"*.csv)
    { head -n1 "${files[0]}"; for f in "${files[@]}"; do tail -n +2 "$f"; done; } \
        | sort -t, -k1,1n -k2,2n -s > "$RAW/../$name.csv"
done
echo "Bateria concluída: resultados em $RAW/.."
