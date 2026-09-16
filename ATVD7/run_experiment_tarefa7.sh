#!/usr/bin/env bash
set -e

SOURCE="tarefa7.c"
OUTPUT="tarefa7.exe"

echo "======================================"
echo "Compilando Tarefa 7"
echo "======================================"
echo

gcc \
    -std=c11 \
    -O2 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -fopenmp \
    "$SOURCE" \
    -o "$OUTPUT"

echo "Compilacao concluida."
echo

echo "======================================"
echo "Executando experimento"
echo "======================================"
echo

./"$OUTPUT"
