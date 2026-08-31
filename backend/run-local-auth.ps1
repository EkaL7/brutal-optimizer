$ErrorActionPreference = "Stop"

if (-not $env:DISCORD_BOT_TOKEN) {
    Write-Host "DISCORD_BOT_TOKEN is missing." -ForegroundColor Red
    Write-Host "Set it first, then run this script again." -ForegroundColor Yellow
    exit 1
}

if (-not $env:USAGE_WEBHOOK_URL -and -not $env:DISCORD_WEBHOOK_URL) {
    Write-Host "USAGE_WEBHOOK_URL is missing. Login will work, but webhook logs will not be sent." -ForegroundColor Yellow
    Write-Host "Set USAGE_WEBHOOK_URL with your Discord webhook before starting this server." -ForegroundColor Yellow
}

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$env:HOST = "127.0.0.1"
$env:PORT = "8787"

node .\backend\local-auth-server.js
