import { supabase } from "@/integrations/supabase/client";
import { acousense } from "./acousenseApi";

export interface SensorData {
  id: string;
  name: string;
  lat: number;
  lng: number;
  db: number;
  status: "online" | "offline" | "warning";
  lastSeen: string;
  threshold: number;
  signal: number;
}

export interface Reading {
  sensor_id: string;
  name: string;
  db: number;
  age: string;
  status: string;
}

export interface Alert {
  id: number;
  level: "critical" | "warning";
  message: string;
  time: string;
  sensor: string;
}

export interface Stream {
  name: string;
  status: "ok" | "warn" | "error";
  icon: string;
}

export interface SensorPayload {
  timestamp: string;
  sensors: SensorData[];
  readings: Reading[];
  alerts: Alert[];
  streams: Stream[];
  timeSeries: { time: string; db: number }[];
  stats: {
    avgDb: number;
    peakDb: number;
    breachCount: number;
    activeSensors: number;
  };
}

const USE_BACKEND = (import.meta.env.VITE_USE_BACKEND ?? "").toLowerCase() === "true";

function ageFromTimestamp(ts: string | number | undefined): string {
  if (!ts) return "unknown";
  const t = typeof ts === "number" ? new Date(ts * 1000) : new Date(ts);
  const seconds = Math.max(0, Math.floor((Date.now() - t.getTime()) / 1000));
  if (seconds < 60) return `${seconds}s ago`;
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes}m ago`;
  const hours = Math.floor(minutes / 60);
  if (hours < 24) return `${hours}h ago`;
  const days = Math.floor(hours / 24);
  return `${days}d ago`;
}

function statusFromDb(db: number, threshold = 70): SensorData["status"] {
  if (!Number.isFinite(db)) return "offline";
  if (db >= threshold) return "warning";
  return "online";
}

/**
 * Build a SensorPayload from the FastAPI backend by calling several endpoints
 * in parallel and merging the results into the shape the UI already consumes.
 */
async function fetchFromBackend(): Promise<SensorPayload> {
  const [latest, stats] = await Promise.all([
    acousense.latest().catch(() => [] as Awaited<ReturnType<typeof acousense.latest>>),
    acousense.stats().catch(() => null),
  ]);

  const sensors: SensorData[] = latest.map((r, idx) => ({
    id: String(r.sensor_id ?? `sensor-${idx}`),
    name: String(r.sensor_id ?? `Sensor ${idx + 1}`),
    lat: Number(r.lat ?? 0),
    lng: Number(r.lng ?? 0),
    db: Number(r.leq_dba ?? 0),
    status: statusFromDb(Number(r.leq_dba ?? 0)),
    lastSeen: ageFromTimestamp(r.timestamp),
    threshold: 70,
    signal: 100,
  }));

  const readings: Reading[] = latest.map((r, idx) => ({
    sensor_id: String(r.sensor_id ?? `sensor-${idx}`),
    name: String(r.sensor_id ?? `Sensor ${idx + 1}`),
    db: Number(r.leq_dba ?? 0),
    age: ageFromTimestamp(r.timestamp),
    status: statusFromDb(Number(r.leq_dba ?? 0)),
  }));

  return {
    timestamp: new Date().toISOString(),
    sensors,
    readings,
    alerts: [],
    streams: [
      { name: "FastAPI", status: stats ? "ok" : "warn", icon: "server" },
      { name: "MQTT", status: "ok", icon: "radio" },
    ],
    timeSeries: [],
    stats: {
      avgDb: Number(stats?.avg_dba ?? 0),
      peakDb: Number(stats?.max_dba ?? 0),
      breachCount: 0,
      activeSensors: sensors.length,
    },
  };
}

async function fetchFromSupabase(): Promise<SensorPayload> {
  const { data, error } = await supabase.functions.invoke<SensorPayload>("sensor-data");
  if (error) throw error;
  return data!;
}

export async function fetchSensorData(): Promise<SensorPayload> {
  return USE_BACKEND ? fetchFromBackend() : fetchFromSupabase();
}
