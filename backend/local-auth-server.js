const http = require("http");
const fs = require("fs");
const path = require("path");

function loadDotEnv() {
  const envPath = path.join(process.cwd(), ".env");
  if (!fs.existsSync(envPath)) return;
  const lines = fs.readFileSync(envPath, "utf8").split(/\r?\n/);
  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#")) continue;
    const index = trimmed.indexOf("=");
    if (index <= 0) continue;
    const key = trimmed.slice(0, index).trim();
    let value = trimmed.slice(index + 1).trim();
    if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith("'") && value.endsWith("'"))) {
      value = value.slice(1, -1);
    }
    if (!process.env[key]) process.env[key] = value;
  }
}

loadDotEnv();

const DISCORD_API = "https://discord.com/api/v10";
const PORT = Number(process.env.PORT || 8080);
const HOST = process.env.HOST || "0.0.0.0";
const GUILD_ID = process.env.GUILD_ID || "YOUR_GUILD_ID";
const INVITE_URL = process.env.INVITE_URL || "https://discord.gg/YOUR_INVITE_CODE";
const USAGE_WEBHOOK_URL = process.env.USAGE_WEBHOOK_URL || process.env.DISCORD_WEBHOOK_URL || "";
const DATA_DIR = process.env.DATA_DIR || path.join(process.cwd(), "data");

const stats = {
  totalRuns: 0,
  allowedRuns: 0,
  users: new Set(),
  machines: new Set()
};

function safeText(value, fallback = "-") {
  const text = String(value || fallback);
  return text.length > 1024 ? `${text.slice(0, 1020)}...` : text;
}

function appendUsageEvent(req, payload, allowed, status, message) {
  fs.mkdirSync(DATA_DIR, { recursive: true });
  const ip = String(req.headers["cf-connecting-ip"] || req.headers["x-forwarded-for"] || req.socket.remoteAddress || "")
    .split(",")[0]
    .trim();

  const event = {
    timestamp: new Date().toISOString(),
    allowed,
    status,
    message,
    ip,
    discord_id: String(payload.discord_id || ""),
    app_version: String(payload.app_version || ""),
    machine_hash: String(payload.machine_hash || ""),
    pc: {
      computer_name: String(payload.computer_name || ""),
      windows_user: String(payload.windows_user || ""),
      os: String(payload.os || ""),
      architecture: String(payload.architecture || ""),
      display: String(payload.display || ""),
      displays: Array.isArray(payload.displays) ? payload.displays : [],
      temperature: String(payload.temperature || "")
    },
    hardware: {
      cpu: String(payload.cpu || ""),
      cpu_threads: String(payload.cpu_threads || ""),
      gpu: String(payload.gpu || ""),
      gpus: Array.isArray(payload.gpus) ? payload.gpus : [],
      motherboard: String(payload.motherboard || ""),
      bios: String(payload.bios || ""),
      ram: String(payload.ram || ""),
      memory_load_percent: payload.memory_load_percent ?? null,
      storage: String(payload.storage || ""),
      storage_free_percent: payload.storage_free_percent ?? null,
      storage_devices: Array.isArray(payload.storage_devices) ? payload.storage_devices : []
    }
  };

  fs.appendFileSync(path.join(DATA_DIR, "usage-events.jsonl"), `${JSON.stringify(event)}\n`);
}

function send(res, status, data) {
  const body = JSON.stringify(data);
  res.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "access-control-allow-origin": "*",
    "access-control-allow-methods": "POST, OPTIONS",
    "access-control-allow-headers": "content-type",
    "content-length": Buffer.byteLength(body)
  });
  res.end(body);
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    req.on("data", (chunk) => {
      body += chunk;
      if (body.length > 128 * 1024) {
        reject(new Error("Body too large."));
        req.destroy();
      }
    });
    req.on("end", () => resolve(body));
    req.on("error", reject);
  });
}

async function discord(path) {
  const token = process.env.DISCORD_BOT_TOKEN;
  if (!token) {
    throw new Error("DISCORD_BOT_TOKEN is not set.");
  }

  return fetch(`${DISCORD_API}${path}`, {
    headers: {
      authorization: `Bot ${token}`,
      "user-agent": "KittyCore/1.0.0-beta"
    }
  });
}

async function guildCounts() {
  try {
    const res = await discord(`/guilds/${GUILD_ID}?with_counts=true`);
    if (!res.ok) return {};
    const data = await res.json();
    return {
      guild_member_count: data.approximate_member_count || null,
      guild_presence_count: data.approximate_presence_count || null
    };
  } catch {
    return {};
  }
}

async function guildInfo() {
  const res = await discord(`/guilds/${GUILD_ID}?with_counts=true`);
  let data = {};
  try {
    data = await res.json();
  } catch {
    data = {};
  }
  return { status: res.status, data };
}

async function webhookLog(payload, allowed, status) {
  const url = USAGE_WEBHOOK_URL;
  if (!url) return;

  const hardware = [
    `CPU: ${payload.cpu || "-"}`,
    `THREADS: ${payload.cpu_threads || "-"}`,
    `GPU: ${payload.gpu || "-"}`,
    `BOARD: ${payload.motherboard || "-"}`,
    `RAM: ${payload.ram || "-"}`,
    `DISK: ${payload.storage || "-"}`
  ].join("\n").slice(0, 1024);

  const machine = [
    `PC: ${payload.computer_name || "-"}`,
    `USER: ${payload.windows_user || "-"}`,
    `OS: ${payload.os || "-"}`,
    `ARCH: ${payload.architecture || "-"}`,
    `DISPLAY: ${payload.display || "-"}`,
    `TEMP: ${payload.temperature || "N/A"}`
  ].join("\n").slice(0, 1024);

  const counters = [
    `TOTAL: ${stats.totalRuns}`,
    `OK: ${stats.allowedRuns}`,
    `USERS: ${stats.users.size}`,
    `MACHINES: ${stats.machines.size}`
  ].join(" | ");

  const response = await fetch(url, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      username: "KittyCore",
      allowed_mentions: { parse: [] },
      embeds: [
        {
          title: allowed ? "KITTYCORE LOGIN OK" : "KITTYCORE LOGIN NEGADO",
          color: allowed ? 0x43ffb0 : 0xff7abf,
          fields: [
            { name: "Player ID", value: String(payload.discord_id || "-"), inline: true },
            { name: "Status", value: String(status), inline: true },
            { name: "Version", value: String(payload.app_version || "-"), inline: true },
            { name: "Counters", value: counters, inline: false },
            { name: "Machine", value: machine, inline: false },
            { name: "Hardware", value: hardware, inline: false },
            { name: "Storage Devices", value: safeText((payload.storage_devices || []).join("\n")), inline: false },
            { name: "Machine Hash", value: String(payload.machine_hash || "-").slice(0, 80), inline: false }
          ],
          timestamp: new Date().toISOString()
        }
      ]
    })
  });

  if (!response.ok) {
    const text = await response.text().catch(() => "");
    throw new Error(`Webhook failed: ${response.status} ${text.slice(0, 160)}`);
  }
}

async function verify(req, res) {
  let payload;
  try {
    payload = JSON.parse(await readBody(req));
  } catch {
    return send(res, 400, { allowed: false, message: "INVALID_JSON" });
  }

  const discordId = String(payload.discord_id || "");
  if (!/^\d{17,20}$/.test(discordId)) {
    return send(res, 400, { allowed: false, message: "INVALID_ID" });
  }

  let memberStatus = 0;
  let allowed = false;
  let message = "DENIED";
  let guild = null;

  try {
    const member = await discord(`/guilds/${GUILD_ID}/members/${discordId}`);
    memberStatus = member.status;
    allowed = member.status === 200;
    if (allowed) {
      message = "OK";
    } else {
      guild = await guildInfo();
      if (guild.status === 200 && String(guild.data.owner_id || "") === discordId) {
        allowed = true;
        message = "OWNER_OK";
      } else if (memberStatus === 401) {
        message = "TOKEN_INVALIDO";
      } else if (memberStatus === 403) {
        message = "BOT_SEM_ACESSO";
      } else if (memberStatus === 404) {
        message = "NAO_MEMBRO";
      } else {
        message = `DISCORD_${memberStatus}`;
      }
    }
  } catch (error) {
    return send(res, 500, { allowed: false, message: error.message });
  }

  stats.totalRuns += 1;
  if (allowed) {
    stats.allowedRuns += 1;
    stats.users.add(discordId);
    if (payload.machine_hash) stats.machines.add(String(payload.machine_hash));
  }

  console.log(`[verify] id=${discordId} discord_status=${memberStatus} allowed=${allowed} message=${message}`);
  try {
    appendUsageEvent(req, payload, allowed, memberStatus, message);
  } catch (error) {
    console.error(`[usage-log] ${error.message}`);
  }

  await webhookLog(payload, allowed, memberStatus).catch((error) => {
    console.error(`[webhook] ${error.message}`);
  });
  const counts = guild && guild.status === 200
    ? {
        guild_member_count: guild.data.approximate_member_count || null,
        guild_presence_count: guild.data.approximate_presence_count || null
      }
    : await guildCounts();

  send(res, allowed ? 200 : 403, {
    allowed,
    message,
    discord_status: memberStatus,
    owner_id: guild && guild.data ? guild.data.owner_id || null : null,
    invite_url: INVITE_URL,
    total_runs: stats.totalRuns,
    allowed_runs: stats.allowedRuns,
    active_users: stats.machines.size,
    unique_discord_users: stats.users.size,
    ...counts
  });
}

http.createServer((req, res) => {
  if (req.method === "OPTIONS") {
    return send(res, 200, { ok: true });
  }

  if (req.method === "GET" && (req.url === "/" || req.url === "/health")) {
    return send(res, 200, {
      ok: true,
      app: "KittyCore Backend",
      guild_id: GUILD_ID,
      webhook: Boolean(USAGE_WEBHOOK_URL)
    });
  }

  if (req.method === "GET" && req.url === "/stats") {
    return send(res, 200, {
      total_runs: stats.totalRuns,
      allowed_runs: stats.allowedRuns,
      unique_discord_users: stats.users.size,
      unique_machines: stats.machines.size,
      data_file: path.join(DATA_DIR, "usage-events.jsonl")
    });
  }

  if (req.method === "POST" && req.url === "/verify") {
    verify(req, res).catch((error) => send(res, 500, { allowed: false, message: error.message }));
    return;
  }

  send(res, 404, { error: "POST /verify" });
}).listen(PORT, HOST, () => {
  console.log(`KittyCore auth server listening on http://${HOST}:${PORT}/verify`);
  console.log(`Guild: ${GUILD_ID}`);
  console.log(`Webhook: ${USAGE_WEBHOOK_URL ? "configured" : "missing USAGE_WEBHOOK_URL"}`);
});
