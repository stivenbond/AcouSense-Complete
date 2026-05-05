import { supabase } from "@/integrations/supabase/client";
import { acousense, type EspDevice, type EspReading, type EspStatusResponse } from "./acousenseApi";

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

export interface DeviceCardData {
  id: string;
  name: string;
  status: "online" | "offline" | "warning";
  lastSeen: string;
  threshold: number;
  signal: number;
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

function alertLevelToStatus(alertLevel: number): SensorData["status"] {
  if (alertLevel >= 2) return "warning";
  return "online";
}

function rssiToSignal(rssi: number | undefined): number {
  if (typeof rssi !== "number" || Number.isNaN(rssi)) return 0;
  if (rssi >= -55) return 5;
  if (rssi >= -67) return 4;
  if (rssi >= -75) return 3;
  if (rssi >= -85) return 2;
  if (rssi >= -95) return 1;
  return 0;
}

function mapEspReadingsToPayload(status: EspStatusResponse, readings: EspReading[]): SensorPayload {
  const latest = readings[0];
  const sensorStatus: SensorData["status"] =
    status.arduino === "online"
      ? alertLevelToStatus(latest?.alert_level ?? 0)
      : "offline";

  const lastTimestamp = latest?.timestamp ?? status.last_sync_ts;
  const currentDb = Number(latest?.avg_level ?? status.last_reading?.avg_level ?? 0);

  const sensors: SensorData[] = [
    {
      id: "ESP32-GW-1",
      name: "AcouSense Gateway",
      lat: 0,
      lng: 0,
      db: currentDb,
      status: sensorStatus,
      lastSeen: ageFromTimestamp(lastTimestamp),
      threshold: 70,
      signal: rssiToSignal(status.wifi_rssi),
    },
  ];

  const readingRows: Reading[] = readings.map((r) => ({
    sensor_id: "ESP32-GW-1",
    name: "AcouSense Gateway",
    db: Number(r.avg_level ?? 0),
    age: ageFromTimestamp(r.timestamp),
    status: alertLevelToStatus(Number(r.alert_level ?? 0)),
  }));

  return {
    timestamp: new Date().toISOString(),
    sensors,
    readings: readingRows,
    alerts: readingRows
      .filter((r) => r.db >= 70)
      .slice(0, 5)
      .map((r, idx) => ({
        id: idx + 1,
        level: r.db >= 80 ? "critical" : "warning",
        message: `${r.sensor_id} measured ${r.db.toFixed(1)} dB`,
        time: r.age,
        sensor: r.sensor_id,
      })),
    streams: [
      { name: "ESP32 API", status: "ok", icon: "server" },
      { name: "Arduino SPI", status: status.arduino === "online" ? "ok" : "error", icon: "memory" },
      { name: "SD Card", status: status.sd_card === "ok" ? "ok" : "error", icon: "storage" },
      { name: "Bluetooth", status: status.bt_status === "SCANNING" || status.bt_status === "SYNCING" ? "ok" : "warn", icon: "bluetooth" },
    ],
    timeSeries: readings
      .slice()
      .reverse()
      .map((r) => ({
        time: new Date(r.timestamp * 1000).toLocaleTimeString(),
        db: Number(r.avg_level ?? 0),
      })),
    stats: {
      avgDb:
        readings.length > 0
          ? Number(
              (
                readings.reduce((sum, r) => sum + Number(r.avg_level ?? 0), 0) / readings.length
              ).toFixed(1),
            )
          : currentDb,
      peakDb: readings.reduce((max, r) => Math.max(max, Number(r.max_level ?? r.avg_level ?? 0)), currentDb),
      breachCount: readings.filter((r) => Number(r.avg_level ?? 0) >= 70).length,
      activeSensors: status.arduino === "online" ? 1 : 0,
    },
  };
}

export interface HistoryPoint {
  time: string;
  db: number;
}

export interface StatsSnapshot {
  avgDb: number;
  peakDb: number;
  breachCount: number;
  uptime: number;
  whoThresholds: Array<{ label: string; limit: number; current: number; period: string }>;
  breachHistory: Array<{ day: string; count: number }>;
}

async function fetchFromEsp(): Promise<SensorPayload> {
  const [status, readings] = await Promise.all([
    acousense.espStatus(),
    acousense.espReadings({ limit: 24 }),
  ]);

  return mapEspReadingsToPayload(status, readings.data ?? []);
}

async function fetchFromSupabase(): Promise<SensorPayload> {
  const { data, error } = await supabase.functions.invoke<SensorPayload>("sensor-data");
  if (error) throw error;
  return data!;
}

export async function fetchSensorData(): Promise<SensorPayload> {
  if (!USE_BACKEND) return fetchFromSupabase();

  try {
    const payload = await fetchFromBackend();
    if (payload.readings.length > 0 || payload.stats.avgDb || payload.stats.peakDb) {
      return payload;
    }
  } catch {
    // Fall through to the ESP32-native API shape.
  }

  return fetchFromEsp();
}

export async function fetchDeviceCards(): Promise<DeviceCardData[]> {
  const devices = await acousense.espDevices();
  const status = await acousense.espStatus().catch(() => null);

  return devices.map((device: EspDevice) => ({
    id: device.mac_address,
    name: device.user_identifier || device.mac_address,
    status: status?.arduino === "online" ? "online" : "offline",
    lastSeen: ageFromTimestamp(device.last_seen),
    threshold: 70,
    signal: rssiToSignal(status?.wifi_rssi),
  }));
}

export async function fetchHistorySeries(hours: number): Promise<HistoryPoint[]> {
  const nowSec = Math.floor(Date.now() / 1000);
  const fromSec = nowSec - hours * 3600;
  const response = await acousense.espReadings({ from: fromSec, to: nowSec, limit: 500 });
  const rows = response.data ?? [];

  return rows.map((row) => ({
    time: new Date(row.timestamp * 1000).toLocaleTimeString("en", {
      hour: "2-digit",
      minute: "2-digit",
    }),
    db: Math.round(Number(row.avg_level ?? 0) * 10) / 10,
  }));
}

export async function fetchStatsSnapshot(): Promise<StatsSnapshot> {
  const nowSec = Math.floor(Date.now() / 1000);
  const fromSec = nowSec - 7 * 24 * 3600;
  const response = await acousense.espReadings({ from: fromSec, to: nowSec, limit: 1000 });
  const rows = response.data ?? [];

  const avgDb = rows.length > 0
    ? rows.reduce((sum, row) => sum + Number(row.avg_level ?? 0), 0) / rows.length
    : 0;
  const peakDb = rows.reduce((max, row) => Math.max(max, Number(row.max_level ?? row.avg_level ?? 0)), 0);
  const breachCount = rows.filter((row) => Number(row.avg_level ?? 0) >= 70).length;

  const byDay = new Map<string, number>();
  for (const row of rows) {
    const key = new Date(row.timestamp * 1000).toLocaleDateString("en", {
      weekday: "short",
      day: "numeric",
    });
    const count = byDay.get(key) ?? 0;
    byDay.set(key, count + (Number(row.avg_level ?? 0) >= 70 ? 1 : 0));
  }

  const recent24h = rows.filter((row) => row.timestamp >= nowSec - 24 * 3600);
  const recentEvening = rows.filter((row) => row.timestamp >= nowSec - 5 * 3600);
  const recentNight = rows.filter((row) => row.timestamp >= nowSec - 8 * 3600);
  const avgFor = (subset: EspReading[]) =>
    subset.length > 0 ? Math.round((subset.reduce((sum, row) => sum + Number(row.avg_level ?? 0), 0) / subset.length) * 10) / 10 : 0;

  return {
    avgDb: Math.round(avgDb * 10) / 10,
    peakDb,
    breachCount,
    uptime: rows.length > 0 ? 99.0 : 0,
    whoThresholds: [
      { label: "Recent 24h", limit: 55, current: avgFor(recent24h), period: "day" },
      { label: "Recent 5h", limit: 65, current: avgFor(recentEvening), period: "evening" },
      { label: "Recent 8h", limit: 70, current: avgFor(recentNight), period: "night" },
    ],
    breachHistory: Array.from(byDay.entries()).map(([day, count]) => ({ day, count })),
  };
}
