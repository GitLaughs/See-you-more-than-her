param(
    [int]$Port = 6202,
    [string]$ListenHost = "127.0.0.1"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

try { & chcp.com 65001 | Out-Null } catch {}
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[Console]::InputEncoding = $utf8NoBom
[Console]::OutputEncoding = $utf8NoBom
$global:OutputEncoding = $utf8NoBom
$env:PYTHONUTF8 = "1"
$env:PYTHONIOENCODING = "utf-8"
$env:NO_PROXY = "127.0.0.1,localhost,::1"
$env:no_proxy = $env:NO_PROXY

function Get-RepoVenvRoot {
    return [System.IO.Path]::GetFullPath((Join-Path $ScriptDir "..\..\venv_39"))
}

function Get-PythonExecutable {
    $venvPython = Join-Path (Get-RepoVenvRoot) "Scripts\python.exe"
    foreach ($candidate in @($venvPython, "python")) {
        try {
            $version = & $candidate --version 2>&1
            if ($version -match "Python 3") { return $candidate }
        } catch {}
    }
    return "python"
}

$Python = Get-PythonExecutable
Write-Host "[PC] Web port: $Port"
Write-Host "[PC] Direct STM32 serial module ready"
Write-Host "[PC] ROS module ready"
Start-Process "http://127.0.0.1:$Port" | Out-Null
& $Python pc_tool.py --port $Port --host $ListenHost
