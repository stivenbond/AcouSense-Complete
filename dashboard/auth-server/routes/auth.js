/**
 * /api/auth/* — registration, login, password reset, session.
 */

import express from "express";
import bcrypt from "bcrypt";
import crypto from "node:crypto";
import {
  publicUser,
  findUserByEmail,
  findUserById,
  createUser,
  touchLastLogin,
  updatePasswordHash,
  recordPasswordReset,
  findValidReset,
  consumeReset,
} from "../db.js";
import {
  setAuthCookie,
  clearAuthCookie,
} from "../utils/jwt.js";
import { sendResetLink } from "../utils/mailer.js";
import { requireAuth } from "../middleware/requireAuth.js";

const router = express.Router();

const BCRYPT_ROUNDS = 12;
const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

function isValidEmail(email) {
  return typeof email === "string" && EMAIL_RE.test(email) && email.length <= 254;
}

function clientErr(res, status, error) {
  return res.status(status).json({ error });
}

// ── POST /register ──────────────────────────────────────────────────────────

router.post("/register", async (req, res, next) => {
  try {
    const { name, email, password, healthConsent } = req.body ?? {};

    if (!name || typeof name !== "string" || !name.trim()) {
      return clientErr(res, 422, "Name is required");
    }
    if (!isValidEmail(email)) {
      return clientErr(res, 422, "A valid email is required");
    }
    if (typeof password !== "string" || password.length < 8) {
      return clientErr(res, 422, "Password must be at least 8 characters");
    }

    if (findUserByEmail(email)) {
      return clientErr(res, 409, "An account with this email already exists");
    }

    const passwordHash = await bcrypt.hash(password, BCRYPT_ROUNDS);
    const user = createUser({
      email: email.trim(),
      name: name.trim(),
      passwordHash,
      healthConsent: !!healthConsent,
    });

    setAuthCookie(res, user);
    return res.json({ success: true, user: publicUser(user) });
  } catch (err) {
    next(err);
  }
});

// ── POST /login ─────────────────────────────────────────────────────────────

router.post("/login", async (req, res, next) => {
  try {
    const { email, password } = req.body ?? {};
    if (!isValidEmail(email) || typeof password !== "string" || !password) {
      return clientErr(res, 401, "Invalid credentials");
    }

    const user = findUserByEmail(email);
    if (!user || !user.password_hash) {
      // Same message regardless of which check failed — no user enumeration.
      return clientErr(res, 401, "Invalid credentials");
    }
    const ok = await bcrypt.compare(password, user.password_hash);
    if (!ok) return clientErr(res, 401, "Invalid credentials");

    touchLastLogin(user.id);
    const fresh = findUserById(user.id);
    setAuthCookie(res, fresh);
    return res.json({ success: true, user: publicUser(fresh) });
  } catch (err) {
    next(err);
  }
});

// ── POST /forgot-password ───────────────────────────────────────────────────

router.post("/forgot-password", async (req, res, next) => {
  try {
    const { email } = req.body ?? {};
    if (!isValidEmail(email)) {
      // Always 200 to avoid leaking whether the email exists.
      return res.json({ success: true });
    }

    const user = findUserByEmail(email);
    if (user) {
      const token = crypto.randomBytes(32).toString("hex");
      const expiresAt = new Date(Date.now() + 60 * 60 * 1000)
        .toISOString()
        .replace("T", " ")
        .replace(/\..+/, "");
      recordPasswordReset({ email: user.email, token, expiresAt });
      try {
        await sendResetLink(user.email, token);
      } catch (e) {
        console.warn("[acousense-auth] mailer failed:", e?.message ?? e);
      }
    }
    return res.json({ success: true });
  } catch (err) {
    next(err);
  }
});

// ── POST /reset-password ────────────────────────────────────────────────────

router.post("/reset-password", async (req, res, next) => {
  try {
    const { token, newPassword } = req.body ?? {};
    if (!token || typeof token !== "string") {
      return clientErr(res, 400, "Missing token");
    }
    if (typeof newPassword !== "string" || newPassword.length < 8) {
      return clientErr(res, 422, "Password must be at least 8 characters");
    }
    const reset = findValidReset(token);
    if (!reset) return clientErr(res, 400, "Invalid or expired reset link");

    const user = findUserByEmail(reset.email);
    if (!user) return clientErr(res, 400, "Invalid or expired reset link");

    const hash = await bcrypt.hash(newPassword, BCRYPT_ROUNDS);
    updatePasswordHash(user.id, hash);
    consumeReset(reset.id);
    return res.json({ success: true });
  } catch (err) {
    next(err);
  }
});

// ── GET /me ─────────────────────────────────────────────────────────────────

router.get("/me", requireAuth, (req, res) => {
  return res.json({ user: publicUser(req.user) });
});

// ── POST /logout ────────────────────────────────────────────────────────────

router.post("/logout", (req, res) => {
  clearAuthCookie(res);
  return res.json({ success: true });
});

export default router;
