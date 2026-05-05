import React, { useState, useMemo } from "react";
import { useQuery } from "@tanstack/react-query";
import { M3Card } from "@/components/M3Card";
import { M3Chip } from "@/components/M3Chip";
import { M3Button } from "@/components/M3Button";
import { Icon } from "@/components/Icon";
import { weeklyHeatmap, dayLabels } from "@/lib/mockData";
import { fetchHistorySeries } from "@/lib/sensorApi";
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Area, AreaChart } from "recharts";

const ranges = [
  { label: "1 h", hours: 1 },
  { label: "6 h", hours: 6 },
  { label: "24 h", hours: 24 },
  { label: "7 d", hours: 168 },
  { label: "30 d", hours: 720 },
];

const History: React.FC = () => {
  const [selectedRange, setSelectedRange] = useState(2);
  const { data: apiData } = useQuery({
    queryKey: ["history-series", ranges[selectedRange].hours],
    queryFn: () => fetchHistorySeries(ranges[selectedRange].hours),
    retry: 1,
  });
  const data = useMemo(() => apiData ?? [], [apiData]);

  const hours = Array.from({ length: 24 }, (_, i) => i);

  return (
    <div className="flex flex-col gap-4 h-full">
      {/* Time range chips */}
      <div className="flex gap-2 flex-wrap">
        {ranges.map((r, i) => (
          <M3Chip key={r.label} label={r.label} selected={i === selectedRange} onClick={() => setSelectedRange(i)} />
        ))}
      </div>

      {/* Time-series chart */}
      <M3Card variant="elevated" className="p-6 min-h-[300px]" style={{ flex: 2 }}>
        <p className="title-medium text-on-surface mb-4">Noise level over time</p>
        <ResponsiveContainer width="100%" height={260}>
          <AreaChart data={data}>
            <defs>
              <linearGradient id="dbGrad" x1="0" y1="0" x2="0" y2="1">
                <stop offset="5%" stopColor="hsl(136, 76%, 67%)" stopOpacity={0.15} />
                <stop offset="95%" stopColor="hsl(136, 76%, 67%)" stopOpacity={0} />
              </linearGradient>
            </defs>
            <CartesianGrid strokeDasharray="3 3" stroke="hsl(140, 10%, 27%)" />
            <XAxis dataKey="time" stroke="hsl(120, 8%, 58%)" tick={{ fontSize: 11, fontFamily: "Roboto Mono" }} interval="preserveStartEnd" />
            <YAxis stroke="hsl(120, 8%, 58%)" tick={{ fontSize: 11, fontFamily: "Roboto Mono" }} domain={[30, 90]} />
            <Tooltip
              contentStyle={{
                background: "hsl(140, 10%, 16%)",
                border: "1px solid hsl(140, 10%, 27%)",
                borderRadius: "12px",
                color: "hsl(120, 10%, 87%)",
                fontFamily: "Roboto Mono",
              }}
            />
            <Area type="monotone" dataKey="db" stroke="hsl(136, 76%, 67%)" strokeWidth={2} fill="url(#dbGrad)" />
          </AreaChart>
        </ResponsiveContainer>
      </M3Card>

      {/* Weekly heatmap */}
      <M3Card variant="elevated" className="p-6" style={{ flex: 1 }}>
        <p className="title-medium text-on-surface mb-4">Weekly pattern</p>
        <div className="overflow-x-auto">
          <div className="grid gap-[2px]" style={{ gridTemplateColumns: `40px repeat(24, 1fr)`, gridTemplateRows: `20px repeat(7, 24px)` }}>
            {/* Hour labels */}
            <div />
            {hours.map(h => (
              <div key={h} className="text-center label-small text-on-surface-variant font-mono" style={{ fontSize: 10 }}>
                {h}
              </div>
            ))}
            {/* Day rows */}
            {weeklyHeatmap.map((row, day) => (
              <React.Fragment key={day}>
                <div className="label-small text-on-surface-variant font-mono flex items-center" style={{ fontSize: 10 }}>
                  {dayLabels[day]}
                </div>
                {row.map((val, hour) => {
                  const pct = Math.min(100, Math.max(0, ((val - 30) / 50) * 100));
                  return (
                    <div
                      key={hour}
                      className="rounded-[4px]"
                      style={{
                        background: `color-mix(in srgb, hsl(136, 76%, 67%) ${pct}%, hsl(140, 17%, 5%))`,
                      }}
                      title={`${dayLabels[day]} ${hour}:00 — ${val} dB`}
                    />
                  );
                })}
              </React.Fragment>
            ))}
          </div>
        </div>
      </M3Card>

      {/* Export row */}
      <div className="flex justify-end">
        <M3Button
          variant="tonal"
          icon="download"
          onClick={() => {
            const base = (import.meta.env.VITE_API_BASE_URL ?? "").replace(/\/$/, "");
            const target = `${base}/api/readings/export`;
            window.open(target, "_blank", "noopener,noreferrer");
          }}
        >
          Export CSV
        </M3Button>
      </div>
    </div>
  );
};

export default History;
