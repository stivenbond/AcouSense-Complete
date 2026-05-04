/**
 * AcouSense — Express auth server entry point.
 *
 * Run with:
 *   node auth-server/index.js
 *
 * Demo mode (default): boots with sensible fallbacks and SQLite at ./acousense.db.
 * Production: requires JWT_SECRET; HTTPS-only cookies; SMTP env vars for the mailer.
 */

import express from "express";
import cookieParser from "cookie-parser";
import cors from "cors";
import helmet from "helmet";
import morgan from "morgan";
import rateLimit from "express-rate-limit";
import dotenv from "dotenv";

dotenv.config();

import authRouter from "./routes/auth.js";
import "./db.js";

const app = express();

const PORT = Number(process.env.PORT || 3030);
const APP_URL = process.env.VITE_APP_URL || "http://localhost:5173";
const isProd = process.env.NODE_ENV === "production";

// Default to demo mode whenever NODE_ENV !== "production" so `npm run dev`
// works out of the box with no .env file at all.
if (process.env.DEMO_MODE === undefined && !isProd) {
  process.env.DEMO_MODE = "true";
}
const isDemo = process.env.DEMO_MODE === "true";

if (isProd && !isDemo && !process.env.JWT_SECRET) {
  throw new Error("JWT_SECRET must be set when NODE_ENV=production");
}

app.set("trust proxy", 1);
app.use(helmet());
app.use(morgan(isProd ? "combined" : "dev"));
app.use(express.json({ limit: "100kb" }));
app.use(cookieParser());
app.use(
  cors({
    origin: APP_URL,
    credentials: true,
  }),
);

// 10 requests / minute on every /api/auth/* route
const authLimiter = rateLimit({
  windowMs: 60 * 1000,
  max: 10,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests. Please wait a minute." },
});
app.use("/api/auth", authLimiter, authRouter);

app.get("/health", (_req, res) => {
  res.json({ status: "ok", service: "acousense-auth", demoMode: isDemo });
});

// Catch-all 404 for unknown routes
app.use((req, res) => {
  res.status(404).json({ error: "Not found" });
});

// Global error handler — never leaks stack traces in production.
// eslint-disable-next-line no-unused-vars
app.use((err, _req, res, _next) => {
  if (!isProd) console.error(err);
  res.status(500).json({ error: "Internal server error" });
});

app.listen(PORT, () => {
  console.log(
    `[acousense-auth] listening on http://localhost:${PORT} ` +
      `(demo=${isDemo ? "on" : "off"}, cors=${APP_URL})`,
  );
});
