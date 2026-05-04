/**
 * Typed wrappers for the AcouSense FastAPI backend
 * (dsp-noise-map/backend/src/acousense/api/fastapi_app.py and server.py).
 *
 * Use these instead of calling Supabase Edge Functions when running against
 * the local DSP backend. Toggle behaviour via VITE_USE_BACKEND in your env.
 */

import { api } from "./apiClient";

export interface BackendReading {
  sensor_id: string;
  timestamp: string;
  leq_dba: number;
  lat?: number | null;
  lng?: number | null;
  [k: string]: unknown;
}

export interface BackendStats {
  total_samples: number;
  avg_dba: number | null;
  min_dba: number | null;
  max_dba: number | null;
  [k: string]: unknown;
}

export interface BackendExposureMetrics {
  [k: string]: unknown;
}

export interface BackendHeatmapPoint {
  lat: number;
  lng: number;
  leq_dba: number;
}

export interface BackendHeatmapResponse {
  readings: unknown[];
  grid: BackendHeatmapPoint[];
  bbox: { min_lat: number; min_lng: number; max_lat: number; max_lng: number };
  generated_at: string;
  error?: string;
}

export const acousense = {
  health: () => api.get<{ status: string; version?: string }>("/health"),
  ready: () => api.get<{ status: string }>("/ready"),
  readings: (opts?: { minutes?: number; sensorId?: string }) =>
    api.get<BackendReading[]>("/api/readings", {
      minutes: opts?.minutes,
      sensor_id: opts?.sensorId,
    }),
  latest: () => api.get<BackendReading[]>("/api/latest"),
  stats: () => api.get<BackendStats>("/api/stats"),
  exposure: (days?: number) =>
    api.get<BackendExposureMetrics>("/api/health/exposure", { days }),
  heatmap: () => api.get<BackendHeatmapResponse>("/api/heatmap"),
  heatmapMatrix: () =>
    api.raw<ArrayBuffer>("GET", "/api/heatmap/matrix", { accept: "arrayBuffer" }),
  heatmapContours: () =>
    api.get<{ type: "FeatureCollection"; features: unknown[] }>(
      "/api/heatmap/contours",
    ),
};
