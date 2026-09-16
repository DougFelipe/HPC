/*
 * Tarefa 7 - Escopo de dados e escalonamento com OpenMP
 *
 * Adaptacao da Tarefa 5. O primeiro experimento compara clausulas
 * de compartilhamento do contador; o segundo compara politicas de
 * escalonamento usando reduction.
 */

#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define NUM_THREADS              4
#define NUM_WARMUPS              3
#define NUM_MEASUREMENTS         20
#define TARGET_BATCH_SECONDS     0.100
#define MAX_INTERNAL_REPETITIONS 1048576

/* Configuracao fixa de schedule(runtime). */
#define RUNTIME_SCHEDULE_KIND    omp_sched_dynamic
#define RUNTIME_SCHEDULE_CHUNK   1

static const int N_VALUES[] = {
    10000, 100000, 1000000, 5000000, 10000000
};

static const long long EXPECTED_PRIMES[] = {
    1229, 9592, 78498, 348513, 664579
};

#define NUM_SIZES (sizeof(N_VALUES) / sizeof(N_VALUES[0]))
#define DATA_VERSION_COUNT 7
#define SCHEDULE_VERSION_COUNT 5

typedef long long (*CountFunction)(int);

static volatile unsigned long long semantic_sink = 0;
static volatile unsigned long long benchmark_sink = 0;


/* Teste de primalidade comum a todas as versoes. */
static int is_prime(int value)
{
    if (value < 2) {
        return 0;
    }

    if (value == 2) {
        return 1;
    }

    if (value % 2 == 0) {
        return 0;
    }

    for (int divisor = 3; divisor <= value / divisor; divisor += 2) {
        if (value % divisor == 0) {
            return 0;
        }
    }

    return 1;
}


/* Baseline sequencial. */
static long long count_primes_sequential(int n)
{
    long long count = 0;

    for (int candidate = 2; candidate <= n; ++candidate) {
        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}


/* Mantem observavel o trabalho das copias privadas. */
static void observe_private_counts(const long long thread_counts[NUM_THREADS])
{
    unsigned long long fingerprint = 0;

    for (int i = 0; i < NUM_THREADS; ++i) {
        fingerprint ^=
            (unsigned long long) thread_counts[i]
            + (unsigned long long) (i + 1);
    }

    semantic_sink ^= fingerprint;
}


/*
 * private: cada thread possui count independente.
 * As copias nao sao combinadas; o count externo permanece zero.
 */
static long long count_primes_private(int n)
{
    long long count = 0;
    long long thread_counts[NUM_THREADS] = {0};

    #pragma omp parallel private(count)
    {
        const int tid = omp_get_thread_num();
        count = 0;

        #pragma omp for schedule(static)
        for (int candidate = 2; candidate <= n; ++candidate) {
            if (is_prime(candidate)) {
                ++count;
            }
        }

        thread_counts[tid] = count;
    }

    observe_private_counts(thread_counts);
    return count;
}


/*
 * firstprivate: cada copia inicia com o valor externo.
 * As copias privadas sao descartadas ao final.
 */
static long long count_primes_firstprivate(int n)
{
    long long count = 0;
    long long thread_counts[NUM_THREADS] = {0};

    #pragma omp parallel firstprivate(count)
    {
        const int tid = omp_get_thread_num();

        #pragma omp for schedule(static)
        for (int candidate = 2; candidate <= n; ++candidate) {
            if (is_prime(candidate)) {
                ++count;
            }
        }

        thread_counts[tid] = count;
    }

    observe_private_counts(thread_counts);
    return count;
}


/*
 * lastprivate: como a copia privada nao e inicializada,
 * cada iteracao atribui count antes do uso.
 */
static long long count_primes_lastprivate(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(static) lastprivate(count)
    for (int candidate = 2; candidate <= n; ++candidate) {
        count = 0;

        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}


/*
 * firstprivate + lastprivate: as copias iniciam com o valor
 * externo e a copia da ultima iteracao e propagada ao final.
 */
static long long count_primes_first_lastprivate(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(static) \
        firstprivate(count) lastprivate(count)
    for (int candidate = 2; candidate <= n; ++candidate) {
        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}


/*
 * default(none): o escopo e explicito, mas count continua
 * compartilhado e sem sincronizacao, produzindo data race.
 */
static long long count_primes_default_none(int n)
{
    long long count = 0;

    #pragma omp parallel for default(none) shared(n, count) schedule(static)
    for (int candidate = 2; candidate <= n; ++candidate) {
        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}


/* reduction com schedule(static): agregacao correta. */
static long long count_primes_reduction(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(static) reduction(+:count)
    for (int candidate = 2; candidate <= n; ++candidate) {
        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}


/* Versoes do experimento de escalonamento. */
static long long count_primes_schedule_dynamic(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(dynamic) reduction(+:count)
    for (int candidate = 2; candidate <= n; ++candidate) {
        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}

static long long count_primes_schedule_guided(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(guided) reduction(+:count)
    for (int candidate = 2; candidate <= n; ++candidate) {
        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}

static long long count_primes_schedule_auto(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(auto) reduction(+:count)
    for (int candidate = 2; candidate <= n; ++candidate) {
        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}

static long long count_primes_schedule_runtime(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(runtime) reduction(+:count)
    for (int candidate = 2; candidate <= n; ++candidate) {
        if (is_prime(candidate)) {
            ++count;
        }
    }

    return count;
}


/* Validacao basica do teste de primalidade. */
static void validate_is_prime(void)
{
    struct PrimeTest {
        int value;
        int expected;
    };

    static const struct PrimeTest tests[] = {
        {0, 0}, {1, 0}, {2, 1}, {3, 1}, {4, 0},
        {5, 1}, {9, 0}, {17, 1}, {25, 0}
    };

    const size_t count = sizeof(tests) / sizeof(tests[0]);

    for (size_t i = 0; i < count; ++i) {
        if (is_prime(tests[i].value) != tests[i].expected) {
            fprintf(stderr, "ERROR: primality test failed for %d.\n",
                    tests[i].value);
            exit(EXIT_FAILURE);
        }
    }
}


/* Confirma o numero real de threads. */
static int get_actual_thread_count(void)
{
    int actual_threads = 1;

    #pragma omp parallel
    {
        #pragma omp single
        actual_threads = omp_get_num_threads();
    }

    return actual_threads;
}


/* Converte a politica do runtime para texto. */
static const char *schedule_name(omp_sched_t kind)
{
    switch (kind) {
        case omp_sched_static:  return "static";
        case omp_sched_dynamic: return "dynamic";
        case omp_sched_guided:  return "guided";
        case omp_sched_auto:    return "auto";
        default:                return "unknown";
    }
}


/*
 * Executa um lote cronometrado. O checksum e consumido depois
 * da medicao para manter o resultado observavel.
 */
static double run_timed_batch(CountFunction function, int n, int repetitions)
{
    volatile int guarded_n = n;
    unsigned long long checksum = 0;
    const double start = omp_get_wtime();

    for (int repetition = 0; repetition < repetitions; ++repetition) {
        checksum += (unsigned long long) function(guarded_n);
    }

    const double end = omp_get_wtime();
    benchmark_sink ^= checksum;

    return end - start;
}


/* Calibra um numero comum de repeticoes para cada N. */
static int calibrate_internal_repetitions(int n)
{
    int repetitions = 1;

    while (1) {
        const double seq_elapsed =
            run_timed_batch(count_primes_sequential, n, repetitions);

        const double omp_elapsed =
            run_timed_batch(count_primes_reduction, n, repetitions);

        if (seq_elapsed >= TARGET_BATCH_SECONDS &&
            omp_elapsed >= TARGET_BATCH_SECONDS) {
            return repetitions;
        }

        if (repetitions >= MAX_INTERNAL_REPETITIONS / 2) {
            return MAX_INTERNAL_REPETITIONS;
        }

        repetitions *= 2;
    }
}


/* Tempo medio de uma chamada dentro do lote. */
static double measure_per_execution(
    CountFunction function,
    int n,
    int repetitions
)
{
    return run_timed_batch(function, n, repetitions)
           / (double) repetitions;
}


/* Aquecimentos com rotacao da ordem das versoes. */
static void run_warmups_rotated(
    CountFunction functions[],
    size_t function_count,
    int n,
    int repetitions
)
{
    for (int warmup = 0; warmup < NUM_WARMUPS; ++warmup) {
        const size_t start = (size_t) warmup % function_count;

        for (size_t step = 0; step < function_count; ++step) {
            const size_t index = (start + step) % function_count;

            (void) measure_per_execution(
                functions[index], n, repetitions
            );
        }
    }
}


/*
 * Realiza 20 medicoes por versao com ordem rotacionada e
 * calcula a media aritmetica dos tempos.
 */
static void benchmark_function_set(
    CountFunction functions[],
    size_t function_count,
    int n,
    int repetitions,
    double mean_seconds[]
)
{
    for (size_t i = 0; i < function_count; ++i) {
        mean_seconds[i] = 0.0;
    }

    for (int measurement = 0;
         measurement < NUM_MEASUREMENTS;
         ++measurement) {

        const size_t start =
            (size_t) measurement % function_count;

        for (size_t step = 0; step < function_count; ++step) {
            const size_t index = (start + step) % function_count;

            mean_seconds[index] +=
                measure_per_execution(
                    functions[index], n, repetitions
                );
        }
    }

    for (size_t i = 0; i < function_count; ++i) {
        mean_seconds[i] /= (double) NUM_MEASUREMENTS;
    }
}


/* Valida versoes que obrigatoriamente devem ser corretas. */
static void require_correct_result(
    const char *name,
    CountFunction function,
    int n,
    long long expected
)
{
    const long long result = function(n);

    if (result != expected) {
        fprintf(stderr,
                "ERROR: %s returned %lld for N=%d; expected %lld.\n",
                name, result, n, expected);
        exit(EXIT_FAILURE);
    }
}


/* Exporta resultados do experimento de data-sharing. */
static void write_data_sharing_csv(
    long long results[NUM_SIZES][DATA_VERSION_COUNT],
    double times_ms[NUM_SIZES][DATA_VERSION_COUNT],
    int repetitions[NUM_SIZES]
)
{
    FILE *file = fopen("data_sharing_results.csv", "w");

    if (file == NULL) {
        fprintf(stderr,
                "ERROR: could not create data_sharing_results.csv.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(file,
            "N,Primes,Reps,"
            "Sequential_result,Private_result,Firstprivate_result,"
            "Lastprivate_result,FirstLastprivate_result,"
            "DefaultNone_result,Reduction_result,"
            "Sequential_ms,Private_ms,Firstprivate_ms,"
            "Lastprivate_ms,FirstLastprivate_ms,"
            "DefaultNone_ms,Reduction_ms\n");

    for (size_t i = 0; i < NUM_SIZES; ++i) {
        fprintf(file,
                "%d,%lld,%d,"
                "%lld,%lld,%lld,%lld,%lld,%lld,%lld,"
                "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                N_VALUES[i], EXPECTED_PRIMES[i], repetitions[i],
                results[i][0], results[i][1], results[i][2],
                results[i][3], results[i][4], results[i][5],
                results[i][6], times_ms[i][0], times_ms[i][1],
                times_ms[i][2], times_ms[i][3], times_ms[i][4],
                times_ms[i][5], times_ms[i][6]);
    }

    fclose(file);
}


/* Exporta resultados do experimento de scheduling. */
static void write_scheduling_csv(
    long long results[NUM_SIZES][SCHEDULE_VERSION_COUNT],
    double times_ms[NUM_SIZES][SCHEDULE_VERSION_COUNT],
    int repetitions[NUM_SIZES]
)
{
    FILE *file = fopen("scheduling_results.csv", "w");

    if (file == NULL) {
        fprintf(stderr,
                "ERROR: could not create scheduling_results.csv.\n");
        exit(EXIT_FAILURE);
    }

    fprintf(file,
            "N,Primes,Reps,"
            "Static_result,Dynamic_result,Guided_result,"
            "Auto_result,Runtime_result,"
            "Static_ms,Dynamic_ms,Guided_ms,Auto_ms,Runtime_ms\n");

    for (size_t i = 0; i < NUM_SIZES; ++i) {
        fprintf(file,
                "%d,%lld,%d,"
                "%lld,%lld,%lld,%lld,%lld,"
                "%.6f,%.6f,%.6f,%.6f,%.6f\n",
                N_VALUES[i], EXPECTED_PRIMES[i], repetitions[i],
                results[i][0], results[i][1], results[i][2],
                results[i][3], results[i][4], times_ms[i][0],
                times_ms[i][1], times_ms[i][2], times_ms[i][3],
                times_ms[i][4]);
    }

    fclose(file);
}


int main(void)
{
    omp_set_dynamic(0);
    omp_set_num_threads(NUM_THREADS);

    /* schedule(runtime) = dynamic,1 para reproducibilidade. */
    omp_set_schedule(
        RUNTIME_SCHEDULE_KIND,
        RUNTIME_SCHEDULE_CHUNK
    );

    validate_is_prime();

    const int actual_threads = get_actual_thread_count();

    if (actual_threads != NUM_THREADS) {
        fprintf(stderr,
                "ERROR: expected %d OpenMP threads, "
                "but runtime created %d.\n",
                NUM_THREADS, actual_threads);
        return EXIT_FAILURE;
    }

    omp_sched_t runtime_kind;
    int runtime_chunk;
    omp_get_schedule(&runtime_kind, &runtime_chunk);

    CountFunction data_functions[DATA_VERSION_COUNT] = {
        count_primes_sequential,
        count_primes_private,
        count_primes_firstprivate,
        count_primes_lastprivate,
        count_primes_first_lastprivate,
        count_primes_default_none,
        count_primes_reduction
    };

    CountFunction schedule_functions[SCHEDULE_VERSION_COUNT] = {
        count_primes_reduction,
        count_primes_schedule_dynamic,
        count_primes_schedule_guided,
        count_primes_schedule_auto,
        count_primes_schedule_runtime
    };

    long long data_results[NUM_SIZES][DATA_VERSION_COUNT];
    double data_times_ms[NUM_SIZES][DATA_VERSION_COUNT];

    long long schedule_results[NUM_SIZES][SCHEDULE_VERSION_COUNT];
    double schedule_times_ms[NUM_SIZES][SCHEDULE_VERSION_COUNT];

    int internal_repetitions[NUM_SIZES];

    /* Valida a referencia e coleta os resultados de data-sharing. */
    for (size_t i = 0; i < NUM_SIZES; ++i) {
        const int n = N_VALUES[i];

        require_correct_result(
            "Sequential",
            count_primes_sequential,
            n,
            EXPECTED_PRIMES[i]
        );

        require_correct_result(
            "Reduction",
            count_primes_reduction,
            n,
            EXPECTED_PRIMES[i]
        );

        for (size_t version = 0;
             version < DATA_VERSION_COUNT;
             ++version) {

            data_results[i][version] =
                data_functions[version](n);
        }
    }

    printf(
        "============================================================\n"
        "PRIME COUNT - OPENMP TASK 7\n"
        "============================================================\n\n"
    );

    printf("Threads          : %d\n", actual_threads);
    printf("Warm-ups         : %d\n", NUM_WARMUPS);
    printf("Measurements     : %d\n", NUM_MEASUREMENTS);
    printf("Statistic        : mean\n");
    printf("Timing batch     : >= %.0f ms\n",
           TARGET_BATCH_SECONDS * 1000.0);
    printf("Runtime schedule : %s,%d\n",
           schedule_name(runtime_kind), runtime_chunk);

    printf(
        "\n------------------------------------------------------------\n"
        "DATA-SHARING - RESULT VALIDATION\n"
        "------------------------------------------------------------\n\n"
    );

    printf("%-10s %-10s %-10s %-11s %-10s %-12s %-12s %-10s\n",
           "N", "Seq", "Private", "FirstPriv", "LastPriv",
           "First+Last", "DefaultNone", "Reduction");

    printf(
        "---------------------------------------------------------------------------------------\n"
    );

    for (size_t i = 0; i < NUM_SIZES; ++i) {
        printf("%-10d %-10lld %-10lld %-11lld %-10lld "
               "%-12lld %-12lld %-10lld\n",
               N_VALUES[i],
               data_results[i][0], data_results[i][1],
               data_results[i][2], data_results[i][3],
               data_results[i][4], data_results[i][5],
               data_results[i][6]);
    }

    printf(
        "\nOnly values equal to Seq are functionally correct for that run.\n"
        "A coincidental match does not remove a data race.\n"
    );

    /* Benchmark das sete versoes de data-sharing. */
    for (size_t i = 0; i < NUM_SIZES; ++i) {
        const int n = N_VALUES[i];

        internal_repetitions[i] =
            calibrate_internal_repetitions(n);

        run_warmups_rotated(
            data_functions,
            DATA_VERSION_COUNT,
            n,
            internal_repetitions[i]
        );

        double means[DATA_VERSION_COUNT];

        benchmark_function_set(
            data_functions,
            DATA_VERSION_COUNT,
            n,
            internal_repetitions[i],
            means
        );

        for (size_t version = 0;
             version < DATA_VERSION_COUNT;
             ++version) {

            data_times_ms[i][version] =
                means[version] * 1000.0;
        }
    }

    printf(
        "\n------------------------------------------------------------\n"
        "DATA-SHARING - MEAN EXECUTION TIME\n"
        "------------------------------------------------------------\n\n"
    );

    printf("%-10s %-6s %-10s %-10s %-11s %-10s "
           "%-12s %-12s %-10s\n",
           "N", "Reps", "Seq(ms)", "Private", "FirstPriv",
           "LastPriv", "First+Last", "DefaultNone", "Reduction");

    printf(
        "------------------------------------------------------------------------------------------------\n"
    );

    for (size_t i = 0; i < NUM_SIZES; ++i) {
        printf("%-10d %-6d %-10.3f %-10.3f %-11.3f %-10.3f "
               "%-12.3f %-12.3f %-10.3f\n",
               N_VALUES[i], internal_repetitions[i],
               data_times_ms[i][0], data_times_ms[i][1],
               data_times_ms[i][2], data_times_ms[i][3],
               data_times_ms[i][4], data_times_ms[i][5],
               data_times_ms[i][6]);
    }

    printf(
        "\nScheduling base: reduction (correct aggregation).\n"
    );

    /*
     * Scheduling: todas as versoes usam reduction e devem
     * coincidir com a referencia sequencial.
     */
    for (size_t i = 0; i < NUM_SIZES; ++i) {
        const int n = N_VALUES[i];
        const long long expected = EXPECTED_PRIMES[i];

        for (size_t version = 0;
             version < SCHEDULE_VERSION_COUNT;
             ++version) {

            schedule_results[i][version] =
                schedule_functions[version](n);

            if (schedule_results[i][version] != expected) {
                fprintf(stderr,
                        "ERROR: scheduling version %zu returned "
                        "%lld for N=%d; expected %lld.\n",
                        version, schedule_results[i][version],
                        n, expected);
                return EXIT_FAILURE;
            }
        }

        run_warmups_rotated(
            schedule_functions,
            SCHEDULE_VERSION_COUNT,
            n,
            internal_repetitions[i]
        );

        double means[SCHEDULE_VERSION_COUNT];

        benchmark_function_set(
            schedule_functions,
            SCHEDULE_VERSION_COUNT,
            n,
            internal_repetitions[i],
            means
        );

        for (size_t version = 0;
             version < SCHEDULE_VERSION_COUNT;
             ++version) {

            schedule_times_ms[i][version] =
                means[version] * 1000.0;
        }
    }

    printf(
        "\n------------------------------------------------------------\n"
        "SCHEDULING - RESULT VALIDATION\n"
        "------------------------------------------------------------\n\n"
    );

    printf("%-10s %-10s %-10s %-10s %-10s %-10s %-10s\n",
           "N", "Primes", "Static", "Dynamic",
           "Guided", "Auto", "Runtime");

    printf(
        "----------------------------------------------------------------------\n"
    );

    for (size_t i = 0; i < NUM_SIZES; ++i) {
        printf("%-10d %-10lld %-10lld %-10lld %-10lld %-10lld %-10lld\n",
               N_VALUES[i], EXPECTED_PRIMES[i],
               schedule_results[i][0], schedule_results[i][1],
               schedule_results[i][2], schedule_results[i][3],
               schedule_results[i][4]);
    }

    printf(
        "\n------------------------------------------------------------\n"
        "SCHEDULING - MEAN EXECUTION TIME\n"
        "------------------------------------------------------------\n\n"
    );

    printf("%-10s %-6s %-12s %-12s %-12s %-12s %-12s\n",
           "N", "Reps", "Static(ms)", "Dynamic(ms)",
           "Guided(ms)", "Auto(ms)", "Runtime(ms)");

    printf(
        "--------------------------------------------------------------------------------\n"
    );

    for (size_t i = 0; i < NUM_SIZES; ++i) {
        printf("%-10d %-6d %-12.3f %-12.3f %-12.3f %-12.3f %-12.3f\n",
               N_VALUES[i], internal_repetitions[i],
               schedule_times_ms[i][0], schedule_times_ms[i][1],
               schedule_times_ms[i][2], schedule_times_ms[i][3],
               schedule_times_ms[i][4]);
    }

    write_data_sharing_csv(
        data_results,
        data_times_ms,
        internal_repetitions
    );

    write_scheduling_csv(
        schedule_results,
        schedule_times_ms,
        internal_repetitions
    );

    printf(
        "\nCSV files generated:\n"
        "  data_sharing_results.csv\n"
        "  scheduling_results.csv\n"
    );

    return EXIT_SUCCESS;
}
