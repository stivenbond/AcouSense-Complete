/**
 * HTTP client for the AcouSense FastAPI backend (dsp-noise-map/backend).
 *
 * In development, requests to relative paths (e.g. "/api/readings") are
 * forwarded by the Vite dev server proxy (see vite.config.ts) to the URL in
 * VITE_BACKEND_URL (default: http://localhost:8000).
 *
 * In production, set VITE_API_BASE_URL to the deployed backend origin
 * (e.g. "https://api.acousense.example.com"). When unset, requests stay
 * relative and assume the frontend is served from the same origin as the API.
 */

const API_BASE_URL: string = (import.meta.env.VITE_API_BASE_URL ?? "").replace(/\/$/, "");

function buildUrl(path: string, params?: Record<string, string | number | boolean | undefined>): string {
  const normalised = path.startsWith("/") ? path : `/${path}`;
  const url = `${API_BASE_URL}${normalised}`;
  if (!params) return url;
  const search = new URLSearchParams();
  for (const [key, value] of Object.entries(params)) {
    if (value === undefined || value === null) continue;
    search.append(key, String(value));
  }
  const qs = search.toString();
  return qs ? `${url}?${qs}` : url;
}

export class ApiError extends Error {
  status: number;
  body: unknown;
  constructor(status: number, body: unknown, message?: string) {
    super(message ?? `Request failed with status ${status}`);
    this.name = "ApiError";
    this.status = status;
    this.body = body;
  }
}

async function request<T>(
  method: "GET" | "POST" | "PUT" | "DELETE" | "PATCH",
  path: string,
  options: {
    params?: Record<string, string | number | boolean | undefined>;
    body?: unknown;
    signal?: AbortSignal;
    headers?: Record<string, string>;
    accept?: "json" | "blob" | "arrayBuffer" | "text";
  } = {},
): Promise<T> {
  const url = buildUrl(path, options.params);
  const headers: Record<string, string> = {
    Accept: "application/json",
    ...(options.headers ?? {}),
  };
  let body: BodyInit | undefined;
  if (options.body !== undefined) {
    headers["Content-Type"] = "application/json";
    body = JSON.stringify(options.body);
  }

  const res = await fetch(url, { method, headers, body, signal: options.signal });

  if (!res.ok) {
    let errBody: unknown = null;
    try {
      errBody = await res.json();
    } catch {
      try {
        errBody = await res.text();
      } catch {
        /* swallow */
      }
    }
    throw new ApiError(res.status, errBody);
  }

  switch (options.accept) {
    case "blob":
      return (await res.blob()) as unknown as T;
    case "arrayBuffer":
      return (await res.arrayBuffer()) as unknown as T;
    case "text":
      return (await res.text()) as unknown as T;
    case "json":
    default:
      if (res.status === 204) return undefined as unknown as T;
      return (await res.json()) as T;
  }
}

export const api = {
  get: <T>(path: string, params?: Record<string, string | number | boolean | undefined>, signal?: AbortSignal) =>
    request<T>("GET", path, { params, signal }),
  post: <T>(path: string, body?: unknown, signal?: AbortSignal) =>
    request<T>("POST", path, { body, signal }),
  put: <T>(path: string, body?: unknown, signal?: AbortSignal) =>
    request<T>("PUT", path, { body, signal }),
  patch: <T>(path: string, body?: unknown, signal?: AbortSignal) =>
    request<T>("PATCH", path, { body, signal }),
  del: <T>(path: string, signal?: AbortSignal) => request<T>("DELETE", path, { signal }),
  raw: request,
};

export const API_BASE = API_BASE_URL;
