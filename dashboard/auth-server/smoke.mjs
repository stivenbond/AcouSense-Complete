// End-to-end smoke test for the auth server.
// Run while the server is up: `node smoke.mjs`
const BASE = "http://localhost:3001";

async function call(name, method, path, body, cookies) {
  const headers = { "Content-Type": "application/json" };
  if (cookies) headers["Cookie"] = cookies;
  const res = await fetch(`${BASE}${path}`, {
    method,
    headers,
    body: body ? JSON.stringify(body) : undefined,
  });
  const setCookie = res.headers.get("set-cookie") ?? "";
  let json = null;
  try { json = await res.json(); } catch {}
  console.log(`${name.padEnd(14)} ${method} ${path} -> ${res.status} ${JSON.stringify(json)}`);
  return { status: res.status, json, setCookie };
}

const email = `smoke-${Date.now()}@example.com`;

await call("health", "GET", "/health");
await call("me-unauth", "GET", "/api/auth/me");
const reg = await call("register", "POST", "/api/auth/register", {
  name: "Smoke Test", email, password: "passw0rd!!", healthConsent: true,
});
const token = (reg.setCookie.match(/acousense_token=[^;]+/) || [""])[0];
await call("me-auth", "GET", "/api/auth/me", null, token);
await call("register-dup", "POST", "/api/auth/register", { name: "x", email, password: "passw0rd!!" });
await call("bad-login", "POST", "/api/auth/login", { email, password: "wrong" });
await call("good-login", "POST", "/api/auth/login", { email, password: "passw0rd!!" });
await call("forgot", "POST", "/api/auth/forgot-password", { email });
await call("forgot-unknown", "POST", "/api/auth/forgot-password", { email: "nope@example.com" });
await call("reset-bad", "POST", "/api/auth/reset-password", { token: "nope", newPassword: "newpassw0rd!!" });
await call("logout", "POST", "/api/auth/logout", null, token);
await call("not-found", "GET", "/api/auth/missing");
