#include <stdio.h>
#include <omp.h>
int main(void) {
    printf("_OPENMP=%d\n", _OPENMP);
    printf("omp_get_num_procs()=%d\n", omp_get_num_procs());
    printf("omp_get_max_threads()=%d\n", omp_get_max_threads());
    #pragma omp parallel
    {
        #pragma omp single
        printf("effective_parallel_threads=%d\n", omp_get_num_threads());
    }
    return 0;
}
