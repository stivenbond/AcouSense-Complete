/**
 * Mailer — sends password-reset links.
 * In demo mode (DEMO_MODE=true) the link is just logged to the console;
 * otherwise it goes through nodemailer using SMTP_* env vars.
 */

import nodemailer from "nodemailer";

const APP_URL = process.env.VITE_APP_URL || "http://localhost:5173";
const FROM = process.env.SMTP_FROM || "noreply@acousense.app";

let transporter = null;
function getTransporter() {
  if (transporter) return transporter;
  if (!process.env.SMTP_HOST) return null;
  transporter = nodemailer.createTransport({
    host: process.env.SMTP_HOST,
    port: Number(process.env.SMTP_PORT || 587),
    secure: Number(process.env.SMTP_PORT || 587) === 465,
    auth:
      process.env.SMTP_USER && process.env.SMTP_PASS
        ? { user: process.env.SMTP_USER, pass: process.env.SMTP_PASS }
        : undefined,
  });
  return transporter;
}

export async function sendResetLink(email, token) {
  const link = `${APP_URL}/auth?reset_token=${encodeURIComponent(token)}`;
  if (process.env.DEMO_MODE === "true" || !getTransporter()) {
    console.log(`[acousense-auth] password reset link for ${email}: ${link}`);
    return { ok: true, demo: true };
  }
  await getTransporter().sendMail({
    from: FROM,
    to: email,
    subject: "AcouSense — Reset your password",
    text: `Use the following link to reset your AcouSense password (valid for 1 hour):\n\n${link}`,
    html: `<p>Use the following link to reset your AcouSense password (valid for 1 hour):</p><p><a href="${link}">${link}</a></p>`,
  });
  return { ok: true };
}
