#!/usr/bin/env bash
set -euo pipefail

SRC="tarefa9.c"
BIN="tarefa9"
CC="${CC:-gcc}"

CFLAGS=(
    -std=c11
    -O2
    -Wall
    -Wextra
    -Wpedantic
    -fopenmp
)

echo "======================================"
echo "Compilando Tarefa 9"
echo "======================================"
echo

"$CC" "${CFLAGS[@]}" "$SRC" -o "$BIN"

echo "Compilacao concluida."
echo
echo "======================================"
echo "Executando experimento"
echo "======================================"
echo

"./$BIN"
