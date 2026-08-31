#!/usr/bin/env bash
# Compara três modos do mesmo núcleo GLS/GFLS otimizado: SLA dura, SLA sempre
# suave e SLA suave condicional. Todas as respostas finais permanecem viáveis.

set -euo pipefail
cd "$(dirname "$0")/.."

make experiment-gls-soft-sla
./build/compare-gls-soft-sla
python3 scripts/summarize_gls_soft_sla_experiment.py
