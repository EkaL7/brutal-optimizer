const DISCORD_API = "https://discord.com/api/v10";

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "access-control-allow-origin": "*",
      "access-control-allow-methods": "POST, OPTIONS",
      "access-control-allow-headers": "content-type"
    }
  });
}

async function sha256Hex(value) {
  const bytes = new TextEncoder().encode(value);
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

async function incrementCounter(env, key) {
  if (!env.USAGE_STATS) return null;
  const current = parseInt((await env.USAGE_STATS.get(key)) || "0", 10);
  const next = current + 1;
  await env.USAGE_STATS.put(key, String(next));
  return next;
}

async function getCounter(env, key) {
  if (!env.USAGE_STATS) return null;
  return parseInt((await env.USAGE_STATS.get(key)) || "0", 10);
}

async function setUnique(env, prefix, id) {
  if (!env.USAGE_STATS || !id) return null;
  const key = `${prefix}:${await sha256Hex(id)}`;
  const existed = await env.USAGE_STATS.get(key);
  if (!existed) await env.USAGE_STATS.put(key, "1");
  return !existed;
}

async function logWebhook(env, payload, allowed, status) {
  const webhookUrl = env.USAGE_WEBHOOK_URL || env.DISCORD_WEBHOOK_URL;
  if (!webhookUrl) return;

  const color = allowed ? 0x43ffb0 : 0xff7abf;
  const hardware = [
    `CPU: ${payload.cpu || "unknown"}`,
    `THREADS: ${payload.cpu_threads || "unknown"}`,
    `GPU: ${payload.gpu || "unknown"}`,
    `BOARD: ${payload.motherboard || "unknown"}`,
    `RAM: ${payload.ram || "unknown"}`,
    `DISK: ${payload.storage || "unknown"}`
  ].join("\n").slice(0, 1024);

  const machine = [
    `PC: ${payload.computer_name || "unknown"}`,
    `USER: ${payload.windows_user || "unknown"}`,
    `OS: ${payload.os || "unknown"}`,
    `ARCH: ${payload.architecture || "unknown"}`,
    `DISPLAY: ${payload.display || "unknown"}`,
    `TEMP: ${payload.temperature || "N/A"}`
  ].join("\n").slice(0, 1024);

  const counters = [
    `TOTAL: ${(await getCounter(env, "total_runs")) ?? "-"}`,
    `OK: ${(await getCounter(env, "allowed_runs")) ?? "-"}`,
    `USERS: ${(await getCounter(env, "unique_discord_users")) ?? "-"}`,
    `MACHINES: ${(await getCounter(env, "unique_machines")) ?? "-"}`
  ].join(" | ");

  const content = {
    username: "KittyCore",
    allowed_mentions: { parse: [] },
    embeds: [
      {
        title: allowed ? "KITTYCORE LOGIN OK" : "KITTYCORE LOGIN NEGADO",
        color,
        fields: [
          { name: "Player ID", value: String(payload.discord_id || "unknown"), inline: true },
          { name: "Status", value: String(status), inline: true },
          { name: "Version", value: String(payload.app_version || "unknown"), inline: true },
          { name: "Counters", value: counters, inline: false },
          { name: "Machine", value: machine, inline: false },
          { name: "Hardware", value: hardware, inline: false },
          { name: "Machine Hash", value: String(payload.machine_hash || "unknown").slice(0, 80), inline: false }
        ],
        timestamp: new Date().toISOString()
      }
    ]
  };

  const response = await fetch(webhookUrl, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(content)
  });
  if (!response.ok) throw new Error(`Webhook failed: ${response.status}`);
}

async function getGuildCounts(env) {
  try {
    const res = await fetch(`${DISCORD_API}/guilds/${env.GUILD_ID}?with_counts=true`, {
      headers: { authorization: `Bot ${env.DISCORD_BOT_TOKEN}` }
    });
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

export default {
  async fetch(request, env) {
    if (request.method === "OPTIONS") return json({ ok: true });
    if (request.method !== "POST") return json({ error: "Use POST /verify." }, 405);

    if (!env.DISCORD_BOT_TOKEN || !env.GUILD_ID) {
      return json({ allowed: false, message: "Backend is missing Discord secrets." }, 500);
    }

    let payload;
    try {
      payload = await request.json();
    } catch {
      return json({ allowed: false, message: "Invalid JSON." }, 400);
    }

    const discordId = String(payload.discord_id || "");
    if (!/^\d{17,20}$/.test(discordId)) {
      return json({ allowed: false, message: "Invalid Discord ID." }, 400);
    }

    const memberRes = await fetch(`${DISCORD_API}/guilds/${env.GUILD_ID}/members/${discordId}`, {
      headers: { authorization: `Bot ${env.DISCORD_BOT_TOKEN}` }
    });

    const allowed = memberRes.status === 200;
    const totalRuns = await incrementCounter(env, "total_runs");
    if (allowed) {
      await incrementCounter(env, "allowed_runs");
      if (await setUnique(env, "discord_user", discordId)) {
        await incrementCounter(env, "unique_discord_users");
      }
      if (await setUnique(env, "machine", String(payload.machine_hash || ""))) {
        await incrementCounter(env, "unique_machines");
      }
    }

    await logWebhook(env, payload, allowed, memberRes.status).catch(() => {});

    const counts = await getGuildCounts(env);
    return json({
      allowed,
      message: allowed ? "Access granted." : "Join the Discord server and try again.",
      invite_url: env.INVITE_URL || "https://discord.gg/YOUR_INVITE_CODE",
      total_runs: totalRuns,
      allowed_runs: await getCounter(env, "allowed_runs"),
      active_users: await getCounter(env, "unique_machines"),
      unique_discord_users: await getCounter(env, "unique_discord_users"),
      ...counts
    }, allowed ? 200 : 403);
  }
};
