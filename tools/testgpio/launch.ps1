param(
    [int]$Port = 5820,
    [string]$ListenHost = "127.0.0.1"
)

$ErrorActionPreference = "Stop"
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir) {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}

function Get-PythonExecutable {
    param(
        [string]$ScriptDir
    )

    $candidates = @(
        (Join-Path $ScriptDir "..\..\venv_39\Scripts\python.exe"),
        (Join-Path $ScriptDir "..\..\.venv39\Scripts\python.exe"),
        "python"
    )
    foreach ($candidate in $candidates) {
        if ($candidate -eq "python") { return $candidate }
        if (Test-Path $candidate) { return $candidate }
    }
    return "python"
}

$RepoRoot = Split-Path -Parent $ScriptDir
Set-Location $RepoRoot
$Python = Get-PythonExecutable -ScriptDir $ScriptDir
$Url = "http://${ListenHost}:$Port"
Start-Process $Url | Out-Null
& $Python -m testgpio.server
