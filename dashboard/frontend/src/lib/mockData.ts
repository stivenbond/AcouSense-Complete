// Mock data for AcouSense dashboard

export const sensors = [
  { id: "SNS-001", name: "City Park East", lat: 51.505, lng: -0.09, db: 52, status: "online" as const, lastSeen: "2 min ago", threshold: 70, signal: 4 },
  { id: "SNS-002", name: "Main St & 5th", lat: 51.51, lng: -0.1, db: 68, status: "online" as const, lastSeen: "1 min ago", threshold: 65, signal: 5 },
  { id: "SNS-003", name: "Industrial Zone", lat: 51.515, lng: -0.08, db: 78, status: "warning" as const, lastSeen: "5 min ago", threshold: 75, signal: 2 },
  { id: "SNS-004", name: "Riverside Walk", lat: 51.5, lng: -0.12, db: 45, status: "online" as const, lastSeen: "1 min ago", threshold: 70, signal: 5 },
  { id: "SNS-005", name: "School District", lat: 51.508, lng: -0.11, db: 58, status: "online" as const, lastSeen: "3 min ago", threshold: 55, signal: 3 },
  { id: "SNS-006", name: "Highway Bridge", lat: 51.512, lng: -0.085, db: 82, status: "online" as const, lastSeen: "1 min ago", threshold: 80, signal: 4 },
  { id: "SNS-007", name: "Hospital Area", lat: 51.503, lng: -0.095, db: 42, status: "offline" as const, lastSeen: "2 hours ago", threshold: 50, signal: 0 },
  { id: "SNS-008", name: "Market Square", lat: 51.507, lng: -0.105, db: 63, status: "online" as const, lastSeen: "1 min ago", threshold: 65, signal: 4 },
];

export const readings = sensors
  .filter(s => s.status !== "offline")
  .map(s => ({ sensor_id: s.id, name: s.name, db: s.db, age: s.lastSeen, status: s.status }));

export const alerts = [
  { id: 1, level: "critical" as const, message: "SNS-003 exceeded 75 dB for 15 min", time: "3 min ago", sensor: "SNS-003" },
  { id: 2, level: "critical" as const, message: "SNS-006 peak at 82 dB — highway spike", time: "8 min ago", sensor: "SNS-006" },
  { id: 3, level: "warning" as const, message: "SNS-005 approaching school zone limit", time: "12 min ago", sensor: "SNS-005" },
  { id: 4, level: "warning" as const, message: "SNS-002 sustained 68 dB evening noise", time: "20 min ago", sensor: "SNS-002" },
];

export const streams = [
  { name: "MQTT broker", status: "ok" as const, icon: "cloud_done" },
  { name: "Postgres", status: "ok" as const, icon: "database" },
  { name: "InfluxDB", status: "ok" as const, icon: "timeline" },
  { name: "Alert engine", status: "ok" as const, icon: "notifications_active" },
  { name: "ML pipeline", status: "warn" as const, icon: "model_training" },
];

// Generate time-series data for history charts
export function generateTimeSeries(hours: number) {
  const now = Date.now();
  const points = Math.min(hours * 6, 200);
  const interval = (hours * 3600 * 1000) / points;
  return Array.from({ length: points }, (_, i) => ({
    time: new Date(now - (points - i) * interval),
    db: 40 + Math.random() * 35 + Math.sin(i * 0.1) * 10,
  }));
}

// Weekly heatmap: 7 days × 24 hours
export const weeklyHeatmap = Array.from({ length: 7 }, (_, day) =>
  Array.from({ length: 24 }, (_, hour) => {
    const base = hour >= 7 && hour <= 22 ? 50 : 35;
    const rush = (hour >= 7 && hour <= 9) || (hour >= 17 && hour <= 19) ? 15 : 0;
    const weekend = day >= 5 ? -5 : 0;
    return Math.round(base + rush + weekend + Math.random() * 10);
  })
);

export const dayLabels = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];

// Stats
export const stats = {
  avgDb: 58.3,
  peakDb: 82,
  breachCount: 14,
  uptime: 97.2,
  whoThresholds: [
    { label: "Daytime (7am–7pm)", limit: 55, current: 62, period: "day" },
    { label: "Evening (7pm–11pm)", limit: 65, current: 58, period: "evening" },
    { label: "Night (11pm–7am)", limit: 70, current: 44, period: "night" },
  ],
  breachHistory: Array.from({ length: 14 }, (_, i) => ({
    day: new Date(Date.now() - (13 - i) * 86400000).toLocaleDateString("en", { weekday: "short", day: "numeric" }),
    count: Math.floor(Math.random() * 5),
  })),
};
