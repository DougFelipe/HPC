#!/usr/bin/env bash

set -e

echo "======================================"
echo "Compilando benchmark"
echo "======================================"

gcc -std=c11 -Wall -Wextra -O0 \
    -DOPT_LEVEL='"O0"' \
    benchmark.c \
    -o benchmark_O0.exe

gcc -std=c11 -Wall -Wextra -O2 \
    -DOPT_LEVEL='"O2"' \
    benchmark.c \
    -o benchmark_O2.exe

gcc -std=c11 -Wall -Wextra -O3 \
    -DOPT_LEVEL='"O3"' \
    benchmark.c \
    -o benchmark_O3.exe

echo
echo "Compilacao concluida."
echo

echo "======================================"
echo "Executando -O0"
echo "======================================"

./benchmark_O0.exe

echo "======================================"
echo "Executando -O2"
echo "======================================"

./benchmark_O2.exe

echo "======================================"
echo "Executando -O3"
echo "======================================"

./benchmark_O3.exe