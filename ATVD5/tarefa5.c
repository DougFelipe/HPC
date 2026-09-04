/*
 * Tarefa 5 - Contagem de numeros primos com OpenMP
 *
 * Implementa tres versoes da contagem de primos:
 * sequencial, OpenMP ingenua (com race condition) e
 * OpenMP correta com reduction.
 *
 * O benchmark compara apenas as versoes corretas.
 * O laco externo e paralelizado; o teste de primalidade
 * permanece sequencial.
 */

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/* Configuracao do experimento. */
#define NUM_THREADS             4
#define NUM_WARMUPS             3
#define NUM_MEASUREMENTS        20

/* Duracao minima desejada para cada lote cronometrado. */
#define TARGET_BATCH_SECONDS    0.100

/* Limite de seguranca da calibracao automatica. */
#define MAX_INTERNAL_REPETITIONS 1048576

/* Valores de N avaliados no experimento. */
static const int N_VALUES[] = {
    10000,
    100000,
    1000000,
    5000000,
    10000000
};

/* Contagens conhecidas usadas na validacao. */
static const long long EXPECTED_PRIMES[] = {
    1229,
    9592,
    78498,
    348513,
    664579
};

#define NUM_SIZES \
    (sizeof(N_VALUES) / sizeof(N_VALUES[0]))

/* Assinatura comum das funcoes de contagem. */
typedef long long (*CountFunction)(int);

/*
 * Verifica se um valor e primo.
 * Testa apenas divisores impares ate a raiz quadrada implicita.
 */
static int is_prime(int value)
{
    
    /* Casos basicos e eliminacao imediata de pares. */
    if (value < 2) {
        return 0;
    }

    if (value == 2) {
        return 1;
    }

    if (value % 2 == 0) {
        return 0;
    }

    /* Testa apenas divisores impares; value/divisor evita overflow. */
    for (int divisor = 3;
         divisor <= value / divisor;
         divisor += 2) {

        if (value % divisor == 0) {
            return 0;
        }
    }

    return 1;
}

/* Conta primos entre 2 e n de forma sequencial. */
static long long count_primes_sequential(int n)
{
    long long count = 0;

    for (int candidate = 2;
         candidate <= n;
         ++candidate) {

        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}

/*
 * Versao OpenMP ingenua.
 * O contador compartilhado e atualizado sem sincronizacao,
 * produzindo uma race condition proposital.
 */
static long long count_primes_parallel_naive(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(static) shared(count)
    for (int candidate = 2;
         candidate <= n;
         ++candidate) {

        if (is_prime(candidate)) {
            /* Atualizacao compartilhada sem protecao: race condition. */
            ++count;
        }
    }

    return count;
}

/*
 * Versao OpenMP correta.
 * A reduction combina com seguranca os contadores das threads.
 */
static long long count_primes_parallel_reduction(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(static) reduction(+:count)
    for (int candidate = 2;
         candidate <= n;
         ++candidate) {

        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}

/* Valida a funcao de primalidade com casos conhecidos. */
static void validate_is_prime(void)
{
    struct PrimeTest {
        int value;
        int expected;
    };

    static const struct PrimeTest tests[] = {
        {0,  0},
        {1,  0},
        {2,  1},
        {3,  1},
        {4,  0},
        {5,  1},
        {9,  0},
        {17, 1},
        {25, 0}
    };

    const size_t num_tests =
        sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0;
         i < num_tests;
         ++i) {

        const int result =
            is_prime(tests[i].value);

        if (result != tests[i].expected) {

            fprintf(
                stderr,
                "ERROR: primality test failed for %d.\n",
                tests[i].value
            );

            exit(EXIT_FAILURE);
        }
    }
}

/* Retorna o numero de threads criado pelo runtime OpenMP. */
static int get_actual_thread_count(void)
{
    int actual_threads = 1;

    #pragma omp parallel
    {
        #pragma omp single
        {
            actual_threads =
                omp_get_num_threads();
        }
    }

    return actual_threads;
}

/*
 * Executa um lote cronometrado e valida o resultado agregado.
 * Retorna o tempo total do lote em segundos.
 */
static double run_timed_batch(
    CountFunction function,
    int n,
    long long expected,
    int repetitions
)
{
    
    /* Evita que chamadas repetidas sejam tratadas como trabalho invariavel. */
    volatile int guarded_n = n;

    long long checksum = 0;

    const double start =
        omp_get_wtime();

    for (int repetition = 0;
         repetition < repetitions;
         ++repetition) {

        const int current_n =
            guarded_n;

        checksum +=
            function(current_n);
    }

    const double end =
        omp_get_wtime();

    /* Validacao fora da regiao cronometrada. */
    const long long expected_checksum =
        expected * (long long) repetitions;

    if (checksum != expected_checksum) {

        fprintf(
            stderr,
            "ERROR: invalid benchmark result for N=%d.\n",
            n
        );

        exit(EXIT_FAILURE);
    }

    return end - start;
}

/*
 * Define repeticoes internas por duplicacao ate que os lotes
 * sequencial e paralelo atinjam a duracao minima desejada.
 */
static int calibrate_internal_repetitions(
    int n,
    long long expected
)
{
    int repetitions = 1;

    while (1) {

        const double seq_elapsed =
            run_timed_batch(
                count_primes_sequential,
                n,
                expected,
                repetitions
            );

        const double omp_elapsed =
            run_timed_batch(
                count_primes_parallel_reduction,
                n,
                expected,
                repetitions
            );

        if (seq_elapsed >= TARGET_BATCH_SECONDS &&
            omp_elapsed >= TARGET_BATCH_SECONDS) {

            return repetitions;
        }

        if (repetitions >=
            MAX_INTERNAL_REPETITIONS / 2) {

            return MAX_INTERNAL_REPETITIONS;
        }

        repetitions *= 2;
    }
}

/* Executa aquecimentos usando o mesmo tamanho de lote calibrado. */
static void run_warmups(
    int n,
    long long expected,
    int repetitions
)
{
    for (int i = 0;
         i < NUM_WARMUPS;
         ++i) {

        /* Alterna a ordem para reduzir vies de execucao. */
        if (i % 2 == 0) {

            (void) run_timed_batch(
                count_primes_sequential,
                n,
                expected,
                repetitions
            );

            (void) run_timed_batch(
                count_primes_parallel_reduction,
                n,
                expected,
                repetitions
            );
        }
        else {

            (void) run_timed_batch(
                count_primes_parallel_reduction,
                n,
                expected,
                repetitions
            );

            (void) run_timed_batch(
                count_primes_sequential,
                n,
                expected,
                repetitions
            );
        }
    }
}

/* Retorna o tempo medio de uma contagem dentro do lote. */
static double measure_per_execution(
    CountFunction function,
    int n,
    long long expected,
    int repetitions
)
{
    const double elapsed =
        run_timed_batch(
            function,
            n,
            expected,
            repetitions
        );

    return elapsed /
           (double) repetitions;
}

/* Comparador usado pelo qsort na ordenacao das medicoes. */
static int compare_double(
    const void *a,
    const void *b
)
{
    const double x =
        *(const double *) a;

    const double y =
        *(const double *) b;

    if (x < y) {
        return -1;
    }

    if (x > y) {
        return 1;
    }

    return 0;
}

/* Ordena as medicoes e calcula a mediana. */
static double calculate_median(
    double values[],
    size_t count
)
{
    qsort(
        values,
        count,
        sizeof(double),
        compare_double
    );

    if (count % 2 != 0) {
        return values[count / 2];
    }

    return (
        values[count / 2 - 1]
        +
        values[count / 2]
    ) / 2.0;
}

/* Coordena validacao, calibracao, benchmark e impressao dos resultados. */
int main(void)
{
    
    /* Mantem fixo o numero de threads do experimento. */
    omp_set_dynamic(0);

    omp_set_num_threads(NUM_THREADS);

    /* Validacao inicial da logica de primalidade. */
    validate_is_prime();

    const int actual_threads =
        get_actual_thread_count();

    if (actual_threads != NUM_THREADS) {

        fprintf(
            stderr,
            "ERROR: expected %d OpenMP threads, "
            "but runtime created %d.\n",
            NUM_THREADS,
            actual_threads
        );

        return EXIT_FAILURE;
    }

    long long sequential_results[NUM_SIZES];
    long long naive_results[NUM_SIZES];
    long long reduction_results[NUM_SIZES];

    int internal_repetitions[NUM_SIZES];

    /* Valida as tres versoes para cada N. */
    for (size_t i = 0;
         i < NUM_SIZES;
         ++i) {

        const int n =
            N_VALUES[i];

        sequential_results[i] =
            count_primes_sequential(n);

        naive_results[i] =
            count_primes_parallel_naive(n);

        reduction_results[i] =
            count_primes_parallel_reduction(n);

        if (sequential_results[i] !=
            EXPECTED_PRIMES[i]) {

            fprintf(
                stderr,
                "ERROR: expected %lld primes for N=%d, "
                "but sequential version returned %lld.\n",
                EXPECTED_PRIMES[i],
                n,
                sequential_results[i]
            );

            return EXIT_FAILURE;
        }

        if (reduction_results[i] !=
            sequential_results[i]) {

            fprintf(
                stderr,
                "ERROR: reduction result differs "
                "from sequential for N=%d.\n",
                n
            );

            return EXIT_FAILURE;
        }
    }

    /* Cabecalho do experimento. */
    printf(
        "============================================================\n"
    );

    printf(
        "PRIME COUNT - OPENMP EXPERIMENT\n"
    );

    printf(
        "============================================================\n\n"
    );

    printf(
        "Threads      : %d\n",
        actual_threads
    );

    printf(
        "Schedule     : static\n"
    );

    printf(
        "Warm-ups     : %d\n",
        NUM_WARMUPS
    );

    printf(
        "Measurements : %d\n",
        NUM_MEASUREMENTS
    );

    printf(
        "Statistic    : median\n"
    );

    printf(
        "Timing batch : >= %.0f ms\n",
        TARGET_BATCH_SECONDS * 1000.0
    );

    /* Tabela 1: validacao dos resultados. */
    printf(
        "\n------------------------------------------------------------\n"
    );

    printf(
        "RESULT VALIDATION\n"
    );

    printf(
        "------------------------------------------------------------\n\n"
    );

    printf(
        "%-13s %-13s %-14s %-15s %-14s\n",
        "N",
        "Sequential",
        "Naive OMP",
        "Reduction OMP",
        "Naive Status"
    );

    printf(
        "-----------------------------------------------------------------------\n"
    );

    int naive_match_detected = 0;

    for (size_t i = 0;
         i < NUM_SIZES;
         ++i) {

        const int naive_matches =
            naive_results[i] ==
            sequential_results[i];

        const char *status;

        if (naive_matches) {

            status = "MATCH*";

            naive_match_detected = 1;
        }
        else {

            status = "INCORRECT";
        }

        printf(
            "%-13d %-13lld %-14lld %-15lld %-14s\n",
            N_VALUES[i],
            sequential_results[i],
            naive_results[i],
            reduction_results[i],
            status
        );
    }

    if (naive_match_detected) {

        printf(
            "\n"
            "* MATCH means only that this particular execution "
            "produced\n"
            "  the same value. The naive version still contains "
            "a race condition.\n"
        );
    }

    /* Tabela 2: desempenho das implementacoes validas. */
    printf(
        "\n\n------------------------------------------------------------\n"
    );

    printf(
        "PERFORMANCE - VALID IMPLEMENTATIONS\n"
    );

    printf(
        "------------------------------------------------------------\n\n"
    );

    printf(
        "%-12s %-11s %-8s %-13s %-13s %-10s\n",
        "N",
        "Primes",
        "Reps",
        "Seq(ms)",
        "OpenMP(ms)",
        "Speedup"
    );

    printf(
        "---------------------------------------------------------------------\n"
    );

    for (size_t i = 0;
         i < NUM_SIZES;
         ++i) {

        const int n =
            N_VALUES[i];

        const long long expected =
            sequential_results[i];

        internal_repetitions[i] =
            calibrate_internal_repetitions(
                n,
                expected
            );

        const int repetitions =
            internal_repetitions[i];

        run_warmups(
            n,
            expected,
            repetitions
        );

        double sequential_times[
            NUM_MEASUREMENTS
        ];

        double parallel_times[
            NUM_MEASUREMENTS
        ];

        /* Alterna SEQ->OMP e OMP->SEQ entre as 20 medicoes. */
        for (int measurement = 0;
             measurement < NUM_MEASUREMENTS;
             ++measurement) {

            if (measurement % 2 == 0) {

                sequential_times[measurement] =
                    measure_per_execution(
                        count_primes_sequential,
                        n,
                        expected,
                        repetitions
                    );

                parallel_times[measurement] =
                    measure_per_execution(
                        count_primes_parallel_reduction,
                        n,
                        expected,
                        repetitions
                    );
            }
            else {

                parallel_times[measurement] =
                    measure_per_execution(
                        count_primes_parallel_reduction,
                        n,
                        expected,
                        repetitions
                    );

                sequential_times[measurement] =
                    measure_per_execution(
                        count_primes_sequential,
                        n,
                        expected,
                        repetitions
                    );
            }
        }

        /* Medianas das 20 medicoes por implementacao. */
        const double seq_median =
            calculate_median(
                sequential_times,
                NUM_MEASUREMENTS
            );

        const double omp_median =
            calculate_median(
                parallel_times,
                NUM_MEASUREMENTS
            );

        if (seq_median <= 0.0 ||
            omp_median <= 0.0) {

            fprintf(
                stderr,
                "ERROR: invalid timing result for N=%d.\n",
                n
            );

            return EXIT_FAILURE;
        }

        /* Speedup = tempo sequencial / tempo OpenMP. */
        const double speedup =
            seq_median /
            omp_median;

        /* Conversao de segundos para milissegundos. */
        const double seq_ms =
            seq_median * 1000.0;

        const double omp_ms =
            omp_median * 1000.0;

        printf(
            "%-12d %-11lld %-8d %-13.3f %-13.3f %.2fx\n",
            n,
            expected,
            repetitions,
            seq_ms,
            omp_ms,
            speedup
        );
    }

    return EXIT_SUCCESS;
}