#!/usr/bin/env bash
# Calibra a GLS com o irace no conjunto de treino (generate_training.sh) e grava as
# configurações de elite em results/best_configurations.csv, a melhor na primeira linha.
# Requer R com o pacote irace. Execute a partir da raiz do repositório:
#   bash experiments/11_calibracao_irace/scripts/run_irace.sh
set -euo pipefail

D="$(pwd)/experiments/11_calibracao_irace"
make build/gls-run
mkdir -p "$D/results/irace"
IRACE="$(Rscript -e 'cat(system.file("bin", "irace", package = "irace", mustWork = TRUE))')"

(cd "$D/irace" && "$IRACE" --scenario scenario.txt) 2>&1 | tee "$D/results/irace.log"

Rscript - "$D/results/irace/irace.Rdata" "$D/results/best_configurations.csv" <<'R'
args <- commandArgs(trailingOnly = TRUE)
if (exists("read_logfile", asNamespace("irace"))) {
    results <- irace::read_logfile(args[1])
} else {
    load(args[1])
    results <- iraceResults
}
elites <- results$allElites[[length(results$allElites)]]
configs <- results$allConfigurations
configs <- configs[match(elites, configs$.ID.), ]
configs <- configs[, !startsWith(names(configs), ".") | names(configs) == ".ID."]
write.csv(configs, args[2], row.names = FALSE)
print(configs)
R
