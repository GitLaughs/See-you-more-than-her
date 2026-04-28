param(
    [int]$Port = 5820,
    [string]$Host = "127.0.0.1"
)

$ErrorActionPreference = "Stop"

function Get-PythonExecutable {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $candidates = @(
        (Join-Path $scriptDir "..\..\venv_39\Scripts\python.exe"),
        (Join-Path $scriptDir "..\..\.venv39\Scripts\python.exe"),
        "python"
    )
    foreach ($candidate in $candidates) {
        if ($candidate -eq "python") { return $candidate }
        if (Test-Path $candidate) { return $candidate }
    }
    return "python"
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir
$Python = Get-PythonExecutable
$Url = "http://${Host}:$Port"
Start-Process $Url | Out-Null
& $Python server.py
