#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

SOURCE="tarefa8_openmp.c"
EXECUTABLE="tarefa8_openmp.exe"

echo "======================================"
echo "Compilando Tarefa 8"
echo "======================================"
echo

if ! command -v gcc >/dev/null 2>&1; then
    echo "Erro: gcc nao foi encontrado no PATH."
    exit 1
fi

gcc \
    -std=c11 \
    -O2 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -fopenmp \
    "$SOURCE" \
    -o "$EXECUTABLE" \
    -lm

echo "Compilacao concluida."
echo

echo "======================================"
echo "Executando experimento"
echo "======================================"
echo

"./$EXECUTABLE"
