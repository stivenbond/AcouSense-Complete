import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";

export type AlertLevel = "warning" | "critical";

export interface AlertItem {
  id: string;
  level: AlertLevel;
  message: string;
  sensor: string;
  db: number;
  threshold: number;
  timestamp: number;
  read: boolean;
}

interface AlertsStore {
  active: AlertItem[];
  dismissed: AlertItem[];
  addAlert: (alert: Omit<AlertItem, "id" | "timestamp" | "read"> & { id?: string; timestamp?: number }) => void;
  dismissAlert: (id: string) => void;
  clearAll: () => void;
  markAllRead: () => void;
  unreadCount: () => number;
  syncFromMock: (mock: { id: number | string; level: AlertLevel; message: string; sensor: string; time?: string }[]) => void;
}

export const useAlerts = create<AlertsStore>()(
  persist(
    (set, get) => ({
      active: [],
      dismissed: [],
      addAlert: (alert) => {
        const id = alert.id ?? `${alert.sensor}-${Date.now()}-${Math.random().toString(36).slice(2, 7)}`;
        if (get().active.some((a) => a.id === id) || get().dismissed.some((a) => a.id === id)) return;
        const item: AlertItem = {
          id,
          level: alert.level,
          message: alert.message,
          sensor: alert.sensor,
          db: alert.db,
          threshold: alert.threshold,
          timestamp: alert.timestamp ?? Date.now(),
          read: false,
        };
        set({ active: [item, ...get().active].slice(0, 50) });
      },
      dismissAlert: (id) =>
        set(({ active, dismissed }) => {
          const target = active.find((a) => a.id === id);
          if (!target) return {};
          return {
            active: active.filter((a) => a.id !== id),
            dismissed: [{ ...target, read: true }, ...dismissed].slice(0, 100),
          };
        }),
      clearAll: () =>
        set(({ active, dismissed }) => ({
          active: [],
          dismissed: [...active.map((a) => ({ ...a, read: true })), ...dismissed].slice(0, 100),
        })),
      markAllRead: () => set(({ active }) => ({ active: active.map((a) => ({ ...a, read: true })) })),
      unreadCount: () => get().active.filter((a) => !a.read).length,
      syncFromMock: (mock) => {
        const existing = new Set([...get().active.map((a) => a.id), ...get().dismissed.map((a) => a.id)]);
        const additions: AlertItem[] = [];
        for (const m of mock) {
          const id = String(m.id);
          if (existing.has(id)) continue;
          additions.push({
            id,
            level: m.level,
            message: m.message,
            sensor: m.sensor,
            db: 0,
            threshold: 0,
            timestamp: Date.now(),
            read: false,
          });
        }
        if (additions.length === 0) return;
        set({ active: [...additions, ...get().active].slice(0, 50) });
      },
    }),
    {
      name: "acousense_alerts",
      storage: createJSONStorage(() => localStorage),
      version: 1,
    },
  ),
);
