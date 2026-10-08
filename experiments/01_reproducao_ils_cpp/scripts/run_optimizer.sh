#!/usr/bin/env bash
# ARQUIVADO: este script depende de código removido na limpeza (ILS, oscilação
# ou GLS com SLA flexível). Para executá-lo, use a tag archive/pre-cleanup:
#   git checkout archive/pre-cleanup
# Executa o otimizador C++ e salva a saída para uso no script de visualização.
# Execute a partir da raiz do projeto:
#   bash experiments/01_reproducao_ils_cpp/scripts/run_optimizer.sh

set -euo pipefail

BINARY="./build/service-selection-optimizer"
OUTPUT="experiments/01_reproducao_ils_cpp/results/cpp_results.txt"

if [ ! -f "$BINARY" ]; then
    echo "[ERRO] Binário não encontrado: $BINARY"
    echo "Compile primeiro: make  ou  cmake --build build"
    exit 1
fi

echo "Executando $BINARY ..."
echo "Saída será salva em $OUTPUT"
echo "---"

"$BINARY" 2>&1 | tee "$OUTPUT"

echo "---"
echo "Concluído. Resultados salvos em $OUTPUT"
echo "Agora execute: python experiments/01_reproducao_ils_cpp/scripts/visualize.py"
