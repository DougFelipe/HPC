#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

/*
 * Configuracao do experimento.
 *
 * N_MIN = menor matriz testada
 * N_MAX = maior matriz testada
 * PASSO = incremento entre os tamanhos
 * REPETICOES = numero de medicoes por versao
 */
#define N_MIN         100
#define N_MAX        5000
#define PASSO         100
#define REPETICOES     10


/*
 * Variavel volatile utilizada para garantir que o compilador
 * nao elimine os calculos durante a otimizacao.
 */
static volatile double benchmark_sink = 0.0;


/* ============================================================
 * TEMPORIZACAO
 * ============================================================ */

/*
 * Calcula o intervalo entre dois instantes obtidos com
 * clock_gettime().
 *
 * Retorno:
 *     tempo em segundos.
 */
static double elapsed_seconds(
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
 * INICIALIZACAO DOS DADOS
 * ============================================================ */

/*
 * Inicializa matriz e vetor de forma deterministica.
 *
 * A matriz e armazenada como um unico bloco continuo:
 *
 *     A[i][j] -> A[i * n + j]
 *
 * Isso corresponde ao armazenamento row-major utilizado em C.
 */
static void inicializar_dados(
    double *A,
    double *x,
    size_t n
)
{
    for (size_t i = 0; i < n; ++i) {

        x[i] = (double)((i % 50) + 1) * 0.01;

        for (size_t j = 0; j < n; ++j) {

            A[i * n + j] =
                (double)(((i + j) % 100) + 1) * 0.001;
        }
    }
}


/* ============================================================
 * MxV - ACESSO POR LINHAS
 * ============================================================ */

/*
 * Linha externa, coluna interna.
 *
 * Ordem de acesso:
 *
 * A[0][0], A[0][1], A[0][2], ...
 * A[1][0], A[1][1], A[1][2], ...
 *
 * Como C armazena matrizes por linhas, os acessos consecutivos
 * tambem correspondem a posicoes consecutivas na memoria.
 */
static void mxv_linhas(
    const double *A,
    const double *x,
    double *y,
    size_t n
)
{
    for (size_t i = 0; i < n; ++i) {

        double soma = 0.0;

        for (size_t j = 0; j < n; ++j) {

            soma += A[i * n + j] * x[j];
        }

        y[i] = soma;
    }
}


/* ============================================================
 * MxV - ACESSO POR COLUNAS
 * ============================================================ */

/*
 * Coluna externa, linha interna.
 *
 * Ordem de acesso:
 *
 * A[0][0], A[1][0], A[2][0], ...
 * A[0][1], A[1][1], A[2][1], ...
 *
 * Os acessos sucessivos ficam separados por aproximadamente
 * n * sizeof(double) bytes na memoria.
 */
static void mxv_colunas(
    const double *A,
    const double *x,
    double *y,
    size_t n
)
{
    for (size_t j = 0; j < n; ++j) {

        /*
         * x[j] sera reutilizado para todas as linhas dessa coluna.
         */
        const double xj = x[j];

        for (size_t i = 0; i < n; ++i) {

            y[i] += A[i * n + j] * xj;
        }
    }
}


/* ============================================================
 * CALCULO DA MEDIANA
 * ============================================================ */

static int comparar_double(
    const void *a,
    const void *b
)
{
    const double da = *(const double *)a;
    const double db = *(const double *)b;

    return (da > db) - (da < db);
}


/*
 * Utiliza a mediana das repeticoes para reduzir a influencia
 * de valores atipicos causados pelo sistema operacional,
 * escalonamento de processos etc.
 */
static double mediana(
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
 * BENCHMARK - LINHAS
 * ============================================================ */

static double medir_linhas(
    const double *A,
    const double *x,
    double *y,
    size_t n
)
{
    struct timespec inicio;
    struct timespec fim;

    if (clock_gettime(CLOCK_MONOTONIC, &inicio) != 0) {

        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }


    /*
     * Apenas a multiplicacao esta dentro da regiao medida.
     */
    mxv_linhas(A, x, y, n);


    if (clock_gettime(CLOCK_MONOTONIC, &fim) != 0) {

        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }


    /*
     * Usa parte do resultado para impedir que um compilador
     * otimizado considere o calculo desnecessario.
     *
     * Isso ocorre DEPOIS da medicao.
     */
    benchmark_sink += y[n / 2];


    return elapsed_seconds(inicio, fim);
}


/* ============================================================
 * BENCHMARK - COLUNAS
 * ============================================================ */

static double medir_colunas(
    const double *A,
    const double *x,
    double *y,
    size_t n
)
{
    struct timespec inicio;
    struct timespec fim;


    /*
     * A versao por colunas acumula os resultados em y[i].
     *
     * Portanto, o vetor precisa ser zerado antes.
     *
     * Esse zeramento ocorre FORA da regiao cronometrada.
     */
    memset(
        y,
        0,
        n * sizeof(double)
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


    return elapsed_seconds(inicio, fim);
}


/* ============================================================
 * VALIDACAO DOS RESULTADOS
 * ============================================================ */

/*
 * Verifica se as duas implementacoes produziram resultados
 * numericamente equivalentes.
 */
static int validar_resultados(
    const double *a,
    const double *b,
    size_t n
)
{
    const double tolerancia_absoluta = 1e-10;
    const double tolerancia_relativa = 1e-9;


    for (size_t i = 0; i < n; ++i) {

        double diferenca =
            fabs(a[i] - b[i]);

        double escala =
            fmax(fabs(a[i]), fabs(b[i]));


        if (
            diferenca >
            tolerancia_absoluta
            +
            tolerancia_relativa * escala
        ) {

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
     * Executa o experimento para:
     *
     * 100
     * 200
     * 300
     * ...
     * 5000
     */
    for (
        size_t n = N_MIN;
        n <= N_MAX;
        n += PASSO
    ) {

        /*
         * Verificacao para evitar overflow no calculo n * n.
         */
        if (
            n > SIZE_MAX / n
            ||
            n * n > SIZE_MAX / sizeof(double)
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
         * ALOCACAO
         * ==================================================== */

        double *A =
            malloc(elementos * sizeof(double));

        double *x =
            malloc(n * sizeof(double));

        double *y_linhas =
            malloc(n * sizeof(double));

        double *y_colunas =
            malloc(n * sizeof(double));


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
         * INICIALIZACAO
         * ==================================================== */

        inicializar_dados(
            A,
            x,
            n
        );


        /* ====================================================
         * WARM-UP
         * ==================================================== */

        /*
         * Executamos uma vez cada implementacao sem medir.
         *
         * Isso reduz efeitos especificos da primeira execucao.
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
            n * sizeof(double)
        );


        mxv_colunas(
            A,
            x,
            y_colunas,
            n
        );


        /* ====================================================
         * VALIDACAO INICIAL
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
                "Erro: resultados divergentes "
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
         * REPETICOES DO BENCHMARK
         * ==================================================== */

        double tempos_linhas[REPETICOES];
        double tempos_colunas[REPETICOES];


        for (
            size_t repeticao = 0;
            repeticao < REPETICOES;
            ++repeticao
        ) {

            /*
             * Alterna a ordem das implementacoes.
             *
             * Repeticao par:
             *     linha -> coluna
             *
             * Repeticao impar:
             *     coluna -> linha
             *
             * Isso reduz o vies causado por executar
             * sempre uma versao primeiro.
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
         * VALIDACAO FINAL
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
                "Erro: resultados divergentes "
                "apos benchmark para N = %zu.\n",
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
            mediana(
                tempos_linhas,
                REPETICOES
            );


        double tempo_colunas =
            mediana(
                tempos_colunas,
                REPETICOES
            );


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


        /*
         * Forca a exibicao imediata de cada tamanho.
         *
         * Isso e util porque N=5000 pode fazer o experimento
         * completo levar algum tempo.
         */
        fflush(stdout);


        /* ====================================================
         * LIBERACAO
         * ==================================================== */

        free(A);
        free(x);
        free(y_linhas);
        free(y_colunas);
    }


    return EXIT_SUCCESS;
}