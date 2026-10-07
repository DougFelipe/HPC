# Tarefa 10 — Arquitetura do computador e possibilidades de paralelismo

**Disciplina:** Computação de Alto Desempenho  
**Computador analisado:** Acer Nitro AN515-55  
**Data da coleta:** 29 de setembro de 2026

## 1. Objetivo e metodologia

O objetivo desta atividade é avaliar a arquitetura de um computador pessoal, identificar as possibilidades de paralelismo disponíveis e discutir estratégias adequadas para aproveitar seus recursos.

A análise foi realizada no Windows, por meio de consultas CIM do PowerShell e da API `GetLogicalProcessorInformationEx`, que forneceram informações sobre processador, memória, caches e topologia. As extensões de instruções foram verificadas com uma sonda baseada em CPUID e na consulta ao suporte de estado do sistema operacional. Para a GPU NVIDIA, foram utilizados `nvidia-smi` e consultas à CUDA Driver API. Também foi compilado e executado um programa mínimo em C com OpenMP, usando o GCC disponível no MSYS2/UCRT64.

Os resultados apresentados foram obtidos diretamente da máquina. Não foram realizados benchmarks; portanto, a análise identifica recursos e estratégias aplicáveis, sem atribuir ganhos de desempenho medidos.

## 2. Arquitetura identificada

| Recurso | Resultado da coleta |
|---|---|
| Sistema operacional | Windows 11 Home Single Language, versão 10.0.26200, 64 bits |
| Processador | Intel Core i5-10300H, arquitetura x64 |
| Organização da CPU | 1 socket, 4 núcleos físicos e 8 processadores lógicos |
| SMT | Ativo, com 2 threads de hardware por núcleo |
| Memória principal | 32 GiB instalados em 2 módulos de 16 GiB; aproximadamente 31,84 GiB reportados pelo sistema |
| Cache L1 | Por núcleo: 32 KiB para dados e 32 KiB para instruções; 256 KiB no total |
| Cache L2 | 256 KiB por núcleo; 1 MiB no total |
| Cache L3 | 8 MiB compartilhados entre os 4 núcleos |
| Topologia exposta pelo Windows | 1 grupo de processadores e 1 nó NUMA, abrangendo os 8 processadores lógicos |
| Extensões de instruções | SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX, AVX2 e FMA disponíveis ao processo; AVX512F não detectado |
| GPU NVIDIA | GeForce GTX 1650, com 4 GiB de VRAM, 14 Streaming Multiprocessors (SMs) e compute capability 7.5 |
| Outro adaptador gráfico | Intel UHD Graphics |
| Compilador | GCC 15.1.0, do MSYS2/UCRT64 |
| OpenMP | Compilação e execução confirmadas com 8 threads |
| CUDA | Driver funcional; versão CUDA 13.0 reportada pelo driver; CUDA Toolkit/NVCC não identificado |
| MPI | Não identificado no ambiente pesquisado |

Um socket corresponde à posição ocupada pelo processador físico; os quatro núcleos são unidades de execução dentro desse processador. Os oito processadores lógicos decorrem do SMT, que permite duas threads de hardware por núcleo. Assim, a máquina não possui oito núcleos físicos.

A distribuição dos caches foi confirmada pelas máscaras de afinidade: cada par de processadores lógicos de um mesmo núcleo compartilha suas instâncias L1 e L2, enquanto o L3 abrange os quatro núcleos. Essa organização torna a localidade dos dados relevante para o aproveitamento da CPU.

O Windows expôs apenas um nó NUMA. Esse resultado descreve a topologia apresentada pelo sistema operacional, mas não comprova, por medição, uniformidade das latências de acesso à memória. A análise considera a programação em memória compartilhada sem atribuir uma classificação física UMA/NUMA além das evidências disponíveis.

## 3. Possibilidades de paralelismo

### 3.1. Multicore, threads e SMT

Os quatro núcleos físicos permitem executar fluxos de instruções independentes, oferecendo paralelismo em nível de threads. Na classificação de Flynn, essa execução pode ser associada ao modelo **MIMD**, com múltiplos fluxos de instruções atuando sobre múltiplos dados.

O SMT amplia para oito o número de contextos de execução apresentados ao sistema. Entretanto, as duas threads de cada núcleo compartilham recursos internos. Por isso, utilizar oito threads pode produzir um resultado diferente de utilizar quatro, mas não garante duplicação de desempenho.

O teste funcional foi compilado com `gcc -O2 -fopenmp` e apresentou:

```text
omp_get_num_procs()=8
omp_get_max_threads()=8
effective_parallel_threads=8
```

Isso confirma que o ambiente consegue executar uma região OpenMP com oito threads. O mesmo resultado foi obtido ao executar o teste a partir do MSYS2, utilizando o mesmo compilador e a mesma máquina. Não houve medição de tempo ou de speedup.

### 3.2. SIMD e paralelismo em nível de instrução

A presença de AVX e AVX2 permite explorar **SIMD**, realizando operações sobre vários elementos de dados por instrução vetorial. Também foi identificado suporte a FMA, que oferece operações combinadas de multiplicação e adição. Esses recursos são relevantes para cálculos regulares sobre vetores, matrizes e outros conjuntos de dados numéricos.

SIMD e MIMD podem coexistir: diferentes núcleos executam fluxos independentes, enquanto cada núcleo processa grupos de elementos por instruções vetoriais. A disponibilidade dessas extensões foi verificada separadamente das opções do compilador; sua presença não demonstra que qualquer programa compilado será automaticamente vetorizado.

Pipeline e execução superscalar constituem mecanismos de paralelismo em nível de instrução abordados na disciplina. Entretanto, seus detalhes, como profundidade do pipeline e quantidade de instruções executadas por ciclo, não foram determinados nesta coleta e não foram usados para quantificar a capacidade da máquina.

### 3.3. Paralelismo em GPU

A GTX 1650 oferece um acelerador com 14 SMs e 4 GiB de memória dedicada. A inicialização bem-sucedida da CUDA Driver API e a consulta de seus atributos confirmam a presença do dispositivo acessível pelo driver. SMs são unidades da GPU e não correspondem a núcleos físicos ou threads da CPU.

Esse recurso permite considerar processamento paralelo de muitos elementos, como operações matriciais e processamento de imagens. Sua utilização depende de um ambiente de programação compatível, do tamanho do problema e do custo de movimentação dos dados entre CPU e GPU. A versão CUDA 13.0 exibida pelo driver não comprova instalação do CUDA Toolkit, que não foi identificado. O uso computacional da Intel UHD não foi validado.

## 4. Estratégias adequadas aos recursos da máquina

A estratégia mais diretamente disponível é o **OpenMP em memória compartilhada**, pois há múltiplos núcleos e suporte funcional já confirmado. Laços com iterações independentes podem ser distribuídos entre threads, enquanto tarefas independentes podem ser executadas concorrentemente. É necessário respeitar dependências de dados e controlar acessos de escrita compartilhados, evitando condições de corrida e sincronização excessiva.

Para cálculos numéricos regulares, é adequado combinar **OpenMP e vetorização SIMD**: distribuir blocos de dados entre os núcleos e explorar operações vetoriais dentro de cada bloco. Acessos contíguos e reutilização de dados favorecem essa organização. Em operações matriciais, o processamento em blocos também pode melhorar o aproveitamento dos caches L1, L2 e L3. Escritas concorrentes em posições próximas devem ser organizadas com cuidado para reduzir possíveis disputas por linhas de cache.

O número de threads deve ser escolhido conforme a aplicação. Configurações com quatro e oito threads são candidatas a uma avaliação posterior, pois permitem comparar o uso dos núcleos físicos com o aproveitamento adicional do SMT. A fração sequencial do programa, o custo de criar e coordenar threads e a demanda por memória limitam os ganhos; oito threads não constituem automaticamente a melhor configuração.

A **aceleração por GPU** é uma alternativa para trabalhos com grande quantidade de operações semelhantes. Deve-se considerar a capacidade de 4 GiB de VRAM e buscar reutilizar dados na GPU para reduzir transferências. Para tarefas pequenas ou com muitas dependências, o custo de preparação e comunicação pode reduzir a vantagem. Um modelo híbrido CPU–GPU pode atribuir à CPU o controle e as etapas menos regulares, deixando à GPU os trechos com maior paralelismo de dados. Essa alternativa exige preparação e validação adicionais do ambiente de desenvolvimento.

O **MPI** não foi identificado e não é necessário para explorar os recursos locais já disponíveis via OpenMP. Embora possa ser utilizado entre processos de uma única máquina, sua adoção faria mais sentido nesta atividade como preparação para programas distribuídos ou para execução em vários computadores. A coleta não identificou nem avaliou um cluster.

## 5. Avaliação final

O computador apresenta possibilidades de paralelismo em diferentes níveis: quatro núcleos físicos para execução concorrente, SMT com oito processadores lógicos, instruções SIMD e uma GPU NVIDIA acessível pelo driver. Para o ambiente atualmente validado, a prioridade é explorar **OpenMP associado à vetorização e à boa localidade de memória**. O uso da GPU constitui uma alternativa para cargas com paralelismo de dados suficiente para justificar a preparação do ambiente e a movimentação de dados.

Essas escolhas são fundamentadas na arquitetura observada, e não em desempenho medido. Permaneceram sem determinação as latências e a largura de banda real da memória, os canais de RAM ativos, detalhes do pipeline e o número ideal de threads. O sistema também reportou a presença de um hipervisor; por isso, os resultados de CPUID devem ser entendidos como os recursos expostos ao processo consultado. As ausências de MPI e CUDA Toolkit se limitam aos caminhos pesquisados. Benchmarks específicos seriam necessários para quantificar o ganho de cada estratégia em uma aplicação concreta.
