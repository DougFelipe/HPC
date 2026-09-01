#!/bin/sh

# Script de apoio para executar a Tarefa 3 no diretorio ATVD3.
# Ele compila os programas e salva as saidas em arquivos de texto
# para uso no relatorio tecnico.

set -eu

echo "Diretorio atual:"
pwd

echo
echo "Compilando cont1.c..."
gcc cont1.c -o cont1

echo
echo "Executando cont1.c 3 vezes..."
: > saida_cont1.txt
i=1
while [ "$i" -le 3 ]; do
    echo "===== EXECUCAO cont1 #$i =====" >> saida_cont1.txt
    ./cont1 >> saida_cont1.txt
    echo >> saida_cont1.txt
    i=$((i + 1))
done

echo
echo "Compilando cont2.c..."
gcc cont2.c -o cont2 -pthread

echo
echo "Executando cont2.c 10 vezes..."
: > saida_cont2.txt
i=1
while [ "$i" -le 10 ]; do
    echo "===== EXECUCAO cont2 #$i =====" >> saida_cont2.txt
    ./cont2 >> saida_cont2.txt
    echo >> saida_cont2.txt
    i=$((i + 1))
done

echo
echo "Arquivos gerados:"
echo " - saida_cont1.txt"
echo " - saida_cont2.txt"
