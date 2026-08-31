$ErrorActionPreference = "Stop"

$logRoot = Join-Path $env:ProgramData "KittyCore"
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logFile = Join-Path $logRoot "optimize-$stamp.log"
Start-Transcript -Path $logFile -Force | Out-Null

Write-Host "KittyCore optimizer engine" -ForegroundColor Magenta
Write-Host "This first build runs in preview mode." -ForegroundColor Yellow
Write-Host "No destructive tweaks are executed yet." -ForegroundColor Yellow
Write-Host ""
Write-Host "Next engine step: split Init.bat into reversible PowerShell modules." -ForegroundColor Cyan
Write-Host "Log: $logFile"
Write-Host ""
Read-Host "Press Enter to close"

Stop-Transcript | Out-Null
