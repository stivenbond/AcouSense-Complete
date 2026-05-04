import React from "react";
import { M3Card } from "@/components/M3Card";
import { Icon } from "@/components/Icon";
import { stats } from "@/lib/mockData";
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Cell } from "recharts";

const kpis = [
  { label: "Average dB", value: stats.avgDb.toFixed(1), accent: "hsl(136, 76%, 67%)", icon: "equalizer" },
  { label: "Peak dB", value: stats.peakDb.toString(), accent: "hsl(0, 100%, 85%)", icon: "show_chart" },
  { label: "Breaches (7d)", value: stats.breachCount.toString(), accent: "hsl(40, 82%, 68%)", icon: "warning" },
  { label: "Uptime", value: `${stats.uptime}%`, accent: "hsl(152, 33%, 72%)", icon: "uptime" },
];

const Stats: React.FC = () => {
  return (
    <div className="flex flex-col gap-4">
      {/* KPI Grid */}
      <div className="grid grid-cols-2 lg:grid-cols-4 gap-4">
        {kpis.map(kpi => (
          <M3Card key={kpi.label} variant="elevated" className="relative overflow-visible">
            <div className="absolute top-4 left-0 w-1 rounded-r-sm" style={{ height: "calc(100% - 32px)", background: kpi.accent }} />
            <div className="p-6 pl-5 flex flex-col gap-2">
              <span className="display-small" style={{ color: kpi.accent, fontFamily: "Roboto Mono" }}>{kpi.value}</span>
              <span className="label-large text-on-surface-variant">{kpi.label}</span>
            </div>
          </M3Card>
        ))}
      </div>

      {/* WHO Comparison */}
      <M3Card variant="elevated" className="p-6">
        <p className="title-large text-on-surface mb-6">WHO guideline comparison</p>
        <div className="flex flex-col gap-4">
          {stats.whoThresholds.map((t, i) => {
            const breached = t.current > t.limit;
            const pct = Math.min(100, (t.current / 100) * 100);
            const limitPct = (t.limit / 100) * 100;
            return (
              <div key={i}>
                <div className="flex items-center justify-between mb-2">
                  <span className="body-medium text-on-surface">{t.label}</span>
                  <div className="flex items-center gap-3">
                    <span className="font-mono label-large" style={{ color: breached ? "hsl(0,100%,85%)" : "hsl(136,76%,67%)" }}>
                      {t.current} dB
                    </span>
                    <span className="body-small text-on-surface-variant">/ {t.limit} dB</span>
                  </div>
                </div>
                <div className="relative h-2 rounded-full bg-surface-container-highest overflow-hidden">
                  <div
                    className="h-full rounded-full transition-all"
                    style={{
                      width: `${pct}%`,
                      background: breached ? "hsl(0, 100%, 85%)" : "hsl(136, 76%, 67%)",
                    }}
                  />
                  <div
                    className="absolute top-0 bottom-0 w-0.5 bg-on-surface-variant"
                    style={{ left: `${limitPct}%` }}
                  />
                </div>
                {breached && (
                  <div className="mt-1.5 inline-flex items-center gap-1 px-2 py-0.5 rounded-full bg-destructive/20">
                    <Icon name="error" size="sm" className="text-destructive" />
                    <span className="label-small text-destructive">Exceeds limit</span>
                  </div>
                )}
                {i < stats.whoThresholds.length - 1 && <div className="border-t border-outline-variant mt-4" />}
              </div>
            );
          })}
        </div>
      </M3Card>

      {/* Breach Timeline */}
      <M3Card variant="elevated" className="p-6 min-h-[240px]">
        <p className="title-large text-on-surface mb-4">Breach events</p>
        <ResponsiveContainer width="100%" height={180}>
          <BarChart data={stats.breachHistory}>
            <CartesianGrid strokeDasharray="3 3" stroke="hsl(140, 10%, 27%)" />
            <XAxis dataKey="day" stroke="hsl(120, 8%, 58%)" tick={{ fontSize: 11, fontFamily: "Roboto Mono" }} />
            <YAxis stroke="hsl(120, 8%, 58%)" tick={{ fontSize: 11, fontFamily: "Roboto Mono" }} />
            <Tooltip
              contentStyle={{
                background: "hsl(140, 10%, 16%)",
                border: "1px solid hsl(140, 10%, 27%)",
                borderRadius: "12px",
                color: "hsl(120, 10%, 87%)",
                fontFamily: "Roboto Mono",
              }}
            />
            <Bar dataKey="count" radius={[4, 4, 0, 0]}>
              {stats.breachHistory.map((entry, i) => (
                <Cell key={i} fill={entry.count > 0 ? "hsl(0, 100%, 85%)" : "hsl(140, 6%, 20%)"} />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </M3Card>
    </div>
  );
};

export default Stats;
