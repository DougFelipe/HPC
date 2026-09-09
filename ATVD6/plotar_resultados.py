#!/usr/bin/env python3
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt

CSV = Path("resultados.csv")

if not CSV.exists():
    raise SystemExit("Arquivo resultados.csv nao encontrado.")

por_versao = defaultdict(list)

with CSV.open(newline="", encoding="utf-8") as arquivo:
    for linha in csv.DictReader(arquivo):
        por_versao[linha["versao"]].append({
            "n": int(linha["N"]),
            "erro": int(linha["erro_abs"]),
            "tempo": float(linha["tempo_medio_ms"]),
            "speedup": float(linha["speedup"]),
        })

for valores in por_versao.values():
    valores.sort(key=lambda item: item["n"])

# Tempo medio x N
plt.figure()
for versao, valores in por_versao.items():
    plt.plot(
        [v["n"] for v in valores],
        [v["tempo"] for v in valores],
        marker="o",
        label=versao,
    )
plt.xscale("log")
plt.yscale("log")
plt.xlabel("Numero de subdivisoes (N)")
plt.ylabel("Tempo medio (ms)")
plt.title("Tempo medio de execucao")
plt.grid(True, which="both", alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig("tempo_medio.png", dpi=180)
plt.close()

# Speedup x N: somente implementacoes funcionalmente corretas.
plt.figure()
for versao in ("Critical", "Atomic", "Reduction"):
    valores = por_versao.get(versao, [])
    if valores:
        plt.plot(
            [v["n"] for v in valores],
            [v["speedup"] for v in valores],
            marker="o",
            label=versao,
        )
plt.axhline(1.0, linestyle="--", linewidth=1)
plt.xscale("log")
plt.xlabel("Numero de subdivisoes (N)")
plt.ylabel("Speedup em relacao ao sequencial")
plt.title("Speedup das versoes paralelas corretas")
plt.grid(True, which="both", alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig("speedup.png", dpi=180)
plt.close()

# Erro absoluto x N
plt.figure()
for versao, valores in por_versao.items():
    plt.plot(
        [v["n"] for v in valores],
        [v["erro"] for v in valores],
        marker="o",
        label=versao,
    )
plt.xscale("log")
plt.yscale("log")
plt.xlabel("Numero de subdivisoes (N)")
plt.ylabel("Erro absoluto")
plt.title("Erro absoluto das implementacoes")
plt.grid(True, which="both", alpha=0.3)
plt.legend()
plt.tight_layout()
plt.savefig("erro_absoluto.png", dpi=180)
plt.close()

print("Graficos gerados:")
print("  tempo_medio.png")
print("  speedup.png")
print("  erro_absoluto.png")
