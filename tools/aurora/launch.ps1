param(
    [int]$Device = -1,
    [int]$Port = 5801
)

$ErrorActionPreference = "Stop"

function Initialize-Utf8Console {
    try { & chcp.com 65001 | Out-Null } catch {}
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [Console]::InputEncoding = $utf8NoBom
    [Console]::OutputEncoding = $utf8NoBom
    $global:OutputEncoding = $utf8NoBom
    $env:PYTHONUTF8 = "1"
    $env:PYTHONIOENCODING = "utf-8"
}

function Get-PythonExecutable {
    $candidates = @(
        (Resolve-Path "..\..\\.venv39\Scripts\python.exe" -ErrorAction SilentlyContinue),
        (Resolve-Path "..\..\.venv39\Scripts\python.exe" -ErrorAction SilentlyContinue),
        "python"
    ) | Where-Object { $_ }
    foreach ($candidate in $candidates) {
        try {
            $version = & $candidate --version 2>&1
            if ($version -match "Python 3") { return $candidate }
        } catch {}
    }
    return "python"
}

function Start-BrowserWhenReady {
    param([int]$ReadyPort, [string]$Url)
    try {
        $browserWorker = @'
$deadline = [DateTime]::UtcNow.AddSeconds(180)
$baseUrl = "__URL__"
while ([DateTime]::UtcNow -lt $deadline) {
    try {
        $client = New-Object System.Net.Sockets.TcpClient
        $async = $client.BeginConnect("127.0.0.1", __PORT__, $null, $null)
        if ($async.AsyncWaitHandle.WaitOne(200)) {
            $client.EndConnect($async)
            $client.Close()
            $response = Invoke-WebRequest -Uri $baseUrl -UseBasicParsing -TimeoutSec 1 -ErrorAction SilentlyContinue
            if ($response.StatusCode -ge 200) { Start-Process $baseUrl | Out-Null; exit 0 }
        } else { $client.Close() }
    } catch {}
    Start-Sleep -Milliseconds 500
}
'@
        $browserWorker = $browserWorker.Replace("__PORT__", [string]$ReadyPort).Replace("__URL__", $Url)
        $encodedCommand = [Convert]::ToBase64String([System.Text.Encoding]::Unicode.GetBytes($browserWorker))
        $shell = Get-Command pwsh -ErrorAction SilentlyContinue
        if (-not $shell) { $shell = Get-Command powershell -ErrorAction SilentlyContinue }
        if ($shell) {
            Start-Process -FilePath $shell.Source -WindowStyle Hidden -ArgumentList @("-NoProfile", "-EncodedCommand", $encodedCommand) | Out-Null
            return
        }
    } catch {}
}

Initialize-Utf8Console
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$Python = Get-PythonExecutable

$browserUrl = "http://127.0.0.1:$Port"

Start-BrowserWhenReady -ReadyPort $Port -Url $browserUrl
& $Python aurora_companion.py --device $Device --port $Port
