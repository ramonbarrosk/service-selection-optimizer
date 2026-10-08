#!/usr/bin/env bash
# ARQUIVADO: este script depende de código removido na limpeza (ILS, oscilação
# ou GLS com SLA flexível). Para executá-lo, use a tag archive/pre-cleanup:
#   git checkout archive/pre-cleanup
# Compara três modos do mesmo núcleo GLS/GFLS otimizado: SLA dura, SLA sempre
# suave e SLA suave condicional. Todas as respostas finais permanecem viáveis.

set -euo pipefail
cd "$(dirname "$0")/../../.."

make experiment-gls-soft-sla
./build/compare-gls-soft-sla
python3 experiments/05_gls_sla_flexivel/scripts/summarize_gls_soft_sla_experiment.py
