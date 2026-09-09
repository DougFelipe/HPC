#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

SRC="tarefa6.c"
EXE="tarefa6.exe"
OUT="saida.txt"

printf '%s\n' "======================================"
printf '%s\n' "Compilando Tarefa 6"
printf '%s\n' "======================================"

rm -f "$EXE" resultados.csv "$OUT" \
      tempo_medio.png speedup.png erro_absoluto.png

gcc -std=c11 -O2 -Wall -Wextra -Wpedantic \
    -fopenmp -fno-tree-vectorize \
    "$SRC" -o "$EXE"

printf '\nCompilacao concluida.\n\n'
printf '%s\n' "======================================"
printf '%s\n' "Executando experimento"
printf '%s\n' "======================================"
printf '\n'

export OMP_NUM_THREADS=4
export OMP_DYNAMIC=FALSE

./"$EXE" | tee "$OUT"

printf '\nResultados salvos em:\n'
printf '  %s\n' "$OUT" "resultados.csv"

PYTHON=""
if command -v python >/dev/null 2>&1; then
    PYTHON="python"
elif command -v python3 >/dev/null 2>&1; then
    PYTHON="python3"
fi

if [[ -n "$PYTHON" ]] && "$PYTHON" -c 'import matplotlib' >/dev/null 2>&1; then
    printf '\nGerando graficos...\n'
    "$PYTHON" plotar_resultados.py
else
    printf '\nAviso: matplotlib nao esta disponivel; os graficos nao foram gerados.\n'
    printf 'O experimento e o CSV foram concluídos normalmente.\n'
fi
