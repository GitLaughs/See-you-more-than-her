param(
    [string]$ContainerName = "A1_Builder_local",
    [switch]$FullBuild,
    [switch]$SkipExport
)

$ErrorActionPreference = "Stop"

function Invoke-ContainerCommand {
    param([string]$Command)

    & docker exec -w /app/src $ContainerName /bin/bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "Docker 命令执行失败: $Command"
    }
}

function Copy-ContainerItem {
    param(
        [string]$ContainerPath,
        [string]$LocalPath
    )

    & docker cp "${ContainerName}:$ContainerPath" $LocalPath
    if ($LASTEXITCODE -ne 0) {
        throw "Export failed: $ContainerPath -> $LocalPath"
    }
}

function Test-ContainerRunning {
    param([string]$Name)

    $state = & docker inspect $Name --format "{{.State.Running}}" 2>$null
    if ($LASTEXITCODE -ne 0) {
        return $false
    }

    return ([string]$state).Trim() -eq "true"
}

Set-Location (Resolve-Path (Join-Path $PSScriptRoot "..\.."))

if (-not (Test-ContainerRunning -Name $ContainerName)) {
    throw "Docker container is not running: $ContainerName"
}

$buildCommand = "/app/scripts/build_complete_evb.sh"
if (-not $FullBuild) {
    $buildCommand += " --app-only"
}

Write-Host "[EVB] Running in container ${ContainerName}: $buildCommand"

$buildFailed = $false
try {
    Invoke-ContainerCommand -Command $buildCommand
} catch {
    $buildFailed = $true
    Write-Host "[EVB] Build failed, exporting any available artifacts."
    Write-Host $_.Exception.Message
}

$exportRoot = Join-Path $PWD "output\evb"
New-Item -ItemType Directory -Force -Path $exportRoot | Out-Null

if (-not $SkipExport) {
    $latestPath = & docker exec $ContainerName /bin/bash -lc 'readlink -f /app/output/evb/latest' 2>$null
    $latestPath = ([string]$latestPath).Trim()

    if (-not [string]::IsNullOrWhiteSpace($latestPath)) {
        $artifactName = Split-Path $latestPath -Leaf
        $localArtifactDir = Join-Path $exportRoot $artifactName
        if (Test-Path $localArtifactDir) {
            Remove-Item -Recurse -Force $localArtifactDir
        }

        Copy-ContainerItem -ContainerPath $latestPath -LocalPath $localArtifactDir
        Set-Content -Path (Join-Path $exportRoot "latest.txt") -Value $localArtifactDir -Encoding UTF8

        Write-Host "[EVB] Exported artifacts to: $localArtifactDir"
    } else {
        Write-Host "[EVB] No exportable latest artifact directory was found."
    }
}

if ($buildFailed) {
    throw "Docker build did not complete successfully. Check container logs."
}

Write-Host "[EVB] Done"