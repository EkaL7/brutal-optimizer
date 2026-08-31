# Brutal Optimizer

Projeto de estudo de uma aplicacao desktop em **C++ com Dear ImGui** e backend em **Node.js**, focado em otimizacao de sistema Windows para gaming.

## Sobre o Projeto

Este repositorio e um projeto educacional que demonstra:

- **Interface grafica nativa** com Dear ImGui + DirectX 11 (Win32)
- **Backend de autenticacao** via Discord API (Cloudflare Worker e servidor local Node.js)
- **Coleta de informacoes do sistema** usando WMI, DXGI, e APIs Win32
- **Servidor HTTP embutido** para exibicao de diagnosticos do PC no navegador
- **Script de otimizacao** PowerShell para tuning de Windows (telemetria, servicos, registro)
- **Deploy serverless** com Discloud

## Estrutura

```
src/
  main.cpp          # Aplicacao principal (ImGui + DX11 + HTTP server)
  resource.h        # IDs de recursos
backend/
  cloudflare-worker.js   # Worker para Cloudflare (auth + webhook)
  local-auth-server.js   # Servidor local Node.js (auth + webhook + logs)
  run-local-auth.ps1     # Script para iniciar o servidor local
config/
  kittycore.json         # Configuracao do app (endpoint, guild, invite)
  kittycore.example.json # Exemplo de configuracao
scripts/
  KittyCoreOptimize.ps1  # Engine de otimizacao PowerShell
docs/
  DISCLOUD_DEPLOY.md     # Guia de deploy na Discloud
  DISCORD_BACKEND.md     # Documentacao do backend Discord
  env.txt                # Template de variaveis de ambiente
```

## Tecnologias Utilizadas

- **C++17** (MSVC / Visual Studio 2022)
- **Dear ImGui** (interface grafica)
- **DirectX 11** (renderizacao)
- **WinHTTP** (requisicoes HTTP nativas)
- **WMI** (informacoes de hardware)
- **Node.js 20+** (backend)
- **Cloudflare Workers** (serverless)
- **PowerShell** (scripts de otimizacao)

## Configuracao

1. Clone o repositorio
2. Copie `config/kittycore.example.json` para `config/kittycore.json`
3. Preencha os valores com suas proprias credenciais:
   - `auth_endpoint`: URL do seu backend de autenticacao
   - `discord_invite_url`: Link de convite do seu servidor Discord
   - `guild_id`: ID do seu servidor Discord
4. Para o backend, configure as variaveis de ambiente:
   - `DISCORD_BOT_TOKEN`: Token do seu bot Discord
   - `USAGE_WEBHOOK_URL`: URL do webhook Discord para logs

## Build (Desktop)

Requer Visual Studio 2022 com workload "Desktop development with C++".

```batch
build.bat
```

## Backend Local

```powershell
$env:DISCORD_BOT_TOKEN="SEU_TOKEN_AQUI"
$env:USAGE_WEBHOOK_URL="SEU_WEBHOOK_AQUI"
.\backend\run-local-auth.ps1
```

## Aviso

Este projeto e apenas para **fins educacionais e de estudo**. Nenhuma credencial real esta incluida no repositorio. Todas as chaves, tokens e endpoints foram substituidos por placeholders.
