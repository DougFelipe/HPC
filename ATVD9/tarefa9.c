#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>

#define MAX_NOME 100
#define NUM_ARQUIVOS 5
#define NUM_DIGITOS 10

typedef struct No {
    int arquivo[MAX_NOME];
    int tamanho_arquivo;
    struct No *proximo;
} No;

// Cria um novo no e preenche o arquivo com algarismos aleatorios.
No *criar_no(void)
{
    No *novo = (No *) malloc(sizeof(No));

    if (novo == NULL) {
        fprintf(stderr, "Erro na alocacao de memoria.\n");
        exit(EXIT_FAILURE);
    }

    // Define aleatoriamente o tamanho do arquivo.
    novo->tamanho_arquivo = 1 + rand() % MAX_NOME;

    // Preenche o arquivo com algarismos de 0 a 9.
    for (int i = 0; i < novo->tamanho_arquivo; i++) {
        novo->arquivo[i] = rand() % NUM_DIGITOS;
    }

    novo->proximo = NULL;
    return novo;
}

// Insere um novo arquivo no final da lista encadeada.
void inserir(No **lista)
{
    No *novo = criar_no();

    if (*lista == NULL) {
        *lista = novo;
        return;
    }

    No *atual = *lista;

    while (atual->proximo != NULL) {
        atual = atual->proximo;
    }

    atual->proximo = novo;
}

// Imprime os arquivos da lista. Mantida para inspecao manual, se necessario.
void imprimir_lista(const No *lista)
{
    const No *atual = lista;
    int numero_arquivo = 1;

    while (atual != NULL) {
        printf("Arquivo %d (tamanho = %d):\n",
               numero_arquivo,
               atual->tamanho_arquivo);

        for (int i = 0; i < atual->tamanho_arquivo; i++) {
            printf("%d ", atual->arquivo[i]);
        }

        printf("\n\n");
        atual = atual->proximo;
        numero_arquivo++;
    }
}

// Libera todos os nos da lista encadeada.
void liberar_lista(No *lista)
{
    No *atual = lista;

    while (atual != NULL) {
        No *temp = atual;
        atual = atual->proximo;
        free(temp);
    }
}

// Zera um histograma de algarismos de 0 a 9.
void zerar_histograma(int histograma[NUM_DIGITOS])
{
    for (int i = 0; i < NUM_DIGITOS; i++) {
        histograma[i] = 0;
    }
}

// Gera a referencia sequencial usada para validar o processamento paralelo.
void contar_sequencial(const No *lista, int histograma[NUM_DIGITOS])
{
    zerar_histograma(histograma);

    for (const No *atual = lista; atual != NULL; atual = atual->proximo) {
        for (int i = 0; i < atual->tamanho_arquivo; i++) {
            int digito = atual->arquivo[i];
            histograma[digito]++;
        }
    }
}

// Processa um unico arquivo dentro de uma task OpenMP.
// A contagem e local; o lock protege apenas a consolidacao no histograma global.
void processar_arquivo(const No *no,
                       int histograma_global[NUM_DIGITOS],
                       omp_lock_t *lock)
{
    int histograma_local[NUM_DIGITOS] = {0};

    for (int i = 0; i < no->tamanho_arquivo; i++) {
        int digito = no->arquivo[i];
        histograma_local[digito]++;
    }

    omp_set_lock(lock);

    for (int d = 0; d < NUM_DIGITOS; d++) {
        histograma_global[d] += histograma_local[d];
    }

    omp_unset_lock(lock);
}

// Percorre a lista com uma unica thread e cria exatamente uma task por arquivo.
void processar_lista_paralelo(const No *lista,
                              int histograma_global[NUM_DIGITOS],
                              int *tasks_criadas,
                              int *threads_utilizadas)
{
    omp_lock_t lock;

    zerar_histograma(histograma_global);
    *tasks_criadas = 0;
    *threads_utilizadas = 0;

    omp_init_lock(&lock);

    #pragma omp parallel shared(lista, histograma_global, lock, tasks_criadas, threads_utilizadas)
    {
        #pragma omp single
        {
            *threads_utilizadas = omp_get_num_threads();

            const No *atual = lista;

            while (atual != NULL) {
                // Captura o no atual antes que o ponteiro de travessia avance.
                const No *no_task = atual;

                #pragma omp task firstprivate(no_task) shared(histograma_global, lock)
                {
                    processar_arquivo(no_task, histograma_global, &lock);
                }

                (*tasks_criadas)++;
                atual = atual->proximo;
            }

            // Garante a conclusao de todas as tasks filhas antes de prosseguir.
            #pragma omp taskwait
        }
    }

    omp_destroy_lock(&lock);
}

// Compara os dez contadores da referencia sequencial e do resultado OpenMP.
int validar_histogramas(const int referencia[NUM_DIGITOS],
                        const int paralelo[NUM_DIGITOS])
{
    for (int d = 0; d < NUM_DIGITOS; d++) {
        if (referencia[d] != paralelo[d]) {
            return 0;
        }
    }

    return 1;
}

// Conta quantos elementos existem em todos os arquivos da lista.
int contar_elementos(const No *lista)
{
    int total = 0;

    for (const No *atual = lista; atual != NULL; atual = atual->proximo) {
        total += atual->tamanho_arquivo;
    }

    return total;
}

// Apresenta apenas as informacoes necessarias para verificar a execucao.
void imprimir_resultado(const int histograma[NUM_DIGITOS],
                        int threads_utilizadas,
                        int tasks_criadas,
                        int total_elementos,
                        int valido)
{
    printf("============================================================\n");
    printf("TAREFA 9 - OPENMP TASKS\n");
    printf("============================================================\n\n");

    printf("CONFIGURACAO\n");
    printf("------------------------------------------------------------\n");
    printf("Arquivos             : %d\n", NUM_ARQUIVOS);
    printf("Elementos processados: %d\n", total_elementos);
    printf("Threads              : %d\n", threads_utilizadas);
    printf("Tasks criadas         : %d\n", tasks_criadas);
    printf("Estrategia            : uma task por arquivo\n");
    printf("Sincronizacao         : omp_lock_t\n");
    printf("Validacao             : referencia sequencial\n\n");

    printf("============================================================\n");
    printf("HISTOGRAMA GLOBAL\n");
    printf("============================================================\n\n");
    printf("Algarismo       Ocorrencias\n");
    printf("------------------------------------------------------------\n");

    for (int d = 0; d < NUM_DIGITOS; d++) {
        printf("%-15d %d\n", d, histograma[d]);
    }

    printf("\n============================================================\n");
    printf("VALIDACAO\n");
    printf("============================================================\n\n");
    printf("Sequencial x OpenMP  : %s\n", valido ? "OK" : "FALHA");
}

int main(void)
{
    No *lista = NULL;
    int histograma_sequencial[NUM_DIGITOS];
    int histograma_paralelo[NUM_DIGITOS];
    int tasks_criadas = 0;
    int threads_utilizadas = 0;

    srand((unsigned int) time(NULL));

    // Inicializacao fornecida originalmente: permanece sequencial.
    for (int i = 0; i < NUM_ARQUIVOS; i++) {
        inserir(&lista);
    }

    // A impressao completa da lista foi mantida como funcao auxiliar,
    // mas nao faz parte do output normal da atividade.
    // imprimir_lista(lista);

    contar_sequencial(lista, histograma_sequencial);

    processar_lista_paralelo(lista,
                             histograma_paralelo,
                             &tasks_criadas,
                             &threads_utilizadas);

    int valido = validar_histogramas(histograma_sequencial,
                                     histograma_paralelo);

    int total_elementos = contar_elementos(lista);

    imprimir_resultado(histograma_paralelo,
                       threads_utilizadas,
                       tasks_criadas,
                       total_elementos,
                       valido);

    liberar_lista(lista);

    return valido ? EXIT_SUCCESS : EXIT_FAILURE;
}
