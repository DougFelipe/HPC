#include <stdio.h>
#include <omp.h>

int main(void)
{
    long long contador_seq = 0;
    long long contador_par = 0;
    const int limite = 1000000;
    int i;
    double t0;
    double t1;

    t0 = omp_get_wtime();
    for (i = 1; i <= limite; ++i) {
        contador_seq += i % 2;
    }
    t1 = omp_get_wtime();
    printf("Sequencial: contador=%lld tempo=%.6f\n", contador_seq, t1 - t0);

    t0 = omp_get_wtime();
#pragma omp parallel for reduction(+:contador_par) schedule(static)
    for (i = 1; i <= limite; ++i) {
        contador_par += i % 2;
    }
    t1 = omp_get_wtime();
    printf("Paralelo(static): contador=%lld tempo=%.6f threads=%d\n",
           contador_par, t1 - t0, omp_get_max_threads());

    contador_par = 0;
    t0 = omp_get_wtime();
#pragma omp parallel for reduction(+:contador_par) schedule(dynamic, 1000)
    for (i = 1; i <= limite; ++i) {
        contador_par += i % 2;
    }
    t1 = omp_get_wtime();
    printf("Paralelo(dynamic): contador=%lld tempo=%.6f threads=%d\n",
           contador_par, t1 - t0, omp_get_max_threads());

    if (contador_seq != 500000 || contador_par != 500000) {
        fprintf(stderr, "ERRO: resultado inconsistente.\n");
        return 1;
    }

    if (contador_seq != contador_par) {
        fprintf(stderr, "ERRO: sequencial e paralelo diferem.\n");
        return 1;
    }

    puts("Validacao: parallel for, reduction e omp_get_wtime OK.");
    return 0;
}
