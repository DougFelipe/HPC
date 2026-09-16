#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define N_POINTS 100000
#define SECTION_THREADS 2
#define TASK_THREADS 2

typedef struct {
    int outer_thread;
    int inner_team_size;
    int inner_seen[TASK_THREADS];
} TaskInfo;

typedef struct {
    int team_size;
    int seen[TASK_THREADS];
} InitInfo;

static void reset_task_info(TaskInfo *info)
{
    info->outer_thread = -1;
    info->inner_team_size = 0;

    for (int i = 0; i < TASK_THREADS; ++i) {
        info->inner_seen[i] = 0;
    }
}

static void reset_init_info(InitInfo *info)
{
    info->team_size = 0;

    for (int i = 0; i < TASK_THREADS; ++i) {
        info->seen[i] = 0;
    }
}

/*
 * Inicializa y[i] = f(x_i) = x_i^2 em paralelo.
 * Cada iteracao escreve em uma posicao independente do vetor.
 */
static void initialize_vector(double *y, int n, double a, double h, InitInfo *info)
{
    #pragma omp parallel num_threads(TASK_THREADS) shared(y, info)
    {
        const int tid = omp_get_thread_num();

        if (tid < TASK_THREADS) {
            info->seen[tid] = 1;
        }

        #pragma omp single
        {
            info->team_size = omp_get_num_threads();
        }

        #pragma omp for
        for (int i = 0; i < n; ++i) {
            const double x = a + (double)i * h;
            y[i] = x * x;
        }
    }
}

/*
 * Calcula a integral pelo metodo composto dos trapezios.
 * O somatorio interno e distribuido entre duas threads e combinado
 * de forma segura por reduction.
 */
static double compute_integral(
    const double *y,
    int n,
    double h,
    TaskInfo *info
)
{
    double sum = 0.0;

    #pragma omp parallel num_threads(TASK_THREADS) shared(y, info, sum)
    {
        const int tid = omp_get_thread_num();

        if (tid < TASK_THREADS) {
            info->inner_seen[tid] = 1;
        }

        #pragma omp single
        {
            info->inner_team_size = omp_get_num_threads();
        }

        #pragma omp for reduction(+:sum)
        for (int i = 1; i < n - 1; ++i) {
            sum += y[i];
        }
    }

    return h * (0.5 * y[0] + sum + 0.5 * y[n - 1]);
}

/*
 * Calcula a derivada numerica.
 *
 * Pontos internos:
 *   diferenca central
 *
 * Extremidades:
 *   diferenca progressiva no primeiro ponto
 *   diferenca regressiva no ultimo ponto
 */
static void compute_derivative(
    const double *y,
    double *dy,
    int n,
    double h,
    TaskInfo *info
)
{
    #pragma omp parallel num_threads(TASK_THREADS) shared(y, dy, info)
    {
        const int tid = omp_get_thread_num();

        if (tid < TASK_THREADS) {
            info->inner_seen[tid] = 1;
        }

        #pragma omp single
        {
            info->inner_team_size = omp_get_num_threads();
        }

        #pragma omp for
        for (int i = 1; i < n - 1; ++i) {
            dy[i] = (y[i + 1] - y[i - 1]) / (2.0 * h);
        }
    }

    dy[0] = (y[1] - y[0]) / h;
    dy[n - 1] = (y[n - 1] - y[n - 2]) / h;
}

static void print_seen_threads(const int seen[TASK_THREADS])
{
    int first = 1;

    for (int i = 0; i < TASK_THREADS; ++i) {
        if (seen[i]) {
            if (!first) {
                printf(", ");
            }

            printf("%d", i);
            first = 0;
        }
    }

    if (first) {
        printf("nenhuma");
    }

    printf("\n");
}

static void print_derivative_samples(
    const double *dy,
    int n,
    double a,
    double h
)
{
    const int indices[5] = {
        0,
        n / 4,
        n / 2,
        (3 * n) / 4,
        n - 1
    };

    printf("%-12s %-14s %-16s %-16s\n",
           "Indice", "x", "Numerica", "Exata");
    printf("------------------------------------------------------------\n");

    for (int k = 0; k < 5; ++k) {
        const int i = indices[k];
        const double x = a + (double)i * h;
        const double exact = 2.0 * x;

        printf("%-12d %-14.6f %-16.6f %-16.6f\n",
               i, x, dy[i], exact);
    }
}

int main(void)
{
    const int n = N_POINTS;
    const double a = 0.0;
    const double b = 10.0;
    const double h = (b - a) / (double)(n - 1);

    double *y = malloc((size_t)n * sizeof(*y));
    double *dy = malloc((size_t)n * sizeof(*dy));

    if (y == NULL || dy == NULL) {
        fprintf(stderr, "Erro: nao foi possivel alocar os vetores.\n");
        free(y);
        free(dy);
        return EXIT_FAILURE;
    }

    InitInfo init_info;
    TaskInfo integral_info;
    TaskInfo derivative_info;

    reset_init_info(&init_info);
    reset_task_info(&integral_info);
    reset_task_info(&derivative_info);

    /*
     * Evita que o runtime reduza dinamicamente o numero de threads e
     * habilita regioes paralelas aninhadas, como exigido pela atividade.
     */
    omp_set_dynamic(0);
    omp_set_nested(1);

    printf("============================================================\n");
    printf("TAREFA 8 - OPENMP\n");
    printf("============================================================\n\n");

    printf("CONFIGURACAO\n");
    printf("------------------------------------------------------------\n");
    printf("Funcao              : f(x) = x^2\n");
    printf("Intervalo           : [%.0f, %.0f]\n", a, b);
    printf("Pontos              : %d\n", n);
    printf("Threads sections    : %d\n", SECTION_THREADS);
    printf("Threads por tarefa  : %d\n", TASK_THREADS);
    printf("Nested parallelism  : %s\n",
           omp_get_nested() ? "habilitado" : "desabilitado");

    /*
     * Fase 1: inicializacao do vetor.
     * O fim da regiao parallel funciona como ponto de sincronizacao:
     * somente depois dele integral e derivada podem usar y[].
     */
    initialize_vector(y, n, a, h, &init_info);

    printf("\n============================================================\n");
    printf("INICIALIZACAO\n");
    printf("============================================================\n\n");
    printf("Inicializacao paralela:\n");
    printf("  Equipe                   : %d threads\n", init_info.team_size);
    printf("  Threads participantes    : ");
    print_seen_threads(init_info.seen);
    printf("\nVetor inicializado com sucesso.\n");

    double integral = 0.0;

    /*
     * Fase 2: paralelismo de tarefas.
     *
     * Integral e derivada sao independentes depois que y[] esta pronto.
     * Cada section e executada por uma thread externa diferente quando
     * o runtime distribui as duas sections entre as duas threads do time.
     *
     * Dentro de cada section, uma nova regiao parallel cria duas threads
     * para explorar paralelismo de dados (nested parallelism).
     */
    #pragma omp parallel num_threads(SECTION_THREADS) \
        shared(y, dy, integral, integral_info, derivative_info)
    {
        #pragma omp sections
        {
            #pragma omp section
            {
                integral_info.outer_thread = omp_get_thread_num();
                integral = compute_integral(
                    y,
                    n,
                    h,
                    &integral_info
                );
            }

            #pragma omp section
            {
                derivative_info.outer_thread = omp_get_thread_num();
                compute_derivative(
                    y,
                    dy,
                    n,
                    h,
                    &derivative_info
                );
            }
        }

        /*
         * A barreira implicita ao fim de sections garante que integral
         * e derivada terminaram antes da impressao.
         *
         * single garante que apenas uma thread do time externo imprima
         * os resultados, conforme solicitado no enunciado.
         */
        #pragma omp single
        {
            const double exact_integral =
                (b * b * b - a * a * a) / 3.0;
            const double absolute_error =
                fabs(integral - exact_integral);

            printf("\n============================================================\n");
            printf("PARALELISMO DE TAREFAS\n");
            printf("============================================================\n\n");

            printf("Section Integral:\n");
            printf("  Thread externa responsavel : %d\n",
                   integral_info.outer_thread);
            printf("  Equipe interna             : %d threads\n",
                   integral_info.inner_team_size);
            printf("  Threads internas           : ");
            print_seen_threads(integral_info.inner_seen);

            printf("\nSection Derivada:\n");
            printf("  Thread externa responsavel : %d\n",
                   derivative_info.outer_thread);
            printf("  Equipe interna             : %d threads\n",
                   derivative_info.inner_team_size);
            printf("  Threads internas           : ");
            print_seen_threads(derivative_info.inner_seen);

            printf("\n============================================================\n");
            printf("INTEGRAL - METODO DOS TRAPEZIOS\n");
            printf("============================================================\n\n");
            printf("Valor numerico : %.12f\n", integral);
            printf("Valor exato    : %.12f\n", exact_integral);
            printf("Erro absoluto  : %.12e\n", absolute_error);

            printf("\n============================================================\n");
            printf("DERIVADA - DIFERENCAS FINITAS\n");
            printf("============================================================\n\n");
            print_derivative_samples(dy, n, a, h);

            printf("\n============================================================\n");
            printf("PROCESSAMENTO CONCLUIDO\n");
            printf("============================================================\n");
        }
    }

    free(y);
    free(dy);

    return EXIT_SUCCESS;
}
