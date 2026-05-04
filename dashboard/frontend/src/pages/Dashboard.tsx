import React, { useEffect, useMemo } from "react";
import { useQuery } from "@tanstack/react-query";
import { Icon } from "@/components/Icon";
import { M3Card } from "@/components/M3Card";
import { NoiseHeatmap } from "@/components/NoiseHeatmap";
import { fetchSensorData } from "@/lib/sensorApi";
import {
  sensors as fallbackSensors,
  readings as fallbackReadings,
  alerts as fallbackAlerts,
  streams as fallbackStreams,
} from "@/lib/mockData";
import { useSettings } from "@/lib/settingsStore";
import { useAlerts } from "@/lib/alertsStore";

const Dashboard: React.FC = () => {
  const pollIntervalSec = useSettings((s) => s.pollIntervalSec);
  const { data, isLoading, error, refetch } = useQuery({
    queryKey: ["sensor-data"],
    queryFn: fetchSensorData,
    refetchInterval: Math.max(1, pollIntervalSec) * 1000,
    retry: 1,
  });

  const sensors = data?.sensors ?? fallbackSensors;
  const readings = data?.readings ?? fallbackReadings;
  const mockAlerts = data?.alerts ?? fallbackAlerts;
  const streams = data?.streams ?? fallbackStreams;
  const activeSensors = data?.stats?.activeSensors ?? sensors.filter((s) => s.status !== "offline").length;
  const latestDb = useMemo(() => Math.round(Math.max(...readings.map((r) => r.db), 0) * 10) / 10, [readings]);
  const isBreaching = sensors.some((s) => s.db > s.threshold);
  const isLive = !!data && !error;

  // Push alerts into the global store (used by the bell drawer)
  const syncFromMock = useAlerts((s) => s.syncFromMock);
  const addAlert = useAlerts((s) => s.addAlert);
  useEffect(() => {
    syncFromMock(mockAlerts);
  }, [mockAlerts, syncFromMock]);
  useEffect(() => {
    for (const s of sensors) {
      if (s.db > s.threshold) {
        addAlert({
          id: `breach-${s.id}-${Math.floor(Date.now() / 60000)}`,
          level: s.db > s.threshold + 5 ? "critical" : "warning",
          message: `${s.id} exceeded ${s.threshold} dB (${s.db.toFixed(1)} dB)`,
          sensor: s.id,
          db: s.db,
          threshold: s.threshold,
        });
      }
    }
  }, [sensors, addAlert]);

  const getDbColor = (db: number) => {
    if (db < 55) return "text-primary";
    if (db < 75) return "text-tertiary";
    return "text-destructive";
  };

  return (
    <div className="dashboard">
      {/* Hero Strip */}
      <div style={{ gridArea: "hero" }}>
        <M3Card variant="filled" className="flex items-center gap-8 px-8 py-6 flex-wrap">
          <div className="flex-1 min-w-[200px]">
            <div className="flex items-center gap-2 mb-1">
              <p className="label-medium text-on-surface-variant">Current noise level</p>
              {isLive && (
                <span className="inline-flex items-center gap-1 px-2 py-0.5 rounded-full bg-primary/15 label-small text-primary">
                  <span className="w-1.5 h-1.5 rounded-full bg-primary animate-pulse-live" />
                  Live
                </span>
              )}
              {isLoading && <span className="label-small text-on-surface-variant">Loading...</span>}
            </div>
            <p className={`display-large ${isLive ? "animate-pulse-live" : ""} ${getDbColor(latestDb)}`}>
              {latestDb} <span className="display-small">dB</span>
            </p>
          </div>
          <div className="flex gap-8 flex-wrap">
            <div>
              <p className="label-medium text-on-surface-variant">Active sensors</p>
              <p className="headline-medium text-on-surface font-mono">{activeSensors}</p>
            </div>
            <div>
              <p className="label-medium text-on-surface-variant">WHO guideline</p>
              <p className={`headline-medium ${isBreaching ? "text-destructive" : "text-primary"}`}>
                {isBreaching ? "Exceeded" : "Within limits"}
              </p>
            </div>
          </div>
          <button
            onClick={() => refetch()}
            className="w-10 h-10 rounded-full bg-secondary-container flex items-center justify-center hover:brightness-110 transition-all"
            aria-label="Refresh"
          >
            <Icon name="refresh" className="text-secondary-container-foreground" />
          </button>
        </M3Card>
      </div>

      {/* Map Section */}
      <div style={{ gridArea: "map" }}>
        <M3Card variant="elevated" className="h-full flex flex-col min-h-[440px]">
          <div className="flex items-center justify-between px-6 py-4">
            <span className="title-medium text-on-surface">Noise heatmap</span>
            <span className={`label-small ${isLive ? "text-primary" : "text-on-surface-variant"}`}>
              {isLive ? "Live" : "Offline"}
            </span>
          </div>
          <div className="flex-1 relative bg-surface-container-lowest rounded-b-[var(--shape-lg)] overflow-hidden">
            <NoiseHeatmap sensors={sensors} />
          </div>
        </M3Card>
      </div>

      {/* Sidebar */}
      <div className="flex flex-col gap-4" style={{ gridArea: "sidebar" }}>
        <M3Card variant="filled" className="flex-1 flex flex-col min-h-0 overflow-hidden">
          <div className="flex items-center justify-between px-6 py-4">
            <span className="title-medium text-on-surface">Live readings</span>
            <span className="label-small text-primary">{readings.length} active</span>
          </div>
          <div className="flex-1 overflow-y-auto px-2">
            {readings.map((r) => (
              <div
                key={r.sensor_id}
                className="flex items-center px-4 py-3 hover:bg-on-surface/[0.05] rounded-[var(--shape-md)] transition-colors"
              >
                <div className="flex-1 min-w-0">
                  <p className="body-medium text-on-surface truncate">{r.sensor_id}</p>
                  <p className="label-small text-on-surface-variant">Updated {r.age}</p>
                </div>
                <span className={`font-mono title-medium ${getDbColor(r.db)}`}>{r.db} dB</span>
              </div>
            ))}
          </div>
        </M3Card>

        <M3Card variant="filled" className="flex-1 flex flex-col min-h-[180px] overflow-hidden">
          <div className="flex items-center justify-between px-6 py-4">
            <span className="title-medium text-on-surface">Active alerts</span>
            {mockAlerts.length > 0 && (
              <span className="bg-destructive text-destructive-foreground label-small px-2.5 py-0.5 rounded-full">
                {mockAlerts.length}
              </span>
            )}
          </div>
          <div className="flex-1 overflow-y-auto px-2">
            {mockAlerts.length === 0 ? (
              <div className="flex flex-col items-center justify-center py-8 gap-2">
                <Icon name="check_circle" className="text-primary" size="lg" />
                <p className="body-medium text-on-surface-variant">No active alerts</p>
              </div>
            ) : (
              mockAlerts.map((alert) => (
                <div
                  key={alert.id}
                  className="flex items-start gap-3 px-4 py-3 hover:bg-on-surface/[0.05] rounded-[var(--shape-md)] transition-colors"
                >
                  <Icon
                    name={alert.level === "critical" ? "error" : "warning"}
                    size="sm"
                    className={alert.level === "critical" ? "text-destructive" : "text-tertiary"}
                  />
                  <div className="flex-1 min-w-0">
                    <p className="body-small text-on-surface">{alert.message}</p>
                    <p className="label-small text-on-surface-variant">{alert.time}</p>
                  </div>
                </div>
              ))
            )}
          </div>
        </M3Card>
      </div>

      {/* Network Status Strip */}
      <div style={{ gridArea: "net" }}>
        <M3Card variant="outlined" className="flex items-center gap-4 px-6 py-3 flex-wrap">
          <span className="label-medium text-on-surface-variant">Data streams</span>
          {streams.map((s) => (
            <span
              key={s.name}
              className={`inline-flex items-center gap-1.5 h-8 px-3 rounded-full label-medium border ${
                s.status === "ok"
                  ? "border-primary/30 text-primary bg-primary/10"
                  : "border-tertiary/30 text-tertiary bg-tertiary/10"
              }`}
            >
              <Icon name={s.status === "ok" ? "check_circle" : "sync_problem"} size="sm" />
              {s.name}
            </span>
          ))}
        </M3Card>
      </div>

      <style>{`
        .dashboard {
          display: grid;
          grid-template-areas:
            "hero hero"
            "map sidebar"
            "net net";
          grid-template-columns: 1fr 360px;
          grid-template-rows: auto 1fr auto;
          gap: 16px;
          height: 100%;
        }
        @media (max-width: 899px) {
          .dashboard {
            grid-template-areas: "hero" "map" "sidebar" "net";
            grid-template-columns: 1fr;
            grid-template-rows: auto auto auto auto;
            height: auto;
          }
        }
      `}</style>
    </div>
  );
};

export default Dashboard;
