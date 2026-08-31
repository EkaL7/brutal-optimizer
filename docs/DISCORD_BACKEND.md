# Discord Backend

The desktop app must not contain a Discord bot token or webhook URL. Both are
extractable from a compiled binary. Keep them in a backend or serverless worker.

Discord membership check:

- Client sends Discord user ID and machine hash to your backend.
- Backend calls Discord `GET /guilds/{guild.id}/members/{user.id}` with a bot
  token.
- Backend returns `allowed: true` only when Discord returns the guild member.
- Backend sends usage logs to your Discord webhook.
- Backend stores aggregate counters if you want usage averages.

Webhook-only logging is not enough for membership validation. A webhook can send
messages, but it cannot prove that a Discord user is inside your server.

## Required Discord Setup

1. Create a Discord application and bot.
2. Add the bot to guild `YOUR_GUILD_ID`.
3. Keep the bot token as a backend secret named `DISCORD_BOT_TOKEN`.
4. Keep the webhook URL as a backend secret named `USAGE_WEBHOOK_URL`.
5. Deploy `backend/cloudflare-worker.js` or equivalent.
6. Set `config/kittycore.json` `auth_endpoint` to your `/verify` URL.

## Local Backend

Set both secrets before starting the local backend:

```powershell
$env:DISCORD_BOT_TOKEN="PASTE_BOT_TOKEN_HERE"
$env:USAGE_WEBHOOK_URL="PASTE_WEBHOOK_URL_HERE"
.\backend\run-local-auth.ps1
```

The webhook log receives Discord ID, login status, counters, PC name, Windows
user, OS, CPU, GPU list summary, motherboard, RAM, storage, temperature when
available, and the machine hash.

## Expected Client Request

```json
{
  "discord_id": "123456789012345678",
  "machine_hash": "hash",
  "app_version": "0.1.0",
  "cpu": "CPU name",
  "gpu": "GPU name",
  "gpus": ["GPU name"],
  "motherboard": "Baseboard",
  "bios": "BIOS",
  "ram": "16.0 GB",
  "storage": "500.0 GB total / 120.0 GB free",
  "storage_devices": ["Disk model"],
  "temperature": "N/A",
  "os": "Windows build",
  "computer_name": "PC",
  "windows_user": "User"
}
```

## Expected Backend Response

```json
{
  "allowed": true,
  "message": "Access granted.",
  "guild_member_count": 1234,
  "total_runs": 99,
  "active_users": 42
}
```
