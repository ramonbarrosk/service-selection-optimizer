#!/usr/bin/env bash
# Gera o conjunto de instâncias difíceis em data/instances_hard.
#
# Cada perfil mantém as distribuições do gerador original e muda um único fator:
#   T97..T100  capacidade apertada: soma dos consumos / (Smax * Vres) = 0,97 a 1,00.
#              É o regime das instâncias 28 e 100, as únicas do artigo cujo ótimo
#              fica acima do limite inferior trivial.
#   C50        custo correlacionado com confiabilidade (rho = 0,5) e Vres original:
#              a SLA força serviços caros, e o limite trivial deixa de ser justo.
# Para cada perfil, as sementes são testadas em ordem a partir de 1 e entram as
# primeiras PER_PROFILE instâncias em que a construção inicial acha solução viável.
# Nenhuma instância é escolhida pelo desempenho dos métodos.
#
# Os arquivos seguem o nome Instance_10_10_<id>, que os leitores C++ e Java esperam.
# O id é ID_BASE + 100 * <perfil> + <n>, por exemplo 1101 a 1115 para T97.
# OUT, MANIFEST, FIRST_SEED e ID_BASE geram outros conjuntos com os mesmos perfis,
# como o de treino da calibração (fase 11). Os padrões geram o conjunto de teste.
# Execute a partir da raiz do repositório:
#   bash experiments/10_instancias_dificeis/scripts/generate_instances.sh

set -euo pipefail

OUT="${OUT:-data/instances_hard}"
MANIFEST="${MANIFEST:-experiments/10_instancias_dificeis/results/manifest.csv}"
PER_PROFILE="${PER_PROFILE:-15}"
FIRST_SEED="${FIRST_SEED:-1}"
ID_BASE="${ID_BASE:-1000}"
MAX_SEED=$((FIRST_SEED + 999))

# perfil:tightness:corr
PROFILES=("T97:0.97:0" "T98:0.98:0" "T99:0.99:0" "T100:1.0:0" "C50:0:0.5")

make build/generate-instance
rm -rf "$OUT"
mkdir -p "$OUT" "$(dirname "$MANIFEST")"
echo "id,profile,seed,tightness,corr" > "$MANIFEST"

for index in "${!PROFILES[@]}"; do
    IFS=: read -r profile tightness corr <<< "${PROFILES[$index]}"
    count=0
    rejected=0
    seed="$FIRST_SEED"
    while [ "$count" -lt "$PER_PROFILE" ]; do
        if [ "$seed" -gt "$MAX_SEED" ]; then
            echo "[ERRO] $profile: só $count instâncias viáveis até a semente $MAX_SEED" >&2
            exit 1
        fi
        id=$((ID_BASE + 100 * (index + 1) + count + 1))
        if ./build/generate-instance --seed "$seed" --tightness "$tightness" --corr "$corr" \
                --out "$OUT/Instance_10_10_$id" --check-feasible; then
            echo "$id,$profile,$seed,$tightness,$corr" >> "$MANIFEST"
            count=$((count + 1))
        else
            rejected=$((rejected + 1))
        fi
        seed=$((seed + 1))
    done
    echo "$profile: $count instâncias, $rejected sementes descartadas por inviabilidade"
done

echo "Concluído. Instâncias em $OUT e manifesto em $MANIFEST"
