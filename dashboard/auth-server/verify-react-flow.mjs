// Verify the React app's /api/auth/* proxy works end-to-end through Vite.
const WEB = "http://localhost:8080";

const log = (label, status, extra = "") => console.log(`${label.padEnd(28)} -> ${status} ${extra}`);

// 1. Frontend HTML loads
let r = await fetch(`${WEB}/`);
log("GET /", r.status, `(${r.headers.get("content-type")?.split(";")[0]})`);

// 2. /api/auth/me through the proxy returns 401 when not signed in
r = await fetch(`${WEB}/api/auth/me`);
log("GET /api/auth/me unauth", r.status, await r.text());

// 3. Register through the proxy sets a same-origin cookie
const email = `react-${Date.now()}@example.com`;
r = await fetch(`${WEB}/api/auth/register`, {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ name: "React Flow", email, password: "passw0rd!!" }),
});
const setCookie = r.headers.get("set-cookie") ?? "";
const cookie = (setCookie.match(/acousense_token=[^;]+/) || [""])[0];
log("POST /api/auth/register", r.status, `cookie: ${cookie ? "set" : "missing"}`);

// 4. /me with that cookie returns the user
r = await fetch(`${WEB}/api/auth/me`, { headers: { Cookie: cookie } });
const me = await r.json();
log("GET /api/auth/me authed", r.status, JSON.stringify(me));

console.log("\nIf all four lines above are 200/200/200/200 (with 401 for unauth /me),");
console.log("opening http://localhost:8080 in a browser will:");
console.log("  • show the login page (RequireAuth redirect)");
console.log("  • after sign-in, navigate to / (the dashboard)");
