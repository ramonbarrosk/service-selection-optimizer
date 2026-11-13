#!/usr/bin/env bash
# Gera o conjunto de treino da calibração em data/instances_train: os mesmos perfis do
# conjunto de teste (fase 10), 10 instâncias por perfil, com sementes a partir de
# 10001. O conjunto de teste usa sementes até 33, então os dois não se sobrepõem.
# Ids de 2101 (T97) a 2510 (C50). Execute a partir da raiz do repositório.
set -euo pipefail

D=experiments/11_calibracao_irace
OUT=data/instances_train MANIFEST=$D/results/train_manifest.csv \
    PER_PROFILE=10 FIRST_SEED=10001 ID_BASE=2000 \
    bash experiments/10_instancias_dificeis/scripts/generate_instances.sh
ls data/instances_train > $D/irace/train-instances.txt
echo "$(wc -l < $D/irace/train-instances.txt) instâncias em $D/irace/train-instances.txt"
