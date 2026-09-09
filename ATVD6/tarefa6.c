/*
 * Tarefa 6 - Integracao Numerica com OpenMP
 *
 * Compara cinco implementacoes do metodo composto do trapezio:
 *   1. sequencial;
 *   2. paralela sem sincronizacao;
 *   3. paralela com critical;
 *   4. paralela com atomic;
 *   5. paralela com reduction.
 *
 * O problema matematico e o mesmo da Tarefa 4:
 *   f(x) = x^2, intervalo [0, 2400000], aritmetica int64_t.
 *
 * Para cada N sao realizados 3 aquecimentos e 20 medicoes.
 * O numero de execucoes por medicao e ajustado automaticamente
 * para produzir um lote de aproximadamente 20 ms. O tempo do lote
 * e normalizado para uma execucao antes do calculo da media.
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <omp.h>

/* Configuracao fixa do experimento paralelo. */
#define NUM_THREADS 4
#define NUM_AQUECIMENTOS 3
#define NUM_MEDICOES 20
#define TEMPO_ALVO_LOTE_NS UINT64_C(20000000)
#define MAX_REPETICOES_LOTE 100000

#define LIMITE_A INT64_C(0)
#define LIMITE_B INT64_C(2400000)
#define INTEGRAL_EXATA INT64_C(4608000000000000000)

static volatile int64_t destino_resultado = 0;

#if defined(__GNUC__)
#define KERNEL __attribute__((noinline, noipa))
#else
#define KERNEL
#endif

/* Assinatura comum a todas as implementacoes da integral. */
typedef int64_t (*FuncaoIntegral)(int64_t, int64_t, int64_t);

/* Metadados de cada versao avaliada. */
typedef struct {
    const char *nome;
    FuncaoIntegral funcao;
    int exige_correcao;
} Versao;

/* Resultado consolidado de uma configuracao (versao, N). */
typedef struct {
    int64_t resultado;
    uint64_t erro_abs;
    int validos;
    int repeticoes_lote;
    double tempo_medio_ms;
} ResultadoBenchmark;

static inline int64_t funcao_integrando(int64_t x)
{
    return x * x;
}

static inline int64_t finalizar_integral(int64_t a, int64_t b, int64_t h, int64_t soma)
{
    const int64_t extremos = funcao_integrando(a) + funcao_integrando(b);
    return h * soma + (h * extremos) / 2;
}

/* Baseline sequencial usado no calculo do speedup. */
static KERNEL int64_t integral_sequencial(int64_t a, int64_t b, int64_t n)
{
    const int64_t h = (b - a) / n;
    int64_t soma = 0;

    for (int64_t i = 1; i < n; ++i) {
        const int64_t x_i = a + i * h;
        soma += funcao_integrando(x_i);
    }

    return finalizar_integral(a, b, h, soma);
}

/* Versao propositalmente sem sincronizacao: contem race condition em soma. */
static KERNEL int64_t integral_sem_sincronizacao(int64_t a, int64_t b, int64_t n)
{
    const int64_t h = (b - a) / n;
    int64_t soma = 0;

    #pragma omp parallel for schedule(static) shared(soma)
    for (int64_t i = 1; i < n; ++i) {
        const int64_t x_i = a + i * h;
        soma += funcao_integrando(x_i);
    }

    return finalizar_integral(a, b, h, soma);
}

/* Atualiza o acumulador dentro de uma regiao critica. */
static KERNEL int64_t integral_critical(int64_t a, int64_t b, int64_t n)
{
    const int64_t h = (b - a) / n;
    int64_t soma = 0;

    #pragma omp parallel for schedule(static) shared(soma)
    for (int64_t i = 1; i < n; ++i) {
        const int64_t x_i = a + i * h;
        const int64_t valor = funcao_integrando(x_i);

        #pragma omp critical
        {
            soma += valor;
        }
    }

    return finalizar_integral(a, b, h, soma);
}

/* Protege apenas a operacao de soma com atomic. */
static KERNEL int64_t integral_atomic(int64_t a, int64_t b, int64_t n)
{
    const int64_t h = (b - a) / n;
    int64_t soma = 0;

    #pragma omp parallel for schedule(static) shared(soma)
    for (int64_t i = 1; i < n; ++i) {
        const int64_t x_i = a + i * h;
        const int64_t valor = funcao_integrando(x_i);

        #pragma omp atomic update
        soma += valor;
    }

    return finalizar_integral(a, b, h, soma);
}

/* Usa acumuladores privados por thread e reducao ao final do loop. */
static KERNEL int64_t integral_reduction(int64_t a, int64_t b, int64_t n)
{
    const int64_t h = (b - a) / n;
    int64_t soma = 0;

    #pragma omp parallel for schedule(static) reduction(+:soma)
    for (int64_t i = 1; i < n; ++i) {
        const int64_t x_i = a + i * h;
        soma += funcao_integrando(x_i);
    }

    return finalizar_integral(a, b, h, soma);
}

static struct timespec obter_tempo(void)
{
    struct timespec tempo;

    if (clock_gettime(CLOCK_MONOTONIC, &tempo) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    return tempo;
}

static uint64_t diferenca_ns(struct timespec inicio, struct timespec fim)
{
    int64_t segundos = (int64_t)fim.tv_sec - (int64_t)inicio.tv_sec;
    int64_t nanos = (int64_t)fim.tv_nsec - (int64_t)inicio.tv_nsec;

    if (nanos < 0) {
        --segundos;
        nanos += INT64_C(1000000000);
    }

    return (uint64_t)segundos * UINT64_C(1000000000) + (uint64_t)nanos;
}

static int64_t resultado_esperado(int64_t a, int64_t b, int64_t n)
{
    const int64_t h = (b - a) / n;
    const int64_t erro = ((b - a) * h * h) / 6;
    return INTEGRAL_EXATA + erro;
}

static uint64_t calcular_erro_abs(int64_t resultado)
{
    if (resultado >= INTEGRAL_EXATA)
        return (uint64_t)(resultado - INTEGRAL_EXATA);

    return (uint64_t)(INTEGRAL_EXATA - resultado);
}

/*
 * Cronometra um lote de chamadas do kernel. Nenhum I/O ocorre
 * dentro da regiao medida. O resultado e consumido somente apos t1.
 */
static uint64_t medir_lote(FuncaoIntegral funcao, int64_t a, int64_t b,
                           int64_t n, int repeticoes, int64_t *resultado)
{
    const struct timespec inicio = obter_tempo();

    for (int r = 0; r < repeticoes; ++r)
        *resultado = funcao(a, b, n);

    const struct timespec fim = obter_tempo();
    destino_resultado = *resultado;

    return diferenca_ns(inicio, fim);
}

/* Determina um lote com duracao proxima ao tempo-alvo. */
static int determinar_repeticoes(FuncaoIntegral funcao, int64_t a, int64_t b, int64_t n)
{
    int repeticoes = 1;
    int64_t resultado = 0;

    for (int tentativa = 0; tentativa < 12; ++tentativa) {
        const uint64_t tempo_ns =
            medir_lote(funcao, a, b, n, repeticoes, &resultado);

        if (tempo_ns >= TEMPO_ALVO_LOTE_NS ||
            repeticoes >= MAX_REPETICOES_LOTE)
            return repeticoes;

        uint64_t proximo;

        if (tempo_ns == 0) {
            proximo = (uint64_t)repeticoes * 10;
        } else {
            proximo =
                (TEMPO_ALVO_LOTE_NS * (uint64_t)repeticoes + tempo_ns - 1) /
                tempo_ns;

            if (proximo <= (uint64_t)repeticoes)
                proximo = (uint64_t)repeticoes * 2;
        }

        /* Evita saltos excessivos causados por uma leitura piloto atipica. */
        const uint64_t limite_crescimento = (uint64_t)repeticoes * 10;
        if (proximo > limite_crescimento)
            proximo = limite_crescimento;
        if (proximo > MAX_REPETICOES_LOTE)
            proximo = MAX_REPETICOES_LOTE;

        repeticoes = (int)proximo;
    }

    return repeticoes;
}

/*
 * Executa calibracao, aquecimentos e 20 medicoes de uma versao.
 * Para SemSync, resultados incorretos sao registrados, nao abortados.
 */
static ResultadoBenchmark medir_versao(const Versao *versao,
                                        int64_t a, int64_t b, int64_t n)
{
    ResultadoBenchmark resultado_benchmark = {0};
    const int64_t esperado = resultado_esperado(a, b, n);
    const int repeticoes = determinar_repeticoes(versao->funcao, a, b, n);
    int64_t resultado = 0;

    for (int aquecimento = 0; aquecimento < NUM_AQUECIMENTOS; ++aquecimento)
        (void)medir_lote(versao->funcao, a, b, n, repeticoes, &resultado);

    double soma_tempos_ns = 0.0;
    int validos = 0;

    for (int medicao = 0; medicao < NUM_MEDICOES; ++medicao) {
        const uint64_t lote_ns =
            medir_lote(versao->funcao, a, b, n, repeticoes, &resultado);

        soma_tempos_ns += (double)lote_ns / (double)repeticoes;

        if (resultado == esperado)
            ++validos;
    }

    if (versao->exige_correcao && validos != NUM_MEDICOES) {
        fprintf(stderr,
                "Erro de validacao em %s para N=%" PRId64
                " (%d/%d resultados corretos).\n",
                versao->nome, n, validos, NUM_MEDICOES);
        exit(EXIT_FAILURE);
    }

    resultado_benchmark.resultado = resultado;
    resultado_benchmark.erro_abs = calcular_erro_abs(resultado);
    resultado_benchmark.validos = validos;
    resultado_benchmark.repeticoes_lote = repeticoes;
    resultado_benchmark.tempo_medio_ms =
        (soma_tempos_ns / (double)NUM_MEDICOES) / 1000000.0;

    return resultado_benchmark;
}

static void imprimir_cabecalho(void)
{
    printf("Tarefa 6 - Integracao por Metodo do Trapezio com OpenMP\n");
    printf("f(x)          : x^2\n");
    printf("Intervalo     : [%" PRId64 ", %" PRId64 "]\n", LIMITE_A, LIMITE_B);
    printf("Tipo numerico : int64_t\n");
    printf("Threads       : %d\n", NUM_THREADS);
    printf("Schedule      : static\n");
    printf("Relogio       : CLOCK_MONOTONIC\n");
    printf("Aquecimentos  : %d\n", NUM_AQUECIMENTOS);
    printf("Medicoes      : %d\n", NUM_MEDICOES);
    printf("Batch alvo    : %.1f ms (adaptativo)\n\n",
           (double)TEMPO_ALVO_LOTE_NS / 1000000.0);

    printf("%-10s %-12s %-22s %-16s %-9s %-12s %-9s %-7s\n",
           "N", "Versao", "Integral", "ErroAbs", "Validos",
           "Media(ms)", "Speedup", "Batch");
    printf("-----------------------------------------------------------------------------------------------------------\n");
}

int main(void)
{
    static const int64_t valores_n[] = {
        INT64_C(24000),
        INT64_C(120000),
        INT64_C(240000),
        INT64_C(400000),
        INT64_C(800000),
        INT64_C(1200000),
        INT64_C(2400000)
    };

    static const Versao versoes[] = {
        {"Sequencial", integral_sequencial, 1},
        {"SemSync", integral_sem_sincronizacao, 0},
        {"Critical", integral_critical, 1},
        {"Atomic", integral_atomic, 1},
        {"Reduction", integral_reduction, 1}
    };

    const size_t quantidade_n = sizeof(valores_n) / sizeof(valores_n[0]);
    const size_t quantidade_versoes = sizeof(versoes) / sizeof(versoes[0]);

    /* Mantem o numero de threads fixo durante todo o experimento. */
    omp_set_dynamic(0);
    omp_set_num_threads(NUM_THREADS);

    FILE *csv = fopen("resultados.csv", "w");
    if (csv == NULL) {
        perror("resultados.csv");
        return EXIT_FAILURE;
    }

    fprintf(csv,
            "N,versao,resultado,erro_abs,validos,medicoes,tempo_medio_ms,speedup,repeticoes_batch\n");

    imprimir_cabecalho();

    for (size_t i = 0; i < quantidade_n; ++i) {
        const int64_t n = valores_n[i];

        if (n <= 0 || (LIMITE_B - LIMITE_A) % n != 0) {
            fprintf(stderr, "Erro: N=%" PRId64 " nao divide o intervalo.\n", n);
            fclose(csv);
            return EXIT_FAILURE;
        }

        ResultadoBenchmark resultados[5];

        for (size_t v = 0; v < quantidade_versoes; ++v)
            resultados[v] = medir_versao(&versoes[v], LIMITE_A, LIMITE_B, n);

        const double tempo_sequencial = resultados[0].tempo_medio_ms;

        for (size_t v = 0; v < quantidade_versoes; ++v) {
            const double speedup =
                tempo_sequencial / resultados[v].tempo_medio_ms;

            printf("%-10" PRId64 " %-12s %-22" PRId64 " %-16" PRIu64
                   " %2d/%-6d %-12.6f %-9.3f %-7d\n",
                   n,
                   versoes[v].nome,
                   resultados[v].resultado,
                   resultados[v].erro_abs,
                   resultados[v].validos,
                   NUM_MEDICOES,
                   resultados[v].tempo_medio_ms,
                   speedup,
                   resultados[v].repeticoes_lote);

            fprintf(csv,
                    "%" PRId64 ",%s,%" PRId64 ",%" PRIu64 ",%d,%d,%.9f,%.6f,%d\n",
                    n,
                    versoes[v].nome,
                    resultados[v].resultado,
                    resultados[v].erro_abs,
                    resultados[v].validos,
                    NUM_MEDICOES,
                    resultados[v].tempo_medio_ms,
                    speedup,
                    resultados[v].repeticoes_lote);
        }

        printf("\n");
    }

    fclose(csv);
    (void)destino_resultado;

    printf("Resultados CSV: resultados.csv\n");
    printf("Nota: o speedup de SemSync nao representa uma solucao funcionalmente valida.\n");

    return EXIT_SUCCESS;
}
