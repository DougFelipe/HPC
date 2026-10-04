[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$results = Join-Path $root 'results'
$raw = Join-Path $results 'windows'
New-Item -ItemType Directory -Force -Path $raw | Out-Null
$unknown = 'não identificado'
$issues = [System.Collections.Generic.List[string]]::new()
function Save-Json($name, $value) { ConvertTo-Json -InputObject $value -Depth 14 | Set-Content -LiteralPath (Join-Path $raw $name) -Encoding UTF8 }
function Save-Text($name, $value) { $value | Out-File -LiteralPath (Join-Path $raw $name) -Encoding UTF8 -Width 240 }
function Query($class, $properties, $file) {
    try { $data = @(Get-CimInstance -ClassName $class | Select-Object -Property $properties); Save-Json $file $data; return $data }
    catch { $issues.Add("$class : $($_.Exception.Message)"); Save-Text $file $unknown; return @() }
}
function Find-Tool($name, $candidates) {
    $command = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    foreach ($candidate in $candidates) { if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate } }
    return $null
}
function Run-Tool($exe, $arguments, $file) {
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $exe @arguments 2>&1
        $code = $LASTEXITCODE
        Save-Text $file (@("Working directory: $(Get-Location)", "Executable: $exe", "Arguments (JSON array): $(ConvertTo-Json -InputObject @($arguments) -Compress)", "Exit code: $code", '') + @($output | ForEach-Object { "$_" }))
        if ($code -ne 0) { $issues.Add("$file : exit code $code") }
        return [pscustomobject]@{ ExitCode=$code; Output=($output -join "`n") }
    } finally { $ErrorActionPreference = $oldPreference }
}
$os = @(Query 'Win32_OperatingSystem' @('Caption','Version','BuildNumber','OSArchitecture','TotalVisibleMemorySize') 'system_info.json')
$system = @(Query 'Win32_ComputerSystem' @('Manufacturer','Model','TotalPhysicalMemory','NumberOfProcessors','NumberOfLogicalProcessors','HypervisorPresent') 'machine_info.json')
$cpu = @(Query 'Win32_Processor' @('Name','Manufacturer','Architecture','SocketDesignation','NumberOfCores','NumberOfEnabledCore','NumberOfLogicalProcessors','MaxClockSpeed','CurrentClockSpeed','L2CacheSize','L3CacheSize') 'cpu_info.json')
$memory = @(Query 'Win32_PhysicalMemory' @('BankLabel','DeviceLocator','Capacity','Speed','ConfiguredClockSpeed','Manufacturer','SMBIOSMemoryType','DataWidth','TotalWidth') 'memory_info.json')
$gpu = @(Query 'Win32_VideoController' @('Name','AdapterCompatibility','AdapterRAM','VideoProcessor','DriverVersion','DriverDate','Status','PNPDeviceID') 'gpu_info.json')
$cacheCim = @(Query 'Win32_CacheMemory' @('Name','Level','InstalledSize','MaxCacheSize','BlockSize','CacheType') 'cache_cim.json')
$topology = $null
try {
    if (-not ('ArchitectureTopology' -as [type])) { Add-Type -Path (Join-Path $root 'probes\topology.cs') }
    $topology = [ArchitectureTopology]::Read() | ConvertTo-Json -Depth 14 | ConvertFrom-Json
    Save-Json 'cpu_topology.json' $topology
} catch { $issues.Add("Windows topology API: $($_.Exception.Message)"); Save-Text 'cpu_topology.json' $unknown }

$gcc = Find-Tool 'gcc' @('C:\msys64\ucrt64\bin\gcc.exe','C:\msys64\mingw64\bin\gcc.exe')
$clang = Find-Tool 'clang' @('C:\Program Files\LLVM\bin\clang.exe')
$msvc = Find-Tool 'cl' @()
$nvccCandidates = @(Get-ChildItem -Path 'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v*\bin\nvcc.exe' -ErrorAction SilentlyContinue | ForEach-Object FullName)
$nvcc = Find-Tool 'nvcc' $nvccCandidates
$smi = Find-Tool 'nvidia-smi' @('C:\Windows\System32\nvidia-smi.exe')
$mpi = [ordered]@{}
foreach ($name in @('mpicc','mpiexec','mpirun')) {
    $candidates = @(); if ($name -eq 'mpiexec') { $candidates = @('C:\Program Files\Microsoft MPI\Bin\mpiexec.exe') }
    $tool = Find-Tool $name $candidates
    $mpi[$name] = $tool
    if ($tool) {
        $versionArguments = @('--version')
        if ($tool -like '*Microsoft MPI*') { $versionArguments = @('-help') }
        $null = Run-Tool $tool $versionArguments "${name}_version.txt"
        Save-Json "${name}_file_version.json" (Get-Item -LiteralPath $tool).VersionInfo
    }
}
Save-Json 'mpi_info.json' $mpi
$mpiStatus = if (@($mpi.Values | Where-Object { $_ }).Count) { 'Executavel MPI detectado; consultar mpi_info.json e versoes. Nenhum job executado.' } else { 'MPI não identificado/instalado no ambiente atual' }
$compilerInfo = [ordered]@{gcc=$gcc; clang=$clang; msvc=$msvc; nvcc=$nvcc}
Save-Json 'compiler_info.json' $compilerInfo
foreach ($entry in @(@($clang,'clang_version.txt'), @($msvc,'msvc_version.txt'), @($nvcc,'nvcc_version.txt'))) {
    if ($entry[0]) { $argsForVersion = @('--version'); if ($entry[1] -eq 'msvc_version.txt') { $argsForVersion = @() }; $null = Run-Tool $entry[0] $argsForVersion $entry[1] }
}
$simdText = $unknown; $ompStatus = $unknown; $compilerVersion = $unknown
$initialPath = $env:PATH
Push-Location -LiteralPath $root
try {
    if ($gcc) {
        $env:PATH = (Split-Path -Parent $gcc) + ';' + $env:PATH
        $version = Run-Tool $gcc @('--version') 'gcc_version.txt'
        $compilerVersion = ($version.Output -split "`n")[0]
        $null = Run-Tool $gcc @('-march=native','-Q','--help=target') 'gcc_native_flags.txt'
        foreach ($probe in @('isa_gpu','openmp_test')) {
            $source = "probes\$probe.c"
            $dest = Join-Path $raw "$probe.exe"
            $relativeDest = "results\windows\$probe.exe"
            $compileArgs = @('-O2',$source,'-o',$relativeDest)
            if ($probe -eq 'openmp_test') { $compileArgs = @('-O2','-fopenmp',$source,'-o',$relativeDest) }
            $compilation = Run-Tool $gcc $compileArgs "${probe}_compile.txt"
            if ($compilation.ExitCode -eq 0) {
                $run = Run-Tool $dest @() "$probe.txt"
                if ($probe -eq 'isa_gpu' -and $run.ExitCode -eq 0) { $simdText = $run.Output }
                if ($probe -eq 'openmp_test' -and $run.ExitCode -eq 0) { $ompStatus = $run.Output }
            }
        }
    } else { $issues.Add('GCC nao encontrado: verificacoes CPUID e OpenMP nao executadas por este coletor.') }
} finally { $env:PATH = $initialPath; Pop-Location }
$smiText = $unknown; $smiQuery = $unknown
if ($smi) {
    $smiRun = Run-Tool $smi @() 'nvidia_smi.txt'; $smiText = $smiRun.Output
    $queryRun = Run-Tool $smi @('--query-gpu=name,memory.total,driver_version,compute_cap','--format=csv') 'nvidia_gpu_query.txt'
    if ($queryRun.ExitCode -eq 0) { $smiQuery = $queryRun.Output }
}
$wsl = Find-Tool 'wsl' @()
if ($wsl) { $null = Run-Tool $wsl @('--list','--quiet') 'wsl_distributions.txt' }
$metadata = [ordered]@{
    timestamp=(Get-Date).ToString('o'); environment='Windows nativo'; powershell=$PSVersionTable.PSVersion.ToString();
    process_bits=([IntPtr]::Size*8); omp_environment=@(Get-ChildItem Env: | Where-Object Name -Match '^(OMP_|GOMP_)' | Select-Object Name,Value);
    tool_search='PATH e caminhos usuais explicitos MSYS2, LLVM, CUDA Toolkit e Microsoft MPI; ausencia nao prova inexistencia em outros locais';
    source_hashes=@(Get-ChildItem -LiteralPath (Join-Path $root 'probes') -File | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash)
}
Save-Json 'collection_metadata.json' $metadata

$cores = @($topology.records | Where-Object { $_.kind -eq 'core' })
$nodes = @($topology.records | Where-Object { $_.kind -eq 'numa_node' })
$caches = @($topology.records | Where-Object { $_.kind -eq 'cache' })
$physicalCores = ($cpu | Measure-Object -Property NumberOfCores -Sum).Sum
$logicalCores = ($cpu | Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum
$threadCounts = @($cores | ForEach-Object { $n=0; foreach ($affinity in $_.affinity) { $n += @($affinity.logical_processors).Count }; $n } | Sort-Object -Unique)
$threadsPerCore = if ($threadCounts.Count) { $threadCounts -join ', ' } else { $unknown }
$smt = if ($cores.Count) { if (@($cores | Where-Object { $_.smt_flag }).Count) { 'Detectado em nucleos expostos pela API Windows' } else { 'Nao exposto como ativo pela API Windows' } } else { $unknown }
$cacheLines = @()
foreach ($level in @(1,2,3)) {
    $atLevel = @($caches | Where-Object { $_.level -eq $level })
    if ($atLevel.Count) {
        $total = ($atLevel | Measure-Object size_bytes -Sum).Sum / 1KB
        $instances = ($atLevel | ForEach-Object { "$($_.size_bytes/1KB) KiB (tipo=$($_.cache_type); grupo=$($_.group); LP=$($_.logical_processors -join ','))" }) -join '; '
        $cacheLines += "L$level : $total KiB somados em $($atLevel.Count) caches; $instances"
    } else { $cacheLines += "L$level : $unknown" }
}
$installedRam = ($memory | Measure-Object Capacity -Sum).Sum
$ramText = if ($installedRam) { '{0:N2} GiB instalados; {1} modulos; {2:N2} GiB reportados por Win32_ComputerSystem' -f ($installedRam/1GB),$memory.Count,($system[0].TotalPhysicalMemory/1GB) } else { $unknown }
$nodeText = if ($nodes.Count) { ($nodes | ForEach-Object { "No $($_.node): grupo=$($_.group); LP=$($_.logical_processors -join ',')" }) -join '; ' } else { $unknown }
$gpuText = ($gpu | ForEach-Object { "$($_.Name); driver=$($_.DriverVersion); AdapterRAM bruto=$($_.AdapterRAM) bytes (nao validado como VRAM dedicada)" }) -join "`n"
$cudaToolkit = if ($nvcc) { "nvcc detectado: $nvcc (ver nvcc_version.txt)" } else { $unknown }
$summary = @"
============================================================
TAREFA 10 - DIAGNOSTICO DA ARQUITETURA
============================================================
Coleta: $($metadata.timestamp) | Windows nativo

SISTEMA
SO: $($os.Caption -join '; ') | versao $($os.Version -join '; ') | $($os.OSArchitecture -join '; ')
Maquina: $($system.Manufacturer) $($system.Model)
HypervisorPresent (CIM): $($system.HypervisorPresent); CPUID descreve recursos expostos ao processo atual.
RAM: $ramText

CPU
Modelo: $($cpu.Name -join '; ')
Fabricante: $($cpu.Manufacturer -join '; ')
Arquitetura CIM: $($cpu.Architecture -join ',') (9=x64; 0=x86; 12=ARM64)
Sockets populados (Win32_Processor): $($cpu.Count)
Nucleos fisicos: $physicalCores
Processadores logicos: $logicalCores
Threads por nucleo, segundo afinidade: $threadsPerCore
SMT: $smt
Grupos Windows: $(@($topology.groups).Count)
Frequencias CIM (nao equivalem necessariamente a base/turbo): MaxClockSpeed=$($cpu.MaxClockSpeed -join ',') MHz; CurrentClockSpeed=$($cpu.CurrentClockSpeed -join ',') MHz
Frequencias CPUID: ver bloco abaixo; nao sao medicoes de clock em carga.

CACHE
Tipos da API: 0=unificado; 1=instrucoes; 2=dados. LP=processador logico.
$($cacheLines -join "`n")

SIMD / ISA E SONDA CUDA DRIVER
hardware=CPUID; os_usable=suporte de estado do SO; compilador em gcc_native_flags.txt
$simdText

TOPOLOGIA DE MEMORIA
Nos NUMA expostos: $(if ($nodes.Count) { $nodes.Count } else { $unknown })
$nodeText
Esta enumeracao nao mede uniformidade de latencias de memoria.

GPU / ADAPTADORES
$gpuText
Consulta NVIDIA (VRAM e compute capability):
$smiQuery
CUDA reportada pelo driver: $(if ($smiText -match 'CUDA Version:\s*([\d.]+)') { $Matches[1] } else { $unknown })
CUDA Toolkit: $cudaToolkit

SOFTWARE PARA PARALELISMO
Compilador: $compilerVersion
OpenMP (teste funcional, sem medicao de desempenho):
$ompStatus
MPI: $mpiStatus

LIMITACOES
Pipeline, largura superscalar e IPC: não identificado nesta coleta.
Latencia e largura de banda real de memoria; ganho de desempenho paralelo: nao medidos.
Frequencia turbo observada em carga: nao medida.
AdapterRAM CIM nao valida capacidade de VRAM dedicada ou memoria compartilhada de GPU integrada.
Consulta de ferramentas limitada aos locais registrados em collection_metadata.json.
Falhas de coleta: $($issues.Count); consultar collection_issues.txt.
============================================================
"@
$summary | Set-Content -LiteralPath (Join-Path $results 'architecture_summary.txt') -Encoding UTF8
$analysis = @"
# Diagnostico tecnico auxiliar da Tarefa 10

Coleta: $($metadata.timestamp). Objetivo: organizar evidencias locais para avaliar possibilidades de paralelismo e fundamentar a discussao de estrategias. Este arquivo nao e o relatorio final e nao contem benchmarks.

## 1. Sistema analisado
$($os.Caption) $($os.Version), $($os.OSArchitecture); $($system.Manufacturer) $($system.Model). Ambiente Windows nativo. HypervisorPresent reportado pelo CIM: $($system.HypervisorPresent). Essa propriedade nao identifica sozinha a finalidade do hipervisor; CPUID descreve o que esta exposto ao processo. Evidencias: [SO](windows/system_info.json), [maquina](windows/machine_info.json), [metadados](windows/collection_metadata.json).

## 2. CPU e topologia
$($cpu.Name -join '; '); $($cpu.Count) socket(s) populado(s); $(@($topology.groups).Count) grupo(s) Windows. Nos NUMA expostos: $(if ($nodes.Count) { $nodes.Count } else { $unknown }). $nodeText.

A quantidade de nos expostos descreve a visao do sistema operacional; nao constitui medicao de latencia nem prova da organizacao fisica completa. Evidencias: [CPU](windows/cpu_info.json), [topologia](windows/cpu_topology.json).

## 3. Nucleos e threads
$physicalCores nucleos fisicos e $logicalCores processadores logicos. Threads por nucleo observadas nas mascaras de afinidade: $threadsPerCore. SMT: $smt. Threads de hardware nao devem ser contadas como nucleos fisicos adicionais. Evidencia: [topologia e afinidades](windows/cpu_topology.json).

## 4. Hierarquia de cache
$($cacheLines -join "`n`n")

Os totais acima somam instancias enumeradas, nao indicam capacidade por nucleo. Cache compartilhado deve ser identificado cruzando suas afinidades com as afinidades dos nucleos. Tipo 0=unificado, 1=instrucoes e 2=dados. Evidencia: [API Windows](windows/cpu_topology.json); [CIM complementar](windows/cache_cim.json).

## 5. Memoria principal
$ramText. Capacidades individuais e velocidades reportadas em [memory_info.json](windows/memory_info.json). Valores de Speed/ConfiguredClockSpeed sao preservados conforme o firmware; nao sao medicao de clock ou largura de banda. Canais ativos de memoria: não identificado.

## 6. Recursos SIMD
Suporte de hardware consultado por CPUID; utilizacao pelo SO consultada por APIs Windows e OSXSAVE/XGETBV. Ver [sonda ISA](windows/isa_gpu.txt), [fonte da sonda](../probes/isa_gpu.c) e [opcoes nativas do GCC](windows/gcc_native_flags.txt). AVX512F e consultado especificamente; nao representa uma enumeracao de todas as subextensoes AVX-512. Flags do compilador nao comprovam que um programa foi vetorizado.

## 7. GPU/aceleradores
$gpuText

$smiQuery

Memoria AdapterRAM do CIM e mantida apenas como dado bruto. Adaptadores de exibicao adicionais nao devem ser automaticamente contados como GPUs fisicas. Evidencias: [adaptadores](windows/gpu_info.json), [nvidia-smi](windows/nvidia_smi.txt), [consulta NVIDIA](windows/nvidia_gpu_query.txt), [CUDA Driver API](windows/isa_gpu.txt). A versao CUDA mostrada pelo driver nao comprova instalacao do Toolkit. Toolkit: $cudaToolkit.

## 8. OpenMP
Resultado funcional:

~~~text
$ompStatus
~~~

Evidencias: [fonte](../probes/openmp_test.c), [compilacao](windows/openmp_test_compile.txt), [execucao](windows/openmp_test.txt). Nao houve cronometragem, medicao de speedup ou determinacao do numero ideal de threads.

## 9. MPI
$mpiStatus. Evidencia: [busca de executaveis](windows/mpi_info.json). Nenhum job MPI foi executado. MPI instalado, quando detectado, nao demonstra existencia de cluster.

## 10. Recursos de paralelismo identificados
- Multicore/threads: interpretar contagens da CPU junto ao teste OpenMP; a presenca de varios nucleos sustenta a discussao de execucao concorrente em memoria compartilhada.
- SMT: interpretar o campo de SMT e as afinidades; nao pressupor que threads logicas adicionais oferecam ganho proporcional.
- SIMD: considerar apenas extensoes marcadas hardware=1 e os_usable=1 para o processo consultado; verificar separadamente a geracao de codigo pelo compilador em trabalhos posteriores.
- MIMD: varios nucleos capazes de executar fluxos de instrucao independentes permitem discutir esse modelo; a classificacao depende do nivel de observacao, e pode coexistir com SIMD dentro de cada nucleo.
- GPU: os dados de dispositivo e a inicializacao da CUDA Driver API permitem avaliar a possibilidade de aceleracao; nao comprovam ganho de desempenho nem a existencia de um ambiente de desenvolvimento completo.
- Paralelismo em nivel de instrucao: pipeline, largura superscalar e execucao fora de ordem nao foram determinados diretamente; nao atribuir valores a essas propriedades.

## 11. Informacoes que nao puderam ser determinadas
Largura de pipeline/execucao superscalar, IPC, canais de RAM ativos e detalhes nao expostos pelas ferramentas: não identificado. Desempenho, speedup, eficiencia paralela, latencias e largura de banda: nao medidos. Ferramentas nao encontradas podem existir fora do PATH e dos caminhos pesquisados. Falhas de comandos: [registro](windows/collection_issues.txt). Uma consulta que falhou nao significa ausencia do recurso.
"@
$analysis | Set-Content -LiteralPath (Join-Path $results 'architecture_analysis.md') -Encoding UTF8
Save-Text 'collection_issues.txt' $(if ($issues.Count) { $issues } else { 'Nenhuma falha registrada.' })
Write-Output $summary
