/**
 * AcouSense — auth API client.
 *
 * Talks to the Express auth server at /api/auth/*. In development, the Vite
 * dev server proxies that prefix to http://localhost:3001 (see vite.config.ts)
 * so the cookie set by the auth server is same-origin and works automatically.
 */

export interface AppUser {
  id: number;
  email: string;
  name: string | null;
  healthConsent?: boolean;
  createdAt?: string | null;
  lastLogin?: string | null;
}

export interface AuthResult {
  ok: boolean;
  user?: AppUser;
  error?: string;
  status?: number;
}

const AUTH_BASE = (import.meta.env.VITE_AUTH_BASE_URL ?? "").replace(/\/$/, "");
const BASE = AUTH_BASE ? `${AUTH_BASE}/api/auth` : "/api/auth";

async function request<T>(
  path: string,
  init: RequestInit = {},
): Promise<{ ok: boolean; status: number; data: T | null; error?: string }> {
  try {
    const res = await fetch(`${BASE}${path}`, {
      ...init,
      credentials: "include",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json",
        ...(init.headers ?? {}),
      },
    });
    let data: T | null = null;
    try {
      data = (await res.json()) as T;
    } catch {
      data = null;
    }
    if (!res.ok) {
      const err = (data as { error?: string } | null)?.error;
      return {
        ok: false,
        status: res.status,
        data,
        error: err ?? mapStatusError(res.status),
      };
    }
    return { ok: true, status: res.status, data };
  } catch {
    return {
      ok: false,
      status: 0,
      data: null,
      error: "Connection error. Please try again.",
    };
  }
}

function mapStatusError(status: number): string {
  if (status === 429) return "Too many attempts. Please wait a minute.";
  if (status === 0) return "Connection error. Please try again.";
  return "Something went wrong. Please try again.";
}

export const authApi = {
  async me(): Promise<AppUser | null> {
    const res = await request<{ user: AppUser }>("/me");
    return res.ok && res.data?.user ? res.data.user : null;
  },

  async login(email: string, password: string): Promise<AuthResult> {
    const res = await request<{ success: boolean; user: AppUser }>("/login", {
      method: "POST",
      body: JSON.stringify({ email, password }),
    });
    return res.ok && res.data?.user
      ? { ok: true, user: res.data.user }
      : { ok: false, error: res.error, status: res.status };
  },

  async register(payload: {
    name: string;
    email: string;
    password: string;
    healthConsent?: boolean;
  }): Promise<AuthResult> {
    const res = await request<{ success: boolean; user: AppUser }>("/register", {
      method: "POST",
      body: JSON.stringify(payload),
    });
    return res.ok && res.data?.user
      ? { ok: true, user: res.data.user }
      : { ok: false, error: res.error, status: res.status };
  },

  async forgotPassword(email: string): Promise<{ ok: boolean; error?: string }> {
    const res = await request<{ success: boolean }>("/forgot-password", {
      method: "POST",
      body: JSON.stringify({ email }),
    });
    return { ok: res.ok, error: res.error };
  },

  async logout(): Promise<void> {
    await request("/logout", { method: "POST" });
  },
};
