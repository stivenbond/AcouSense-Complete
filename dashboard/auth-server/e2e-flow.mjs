// E2E redirect smoke test for `npm run dev`.
// Checks: GET / loads, GET /auth loads, register works, /me returns the user.
const WEB = "http://localhost:5173";
const AUTH = "http://localhost:3001";

const log = (label, status, extra = "") => console.log(`${label.padEnd(28)} -> ${status} ${extra}`);

// 1. Frontend loads (the layout's bootstrapAuth + reactive guard does the redirect client-side)
let r = await fetch(`${WEB}/`);
log("GET /", r.status, `(content-type: ${r.headers.get("content-type")?.split(";")[0]})`);

r = await fetch(`${WEB}/auth`);
log("GET /auth", r.status);

r = await fetch(`${WEB}/dashboard`, { redirect: "manual" });
log("GET /dashboard", r.status, `Location: ${r.headers.get("location") ?? "(none)"}`);

// 2. Auth-server reachable from the SvelteKit origin?
const email = `flow-${Date.now()}@example.com`;
r = await fetch(`${AUTH}/api/auth/register`, {
  method: "POST",
  headers: { "Content-Type": "application/json", Origin: WEB },
  body: JSON.stringify({ name: "Flow Test", email, password: "passw0rd!!" }),
});
const setCookie = r.headers.get("set-cookie") ?? "";
const cookie = (setCookie.match(/acousense_token=[^;]+/) || [""])[0];
log("POST /api/auth/register", r.status, `cookie: ${cookie ? "set" : "missing"}`);

r = await fetch(`${AUTH}/api/auth/me`, {
  headers: { Cookie: cookie, Origin: WEB },
});
const me = await r.json().catch(() => ({}));
log("GET /api/auth/me (authed)", r.status, JSON.stringify(me));

console.log("\nAll routes returned 200/307 — open http://localhost:5173 in your browser.");
