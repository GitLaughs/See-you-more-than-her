$ErrorActionPreference = "Stop"

function Write-Log {
    param([string]$Message, [string]$Level="INFO")
    switch ($Level) {
        "OK" { Write-Host "[OK] $Message" -ForegroundColor Green }
        "WARN" { Write-Host "[WARN] $Message" -ForegroundColor Yellow }
        "ERR" { Write-Host "[ERROR] $Message" -ForegroundColor Red }
        default { Write-Host "[INFO] $Message" -ForegroundColor Cyan }
    }
}

function Check-Port {
    param([int]$Port)
    $connection = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
    if ($connection) {
        Write-Log "Port $Port is already in use by PID $($connection.OwningProcess)." "ERR"
        exit 1
    }
    Write-Log "Port $Port is available." "OK"
}

function Check-Python {
    $python = "python"
    try {
        $versionStr = & $python --version 2>&1
        if ($versionStr -match "Python (\d+)\.(\d+)") {
            $major = [int]$matches[1]
            $minor = [int]$matches[2]
            if ($major -lt 3 -or ($major -eq 3 -and $minor -lt 8)) {
                Write-Log "Python version must be >= 3.8, but found $versionStr." "ERR"
                exit 1
            }
            Write-Log "Found $versionStr." "OK"
        } else {
            Write-Log "Could not parse Python version: $versionStr" "ERR"
            exit 1
        }
    } catch {
        Write-Log "Python is not installed or not in PATH." "ERR"
        exit 1
    }
    return $python
}

function Check-Dependencies {
    param([string]$Python)
    $reqFile = "requirements.txt"
    if (-not (Test-Path $reqFile)) {
        Write-Log "requirements.txt not found." "ERR"
        exit 1
    }
    
    Write-Log "Checking dependencies..." "INFO"
    $missing = $false
    try {
        # Fast check if all are installed
        & $Python -c "import flask, flask_socketio, cv2, numpy" 2>$null
        if ($LASTEXITCODE -ne 0) {
            $missing = $true
        }
    } catch {
        $missing = $true
    }
    
    if ($missing) {
        Write-Log "Missing dependencies, installing from requirements.txt..." "WARN"
        & $Python -m pip install -r $reqFile
        if ($LASTEXITCODE -ne 0) {
            Write-Log "Failed to install dependencies." "ERR"
            exit 1
        }
        Write-Log "Dependencies installed successfully." "OK"
    } else {
        Write-Log "All dependencies are installed." "OK"
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptDir

Write-Log "Starting A1 Camera Preview Tool Environment Check..." "INFO"

Check-Port -Port 8000
$py = Check-Python
Check-Dependencies -Python $py

Write-Log "Starting main.py..." "INFO"
Start-Process "http://localhost:8000" | Out-Null
& $py main.py
if ($LASTEXITCODE -ne 0) {
    Write-Log "main.py exited with code $LASTEXITCODE." "ERR"
    exit 1
}
