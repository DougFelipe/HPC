#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>


/* ============================================================
 * PARÂMETROS DO EXPERIMENTO
 * ============================================================ */

#define N_MIN         100
#define N_MAX        5000
#define PASSO         100
#define REPETICOES     10


/*
 * Impede que o compilador elimine os cálculos durante
 * a otimização do programa.
 */
static volatile long long benchmark_sink = 0;


/* ============================================================
 * TEMPORIZAÇÃO
 * ============================================================ */

/*
 * Calcula o intervalo entre dois instantes de tempo.
 * O resultado é retornado em segundos.
 */
static double tempo_decorrido(
    struct timespec inicio,
    struct timespec fim
)
{
    double segundos =
        (double)(fim.tv_sec - inicio.tv_sec);

    double nanossegundos =
        (double)(fim.tv_nsec - inicio.tv_nsec) / 1e9;

    return segundos + nanossegundos;
}


/* ============================================================
 * INICIALIZAÇÃO DOS DADOS
 * ============================================================ */

/*
 * Inicializa a matriz e o vetor com valores inteiros pequenos
 * e determinísticos.
 *
 * A matriz é armazenada em um bloco contínuo de memória,
 * seguindo a organização row-major:
 *
 *     A[i][j] -> A[i * n + j]
 */
static void inicializar_dados(
    int *A,
    int *x,
    size_t n
)
{
    for (size_t i = 0; i < n; ++i) {

        x[i] = (int)(i % 5) + 1;

        for (size_t j = 0; j < n; ++j) {

            A[i * n + j] =
                (int)((i + j) % 10) + 1;
        }
    }
}


/* ============================================================
 * MULTIPLICAÇÃO MATRIZ-VETOR POR LINHAS
 * ============================================================ */

/*
 * Percorre primeiro as linhas e depois as colunas.
 *
 * A ordem de acesso segue:
 *
 *     A[0][0], A[0][1], A[0][2], ...
 *     A[1][0], A[1][1], A[1][2], ...
 *
 * Esse padrão acompanha a disposição row-major dos dados.
 */
static void mxv_linhas(
    const int *A,
    const int *x,
    int *y,
    size_t n
)
{
    for (size_t i = 0; i < n; ++i) {

        int soma = 0;

        for (size_t j = 0; j < n; ++j) {

            soma += A[i * n + j] * x[j];
        }

        y[i] = soma;
    }
}


/* ============================================================
 * MULTIPLICAÇÃO MATRIZ-VETOR POR COLUNAS
 * ============================================================ */

/*
 * Percorre primeiro as colunas e depois as linhas.
 *
 * A ordem de acesso segue:
 *
 *     A[0][0], A[1][0], A[2][0], ...
 *     A[0][1], A[1][1], A[2][1], ...
 *
 * Nesse caso, acessos consecutivos à matriz apresentam
 * distância aproximada de n * sizeof(int) bytes.
 */
static void mxv_colunas(
    const int *A,
    const int *x,
    int *y,
    size_t n
)
{
    for (size_t j = 0; j < n; ++j) {

        int xj = x[j];

        for (size_t i = 0; i < n; ++i) {

            y[i] += A[i * n + j] * xj;
        }
    }
}


/* ============================================================
 * CÁLCULO DA MEDIANA
 * ============================================================ */

/*
 * Função auxiliar para ordenação dos tempos medidos.
 */
static int comparar_double(
    const void *a,
    const void *b
)
{
    double x = *(const double *)a;
    double y = *(const double *)b;

    return (x > y) - (x < y);
}


/*
 * Calcula a mediana dos tempos obtidos nas repetições.
 */
static double calcular_mediana(
    double *valores,
    size_t quantidade
)
{
    qsort(
        valores,
        quantidade,
        sizeof(double),
        comparar_double
    );

    if (quantidade % 2 == 1) {

        return valores[quantidade / 2];
    }

    return (
        valores[quantidade / 2 - 1]
        +
        valores[quantidade / 2]
    ) / 2.0;
}


/* ============================================================
 * MEDIÇÃO DO ACESSO POR LINHAS
 * ============================================================ */

static double medir_linhas(
    const int *A,
    const int *x,
    int *y,
    size_t n
)
{
    struct timespec inicio;
    struct timespec fim;

    if (clock_gettime(CLOCK_MONOTONIC, &inicio) != 0) {

        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    mxv_linhas(A, x, y, n);

    if (clock_gettime(CLOCK_MONOTONIC, &fim) != 0) {

        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    benchmark_sink += y[n / 2];

    return tempo_decorrido(inicio, fim);
}


/* ============================================================
 * MEDIÇÃO DO ACESSO POR COLUNAS
 * ============================================================ */

static double medir_colunas(
    const int *A,
    const int *x,
    int *y,
    size_t n
)
{
    struct timespec inicio;
    struct timespec fim;

    /*
     * A implementação por colunas acumula valores em y.
     * O vetor é zerado antes da região cronometrada.
     */
    memset(
        y,
        0,
        n * sizeof(int)
    );

    if (clock_gettime(CLOCK_MONOTONIC, &inicio) != 0) {

        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    mxv_colunas(A, x, y, n);

    if (clock_gettime(CLOCK_MONOTONIC, &fim) != 0) {

        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    benchmark_sink += y[n / 2];

    return tempo_decorrido(inicio, fim);
}


/* ============================================================
 * VALIDAÇÃO DOS RESULTADOS
 * ============================================================ */

/*
 * Verifica se as duas implementações produziram exatamente
 * o mesmo vetor resultado.
 */
static int validar_resultados(
    const int *a,
    const int *b,
    size_t n
)
{
    for (size_t i = 0; i < n; ++i) {

        if (a[i] != b[i]) {

            return 0;
        }
    }

    return 1;
}


/* ============================================================
 * PROGRAMA PRINCIPAL
 * ============================================================ */

int main(void)
{
    printf(
        "%-8s %-15s %-15s %-10s\n",
        "N",
        "Linha(s)",
        "Coluna(s)",
        "C/L"
    );

    printf(
        "-------------------------------------------------------\n"
    );


    /*
     * Executa o experimento para matrizes quadradas entre
     * N_MIN e N_MAX, utilizando o incremento definido em PASSO.
     */
    for (
        size_t n = N_MIN;
        n <= N_MAX;
        n += PASSO
    ) {

        /*
         * Verifica a possibilidade de overflow antes
         * da alocação da matriz.
         */
        if (
            n > SIZE_MAX / n
            ||
            n * n > SIZE_MAX / sizeof(int)
        ) {

            fprintf(
                stderr,
                "Erro: tamanho da matriz excede "
                "o limite de alocacao.\n"
            );

            return EXIT_FAILURE;
        }


        size_t elementos = n * n;


        /* ====================================================
         * ALOCAÇÃO DE MEMÓRIA
         * ==================================================== */

        int *A =
            malloc(elementos * sizeof(int));

        int *x =
            malloc(n * sizeof(int));

        int *y_linhas =
            malloc(n * sizeof(int));

        int *y_colunas =
            malloc(n * sizeof(int));


        if (
            A == NULL
            ||
            x == NULL
            ||
            y_linhas == NULL
            ||
            y_colunas == NULL
        ) {

            fprintf(
                stderr,
                "Erro: falha de alocacao para N = %zu.\n",
                n
            );

            free(A);
            free(x);
            free(y_linhas);
            free(y_colunas);

            return EXIT_FAILURE;
        }


        /* ====================================================
         * INICIALIZAÇÃO
         * ==================================================== */

        inicializar_dados(
            A,
            x,
            n
        );


        /* ====================================================
         * AQUECIMENTO
         * ====================================================
         *
         * Executa previamente as duas implementações.
         * Essas execuções não participam das medições.
         */

        mxv_linhas(
            A,
            x,
            y_linhas,
            n
        );

        memset(
            y_colunas,
            0,
            n * sizeof(int)
        );

        mxv_colunas(
            A,
            x,
            y_colunas,
            n
        );


        /* ====================================================
         * VALIDAÇÃO INICIAL
         * ==================================================== */

        if (
            !validar_resultados(
                y_linhas,
                y_colunas,
                n
            )
        ) {

            fprintf(
                stderr,
                "Erro: resultados divergentes para N = %zu.\n",
                n
            );

            free(A);
            free(x);
            free(y_linhas);
            free(y_colunas);

            return EXIT_FAILURE;
        }


        /* ====================================================
         * REPETIÇÕES DO BENCHMARK
         * ==================================================== */

        double tempos_linhas[REPETICOES];
        double tempos_colunas[REPETICOES];


        for (
            size_t repeticao = 0;
            repeticao < REPETICOES;
            ++repeticao
        ) {

            /*
             * A ordem das implementações é alternada entre
             * as repetições para reduzir viés de execução.
             */
            if (repeticao % 2 == 0) {

                tempos_linhas[repeticao] =
                    medir_linhas(
                        A,
                        x,
                        y_linhas,
                        n
                    );

                tempos_colunas[repeticao] =
                    medir_colunas(
                        A,
                        x,
                        y_colunas,
                        n
                    );

            } else {

                tempos_colunas[repeticao] =
                    medir_colunas(
                        A,
                        x,
                        y_colunas,
                        n
                    );

                tempos_linhas[repeticao] =
                    medir_linhas(
                        A,
                        x,
                        y_linhas,
                        n
                    );
            }
        }


        /* ====================================================
         * VALIDAÇÃO FINAL
         * ==================================================== */

        if (
            !validar_resultados(
                y_linhas,
                y_colunas,
                n
            )
        ) {

            fprintf(
                stderr,
                "Erro: resultados divergentes apos benchmark "
                "para N = %zu.\n",
                n
            );

            free(A);
            free(x);
            free(y_linhas);
            free(y_colunas);

            return EXIT_FAILURE;
        }


        /* ====================================================
         * RESULTADOS
         * ==================================================== */

        double tempo_linhas =
            calcular_mediana(
                tempos_linhas,
                REPETICOES
            );

        double tempo_colunas =
            calcular_mediana(
                tempos_colunas,
                REPETICOES
            );


        /*
         * A métrica C/L representa a razão entre o tempo
         * por colunas e o tempo por linhas.
         *
         * C/L = 1  -> tempos equivalentes
         * C/L > 1  -> acesso por colunas mais lento
         * C/L < 1  -> acesso por colunas mais rápido
         */
        double razao = 0.0;

        if (tempo_linhas > 0.0) {

            razao =
                tempo_colunas
                /
                tempo_linhas;
        }


        printf(
            "%-8zu %-15.9f %-15.9f %-10.3f\n",
            n,
            tempo_linhas,
            tempo_colunas,
            razao
        );

        fflush(stdout);


        /* ====================================================
         * LIBERAÇÃO DE MEMÓRIA
         * ==================================================== */

        free(A);
        free(x);
        free(y_linhas);
        free(y_colunas);
    }


    return EXIT_SUCCESS;
}