#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 * Exemplo de concorrencia com processos.
 *
 * Compilacao:
 *     gcc cont1.c -o cont1
 *
 * Este programa cria quatro processos filhos com fork().
 * Cada filho recebe sua propria copia da variavel global
 * "contador" e realiza um milhao de incrementos localmente.
 *
 * Como os processos possuem espacos de memoria independentes,
 * o valor alterado por cada filho nao e compartilhado com o
 * processo pai. Por isso, ao final da execucao, o pai ainda
 * imprime o valor original do contador global.
 */

#define NUM_PROCESSOS 4
#define INCREMENTOS 1000000

/* Variavel global usada para ilustrar o efeito de memoria separada. */
int contador = 0;

int main() {
    printf("Contador inicial: %d\n", contador);

    for (int i = 0; i < NUM_PROCESSOS; i++) {
        pid_t pid = fork();

        /* Verifica falha na criacao do processo filho. */
        if (pid < 0) {
            perror("Erro ao criar processo");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            /* Codigo executado apenas no processo filho. */
            for (int j = 0; j < INCREMENTOS; j++) {
                contador++;
            }

            /*
             * Cada filho mostra o proprio PID e o valor do contador
             * presente em sua copia privada da memoria do processo.
             */
            printf("Filho %d: contador = %d\n", getpid(), contador);

            exit(EXIT_SUCCESS);
        }
    }

    /* O processo pai aguarda o termino de todos os processos filhos. */
    for (int i = 0; i < NUM_PROCESSOS; i++) {
        wait(NULL);
    }

    printf("\nContador no processo pai: %d\n", contador);

    return 0;
}
