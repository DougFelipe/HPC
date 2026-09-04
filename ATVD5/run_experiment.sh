#!/usr/bin/env bash

set -e

SOURCE="tarefa5.c"
OUTPUT="tarefa5.exe"

echo "======================================"
echo "Compilando Tarefa 5"
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