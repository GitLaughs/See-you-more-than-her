param(
    [string]$HostName = "127.0.0.1",
    [int]$Port = 6210
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
Set-Location $RepoRoot
python tools/video/video_label_tool.py --host $HostName --port $Port
