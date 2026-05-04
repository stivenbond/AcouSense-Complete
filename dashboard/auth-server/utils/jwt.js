import jwt from "jsonwebtoken";

const DEMO_FALLBACK_SECRET = "acousense-demo-secret-do-not-use-in-prod";

export const COOKIE_NAME = "acousense_token";
export const COOKIE_MAX_AGE_MS = 7 * 24 * 60 * 60 * 1000; // 7 days

function getSecret() {
  const secret = process.env.JWT_SECRET;
  if (secret) return secret;
  if (process.env.NODE_ENV === "production" && process.env.DEMO_MODE !== "true") {
    throw new Error("JWT_SECRET must be set in production");
  }
  return DEMO_FALLBACK_SECRET;
}

export function signToken(user) {
  return jwt.sign(
    { sub: user.id, email: user.email, name: user.name },
    getSecret(),
    { expiresIn: "7d" },
  );
}

export function verifyToken(token) {
  return jwt.verify(token, getSecret());
}

export function setAuthCookie(res, user) {
  const token = signToken(user);
  const isDemo = process.env.DEMO_MODE === "true";
  res.cookie(COOKIE_NAME, token, {
    httpOnly: true,
    sameSite: "strict",
    secure: !isDemo,
    maxAge: COOKIE_MAX_AGE_MS,
    path: "/",
  });
}

export function clearAuthCookie(res) {
  res.clearCookie(COOKIE_NAME, { path: "/" });
}
