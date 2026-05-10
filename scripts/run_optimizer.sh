#!/usr/bin/env bash
# Executa o otimizador C++ e salva a saída para uso no script de visualização.
# Execute a partir da raiz do projeto:
#   bash scripts/run_optimizer.sh

set -euo pipefail

BINARY="./build/service-selection-optimizer"
OUTPUT="data/cpp_results.txt"

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
echo "Agora execute: python scripts/visualize.py"
