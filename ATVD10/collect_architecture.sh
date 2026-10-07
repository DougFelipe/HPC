#!/usr/bin/env bash
# Coleta complementar; nao instala ferramentas nem executa benchmarks.
set -u
export LC_ALL=C
export PATH="/usr/bin:/bin:$PATH"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$script_dir" || exit 1
case "$(uname -s)" in
  MSYS*|MINGW*|CYGWIN*) environment=msys2; export PATH="/ucrt64/bin:/mingw64/bin:$PATH" ;;
  *) if [ -r /proc/sys/kernel/osrelease ] && grep -qi microsoft /proc/sys/kernel/osrelease; then environment=wsl; else environment=linux; fi ;;
esac
out="results/$environment"
mkdir -p "$out" || exit 1
capture() {
  local name="$1"; shift
  { printf 'Command:'; printf ' %q' "$@"; printf '\n'; "$@"; code=$?; printf '\nExit code: %s\n' "$code"; } > "$out/$name.txt" 2>&1
}
{
  printf 'Time: '; date -Iseconds
  printf 'Environment: %s\n' "$environment"
  printf 'WSL/MSYS evidence is separate from native Windows evidence.\n'
  printf 'Unavailable tools: nao identificado; lookup limited to PATH.\n'
} > "$out/metadata.txt"
capture uname uname -a
for tool in lscpu free numactl gcc clang mpicc mpiexec mpirun nvcc nvidia-smi; do
  if command -v "$tool" >/dev/null 2>&1; then
    command -v "$tool" >> "$out/metadata.txt"
    case "$tool" in
      lscpu) capture lscpu lscpu ;;
      free) capture memory free -b ;;
      numactl) capture numa numactl --hardware ;;
      nvidia-smi) capture nvidia_smi nvidia-smi ;;
      *) capture "${tool}_version" "$tool" --version ;;
    esac
  else printf '%s: nao identificado\n' "$tool" >> "$out/metadata.txt"; fi
done
if [ -r /proc/cpuinfo ]; then capture cpuinfo cat /proc/cpuinfo; fi
if [ -r /proc/meminfo ]; then capture meminfo cat /proc/meminfo; fi
if command -v gcc >/dev/null 2>&1; then
  capture gcc_native_flags gcc -march=native -Q --help=target
  exe="$out/openmp_test"; case "$environment" in msys2) exe="$exe.exe" ;; esac
  if gcc -O2 -fopenmp probes/openmp_test.c -o "$exe" > "$out/openmp_compile.txt" 2>&1; then
    printf 'gcc -O2 -fopenmp probes/openmp_test.c -o %q\n' "$exe" >> "$out/openmp_compile.txt"
    capture openmp_test "$exe"
  else printf '\nCompilation failed; test not run.\n' >> "$out/openmp_compile.txt"; fi
fi
printf 'Complementary evidence: %s\n' "$out"
