# Tarefa 10 — diagnóstico da máquina local

Objetivo da atividade: avaliar a arquitetura do computador, identificar as possibilidades de paralelismo e discutir estratégias adequadas aos recursos encontrados.

Esta etapa coleta evidências e produz uma análise auxiliar, sem benchmarks e sem escrever o relatório final. Todos os arquivos ficam em `ATVD10`. `instruction.md` é preservado.

## Executar no Windows

No PowerShell, na raiz do projeto:

```powershell
& .\ATVD10\collect_architecture.ps1
```

Ou, estando em `ATVD10`:

```powershell
& .\collect_architecture.ps1
```

Requer PowerShell no Windows; prefira processo de 64 bits. O coletor usa CIM e a API `GetLogicalProcessorInformationEx`, por meio do C# em `probes/topology.cs`. Não requer privilégios administrativos. Não altera configurações, não instala pacotes e não acessa a Internet. Uma política local que impeça scripts deve ser respeitada e registrada, sem mudanças automáticas de política.

Procura GCC no `PATH` e nos diretórios usuais `C:\msys64\ucrt64\bin` e `C:\msys64\mingw64\bin`. Acrescenta o diretório do compilador ao `PATH` apenas durante os testes, restaurando-o em seguida. Procura outros compiladores, MPI, NVIDIA-SMI e NVCC nos locais documentados pelo próprio script. Falha nessa busca não prova ausência de software em todos os diretórios do computador.

Os programas em `probes/` são verificações pequenas: enumeração CPUID/estado SIMD, consulta de atributos da CUDA Driver API já instalada e uma região paralela OpenMP. A sonda CUDA pode inicializar o driver/dispositivo; não executa kernels nem benchmarks. Não exige CUDA Toolkit, pois consulta a DLL do driver dinamicamente.

## Coleta complementar

Em MSYS2, WSL ou Linux:

```bash
bash ./collect_architecture.sh
```

No PowerShell, se MSYS2 estiver no caminho usual e o diretório atual for `ATVD10`:

```powershell
& 'C:\msys64\usr\bin\bash.exe' './collect_architecture.sh'
```

O script salva os dados em subdiretórios diferentes conforme o ambiente. A visão de WSL pode refletir limites da máquina virtual; não deve substituir automaticamente a topologia nativa. MSYS2 não é Linux, e ferramentas ausentes são registradas.

## Arquivos produzidos

- `results/architecture_summary.txt`: resumo gerado pelo PowerShell e impresso no terminal.
- `results/architecture_analysis.md`: organização automática dos dados, com referências às evidências.
- `results/apoio_a_redacao.md`: interpretação revisada da coleta de 29/09/2026 e estratégias candidatas para discutir o enunciado; este arquivo não é atualizado automaticamente pelo coletor.
- `results/windows/`: JSON de CIM/API, saídas de comandos, compilação, testes, metadados e eventuais falhas.
- `results/msys2/`, `results/wsl/` ou `results/linux/`: coleta complementar, quando executada.
- `probes/`: fontes preservadas das sondas usadas; executáveis são gerados no diretório de resultados do ambiente.

Nova execução atualiza os arquivos de mesmo nome. Para preservar execuções anteriores, copie `results/` antes de repetir. Verifique data, código de saída e metadados antes de usar evidências; arquivos opcionais de uma execução anterior podem permanecer se a ferramenta deixar de estar disponível.

As consultas selecionam campos técnicos, evitando números de série, usuários e hostname. Saídas brutas de ferramentas podem conter caminhos locais ou processos; revise-as antes de publicar.

## Como usar na redação posterior

Para cada estratégia, relacionar evidência, possibilidade de paralelismo, tipo de trabalho adequado e limitação. Distinguir detecção de inferência; registrar o que não foi identificado. Não declarar speedup, largura de banda medida ou número ideal de threads, pois não houve benchmark. O teste OpenMP demonstra funcionamento do compilador/runtime, não desempenho.

O resumo distingue CPU física/processadores lógicos, capacidades de SIMD/estado do SO/opções do compilador e driver CUDA/Toolkit. Uma GPU detectada não comprova por si só que todas as ferramentas de programação estejam instaladas. Um nó NUMA exposto não mede uniformidade de latência. Pipeline e largura superscalar ficam como não identificados quando não houver evidência direta.
