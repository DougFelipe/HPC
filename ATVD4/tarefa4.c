/*
 * Tarefa 4 - Integração Numérica Sequencial
 *
 * Método: Trapézio Composto
 * Função: f(x) = x^2
 * Intervalo: [0, 2400000]
 *
 * O programa avalia diferentes números de subdivisões N e mede
 * o tempo de execução da integração sequencial.
 *
 * Para tornar a medição mais estável:
 *   - são feitos 3 aquecimentos;
 *   - são realizadas 20 medições;
 *   - cada medição executa o kernel 1000 vezes;
 *   - o tempo do lote é normalizado para uma única execução;
 *   - a mediana é utilizada como tempo representativo.
 *
 * A implementação utiliza int64_t para evitar erros de ponto
 * flutuante durante o cálculo da integral.
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>      /* printf, fprintf, perror */
#include <stdint.h>     /* int64_t, uint64_t */
#include <stdlib.h>     /* qsort, exit */
#include <inttypes.h>   /* PRId64 */
#include <time.h>       /* clock_gettime, timespec */


/* Configuração do experimento. */
#define NUM_AQUECIMENTOS     3
#define NUM_MEDICOES         20
#define REPETICOES_INTERNAS  1000

/* Intervalo de integração. */
#define LIMITE_A INT64_C(0)
#define LIMITE_B INT64_C(2400000)

/*
 * Valor analítico exato:
 *
 *           2400000
 *             ∫ x² dx
 *               0
 *
 * = 2400000³ / 3
 * = 4608000000000000000
 */
#define INTEGRAL_EXATA INT64_C(4608000000000000000)


/*
 * Recebe o resultado calculado após a região cronometrada.
 *
 * O uso de volatile aqui cria um efeito observável e ajuda a impedir
 * que o compilador considere as chamadas do kernel desnecessárias.
 *
 * O acumulador da integral NÃO é volatile, pois isso alteraria
 * artificialmente o desempenho do algoritmo.
 */
static volatile int64_t destino_resultado = 0;


/*
 * O kernel deve ser realmente executado em cada repetição.
 *
 * noinline:
 *   impede que a função seja incorporada diretamente ao chamador.
 *
 * noipa:
 *   impede otimizações interprocedurais que poderiam fazer o GCC
 *   perceber que chamadas repetidas com os mesmos argumentos
 *   produzem sempre o mesmo resultado e reutilizar o valor.
 */
#if defined(__GNUC__)
#define KERNEL __attribute__((noinline, noipa))
#else
#define KERNEL
#endif


/*
 * Função utilizada na integração:
 *
 *     f(x) = x²
 */
static inline int64_t funcao_integrando(int64_t x)
{
    return x * x;
}


/*
 * Calcula a integral utilizando o método composto do trapézio.
 *
 * Para N subdivisões:
 *
 *     h = (b - a) / N
 *
 * Os pontos internos são:
 *
 *     x_i = a + i*h
 *
 * A formulação utilizada é:
 *
 *     I = h * soma(f(x_i))
 *         + h * (f(a) + f(b)) / 2
 *
 * com i variando de 1 até N-1.
 *
 * O ponto x_i é calculado diretamente a partir de i, evitando
 * dependência entre x_i e x_(i-1). Dessa forma, a dependência
 * sequencial relevante do loop fica concentrada no acumulador soma.
 */
static KERNEL int64_t integral_trapezio(
    int64_t a,
    int64_t b,
    int64_t n)
{
    /* Largura de cada subdivisão do intervalo. */
    const int64_t h = (b - a) / n;

    /* Acumula os valores da função nos pontos internos. */
    int64_t soma = 0;

    for (int64_t i = 1; i < n; ++i) {

        /*
         * O ponto atual depende apenas de a, h e i.
         * Não depende do ponto calculado na iteração anterior.
         */
        const int64_t x_i = a + i * h;

        soma += funcao_integrando(x_i);
    }

    /* f(a) + f(b), que recebem peso 1 no método do trapézio. */
    const int64_t extremos =
        funcao_integrando(a) +
        funcao_integrando(b);

    /*
     * Os pontos internos possuem peso 2 na fórmula tradicional.
     *
     * A expressão abaixo é uma forma equivalente reorganizada para
     * evitar a multiplicação intermediária 2*soma, que poderia
     * aproximar-se desnecessariamente do limite de int64_t.
     */
    return h * soma + (h * extremos) / 2;
}


/*
 * Obtém um instante usando CLOCK_MONOTONIC.
 *
 * CLOCK_MONOTONIC é utilizado porque representa tempo decorrido
 * e não sofre alterações caso o relógio do sistema seja ajustado.
 */
static struct timespec obter_tempo(void)
{
    struct timespec tempo;

    if (clock_gettime(CLOCK_MONOTONIC, &tempo) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    return tempo;
}


/*
 * Calcula a diferença entre dois instantes em nanossegundos.
 *
 * timespec armazena separadamente:
 *   - segundos;
 *   - nanossegundos.
 *
 * O ajuste no if trata o caso em que tv_nsec do instante final
 * é menor que tv_nsec do instante inicial devido à mudança
 * de segundo entre as duas leituras.
 */
static uint64_t diferenca_ns(
    struct timespec inicio,
    struct timespec fim)
{
    int64_t segundos =
        (int64_t)fim.tv_sec - (int64_t)inicio.tv_sec;

    int64_t nanos =
        (int64_t)fim.tv_nsec - (int64_t)inicio.tv_nsec;

    if (nanos < 0) {
        --segundos;
        nanos += INT64_C(1000000000);
    }

    return
        (uint64_t)segundos * UINT64_C(1000000000) +
        (uint64_t)nanos;
}


/*
 * Função auxiliar utilizada pelo qsort para ordenar
 * os tempos de execução em ordem crescente.
 */
static int comparar_uint64(const void *a, const void *b)
{
    const uint64_t x = *(const uint64_t *)a;
    const uint64_t y = *(const uint64_t *)b;

    return (x > y) - (x < y);
}


/*
 * Calcula a mediana das 20 medições.
 *
 * Como NUM_MEDICOES é par, a mediana corresponde à média
 * dos dois valores centrais após a ordenação.
 *
 * A mediana é utilizada porque é menos sensível a medições
 * isoladas afetadas por interrupções ou outros processos.
 */
static double calcular_mediana(
    uint64_t tempos[NUM_MEDICOES])
{
    qsort(
        tempos,
        NUM_MEDICOES,
        sizeof(uint64_t),
        comparar_uint64
    );

    const size_t meio = NUM_MEDICOES / 2;

    return
        ((double)tempos[meio - 1] +
         (double)tempos[meio]) / 2.0;
}


/*
 * Calcula o resultado que deve ser obtido pelo método do trapézio.
 *
 * Para f(x) = x², o erro do método composto é conhecido:
 *
 *     erro = (b - a) * h² / 6
 *
 * Como x² é convexa, o método do trapézio superestima a integral.
 * Portanto:
 *
 *     resultado esperado = integral exata + erro
 *
 * Isso permite validar a implementação usando somente inteiros.
 */
static int64_t resultado_esperado(
    int64_t a,
    int64_t b,
    int64_t n)
{
    const int64_t h = (b - a) / n;

    const int64_t erro =
        ((b - a) * h * h) / 6;

    return INTEGRAL_EXATA + erro;
}


/*
 * Executa todo o experimento correspondente a um valor de N.
 *
 * Etapas:
 *   1. verificar se N é válido;
 *   2. realizar aquecimentos;
 *   3. executar as 20 medições;
 *   4. validar o resultado;
 *   5. calcular mediana e métricas;
 *   6. imprimir uma linha da tabela.
 */
static void executar_benchmark(int64_t n)
{
    const int64_t a = LIMITE_A;
    const int64_t b = LIMITE_B;

    /*
     * Como o experimento utiliza somente aritmética inteira,
     * h = (b-a)/N deve resultar em um valor inteiro.
     */
    if (n <= 0 || (b - a) % n != 0) {
        fprintf(
            stderr,
            "Erro: N=%" PRId64 " nao divide o intervalo.\n",
            n
        );

        exit(EXIT_FAILURE);
    }

    int64_t resultado = 0;
    uint64_t tempos[NUM_MEDICOES];


    /*
     * --------------------------------------------------------
     * Aquecimento
     * --------------------------------------------------------
     *
     * São executados 3 lotes antes das medições.
     * Esses tempos não são armazenados.
     *
     * O objetivo é reduzir efeitos relacionados ao estado inicial
     * do processador, caches e primeiras execuções do programa.
     */
    for (int aquecimento = 0;
         aquecimento < NUM_AQUECIMENTOS;
         ++aquecimento) {

        for (int r = 0; r < REPETICOES_INTERNAS; ++r) {
            resultado = integral_trapezio(a, b, n);
        }

        destino_resultado = resultado;
    }


    /*
     * --------------------------------------------------------
     * Medições
     * --------------------------------------------------------
     *
     * Cada uma das 20 medições cronometra 1000 execuções.
     *
     * Isso aumenta a duração da região cronometrada e reduz
     * proporcionalmente a influência do custo do temporizador.
     *
     * Nenhum printf ou cálculo estatístico ocorre entre inicio e fim.
     */
    for (int medicao = 0;
         medicao < NUM_MEDICOES;
         ++medicao) {

        const struct timespec inicio = obter_tempo();

        for (int r = 0; r < REPETICOES_INTERNAS; ++r) {
            resultado = integral_trapezio(a, b, n);
        }

        const struct timespec fim = obter_tempo();

        /*
         * Uso observável depois da região cronometrada.
         */
        destino_resultado = resultado;

        tempos[medicao] =
            diferenca_ns(inicio, fim);
    }


    /*
     * --------------------------------------------------------
     * Validação
     * --------------------------------------------------------
     *
     * O desempenho somente é considerado válido se o resultado
     * obtido for exatamente o esperado.
     */
    const int64_t esperado =
        resultado_esperado(a, b, n);

    if (resultado != esperado) {
        fprintf(
            stderr,
            "Erro de validacao para N=%" PRId64 "\n",
            n
        );

        exit(EXIT_FAILURE);
    }


    /*
     * Diferença entre a aproximação pelo trapézio
     * e o valor analítico da integral.
     */
    const int64_t erro_absoluto =
        resultado - INTEGRAL_EXATA;


    /*
     * --------------------------------------------------------
     * Tratamento das medições
     * --------------------------------------------------------
     */

    /* Mediana do tempo gasto pelo lote de 1000 execuções. */
    const double mediana_lote_ns =
        calcular_mediana(tempos);

    /*
     * Normaliza o tempo do lote para obter o tempo estimado
     * de uma única execução da integração.
     */
    const double mediana_execucao_ns =
        mediana_lote_ns / REPETICOES_INTERNAS;

    /* Conversão para milissegundos para apresentação. */
    const double mediana_ms =
        mediana_execucao_ns / 1000000.0;

    /*
     * Custo médio normalizado pelo número de trapézios.
     * Essa métrica ajuda a verificar o comportamento O(N).
     */
    const double ns_por_trapezio =
        mediana_execucao_ns / (double)n;


    /* Uma linha de resultado para o valor atual de N. */
    printf(
        "%-12" PRId64
        " %-22" PRId64
        " %-16" PRId64
        " %-14.6f"
        " %-12.3f\n",
        n,
        resultado,
        erro_absoluto,
        mediana_ms,
        ns_por_trapezio
    );
}


int main(void)
{
    /*
     * Valores de N avaliados no experimento.
     *
     * Todos são divisores exatos de 2400000, garantindo que:
     *
     *     h = (b-a)/N
     *
     * permaneça inteiro em todas as configurações.
     */
    static const int64_t valores_n[] = {
        INT64_C(24000),
        INT64_C(120000),
        INT64_C(240000),
        INT64_C(400000),
        INT64_C(800000),
        INT64_C(1200000),
        INT64_C(2400000)
    };

    const size_t quantidade =
        sizeof(valores_n) / sizeof(valores_n[0]);


    /*
     * Informações fixas do experimento.
     */
    printf("Integracao Sequencial - Metodo do Trapezio\n");
    printf("f(x)          : x^2\n");
    printf(
        "Intervalo     : [%" PRId64 ", %" PRId64 "]\n",
        LIMITE_A,
        LIMITE_B
    );
    printf("Tipo numerico : int64_t\n");
    printf("Relogio       : CLOCK_MONOTONIC\n");
    printf("Aquecimentos  : %d\n", NUM_AQUECIMENTOS);
    printf("Medicoes      : %d\n", NUM_MEDICOES);
    printf(
        "Repeticoes    : %d por medicao\n\n",
        REPETICOES_INTERNAS
    );


    /*
     * Cabeçalho da tabela.
     *
     * Integral   = aproximação calculada;
     * ErroAbs    = diferença para a integral exata;
     * Mediana    = tempo normalizado de uma execução;
     * ns/trap    = custo normalizado por trapézio.
     */
    printf(
        "%-12s %-22s %-16s %-14s %-12s\n",
        "N",
        "Integral",
        "ErroAbs",
        "Mediana(ms)",
        "ns/trap"
    );

    printf(
        "-------------------------------------------------------------------------------\n"
    );


    /*
     * Executa o benchmark completo para cada valor de N.
     */
    for (size_t i = 0; i < quantidade; ++i) {
        executar_benchmark(valores_n[i]);
    }


    /*
     * Mantém uma referência explícita à variável usada
     * para tornar os resultados observáveis.
     */
    (void)destino_resultado;

    return EXIT_SUCCESS;
}