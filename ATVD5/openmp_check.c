#include <stdio.h>
#include <omp.h>

int main(void)
{
#ifdef _OPENMP
    printf("OpenMP habilitado.\n");
    printf("_OPENMP = %d\n", _OPENMP);
#else
    printf("OpenMP NAO habilitado na compilacao.\n");
#endif

    printf("Versao runtime / teste de threads:\n");

#pragma omp parallel
    {
#pragma omp single
        {
            printf("Numero de threads = %d\n", omp_get_num_threads());
        }
    }

    return 0;
}
