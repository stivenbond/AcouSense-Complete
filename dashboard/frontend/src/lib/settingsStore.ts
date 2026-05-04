import { create } from "zustand";
import { persist, createJSONStorage } from "zustand/middleware";

export type ThemeMode = "dark" | "light";
export type MapStyleId = "dark" | "positron" | "satellite" | "terrain";

export interface AppSettings {
  // Monitoring
  demoMode: boolean;
  pollIntervalSec: number;

  // Display / theme
  theme: ThemeMode;
  mapStyle: MapStyleId;

  // Alerts
  emailAlerts: boolean;
  pushAlerts: boolean;
}

const DEFAULT_SETTINGS: AppSettings = {
  demoMode: true,
  pollIntervalSec: 10,
  theme: "dark",
  mapStyle: "dark",
  emailAlerts: true,
  pushAlerts: false,
};

interface SettingsStore extends AppSettings {
  set: <K extends keyof AppSettings>(key: K, value: AppSettings[K]) => void;
  patch: (partial: Partial<AppSettings>) => void;
  reset: () => void;
}

const STORAGE_KEY = "acousense_settings";

export const useSettings = create<SettingsStore>()(
  persist(
    (set) => ({
      ...DEFAULT_SETTINGS,
      set: (key, value) => set({ [key]: value } as Partial<AppSettings>),
      patch: (partial) => set(partial),
      reset: () => set(DEFAULT_SETTINGS),
    }),
    {
      name: STORAGE_KEY,
      storage: createJSONStorage(() => localStorage),
      version: 1,
    },
  ),
);

/** Apply the theme attribute on <html> for CSS-variable theming. */
export function applyTheme(theme: ThemeMode) {
  const root = document.documentElement;
  if (theme === "light") {
    root.setAttribute("data-theme", "light");
  } else {
    root.removeAttribute("data-theme");
  }
}

/** Hydrate the theme attribute on first load. Call once at app startup. */
export function bootstrapTheme() {
  const state = useSettings.getState();
  applyTheme(state.theme);
}
