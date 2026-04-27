import { readFileSync, writeFileSync, mkdirSync, readdirSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const SCRIPT_DIR = path.dirname(fileURLToPath(import.meta.url));
const PROJECT_ROOT = path.resolve(SCRIPT_DIR, "..");
const OUTPUT_DIR = path.join(PROJECT_ROOT, "output", "conversationsw");
const CODEX_SESSIONS_DIR = path.join(
  process.env.USERPROFILE || process.env.HOME,
  ".codex",
  "sessions"
);

mkdirSync(OUTPUT_DIR, { recursive: true });

function readJsonl(filePath) {
  const content = readFileSync(filePath, "utf8");
  const lines = content.split(/\r?\n/).filter(Boolean);
  const records = [];
  for (const line of lines) {
    try {
      records.push({ record: JSON.parse(line) });
    } catch {}
  }
  return records;
}

function extractMessages(records) {
  const messages = [];
  for (const entry of records) {
    const record = entry.record;
    if (record.type !== "response_item") continue;
    const payload = record.payload || {};
    if (payload.type !== "message") continue;
    const role = payload.role;
    if (role !== "user" && role !== "assistant") continue;
    const content = payload.content || [];
    const parts = [];
    for (const item of content) {
      if (!item || typeof item !== "object") continue;
      if (typeof item.text === "string") parts.push(item.text);
      else if (typeof item.input_text === "string") parts.push(item.input_text);
      else if (typeof item.output_text === "string") parts.push(item.output_text);
    }
    let text = parts.join("\n\n");
    if (!text && typeof payload.text === "string") text = payload.text;
    text = text.trim();
    if (!text) continue;
    if (role === "user" && text.trimStart().startsWith("# AGENTS.md instructions for ")) continue;
    messages.push({ role, text, timestamp: record.timestamp || "" });
  }
  return messages;
}

function mergeMessages(messages) {
  const merged = [];
  for (const msg of messages) {
    const last = merged[merged.length - 1];
    if (last && last.role === msg.role) {
      last.text += "\n\n" + msg.text;
      last.timestamp = msg.timestamp || last.timestamp;
    } else {
      merged.push({ ...msg });
    }
  }
  return merged;
}

function getAllSessionFiles() {
  const results = [];
  function walk(dir) {
    try {
      const entries = readdirSync(dir, { withFileTypes: true });
      for (const entry of entries) {
        const fp = path.join(dir, entry.name);
        if (entry.isDirectory()) walk(fp);
        else if (entry.isFile() && entry.name.endsWith(".jsonl")) results.push(fp);
      }
    } catch {}
  }
  walk(CODEX_SESSIONS_DIR);
  return results;
}

function main() {
  const sessionFiles = getAllSessionFiles();
  if (sessionFiles.length === 0) {
    console.log("No Codex session files found.");
    return;
  }

  for (const filePath of sessionFiles) {
    const match = filePath.match(
      /([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})/i
    );
    const sessionId = match ? match[1] : path.basename(filePath, ".jsonl");
    const records = readJsonl(filePath);
    let messages = extractMessages(records);
    messages = mergeMessages(messages);
    const title =
      messages.find((m) => m.role === "user")?.text.slice(0, 60).replace(/\s+/g, " ").trim() ||
      sessionId;
    let md = `# ${title}\n\n`;
    md += `- **Session ID**: \`${sessionId}\`\n`;
    md += `- **Source**: \`${filePath}\`\n`;
    md += `- **Messages**: ${messages.length}\n\n---\n\n`;
    for (const msg of messages) {
      const label = msg.role === "user" ? "## User" : "## Assistant";
      md += `${label}\n\n${msg.text}\n\n---\n\n`;
    }
    const sanitizedTitle = title.replace(/[<>:"/\\|?*]/g, "_").slice(0, 80);
    const outputPath = path.join(OUTPUT_DIR, `${sanitizedTitle}-${sessionId.slice(0, 8)}.md`);
    writeFileSync(outputPath, md, "utf8");
    console.log(`[OK] ${outputPath} (${messages.length} messages)`);
  }

  console.log(`\nDone! ${sessionFiles.length} session(s) exported to ${OUTPUT_DIR}`);
}

main();
