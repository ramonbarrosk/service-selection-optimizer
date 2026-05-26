#!/usr/bin/env bash
# Executa o ILS Java e salva a saída para uso no script de visualização.
# Execute a partir da raiz do projeto C++ (service-selection-optimizer):
#   bash scripts/run_java.sh

set -euo pipefail

JAVA_PROJECT="/home/ramon/MyProjects/Service-Selection-Under-Uncertainty/Service Selection Under Uncertainty"
OUTPUT="data/java_results_raw.txt"

if [ ! -d "$JAVA_PROJECT/bin" ]; then
    echo "[ERRO] Diretório bin não encontrado: $JAVA_PROJECT/bin"
    echo "Compile o projeto Java primeiro."
    exit 1
fi

echo "Executando ILS Java a partir de: $JAVA_PROJECT"
echo "Saída será salva em: $OUTPUT"
echo "---"

(cd "$JAVA_PROJECT" && java -cp bin main.ArticleResult) 2>&1 | tee "$OUTPUT"

echo "---"
echo "Concluído. Resultados salvos em $OUTPUT"
echo "Agora execute: python scripts/visualize.py --java-raw $OUTPUT"
