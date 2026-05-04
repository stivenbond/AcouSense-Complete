/**
 * AcouSense — SQLite database wrapper.
 * Schema is created on first run; safe to call multiple times.
 */

import path from "node:path";
import fs from "node:fs";

const DB_PATH = process.env.DATABASE_URL || "./acousense.db";

// Ensure parent dir exists when DB_PATH is e.g. ./data/acousense.db
const dir = path.dirname(DB_PATH);
if (dir && dir !== "." && !fs.existsSync(dir)) {
  fs.mkdirSync(dir, { recursive: true });
}

// Lazy import so we can show a clearer error if the native binary is stale.
let Database;
try {
  Database = (await import("better-sqlite3")).default;
} catch (err) {
  if (err && err.code === "ERR_DLOPEN_FAILED") {
    console.error(
      "\n[acousense-auth] better-sqlite3 native binary is stale.\n" +
        "                  Run:\n\n" +
        "                  cd auth-server && npm rebuild better-sqlite3\n",
    );
  }
  throw err;
}

const db = new Database(DB_PATH);
db.pragma("journal_mode = WAL");
db.pragma("foreign_keys = ON");

db.exec(`
  CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    email TEXT UNIQUE NOT NULL,
    name TEXT,
    password_hash TEXT,
    health_consent INTEGER DEFAULT 0,
    created_at TEXT DEFAULT (datetime('now')),
    last_login TEXT
  );

  CREATE TABLE IF NOT EXISTS password_resets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    email TEXT NOT NULL,
    token TEXT NOT NULL,
    expires_at TEXT NOT NULL,
    used INTEGER DEFAULT 0
  );

  CREATE INDEX IF NOT EXISTS idx_users_email ON users (email);
  CREATE INDEX IF NOT EXISTS idx_resets_token ON password_resets (token);
`);

export default db;

// ── Repository helpers ──────────────────────────────────────────────────────

export function findUserByEmail(email) {
  return db
    .prepare("SELECT * FROM users WHERE lower(email) = lower(?)")
    .get(email);
}

export function findUserById(id) {
  return db.prepare("SELECT * FROM users WHERE id = ?").get(id);
}

export function createUser({ email, name, passwordHash, healthConsent }) {
  const stmt = db.prepare(`
    INSERT INTO users (email, name, password_hash, health_consent, last_login)
    VALUES (@email, @name, @passwordHash, @healthConsent, datetime('now'))
  `);
  const info = stmt.run({
    email,
    name: name ?? null,
    passwordHash: passwordHash ?? null,
    healthConsent: healthConsent ? 1 : 0,
  });
  return findUserById(info.lastInsertRowid);
}

export function touchLastLogin(id) {
  db.prepare("UPDATE users SET last_login = datetime('now') WHERE id = ?").run(id);
}

export function updatePasswordHash(id, passwordHash) {
  db.prepare("UPDATE users SET password_hash = ? WHERE id = ?").run(passwordHash, id);
}

export function recordPasswordReset({ email, token, expiresAt }) {
  db.prepare(
    "INSERT INTO password_resets (email, token, expires_at) VALUES (?, ?, ?)",
  ).run(email, token, expiresAt);
}

export function findValidReset(token) {
  return db
    .prepare(
      "SELECT * FROM password_resets WHERE token = ? AND used = 0 AND expires_at > datetime('now')",
    )
    .get(token);
}

export function consumeReset(id) {
  db.prepare("UPDATE password_resets SET used = 1 WHERE id = ?").run(id);
}

export function publicUser(row) {
  if (!row) return null;
  return {
    id: row.id,
    email: row.email,
    name: row.name,
    healthConsent: !!row.health_consent,
    createdAt: row.created_at,
    lastLogin: row.last_login,
  };
}
