#!/usr/bin/env bash
# Executa o método exato (MIP do projeto de referência, CPLEX com limite de 15 min)
# nas instâncias de data/instances_hard e grava um log por instância em
# data/Log_hard, no mesmo formato de data/Log.
#
# Só ficam em data/Log_hard os logs com "Status = Optimal". Se o CPLEX parar pelo
# limite de tempo, o log vai para data/Log_hard_timeout: o custo é só um limite
# superior, e os leitores C++ e Java não podem tratá-lo como ótimo.
# Instâncias que já têm log são puladas, então dá para retomar a execução.
#
# EXACT_BIN aponta para o executável "services" (compilado com o CPLEX estático);
# JOBS controla quantas instâncias rodam em paralelo (o CPLEX usa 1 thread).
# Execute a partir da raiz do repositório:
#   bash experiments/10_instancias_dificeis/scripts/run_exato.sh

set -euo pipefail

EXACT_BIN="${EXACT_BIN:-/mnt/h/Downloads/TCC/Services/Services/services}"
JOBS="${JOBS:-3}"
INSTANCES="$(pwd)/data/instances_hard"
LOGS="$(pwd)/data/Log_hard"
TIMEOUTS="$(pwd)/data/Log_hard_timeout"

mkdir -p "$LOGS" "$TIMEOUTS"
BIN="$(mktemp -d)/services"
cp "$EXACT_BIN" "$BIN"
chmod +x "$BIN"

solve() {
    local instance="$1" name
    name="$(basename "$instance")"
    if [ -f "$LOGS/$name" ] || [ -f "$TIMEOUTS/$name" ]; then
        return
    fi
    local work
    work="$(mktemp -d)"
    if (cd "$work" && "$BIN" "$instance" > "$work/$name" 2>&1) \
            && grep -q "Status = Optimal" "$work/$name"; then
        mv "$work/$name" "$LOGS/$name"
        echo "$name: ótimo $(tail -1 "$LOGS/$name" | sed 's/.*Cost = //')"
    else
        mv "$work/$name" "$TIMEOUTS/$name"
        echo "$name: sem ótimo provado ($(grep -m1 'Status = ' "$TIMEOUTS/$name" || echo 'erro'))"
    fi
    rm -rf "$work"
}
export -f solve
export BIN LOGS TIMEOUTS

ls "$INSTANCES"/Instance_10_10_* | xargs -P "$JOBS" -I{} bash -c 'solve "$@"' _ {}

echo "Concluído: $(ls "$LOGS" | wc -l) com ótimo, $(ls "$TIMEOUTS" | wc -l) sem ótimo provado."
