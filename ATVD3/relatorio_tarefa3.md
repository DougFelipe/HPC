# Tarefa 3: Contador com Processos e Threads

## 1. Objetivo da atividade

Esta atividade tem como objetivo analisar o comportamento da variavel `contador` em dois programas concorrentes escritos em C:

- `cont1.c`, baseado em processos com `fork()`;
- `cont2.c`, baseado em threads com `pthread`.

O foco da analise e explicar os valores de saida observados, identificar os problemas presentes em cada implementacao e discutir uma solucao adequada em linguagem natural, sem alterar o codigo original da atividade.

## 2. Arquivos analisados

- `cont1.c`
- `cont2.c`

Todos os arquivos desta atividade foram organizados no diretorio `ATVD3`.

## 3. Metodologia de execucao

A proposta de execucao para obtencao dos resultados reais e:

1. acessar o ambiente URCT;
2. entrar no diretorio `ATVD3`;
3. compilar cada programa;
4. executar varias vezes os binarios;
5. registrar as saidas observadas para posterior discussao.

Comandos previstos:

```bash
gcc cont1.c -o cont1
./cont1

gcc cont2.c -o cont2 -pthread
./cont2
```

Para a coleta dos resultados:

- `cont1.c` deve ser executado ao menos 3 vezes;
- `cont2.c` deve ser executado ao menos 10 vezes, para evidenciar a variacao do valor final.

## 4. Execucao realizada e base experimental

Os programas foram executados localmente em ambiente `UCRT64`, no diretorio `ATVD3`, por meio do script `executar_urct.sh`, em **19 de agosto de 2026**.

Os arquivos gerados para analise foram:

- `saida_cont1.txt`
- `saida_cont2.txt`

Esses resultados experimentais passam a ser a base principal da discussao deste relatorio.

## 5. Analise tecnica do `cont1.c`

O programa `cont1.c` cria quatro processos filhos por meio da chamada `fork()`. Cada processo filho executa um laco com um milhao de incrementos sobre a variavel global `contador`.

### 5.1. Comportamento observado esperado

O comportamento esperado e:

- o contador inicial impresso pelo processo pai sera `0`;
- cada processo filho devera imprimir um valor local de `1000000`;
- ao final, o processo pai devera imprimir `0`.

### 5.1.1. Resultado efetivamente observado

Nas tres execucoes registradas em `saida_cont1.txt`, foi observado o seguinte padrao:

- todos os filhos imprimiram `contador = 1000000`;
- o processo pai imprimiu `Contador no processo pai: 0`;
- a mensagem `Contador inicial: 0` apareceu repetida multiplas vezes dentro da mesma execucao.

### 5.2. Explicacao do comportamento

Esse resultado ocorre porque `fork()` cria um novo processo com **espaco de memoria proprio**. No momento da criacao, o processo filho recebe uma copia do estado do processo pai, incluindo a variavel global `contador`, inicialmente igual a `0`.

A partir desse ponto, cada filho incrementa **sua propria copia** da variavel. Isso significa que os incrementos realizados por um processo filho nao sao refletidos nem no pai nem nos outros filhos.

Portanto:

- cada filho parte de `0` e chega a `1000000`;
- o processo pai nunca altera sua propria copia de `contador`;
- o valor final no pai continua sendo `0`.

Ha ainda um detalhe importante na saida observada: a repeticao da linha `Contador inicial: 0` nao indica que o bloco inicial do `main` foi executado varias vezes do zero por erro logico. O que ocorre e um efeito de **buffer de saida padrao**.

Como o `printf("Contador inicial: %d\n", contador);` foi executado antes das chamadas subsequentes a `fork()`, o conteudo do buffer de `stdout` pode ser herdado pelos processos filhos. Quando cada processo termina ou descarrega sua saida, essa mensagem pode reaparecer, produzindo a repeticao observada no arquivo.

Assim, no `cont1.c`, existem dois fenomenos simultaneos:

- isolamento de memoria entre processos;
- duplicacao de saida causada por heranca do buffer de `stdout` no uso de `fork()`.

### 5.3. Problema identificado

O problema conceitual do codigo e que ele aparenta realizar um trabalho concorrente sobre um contador comum, mas na pratica **nao existe compartilhamento real de memoria** entre os processos.

Em outras palavras, o valor final do pai nao representa a soma do trabalho dos filhos.

### 5.4. Solucao adequada

A solucao adequada e utilizar um mecanismo de **comunicacao entre processos** para compartilhar ou transferir o resultado.

As alternativas teoricas mais apropriadas sao:

- memoria compartilhada;
- pipes;
- filas de mensagens;
- memoria mapeada.

Se a intencao for que todos os processos atualizem um mesmo contador global, a abordagem mais direta e usar **memoria compartilhada com controle de sincronizacao**, evitando escrita concorrente desordenada.

## 6. Analise tecnica do `cont2.c`

O programa `cont2.c` cria quatro threads com `pthread_create`. Todas executam a funcao `incrementar`, que realiza um milhao de incrementos sobre a variavel global `contador`.

### 6.1. Comportamento observado esperado

O valor teorico esperado seria:

```text
4 x 1000000 = 4000000
```

Entretanto, na pratica, o valor final impresso pode:

- variar entre execucoes;
- ficar abaixo de `4000000`;
- ocasionalmente aproximar-se do valor esperado, mas sem garantia.

### 6.1.1. Resultado efetivamente observado

Nas dez execucoes registradas em `saida_cont2.txt`, os valores finais foram:

- `1431347`
- `1305740`
- `1171730`
- `1306585`
- `1235595`
- `1292238`
- `1222249`
- `1681956`
- `1240474`
- `1347679`

Resumo quantitativo das execucoes:

- minimo observado: `1171730`
- maximo observado: `1681956`
- media observada: `1323559,3`

Nenhuma execucao atingiu o valor teorico de `4000000`.

### 6.2. Explicacao do comportamento

Ao contrario dos processos, as threads compartilham o mesmo espaco de memoria do processo. Assim, todas acessam a mesma variavel global `contador`.

O problema e que a operacao:

```c
contador++;
```

nao e atomica. Ela envolve, conceitualmente, tres etapas:

1. ler o valor atual de `contador`;
2. somar `1`;
3. gravar o novo valor.

Quando duas ou mais threads executam essa sequencia ao mesmo tempo, uma atualizacao pode sobrescrever a outra. Esse fenomeno caracteriza uma **condicao de corrida**.

Como consequencia, parte dos incrementos se perde, e o valor final deixa de ser deterministico.

### 6.3. Problema identificado

O problema central do codigo e a ausencia de **sincronizacao** no acesso a um dado compartilhado.

O codigo cria concorrencia real entre as threads, mas nao protege a regiao critica onde o contador e atualizado.

### 6.4. Solucao adequada

A solucao adequada e usar um mecanismo de sincronizacao para garantir exclusao mutua durante a atualizacao do contador.

As alternativas teoricas mais apropriadas sao:

- `pthread_mutex_t` para proteger a regiao critica;
- operacoes atomicas;
- reducao local por thread seguida de agregacao final controlada.

A resposta mais classica para esse caso e utilizar um **mutex**, de modo que apenas uma thread por vez possa modificar `contador`.

## 7. Comparacao entre os dois codigos

Os dois programas trabalham com concorrencia, mas apresentam problemas diferentes:

- em `cont1.c`, o problema principal nao e disputa de escrita, e sim a **falta de memoria compartilhada**;
- em `cont2.c`, a memoria e compartilhada, mas falta **sincronizacao** no acesso concorrente.

Essa diferenca explica por que:

- no programa com processos, cada filho chega ao valor correto localmente, o pai permanece com `0` e a saida ainda pode repetir mensagens por causa do buffer herdado no `fork()`;
- no programa com threads, existe um unico contador compartilhado, mas o valor final pode sair incorreto devido a perdas de atualizacao.

## 8. Conclusao

O estudo dos dois codigos mostra que concorrencia nao significa automaticamente cooperacao correta entre unidades de execucao.

No caso dos processos, o isolamento de memoria impede que o pai enxergue os incrementos feitos pelos filhos, a menos que seja utilizado um mecanismo explicito de compartilhamento ou comunicacao.

No caso das threads, o compartilhamento de memoria existe naturalmente, mas isso exige sincronizacao apropriada para evitar condicoes de corrida.

Assim, a solucao correta depende do modelo de concorrencia adotado:

- para processos, e necessario compartilhar ou transmitir os resultados entre os processos;
- para threads, e necessario proteger o acesso ao dado compartilhado.

## 9. Resultados reais registrados

### `cont1.c`

Execucoes observadas em `saida_cont1.txt`:

- Execucao 1: quatro filhos com `1000000` e pai com `0`
- Execucao 2: quatro filhos com `1000000` e pai com `0`
- Execucao 3: quatro filhos com `1000000` e pai com `0`

Pontos a observar:

- cada filho imprimiu `1000000`;
- o pai imprimiu `0`;
- a ordem das mensagens pode variar;
- a mensagem `Contador inicial: 0` apareceu repetida devido ao buffer de `stdout` herdado pelos filhos.

### `cont2.c`

Execucoes observadas em `saida_cont2.txt`:

- Execucao 1: `1431347`
- Execucao 2: `1305740`
- Execucao 3: `1171730`
- Execucao 4: `1306585`
- Execucao 5: `1235595`
- Execucao 6: `1292238`
- Execucao 7: `1222249`
- Execucao 8: `1681956`
- Execucao 9: `1240474`
- Execucao 10: `1347679`

Pontos a observar:

- o valor final variou entre execucoes;
- o valor ficou abaixo de `4000000` em todas as execucoes;
- isso confirma a condicao de corrida.
