$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
    throw "Node.js nao encontrado."
}

if (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
    throw "npm nao encontrado."
}

if (-not (Get-Command discloud -ErrorAction SilentlyContinue)) {
    Write-Host "Instalando Discloud CLI..." -ForegroundColor Cyan
    npm install -g discloud-cli
}

Write-Host "Validando backend..." -ForegroundColor Cyan
npm run check

$config = Get-Content -LiteralPath ".\discloud.config" -Raw
if ($config -match "ID=kittycore-backend") {
    Write-Host "Confira o ID no discloud.config antes do deploy final." -ForegroundColor Yellow
}

Write-Host "Testando login Discloud..." -ForegroundColor Cyan
$loginOk = $true
try {
    discloud user info | Out-Host
} catch {
    $loginOk = $false
}

if (-not $loginOk) {
    Write-Host "Faca login primeiro:" -ForegroundColor Yellow
    Write-Host "discloud login" -ForegroundColor White
    exit 1
}

Write-Host "Enviando backend para Discloud..." -ForegroundColor Cyan
discloud app upload .

Write-Host "Deploy enviado. Use a URL publica do app em config\kittycore.json auth_endpoint." -ForegroundColor Green
