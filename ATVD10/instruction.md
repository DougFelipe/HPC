# Tarefa 10 — HPC — Diagnóstico da arquitetura do computador

Estou realizando a Tarefa 10 da disciplina de Computação de Alto Desempenho.

Nesta etapa NÃO quero escrever o relatório final e NÃO quero executar benchmarks de desempenho. Quero apenas construir um diagnóstico técnico e reproduzível da arquitetura do computador em que este projeto está sendo executado.

O objetivo da tarefa é posteriormente responder:

> Avalie a arquitetura do seu computador pessoal (ou de um computador do laboratório) e identifique as possibilidades de paralelismo disponíveis nesse ambiente. Com base nessa análise, discuta quais estratégias de paralelismo são mais adequadas para aproveitar os recursos da máquina.

O conteúdo da disciplina aborda principalmente:

- paralelismo em nível de instrução;
- pipeline e execução superscalar;
- SIMD/vetorização;
- paralelismo em nível de thread;
- multithreading/SMT;
- multicore;
- multiprocessadores;
- memória compartilhada;
- classificação de Flynn;
- SISD, SIMD e MIMD;
- UMA e NUMA;
- hierarquia de memória e caches;
- aceleradores;
- GPU;
- paralelismo em GPU;
- OpenMP;
- MPI;
- modelos híbridos de programação.

## Objetivo

Crie dentro do diretório atual um pequeno conjunto de scripts que obtenha automaticamente o máximo possível de informações REAIS sobre a arquitetura desta máquina.

Não presuma qual é o processador, GPU, quantidade de memória, número de núcleos ou qualquer outra característica.

Tudo deve ser descoberto consultando o sistema.

## Ambiente

Estou executando o projeto pelo VS Code em Windows.

Posso ter disponíveis:

- PowerShell;
- Windows 10 ou Windows 11;
- MSYS2/UCRT64;
- WSL;
- GCC.

A coleta principal deve funcionar diretamente no Windows.

Quando alguma informação não puder ser obtida de maneira confiável, registre explicitamente:

`não identificado`

em vez de inferir ou inventar.

---

# 1. Estrutura a criar

Crie:

    collect_architecture.ps1
    collect_architecture.sh
    README.md
    results/

O script PowerShell deve ser a implementação principal.

O script `.sh` deve servir como coleta complementar caso seja executado em MSYS2/WSL/Linux.

Todos os resultados devem ser armazenados em `results/`.

Não altere outros arquivos existentes no projeto.

---

# 2. Informações gerais do sistema

Colete:

- sistema operacional;
- versão;
- arquitetura do sistema operacional;
- arquitetura da CPU;
- fabricante/modelo da máquina, quando disponível;
- hostname apenas se for útil para identificar a execução;
- quantidade total de RAM física.

Evite coletar informações pessoais desnecessárias.

---

# 3. CPU

Obtenha, quando disponíveis:

- fabricante;
- modelo exato;
- arquitetura;
- número de sockets;
- número de núcleos físicos;
- número de processadores lógicos;
- threads por núcleo;
- presença de SMT/Hyper-Threading ou tecnologia equivalente;
- frequência base;
- frequência máxima/turbo, caso o sistema exponha essa informação.

Diferencie explicitamente:

- socket;
- núcleo físico;
- processador lógico/thread de hardware.

Não confunda processadores lógicos com núcleos físicos.

---

# 4. Topologia da CPU

Investigue a topologia disponível:

- sockets;
- cores;
- threads;
- grupos de processadores do Windows, se aplicável;
- informações NUMA, se disponíveis;
- número de nós NUMA;
- relação entre processadores e nós NUMA, se o sistema fornecer essa informação.

Não classifique automaticamente a máquina como UMA ou NUMA apenas pela quantidade de núcleos.

Registre somente aquilo que puder ser sustentado pelas informações obtidas.

---

# 5. Hierarquia de cache

Colete o máximo possível sobre:

- cache L1;
- cache L2;
- cache L3;
- tamanho de cada nível;
- quantidade de caches, quando disponível;
- se determinado nível parece ser privado por núcleo ou compartilhado entre núcleos, SOMENTE se isso puder ser determinado confiavelmente.

Dê preferência a ferramentas do próprio sistema.

Se informações mais detalhadas dependerem de uma ferramenta não instalada, não instale automaticamente. Apenas registre qual ferramenta poderia complementar a análise.

---

# 6. Conjunto de instruções e SIMD

Investigue quais extensões de instrução podem ser identificadas, por exemplo:

- SSE;
- SSE2;
- SSE3;
- SSSE3;
- SSE4.1;
- SSE4.2;
- AVX;
- AVX2;
- AVX-512;
- FMA;
- outras extensões relevantes encontradas.

É importante distinguir:

1. suporte do hardware;
2. suporte exposto pelo sistema operacional;
3. suporte do compilador.

Não declare que uma extensão está disponível apenas porque o compilador conhece uma flag correspondente.

Se GCC estiver disponível, registre também:

    gcc --version
    gcc -march=native -Q --help=target

ou alternativa equivalente compatível com a instalação encontrada.

Salve a saída completa para análise posterior.

---

# 7. GPU / aceleradores

Detecte todas as GPUs disponíveis.

Para cada GPU, tente obter:

- fabricante;
- modelo;
- memória dedicada/VRAM, quando disponível;
- tipo de adaptador;
- driver;
- outras informações úteis para avaliar seu uso como acelerador.

Se houver GPU NVIDIA e `nvidia-smi` estiver disponível, execute:

    nvidia-smi

e salve a saída completa.

Tente também obter, quando possível:

- versão do driver;
- versão CUDA reportada pelo driver;
- memória total;
- compute capability, SOMENTE se puder ser obtida de maneira confiável;
- quantidade de Streaming Multiprocessors (SMs), SOMENTE se alguma ferramenta instalada fornecer essa informação.

Não confunda:

- CUDA Cores;
- Streaming Multiprocessors (SM);
- threads de CPU.

Se houver GPU integrada e dedicada, registre ambas separadamente.

Se CUDA Toolkit não estiver instalado, não instale automaticamente.

---

# 8. Memória principal

Colete:

- RAM física total;
- quantidade de módulos/bancos quando disponível;
- capacidade dos módulos;
- velocidade/frequência reportada;
- fabricante, quando disponível.

Não tente inferir largura de banda real apenas pela frequência da memória.

---

# 9. OpenMP

Verifique:

- compilador C disponível;
- versão do GCC/Clang/MSVC encontrada;
- se OpenMP parece estar disponível.

Caso GCC esteja disponível, crie temporariamente um programa C mínimo para verificar OpenMP.

O programa deve imprimir:

- `omp_get_num_procs()`;
- `omp_get_max_threads()`;
- número efetivo de threads dentro de uma região paralela.

Compile com:

    gcc -O2 -fopenmp

ou equivalente adequado ao ambiente.

Isso NÃO deve ser tratado como benchmark.

É apenas uma verificação funcional da capacidade de paralelismo por threads.

Salve código, comando de compilação e saída.

---

# 10. MPI

Verifique apenas se alguma implementação MPI já está instalada.

Procure, quando aplicável:

- `mpicc`;
- `mpiexec`;
- `mpirun`.

Registre versão e implementação encontrada.

Não instale MPI automaticamente.

A ausência de MPI deve ser registrada simplesmente como:

`MPI não identificado/instalado no ambiente atual`

Não interprete a presença de MPI como evidência de que esta máquina seja um cluster.

---

# 11. Ferramentas auxiliares

Utilize apenas ferramentas já disponíveis.

No Windows, considere quando apropriado:

- PowerShell;
- Get-CimInstance;
- Get-ComputerInfo;
- Get-Counter;
- systeminfo;
- Windows APIs ou comandos equivalentes.

No MSYS2/WSL/Linux, quando disponíveis:

- lscpu;
- free;
- /proc/cpuinfo;
- numactl --hardware;
- gcc;
- nvidia-smi.

Não instale software sem autorização.

---

# 12. Evidências brutas

Além do relatório resumido, salve as saídas brutas importantes em arquivos separados, por exemplo:

    results/
        architecture_summary.txt
        system_info.txt
        cpu_info.txt
        cpu_topology.txt
        cache_info.txt
        simd_info.txt
        memory_info.txt
        gpu_info.txt
        nvidia_smi.txt
        compiler_info.txt
        gcc_native_flags.txt
        openmp_test.txt
        mpi_info.txt

Arquivos que não se aplicarem podem ser omitidos.

---

# 13. architecture_summary.txt

Ao final, gere automaticamente:

    results/architecture_summary.txt

com estrutura aproximadamente:

============================================================
TAREFA 10 — DIAGNÓSTICO DA ARQUITETURA
============================================================

SISTEMA
------------------------------------------------------------
Sistema operacional :
Arquitetura          :
Memória RAM          :

CPU
------------------------------------------------------------
Modelo               :
Arquitetura          :
Sockets              :
Núcleos físicos      :
Threads lógicas      :
Threads por núcleo   :
SMT                  :

CACHE
------------------------------------------------------------
L1                   :
L2                   :
L3                   :

SIMD / ISA
------------------------------------------------------------
SSE                   :
AVX                   :
AVX2                  :
AVX-512               :
FMA                   :
Outras                :

TOPOLOGIA DE MEMÓRIA
------------------------------------------------------------
Nós NUMA             :
Informações adicionais:

GPU / ACELERADORES
------------------------------------------------------------
GPU 0                :
GPU 1                :
VRAM                 :
CUDA disponível      :

SOFTWARE PARA PARALELISMO
------------------------------------------------------------
Compilador C         :
OpenMP               :
MPI                  :
CUDA Toolkit         :

============================================================

Adapte os campos conforme as informações realmente encontradas.

---

# 14. Relatório técnico auxiliar

Crie também:

    results/architecture_analysis.md

Este arquivo deve ORGANIZAR os dados coletados, mas ainda não deve produzir conclusões definitivas para a tarefa.

Divida em:

1. Sistema analisado
2. CPU e topologia
3. Núcleos e threads
4. Hierarquia de cache
5. Memória principal
6. Recursos SIMD
7. GPU/aceleradores
8. OpenMP
9. MPI
10. Recursos de paralelismo identificados
11. Informações que não puderam ser determinadas

Na seção "Recursos de paralelismo identificados", apenas relacione as evidências observadas a possíveis categorias:

- paralelismo em nível de instrução;
- SIMD;
- paralelismo em nível de threads;
- multicore;
- memória compartilhada;
- MIMD;
- GPU/aceleradores.

Não force uma classificação quando as evidências forem insuficientes.

---

# 15. Requisitos de confiabilidade

Muito importante:

- NÃO invente informações.
- NÃO use especificações presumidas a partir do nome comercial sem deixar isso explicitamente identificado.
- Priorize dados obtidos diretamente da máquina.
- Diferencie "detectado" de "inferido".
- Não confunda núcleos físicos com threads lógicas.
- Não confunda SIMD de CPU com paralelismo de GPU.
- Não classifique automaticamente multicore como NUMA.
- Não trate uma GPU apenas gráfica como necessariamente adequada para CUDA/OpenCL.
- Não trate `nvidia-smi` como evidência de que o CUDA Toolkit esteja instalado.
- Não execute benchmarks pesados.
- Não altere configurações do sistema.
- Não instale dependências.
- Não use acesso à Internet para preencher dados ausentes.

---

# 16. Execução

Depois de criar os scripts:

1. revise-os;
2. execute `collect_architecture.ps1`;
3. corrija somente problemas necessários para completar a coleta;
4. se estiver disponível um shell MSYS2/WSL/Linux, execute também a coleta complementar;
5. execute o pequeno teste OpenMP, se houver compilador compatível;
6. confirme que os arquivos foram gerados em `results/`;
7. mostre no terminal o conteúdo final de:

    results/architecture_summary.txt

8. informe quais dados não puderam ser obtidos automaticamente.

Não escreva ainda o relatório final da Tarefa 10.
Não faça análise do supercomputador NPAD.
Não execute benchmarks de desempenho.
O objetivo desta etapa é somente obter uma caracterização confiável e reproduzível da máquina local.