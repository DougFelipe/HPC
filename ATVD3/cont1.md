# cont1.c

## Compilacao

```bash
gcc cont1.c -o cont1
```

## Objetivo

Demonstrar o uso de processos com `fork()` e evidenciar que cada processo filho trabalha com uma copia propria da variavel global `contador`.

## Funcionamento tecnico

1. O programa declara a variavel global `contador` com valor inicial `0`.
2. O processo principal cria `NUM_PROCESSOS` filhos por meio de `fork()`.
3. Cada processo filho executa `INCREMENTOS` operacoes de incremento sobre sua propria copia de `contador`.
4. Cada filho imprime seu PID e o valor local do contador.
5. O processo pai aguarda todos os filhos com `wait(NULL)`.
6. Ao final, o pai imprime seu proprio valor de `contador`, que permanece inalterado.

## Observacao importante

O comportamento central do exemplo e mostrar que processos nao compartilham automaticamente a memoria global. Assim, os incrementos feitos pelos filhos nao alteram o `contador` do processo pai.

## Codigo comentado

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define NUM_PROCESSOS 4
#define INCREMENTOS 1000000

int contador = 0;

int main() {
    printf("Contador inicial: %d\n", contador);

    for (int i = 0; i < NUM_PROCESSOS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Erro ao criar processo");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            for (int j = 0; j < INCREMENTOS; j++) {
                contador++;
            }

            printf("Filho %d: contador = %d\n", getpid(), contador);

            exit(EXIT_SUCCESS);
        }
    }

    for (int i = 0; i < NUM_PROCESSOS; i++) {
        wait(NULL);
    }

    printf("\nContador no processo pai: %d\n", contador);

    return 0;
}
```
