const corsHeaders = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'authorization, x-client-info, apikey, content-type, x-supabase-client-platform, x-supabase-client-platform-version, x-supabase-client-runtime, x-supabase-client-runtime-version',
};

const baseSensors = [
  { id: "SNS-001", name: "City Park East", lat: 51.505, lng: -0.09, baseDb: 52, status: "online", threshold: 70, signal: 4 },
  { id: "SNS-002", name: "Main St & 5th", lat: 51.51, lng: -0.1, baseDb: 68, status: "online", threshold: 65, signal: 5 },
  { id: "SNS-003", name: "Industrial Zone", lat: 51.515, lng: -0.08, baseDb: 78, status: "warning", threshold: 75, signal: 2 },
  { id: "SNS-004", name: "Riverside Walk", lat: 51.5, lng: -0.12, baseDb: 45, status: "online", threshold: 70, signal: 5 },
  { id: "SNS-005", name: "School District", lat: 51.508, lng: -0.11, baseDb: 58, status: "online", threshold: 55, signal: 3 },
  { id: "SNS-006", name: "Highway Bridge", lat: 51.512, lng: -0.085, baseDb: 82, status: "online", threshold: 80, signal: 4 },
  { id: "SNS-007", name: "Hospital Area", lat: 51.503, lng: -0.095, baseDb: 42, status: "offline", threshold: 50, signal: 0 },
  { id: "SNS-008", name: "Market Square", lat: 51.507, lng: -0.105, baseDb: 63, status: "online", threshold: 65, signal: 4 },
];

function jitter(base: number, range: number): number {
  return Math.round(base + (Math.random() - 0.5) * range * 2);
}

function relativeTime(seconds: number): string {
  if (seconds < 60) return `${seconds} sec ago`;
  const mins = Math.floor(seconds / 60);
  if (mins < 60) return `${mins} min ago`;
  const hrs = Math.floor(mins / 60);
  return `${hrs} hours ago`;
}

declare const Deno: any;

Deno.serve(async (req: Request) => {
  if (req.method === "OPTIONS") {
    return new Response("ok", { headers: corsHeaders });
  }

  try {
    const now = Date.now();

    // Simulate live sensor data with jitter
    const sensors = baseSensors.map((s) => {
      const db = s.status === "offline" ? s.baseDb : jitter(s.baseDb, 5);
      const ageSec = s.status === "offline" ? 7200 : Math.floor(Math.random() * 300) + 10;
      return {
        id: s.id,
        name: s.name,
        lat: s.lat,
        lng: s.lng,
        db,
        status: s.status,
        lastSeen: relativeTime(ageSec),
        threshold: s.threshold,
        signal: s.signal,
      };
    });

    const readings = sensors
      .filter((s) => s.status !== "offline")
      .map((s) => ({
        sensor_id: s.id,
        name: s.name,
        db: s.db,
        age: s.lastSeen,
        status: s.status,
      }));

    // Generate alerts dynamically based on thresholds
    const alerts: { id: number; level: string; message: string; time: string; sensor: string }[] = [];
    let alertId = 1;
    for (const s of sensors) {
      if (s.status === "offline") continue;
      if (s.db > s.threshold) {
        alerts.push({
          id: alertId++,
          level: s.db > s.threshold + 5 ? "critical" : "warning",
          message: `${s.id} at ${s.db} dB — exceeds ${s.threshold} dB limit`,
          time: s.lastSeen,
          sensor: s.id,
        });
      }
    }

    const streams = [
      { name: "MQTT broker", status: "ok", icon: "cloud_done" },
      { name: "Postgres", status: "ok", icon: "database" },
      { name: "InfluxDB", status: "ok", icon: "timeline" },
      { name: "Alert engine", status: alerts.length > 0 ? "ok" : "ok", icon: "notifications_active" },
      { name: "ML pipeline", status: Math.random() > 0.8 ? "warn" : "ok", icon: "model_training" },
    ];

    // Time-series (last N points)
    const timeSeriesPoints = 60;
    const timeSeries = Array.from({ length: timeSeriesPoints }, (_, i) => ({
      time: new Date(now - (timeSeriesPoints - i) * 10000).toISOString(),
      db: Math.round((45 + Math.random() * 30 + Math.sin(i * 0.15) * 8) * 10) / 10,
    }));

    // Stats summary
    const allDbs = sensors.filter((s) => s.status !== "offline").map((s) => s.db);
    const avgDb = Math.round((allDbs.reduce((a, b) => a + b, 0) / allDbs.length) * 10) / 10;
    const peakDb = Math.max(...allDbs);

    const data = {
      timestamp: new Date(now).toISOString(),
      sensors,
      readings,
      alerts,
      streams,
      timeSeries,
      stats: {
        avgDb,
        peakDb,
        breachCount: alerts.length,
        activeSensors: sensors.filter((s) => s.status !== "offline").length,
      },
    };

    return new Response(JSON.stringify(data), {
      headers: { ...corsHeaders, "Content-Type": "application/json" },
      status: 200,
    });
  } catch (error: any) {
    return new Response(JSON.stringify({ error: error.message }), {
      headers: { ...corsHeaders, "Content-Type": "application/json" },
      status: 500,
    });
  }
});
