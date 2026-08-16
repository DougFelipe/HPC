#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <string.h>

#ifndef OPT_LEVEL
#define OPT_LEVEL "unknown"
#endif

#define WARMUP_RUNS 3
#define MEASURE_RUNS 20
#define NUM_ACCUMULATORS 4

#if defined(__GNUC__)
#define NOINLINE __attribute__((noinline))
#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
#define NOINLINE
#define COMPILER_BARRIER() ((void)0)
#endif

/*
 * Evita que resultados das somas sejam considerados inúteis
 * pelo compilador.
 */
static volatile int benchmark_sink = 0;

/*
 * Valores escolhidos em função da hierarquia de cache da máquina:
 *
 *   32.768      ints = 128 KiB
 *   65.536      ints = 256 KiB
 *   262.144     ints =   1 MiB
 *   1.048.576   ints =   4 MiB
 *   2.097.152   ints =   8 MiB
 *   4.194.304   ints =  16 MiB
 *   16.777.216  ints =  64 MiB
 */
static const size_t N_VALUES[] = {
    32768,
    65536,
    262144,
    1048576,
    2097152,
    4194304,
    16777216
};

#define NUM_N_VALUES (sizeof(N_VALUES) / sizeof(N_VALUES[0]))


/* ============================================================
 * RELÓGIO
 * ============================================================
 */

static double elapsed_ms(
    const struct timespec *start,
    const struct timespec *end
)
{
    double seconds =
        (double)(end->tv_sec - start->tv_sec);

    double nanoseconds =
        (double)(end->tv_nsec - start->tv_nsec);

    return (seconds * 1000.0) +
           (nanoseconds / 1000000.0);
}


/* ============================================================
 * LOOP 1
 *
 * Inicialização independente dos elementos do vetor.
 *
 * Cada iteração escreve em uma posição distinta.
 * ============================================================
 */

static NOINLINE void initialize_vector(int *vector, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        vector[i] = (int)((i & 7u) + 1u);
    }
}


/* ============================================================
 * LOOP 2
 *
 * Soma com um único acumulador.
 *
 * Existe uma dependência carregada entre iterações:
 *
 * sum(i + 1) depende de sum(i)
 * ============================================================
 */

static NOINLINE int sum_single_accumulator(
    const int *vector,
    size_t n
)
{
    int sum = 0;

    for (size_t i = 0; i < n; ++i) {
        sum += vector[i];
    }

    return sum;
}


/* ============================================================
 * LOOP 3
 *
 * Soma com quatro acumuladores independentes.
 *
 * As quatro cadeias de dependência podem avançar de maneira
 * independente, aumentando o ILP disponível.
 * ============================================================
 */

static NOINLINE int sum_four_accumulators(
    const int *vector,
    size_t n
)
{
    int sum0 = 0;
    int sum1 = 0;
    int sum2 = 0;
    int sum3 = 0;

    size_t i = 0;

    for (; i + 3 < n; i += 4) {
        sum0 += vector[i];
        sum1 += vector[i + 1];
        sum2 += vector[i + 2];
        sum3 += vector[i + 3];
    }

    /*
     * Tratamento genérico caso N não seja múltiplo de 4.
     * Os valores usados neste experimento são múltiplos de 4,
     * mas mantemos o código correto para qualquer N.
     */
    for (; i < n; ++i) {
        sum0 += vector[i];
    }

    return sum0 + sum1 + sum2 + sum3;
}


/* ============================================================
 * FUNÇÕES DE MEDIÇÃO
 * ============================================================
 */

static double measure_initialization(
    int *vector,
    size_t n
)
{
    struct timespec start;
    struct timespec end;

    COMPILER_BARRIER();

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    COMPILER_BARRIER();

    initialize_vector(vector, n);

    COMPILER_BARRIER();

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    COMPILER_BARRIER();

    return elapsed_ms(&start, &end);
}


static double measure_single(
    const int *vector,
    size_t n,
    int *result
)
{
    struct timespec start;
    struct timespec end;

    COMPILER_BARRIER();

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    COMPILER_BARRIER();

    *result = sum_single_accumulator(vector, n);

    COMPILER_BARRIER();

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    COMPILER_BARRIER();

    return elapsed_ms(&start, &end);
}


static double measure_multi4(
    const int *vector,
    size_t n,
    int *result
)
{
    struct timespec start;
    struct timespec end;

    COMPILER_BARRIER();

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    COMPILER_BARRIER();

    *result = sum_four_accumulators(vector, n);

    COMPILER_BARRIER();

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    COMPILER_BARRIER();

    return elapsed_ms(&start, &end);
}


/* ============================================================
 * ESTATÍSTICA
 * ============================================================
 */

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a;
    double y = *(const double *)b;

    if (x < y) {
        return -1;
    }

    if (x > y) {
        return 1;
    }

    return 0;
}


static double median(double values[], size_t count)
{
    qsort(
        values,
        count,
        sizeof(double),
        compare_double
    );

    if ((count % 2) == 0) {
        return (
            values[(count / 2) - 1] +
            values[count / 2]
        ) / 2.0;
    }

    return values[count / 2];
}


/* ============================================================
 * VALIDAÇÃO DO RESULTADO
 * ============================================================
 */

static int expected_sum(size_t n)
{
    /*
     * O vetor contém repetidamente:
     *
     * 1 2 3 4 5 6 7 8
     *
     * Cada bloco de 8 elementos soma 36.
     *
     * Todos os N deste experimento são múltiplos de 8.
     */

    long long value =
        (long long)(n / 8u) * 36LL;

    if (value > INT32_MAX) {
        fprintf(
            stderr,
            "Erro: soma esperada excede o limite de int.\n"
        );

        exit(EXIT_FAILURE);
    }

    return (int)value;
}


/* ============================================================
 * BENCHMARK PARA UM VALOR DE N
 * ============================================================
 */

static int run_for_n(size_t n)
{
    size_t bytes = n * sizeof(int);

    int *vector = malloc(bytes);

    if (vector == NULL) {
        fprintf(
            stderr,
            "Erro: nao foi possivel alocar %zu bytes.\n",
            bytes
        );

        return 0;
    }

    /*
     * Primeira escrita fora da região medida.
     *
     * Isso força a materialização das páginas de memória antes
     * do benchmark e reduz a interferência de page faults na
     * primeira inicialização cronometrada.
     */
    memset(vector, 0, bytes);

    COMPILER_BARRIER();

    /*
     * Warm-up.
     *
     * Nenhum destes tempos entra no resultado final.
     */
    for (int w = 0; w < WARMUP_RUNS; ++w) {

        initialize_vector(vector, n);

        int warm_single =
            sum_single_accumulator(vector, n);

        int warm_multi =
            sum_four_accumulators(vector, n);

        if (warm_single != warm_multi) {
            fprintf(
                stderr,
                "Erro durante warm-up para N=%zu.\n",
                n
            );

            free(vector);
            return 0;
        }

        benchmark_sink = warm_single;
        benchmark_sink = warm_multi;
    }

    double initialization_times[MEASURE_RUNS];
    double single_times[MEASURE_RUNS];
    double multi_times[MEASURE_RUNS];

    int last_single = 0;
    int last_multi = 0;

    /*
     * Medições.
     *
     * A ordem Single/Multi4 é alternada entre repetições para
     * reduzir vantagem sistemática causada pelo estado do cache.
     */
    for (int r = 0; r < MEASURE_RUNS; ++r) {

        initialization_times[r] =
            measure_initialization(vector, n);

        int single_result = 0;
        int multi_result = 0;

        if ((r % 2) == 0) {

            single_times[r] =
                measure_single(
                    vector,
                    n,
                    &single_result
                );

            benchmark_sink = single_result;

            multi_times[r] =
                measure_multi4(
                    vector,
                    n,
                    &multi_result
                );

            benchmark_sink = multi_result;

        } else {

            multi_times[r] =
                measure_multi4(
                    vector,
                    n,
                    &multi_result
                );

            benchmark_sink = multi_result;

            single_times[r] =
                measure_single(
                    vector,
                    n,
                    &single_result
                );

            benchmark_sink = single_result;
        }

        if (single_result != multi_result) {

            fprintf(
                stderr,
                "Erro: resultados diferentes para N=%zu "
                "(single=%d, multi=%d).\n",
                n,
                single_result,
                multi_result
            );

            free(vector);
            return 0;
        }

        last_single = single_result;
        last_multi = multi_result;
    }

    int expected = expected_sum(n);

    if (
        last_single != expected ||
        last_multi != expected
    ) {

        fprintf(
            stderr,
            "Erro de validacao para N=%zu. "
            "Esperado=%d, single=%d, multi=%d\n",
            n,
            expected,
            last_single,
            last_multi
        );

        free(vector);
        return 0;
    }

    double init_median =
        median(
            initialization_times,
            MEASURE_RUNS
        );

    double single_median =
        median(
            single_times,
            MEASURE_RUNS
        );

    double multi_median =
        median(
            multi_times,
            MEASURE_RUNS
        );

    double speedup = 0.0;

    if (multi_median > 0.0) {
        speedup =
            single_median / multi_median;
    }

    double size_mib =
        (double)bytes /
        (1024.0 * 1024.0);

    printf(
        "%-11zu %10.3f %12.6f %14.6f %14.6f %9.3fx\n",
        n,
        size_mib,
        init_median,
        single_median,
        multi_median,
        speedup
    );

    free(vector);

    return 1;
}


/* ============================================================
 * MAIN
 * ============================================================
 */

int main(void)
{
    printf("\n");
    printf("ILP Benchmark\n");
    printf("Optimization : %s\n", OPT_LEVEL);
    printf("Data type    : int (%zu bytes)\n", sizeof(int));
    printf("Clock        : CLOCK_MONOTONIC\n");
    printf("Warm-ups     : %d\n", WARMUP_RUNS);
    printf("Measurements : %d\n", MEASURE_RUNS);
    printf("Accumulators : %d\n", NUM_ACCUMULATORS);

    printf("\n");

    printf(
        "%-11s %10s %12s %14s %14s %9s\n",
        "N",
        "Size(MiB)",
        "Init(ms)",
        "Single(ms)",
        "Multi4(ms)",
        "Speedup"
    );

    printf(
        "-------------------------------------------------------------------------------\n"
    );

    for (size_t i = 0; i < NUM_N_VALUES; ++i) {

        if (!run_for_n(N_VALUES[i])) {
            printf("\nStatus: ERROR\n");
            return EXIT_FAILURE;
        }
    }

    printf(
        "-------------------------------------------------------------------------------\n"
    );

    printf("Status: OK\n");
    printf("Sink: %d\n", benchmark_sink);

    printf("\n");

    return EXIT_SUCCESS;
}