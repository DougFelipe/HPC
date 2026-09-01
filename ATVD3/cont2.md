# cont2.c

## Compilacao

```bash
gcc cont2.c -o cont2 -pthread
```

## Objetivo

Demonstrar o uso de threads POSIX (`pthread`) e o acesso concorrente a uma variavel global compartilhada entre todas as threads do processo.

## Funcionamento tecnico

1. O programa declara a variavel global `contador` com valor inicial `0`.
2. A funcao `incrementar` e usada como rotina de execucao para cada thread.
3. O processo principal cria `NUM_THREADS` threads com `pthread_create`.
4. Cada thread realiza `INCREMENTOS` incrementos sobre a mesma variavel global.
5. O programa espera todas as threads finalizarem com `pthread_join`.
6. Ao final, imprime o valor final de `contador`.

## Observacao importante

O codigo foi mantido sem sincronizacao para preservar a implementacao da atividade. Tecnicamente, isso significa que varias threads podem acessar e modificar `contador` ao mesmo tempo.

## Codigo comentado

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTOS 1000000

int contador = 0;

void *incrementar(void *arg) {
    for (int i = 0; i < INCREMENTOS; i++) {
        contador++;
    }

    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    printf("Contador inicial: %d\n", contador);

    for (int i = 0; i < NUM_THREADS; i++) {
        int erro = pthread_create(
            &threads[i],
            NULL,
            incrementar,
            NULL
        );

        if (erro != 0) {
            fprintf(stderr, "Erro ao criar thread\n");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nContador final: %d\n", contador);

    return 0;
}
```
