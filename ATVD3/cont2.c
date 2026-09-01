#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/*
 * Exemplo de concorrencia com threads.
 *
 * Compilacao:
 *     gcc cont2.c -o cont2 -pthread
 *
 * Este programa cria quatro threads que executam a mesma funcao
 * de incremento sobre a variavel global "contador".
 *
 * Diferentemente do caso com processos, todas as threads compartilham
 * o mesmo espaco de memoria do processo. Assim, todas acessam a mesma
 * variavel global durante a execucao.
 *
 * Observacao tecnica: o codigo nao usa mecanismos de sincronizacao
 * como mutex. Portanto, ele foi mantido exatamente no formato da
 * atividade, servindo para demonstrar acesso concorrente a memoria
 * compartilhada.
 */

#define NUM_THREADS 4
#define INCREMENTOS 1000000

/* Variavel global compartilhada entre todas as threads criadas. */
int contador = 0;

void *incrementar(void *arg) {
    /*
     * Cada thread executa a mesma rotina de incremento repetido.
     * O parametro "arg" existe por compatibilidade com pthread_create,
     * embora nao seja utilizado nesta implementacao.
     */
    for (int i = 0; i < INCREMENTOS; i++) {
        contador++;
    }

    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    printf("Contador inicial: %d\n", contador);

    /* Cria as threads e associa cada uma a rotina "incrementar". */
    for (int i = 0; i < NUM_THREADS; i++) {
        int erro = pthread_create(
            &threads[i],
            NULL,
            incrementar,
            NULL
        );

        /* Interrompe o programa caso a thread nao possa ser criada. */
        if (erro != 0) {
            fprintf(stderr, "Erro ao criar thread\n");
            exit(EXIT_FAILURE);
        }
    }

    /* Aguarda a finalizacao de todas as threads antes de encerrar. */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nContador final: %d\n", contador);

    return 0;
}
