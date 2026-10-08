#!/bin/sh
# Varredura de alpha da GLS promovida nas 94 instâncias (0.55 s e 10 s, 1 execução:
# a GLS é determinística). Uso: sh data/experiments/alpha_sweep_all94/run.sh
cd "$(dirname "$0")/../../.." || exit 1
for alpha in 0.1 0.3 0.5 1.0 2.0; do echo $alpha; done | xargs -P4 -I{} sh -c \
  'SSO_ALPHA={} SSO_INSTANCE_SET=all SSO_VARIANTS=i1_s1_l0_r1 SSO_FACTORIAL=1 SSO_REPETITIONS=1 SSO_TIME_CURVE=0.55,10 SSO_EXPERIMENT_OUTPUT=data/experiments/alpha_sweep_all94/alpha_{}.csv ./build/compare-gls-improvements > data/experiments/alpha_sweep_all94/alpha_{}.log 2>&1'
