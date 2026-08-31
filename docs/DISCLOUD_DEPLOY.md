# KittyCore Backend on Discloud

Use this as a Discloud website/API app, not a background-only bot.

## Files

- `package.json`
- `discloud.config`
- `.discloudignore`
- `backend/local-auth-server.js`

## Required Secrets

Set these in the Discloud environment panel:

```text
DISCORD_BOT_TOKEN=your_bot_token
USAGE_WEBHOOK_URL=your_discord_webhook
GUILD_ID=YOUR_GUILD_ID
INVITE_URL=https://discord.gg/YOUR_INVITE_CODE
```

Do not put these values inside the desktop `.exe`.

## Discloud Config

Edit this before deploy:

```text
ID=kittycore-backend
```

Use the subdomain/app id that you created in Discloud.

## App Endpoint

After deploy, the desktop app must point to:

```text
https://YOUR_BACKEND_HOST/verify
```

Health check:

```text
https://YOUR_BACKEND_HOST/health
```

Stats:

```text
https://YOUR_BACKEND_HOST/stats
```

## CLI Deploy

Install/login using the Discloud CLI:

```powershell
npm install -g discloud-cli
discloud login
```

Then run:

```powershell
.\deploy-discloud.ps1
```

The script validates Node syntax and uploads the backend using:

```powershell
discloud app upload .
```

After Discloud gives you the public domain, set the desktop app config to:

```json
{
  "auth_endpoint": "https://YOUR_BACKEND_HOST/verify"
}
```

Then rebuild the `.exe`.

## Payload Collected

The backend stores usage events in:

```text
data/usage-events.jsonl
```

Each event includes Discord player ID, access status, app version, machine hash,
Windows user, computer name, OS, architecture, display, CPU, threads, GPU list,
motherboard, BIOS, RAM, memory load, storage, storage free percentage, storage
devices, temperature when available, and request IP.
