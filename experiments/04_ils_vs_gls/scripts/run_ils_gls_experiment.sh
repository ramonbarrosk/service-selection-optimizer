#!/usr/bin/env bash
# ARQUIVADO: este script depende de código removido na limpeza (ILS, oscilação
# ou GLS com SLA flexível). Para executá-lo, use a tag archive/pre-cleanup:
#   git checkout archive/pre-cleanup
# Compara três mecanismos sob o mesmo orçamento por instância e repetição:
# ILS original, GLS sem ILS e GLS com oscilação sem ILS.
#
# Exemplo rápido, com duas instâncias:
#   SSO_INSTANCES=1,100 SSO_REPETITIONS=1 SSO_TIME_SECONDS=0.1 \
#       bash experiments/04_ils_vs_gls/scripts/run_ils_gls_experiment.sh
#
# Orçamento curto fixo:
#   SSO_REPETITIONS=1 SSO_TIME_SECONDS=0.55 \
#       bash experiments/04_ils_vs_gls/scripts/run_ils_gls_experiment.sh
#
# Sem SSO_TIME_SECONDS, usa o tempo até o melhor resultado medido no relatório
# Java, com piso de 0,55 segundo. O piso pode ser alterado por
# SSO_MIN_TIME_SECONDS, mas nunca varia entre os três métodos comparados.

set -euo pipefail
cd "$(dirname "$0")/../../.."

make experiment-ils-gls
experiment_output="${SSO_EXPERIMENT_OUTPUT:-experiments/04_ils_vs_gls/results/ils_gls_java_measured_time_comparison.csv}"
export SSO_EXPERIMENT_OUTPUT="$experiment_output"
./build/compare-ils-gls
python3 experiments/04_ils_vs_gls/scripts/summarize_ils_gls_experiment.py \
    "$experiment_output" "${experiment_output%.csv}_summary.md"
