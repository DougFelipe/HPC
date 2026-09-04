#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/*
 * ============================================================
 * Tarefa 5 - Contagem de numeros primos com OpenMP
 * ============================================================
 *
 * O programa implementa tres versoes da mesma contagem:
 *
 * 1. Sequencial
 * 2. OpenMP ingenua
 *    - possui propositalmente uma race condition;
 * 3. OpenMP correta
 *    - utiliza reduction.
 *
 * O benchmark de desempenho compara somente:
 *
 *     Sequencial x OpenMP com reduction
 *
 * O laco externo, que percorre os candidatos entre 2 e N,
 * e o unico paralelizado.
 *
 * O laco interno do teste de primalidade permanece sequencial.
 *
 * Para evitar tempos muito pequenos em valores baixos de N,
 * cada medicao pode executar varias contagens internamente.
 *
 * O numero de repeticoes internas e calibrado automaticamente
 * para que cada lote de medicao tenha duracao suficientemente
 * grande para omp_get_wtime().
 */


/* ============================================================
 * CONFIGURACAO DO EXPERIMENTO
 * ============================================================
 */

#define NUM_THREADS             4
#define NUM_WARMUPS             3
#define NUM_MEASUREMENTS        20

/*
 * Duracao minima desejada para cada lote cronometrado.
 *
 * 0.100 s = 100 ms.
 *
 * Isso reduz o impacto da resolucao efetiva do relogio sobre
 * cargas pequenas.
 */
#define TARGET_BATCH_SECONDS    0.100

/*
 * Limite de seguranca para a calibracao automatica.
 */
#define MAX_INTERNAL_REPETITIONS 1048576


/*
 * Valores de N utilizados no experimento.
 */
static const int N_VALUES[] = {
    10000,
    100000,
    1000000,
    5000000,
    10000000
};


/*
 * Quantidades conhecidas de primos entre 2 e N.
 *
 * Servem como referencia independente para validar
 * a implementacao sequencial.
 */
static const long long EXPECTED_PRIMES[] = {
    1229,
    9592,
    78498,
    348513,
    664579
};


#define NUM_SIZES \
    (sizeof(N_VALUES) / sizeof(N_VALUES[0]))


/*
 * Tipo de ponteiro para uma funcao de contagem.
 *
 * Tanto a versao sequencial quanto a paralela correta possuem
 * esta mesma assinatura.
 */
typedef long long (*CountFunction)(int);


/* ============================================================
 * TESTE DE PRIMALIDADE
 * ============================================================
 */

/*
 * Retorna:
 *
 *     1 -> se value for primo
 *     0 -> caso contrario
 *
 * A mesma funcao e utilizada pelas tres implementacoes.
 */
static int is_prime(int value)
{
    /*
     * Numeros menores que 2 nao sao primos.
     */
    if (value < 2) {
        return 0;
    }

    /*
     * 2 e o unico primo par.
     */
    if (value == 2) {
        return 1;
    }

    /*
     * Qualquer outro numero par pode ser eliminado
     * imediatamente.
     */
    if (value % 2 == 0) {
        return 0;
    }

    /*
     * Testamos somente divisores impares.
     *
     * A condicao:
     *
     *     divisor <= value / divisor
     *
     * equivale a:
     *
     *     divisor * divisor <= value
     *
     * mas evita multiplicacao potencialmente problematica
     * e dispensa o uso de sqrt().
     *
     * Este laco permanece sequencial mesmo quando a funcao
     * e chamada dentro de uma regiao OpenMP.
     */
    for (int divisor = 3;
         divisor <= value / divisor;
         divisor += 2) {

        if (value % divisor == 0) {
            return 0;
        }
    }

    return 1;
}


/* ============================================================
 * VERSAO 1 - SEQUENCIAL
 * ============================================================
 */

/*
 * Baseline do experimento.
 *
 * Percorre todos os candidatos entre 2 e N e conta quantos
 * deles sao primos.
 */
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


/* ============================================================
 * VERSAO 2 - OPENMP INGENUA
 * ============================================================
 */

/*
 * Implementacao propositalmente incorreta.
 *
 * O laco externo e paralelizado, mas todas as threads
 * compartilham e modificam diretamente "count".
 *
 * A operacao:
 *
 *     ++count
 *
 * nao e atomica.
 *
 * Portanto, atualizacoes podem ser perdidas quando duas ou
 * mais threads acessam o contador simultaneamente.
 *
 * Esta versao existe apenas para demonstrar a race condition
 * e NAO participa da comparacao valida de desempenho.
 */
static long long count_primes_parallel_naive(int n)
{
    long long count = 0;

    #pragma omp parallel for schedule(static) shared(count)
    for (int candidate = 2;
         candidate <= n;
         ++candidate) {

        if (is_prime(candidate)) {

            /*
             * Race condition proposital.
             *
             * Nao adicionar:
             *
             * atomic
             * critical
             * reduction
             *
             * nesta implementacao.
             */
            ++count;
        }
    }

    return count;
}


/* ============================================================
 * VERSAO 3 - OPENMP CORRETA
 * ============================================================
 */

/*
 * Implementacao paralela utilizada no benchmark.
 *
 * A clausula reduction cria resultados parciais privados
 * para as threads e combina esses resultados ao final.
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


/* ============================================================
 * VALIDACAO DA FUNCAO DE PRIMALIDADE
 * ============================================================
 */

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


/* ============================================================
 * NUMERO REAL DE THREADS
 * ============================================================
 */

/*
 * Confirma quantas threads o runtime OpenMP realmente criou.
 */
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


/* ============================================================
 * EXECUCAO DE UM LOTE
 * ============================================================
 */

/*
 * Executa a mesma funcao varias vezes dentro de uma unica
 * regiao cronometrada.
 *
 * Essa estrategia resolve o problema encontrado anteriormente,
 * no qual N pequeno produzia:
 *
 *     0.000 ms
 *
 * O tempo retornado por esta funcao corresponde ao TEMPO TOTAL
 * do lote.
 *
 * Posteriormente ele sera dividido pelo numero de repeticoes
 * para obter o tempo medio de uma unica contagem.
 */
static double run_timed_batch(
    CountFunction function,
    int n,
    long long expected,
    int repetitions
)
{
    /*
     * O valor de N e mantido em uma variavel volatile apenas
     * no harness de benchmark.
     *
     * Isso impede que o compilador considere todas as chamadas
     * repetidas como uma unica computacao invariavel e tente
     * desloca-la para fora do laco.
     *
     * O algoritmo que esta sendo medido nao utiliza volatile.
     */
    volatile int guarded_n = n;

    long long checksum = 0;


    const double start =
        omp_get_wtime();


    for (int repetition = 0;
         repetition < repetitions;
         ++repetition) {

        /*
         * A leitura ocorre a cada repeticao.
         */
        const int current_n =
            guarded_n;

        checksum +=
            function(current_n);
    }


    const double end =
        omp_get_wtime();


    /*
     * A validacao acontece DEPOIS do cronometro.
     *
     * Portanto, nao interfere no tempo medido.
     */
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


/* ============================================================
 * CALIBRACAO DAS REPETICOES INTERNAS
 * ============================================================
 */

/*
 * Descobre automaticamente quantas repeticoes internas devem
 * ser utilizadas para um determinado N.
 *
 * A mesma quantidade sera utilizada tanto na versao sequencial
 * quanto na paralela.
 *
 * Comecamos com:
 *
 *     1 repeticao
 *
 * e dobramos:
 *
 *     1, 2, 4, 8, 16, 32, ...
 *
 * ate que AMBAS as implementacoes tenham um lote com pelo
 * menos TARGET_BATCH_SECONDS.
 *
 * Isso garante que inclusive a versao mais rapida tenha uma
 * duracao suficientemente longa para ser medida.
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


        /*
         * A calibracao termina somente quando os dois lotes
         * atingem o tempo minimo desejado.
         */
        if (seq_elapsed >= TARGET_BATCH_SECONDS &&
            omp_elapsed >= TARGET_BATCH_SECONDS) {

            return repetitions;
        }


        /*
         * Protecao contra crescimento indefinido.
         */
        if (repetitions >=
            MAX_INTERNAL_REPETITIONS / 2) {

            return MAX_INTERNAL_REPETITIONS;
        }


        repetitions *= 2;
    }
}


/* ============================================================
 * AQUECIMENTO
 * ============================================================
 */

/*
 * Os warm-ups utilizam o mesmo tamanho de lote determinado
 * pela calibracao.
 *
 * Eles nao entram nas 20 medicoes oficiais.
 */
static void run_warmups(
    int n,
    long long expected,
    int repetitions
)
{
    for (int i = 0;
         i < NUM_WARMUPS;
         ++i) {

        /*
         * A ordem e alternada tambem durante o aquecimento.
         */
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


/* ============================================================
 * MEDICAO POR EXECUCAO
 * ============================================================
 */

/*
 * Executa um lote e converte seu tempo total no tempo medio
 * de UMA UNICA contagem.
 *
 * Exemplo:
 *
 *     lote = 200 ms
 *     repeticoes = 100
 *
 *     tempo por contagem = 2 ms
 */
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


/* ============================================================
 * MEDIANA
 * ============================================================
 */

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


/*
 * Ordena as medicoes e retorna a mediana.
 *
 * Como temos 20 valores, a mediana sera a media entre
 * os dois valores centrais.
 */
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


/* ============================================================
 * MAIN
 * ============================================================
 */

int main(void)
{
    /*
     * Nao permite que o runtime OpenMP reduza dinamicamente
     * a quantidade solicitada de threads.
     */
    omp_set_dynamic(0);


    /*
     * Numero fixo de threads do experimento.
     */
    omp_set_num_threads(NUM_THREADS);


    /*
     * Valida inicialmente o teste de primalidade.
     */
    validate_is_prime();


    /*
     * Confirma que o runtime criou exatamente a quantidade
     * esperada de threads.
     */
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


    /*
     * Armazena os resultados das tres versoes.
     */
    long long sequential_results[NUM_SIZES];
    long long naive_results[NUM_SIZES];
    long long reduction_results[NUM_SIZES];


    /*
     * Numero de repeticoes internas determinado
     * individualmente para cada N.
     */
    int internal_repetitions[NUM_SIZES];


    /* ========================================================
     * ETAPA 1 - VALIDACAO FUNCIONAL
     * ========================================================
     */

    for (size_t i = 0;
         i < NUM_SIZES;
         ++i) {

        const int n =
            N_VALUES[i];


        /*
         * As tres versoes sao executadas fora de qualquer
         * benchmark oficial.
         */
        sequential_results[i] =
            count_primes_sequential(n);


        naive_results[i] =
            count_primes_parallel_naive(n);


        reduction_results[i] =
            count_primes_parallel_reduction(n);


        /*
         * Primeiro, validamos a implementacao sequencial
         * usando a contagem matematicamente conhecida.
         */
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


        /*
         * Depois, a versao paralela correta precisa produzir
         * exatamente o mesmo resultado.
         */
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


    /* ========================================================
     * CABECALHO
     * ========================================================
     */

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


    /* ========================================================
     * TABELA 1 - RESULT VALIDATION
     * ========================================================
     */

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


    /* ========================================================
     * TABELA 2 - PERFORMANCE
     * ========================================================
     */

    printf(
        "\n\n------------------------------------------------------------\n"
    );

    printf(
        "PERFORMANCE - VALID IMPLEMENTATIONS\n"
    );

    printf(
        "------------------------------------------------------------\n\n"
    );


    /*
     * A coluna Reps informa quantas execucoes internas foram
     * utilizadas em cada lote de temporizacao.
     */
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


        /*
         * Determina automaticamente o numero apropriado
         * de repeticoes internas.
         */
        internal_repetitions[i] =
            calibrate_internal_repetitions(
                n,
                expected
            );


        const int repetitions =
            internal_repetitions[i];


        /*
         * Depois da calibracao sao realizados os tres
         * warm-ups previstos na metodologia.
         */
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


        /*
         * Sao realizadas 20 medicoes.
         *
         * A ordem e alternada:
         *
         *     SEQ -> OMP
         *     OMP -> SEQ
         *     SEQ -> OMP
         *     ...
         */
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


        /*
         * Mediana dos 20 tempos por execucao.
         */
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


        /*
         * Como os lotes agora possuem duracao adequada,
         * nao devemos obter zero no denominador.
         *
         * Ainda assim, a verificacao protege contra um
         * resultado numericamente invalido.
         */
        if (seq_median <= 0.0 ||
            omp_median <= 0.0) {

            fprintf(
                stderr,
                "ERROR: invalid timing result for N=%d.\n",
                n
            );

            return EXIT_FAILURE;
        }


        /*
         * Speedup:
         *
         *             T_seq
         *     S = -------------
         *             T_omp
         */
        const double speedup =
            seq_median /
            omp_median;


        /*
         * omp_get_wtime() trabalha em segundos.
         *
         * A conversao para milissegundos ocorre apenas
         * para apresentacao.
         */
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