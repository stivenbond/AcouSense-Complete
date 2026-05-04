import React, { useEffect, useMemo, useState } from "react";
import { M3Card } from "@/components/M3Card";
import { M3Button } from "@/components/M3Button";
import { Icon } from "@/components/Icon";
import {
  applyTheme,
  useSettings,
  type AppSettings,
  type MapStyleId,
  type ThemeMode,
} from "@/lib/settingsStore";
import { useUser } from "@/lib/userStore";
import {
  disablePushNotifications,
  enablePushNotifications,
} from "@/lib/pushNotifications";
import { MAP_STYLE_LABELS } from "@/lib/mapStyles";
import { useToast } from "@/lib/toastStore";

const Settings: React.FC = () => {
  const settings = useSettings();
  const setSetting = useSettings((s) => s.set);
  const showToast = useToast((s) => s.show);
  const email = useUser((s) => s.email);
  const signOut = useUser((s) => s.signOut);

  // Local draft so the user can validate before persisting via "Save".
  const [draft, setDraft] = useState<AppSettings>(extractDraft(settings));
  const [errors, setErrors] = useState<Partial<Record<keyof AppSettings, string>>>({});
  const [pushWarning, setPushWarning] = useState<string | null>(null);

  // Keep the draft in sync if the store rehydrates (e.g. after first paint).
  useEffect(() => {
    setDraft(extractDraft(useSettings.getState()));
  }, []);

  const setField = <K extends keyof AppSettings>(key: K, value: AppSettings[K]) => {
    setDraft((d) => ({ ...d, [key]: value }));
    setErrors((e) => ({ ...e, [key]: undefined }));
  };

  // Theme switches preview live so users see the change immediately.
  // When toggling between dark/light, also flip the matching CARTO base map
  // (dark-matter ↔ positron) so the dashboard stays readable. Satellite and
  // Terrain selections are preserved.
  useEffect(() => {
    applyTheme(draft.theme);
    setDraft((d) => {
      if (d.theme === "dark" && d.mapStyle === "positron") return { ...d, mapStyle: "dark" };
      if (d.theme === "light" && d.mapStyle === "dark") return { ...d, mapStyle: "positron" };
      return d;
    });
  }, [draft.theme]);

  // Persist map style on change so the dashboard updates without reload.
  useEffect(() => {
    if (settings.mapStyle !== draft.mapStyle) {
      setSetting("mapStyle", draft.mapStyle);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [draft.mapStyle]);

  const validate = (d: AppSettings): Partial<Record<keyof AppSettings, string>> => {
    const errs: Partial<Record<keyof AppSettings, string>> = {};
    if (![5, 10, 30, 60].includes(d.pollIntervalSec)) errs.pollIntervalSec = "Choose a valid interval";
    return errs;
  };

  const handleSave = () => {
    const v = validate(draft);
    setErrors(v);
    if (Object.values(v).some(Boolean)) {
      showToast("Fix invalid fields before saving", "error");
      return;
    }
    useSettings.getState().patch(draft);
    applyTheme(draft.theme);
    showToast("Configuration saved");
  };

  const handleTogglePush = async (next: boolean) => {
    if (!next) {
      setField("pushAlerts", false);
      await disablePushNotifications();
      return;
    }
    const result = await enablePushNotifications({ demoMode: draft.demoMode });
    if (result.ok) {
      setField("pushAlerts", true);
      setPushWarning(null);
    } else {
      setField("pushAlerts", false);
      if (result.reason === "denied") {
        setPushWarning(
          "Push notifications were blocked. Please enable them in your browser settings.",
        );
      } else if (result.reason === "unsupported") {
        setPushWarning("This browser does not support push notifications.");
      } else {
        setPushWarning(`Could not enable push notifications: ${result.error?.message ?? "unknown error"}`);
      }
    }
  };

  const mapStyles = useMemo(
    () => Object.entries(MAP_STYLE_LABELS) as [MapStyleId, string][],
    [],
  );

  return (
    <div className="flex flex-col gap-4 max-w-3xl mx-auto w-full" style={{ paddingBottom: "8rem" }}>
      <SectionCard title="Monitoring">
        <SettingRow label="Demo mode" desc="Use simulated sensor data">
          <Toggle checked={draft.demoMode} onChange={(v) => setField("demoMode", v)} />
        </SettingRow>
        <SettingRow label="Polling interval" desc="How often the dashboard refreshes">
          <select
            value={draft.pollIntervalSec}
            onChange={(e) => setField("pollIntervalSec", Number(e.target.value))}
            className={inputCls(errors.pollIntervalSec)}
          >
            <option value={5}>5 seconds</option>
            <option value={10}>10 seconds</option>
            <option value={30}>30 seconds</option>
            <option value={60}>60 seconds</option>
          </select>
        </SettingRow>
        {errors.pollIntervalSec && <FieldError msg={errors.pollIntervalSec} />}
      </SectionCard>

      <SectionCard title="Alerts">
        <SettingRow label="Email notifications" desc="Receive alerts via email">
          <Toggle checked={draft.emailAlerts} onChange={(v) => setField("emailAlerts", v)} />
        </SettingRow>
        {draft.emailAlerts && (
          <div className="px-6 py-4 border-t border-outline-variant flex flex-col gap-3">
            {email ? (
              <div className="flex items-center justify-between gap-3 flex-wrap">
                <div className="flex items-center gap-2">
                  <Icon name="mail" size="sm" className="text-primary" />
                  <span className="body-medium text-on-surface truncate">{email}</span>
                </div>
                <button
                  onClick={() => {
                    void signOut();
                    showToast("Signed out");
                  }}
                  className="label-medium text-destructive hover:bg-destructive/10 px-3 py-1.5 rounded-full transition-colors"
                >
                  Sign out
                </button>
              </div>
            ) : (
              <p className="body-small text-on-surface-variant">
                Sign in to receive email alerts at your account address.
              </p>
            )}
          </div>
        )}
        <SettingRow label="Push notifications" desc="Browser push for critical alerts">
          <Toggle checked={draft.pushAlerts} onChange={handleTogglePush} />
        </SettingRow>
        {pushWarning && (
          <div className="px-6 py-3 border-t border-outline-variant flex items-center gap-2">
            <Icon name="warning" size="sm" className="text-tertiary" />
            <p className="body-small text-tertiary">{pushWarning}</p>
          </div>
        )}
      </SectionCard>

      <SectionCard title="Display">
        <SettingRow label="Theme" desc="Switch between dark and light themes">
          <ThemeToggle value={draft.theme} onChange={(v) => setField("theme", v)} />
        </SettingRow>
        <SettingRow label="Map style" desc="Base map for the dashboard heatmap">
          <select
            value={draft.mapStyle}
            onChange={(e) => setField("mapStyle", e.target.value as MapStyleId)}
            className={inputCls()}
          >
            {mapStyles.map(([id, label]) => (
              <option key={id} value={id}>
                {label}
              </option>
            ))}
          </select>
        </SettingRow>
      </SectionCard>

      <SectionCard title="About">
        <SettingRow label="Version">
          <span className="font-mono label-medium text-on-surface-variant">2.4.1</span>
        </SettingRow>
        <SettingRow label="License">
          <span className="label-medium text-on-surface-variant">MIT</span>
        </SettingRow>
      </SectionCard>

      <div className="flex justify-end pt-2">
        <M3Button onClick={handleSave} icon="save">
          Save configuration
        </M3Button>
      </div>
    </div>
  );
};

function extractDraft(s: AppSettings): AppSettings {
  return {
    demoMode: s.demoMode,
    pollIntervalSec: s.pollIntervalSec,
    theme: s.theme,
    mapStyle: s.mapStyle,
    emailAlerts: s.emailAlerts,
    pushAlerts: s.pushAlerts,
  };
}

const SectionCard: React.FC<{ title: string; children: React.ReactNode }> = ({ title, children }) => (
  <M3Card variant="outlined" className="overflow-hidden">
    <div className="px-6 py-4 border-b border-outline-variant">
      <span className="title-medium text-on-surface">{title}</span>
    </div>
    <div className="divide-y divide-outline-variant">{children}</div>
  </M3Card>
);

const SettingRow: React.FC<{ label: string; desc?: string; children: React.ReactNode }> = ({
  label,
  desc,
  children,
}) => (
  <div className="flex items-center justify-between px-6 py-4 gap-4 flex-wrap">
    <div className="min-w-0">
      <p className="body-large text-on-surface">{label}</p>
      {desc && <p className="body-small text-on-surface-variant">{desc}</p>}
    </div>
    <div className="shrink-0">{children}</div>
  </div>
);

const FieldError: React.FC<{ msg: string }> = ({ msg }) => (
  <div className="px-6 pb-3 -mt-1">
    <p className="label-small text-destructive">{msg}</p>
  </div>
);

const Toggle: React.FC<{ checked: boolean; onChange: (v: boolean) => void; ariaLabel?: string }> = ({
  checked,
  onChange,
  ariaLabel,
}) => (
  <button
    type="button"
    onClick={() => onChange(!checked)}
    aria-pressed={checked}
    aria-label={ariaLabel}
    className={`relative w-[52px] h-8 rounded-full transition-colors focus:outline focus:outline-2 focus:outline-primary ${
      checked ? "bg-primary" : "bg-surface-container-highest"
    }`}
  >
    <div
      className={`absolute top-1 w-6 h-6 rounded-full transition-all shadow-md ${
        checked ? "left-[24px] bg-primary-foreground" : "left-1 bg-outline"
      }`}
    />
  </button>
);

const ThemeToggle: React.FC<{ value: ThemeMode; onChange: (v: ThemeMode) => void }> = ({ value, onChange }) => (
  <div className="inline-flex items-center bg-surface-container-highest rounded-full p-1 gap-1">
    {(["dark", "light"] as ThemeMode[]).map((mode) => (
      <button
        key={mode}
        onClick={() => onChange(mode)}
        className={`px-4 h-8 rounded-full label-medium inline-flex items-center gap-1.5 transition-colors ${
          value === mode ? "bg-primary text-primary-foreground" : "text-on-surface-variant hover:text-on-surface"
        }`}
      >
        <Icon name={mode === "dark" ? "dark_mode" : "light_mode"} size="sm" />
        {mode === "dark" ? "Dark" : "Light"}
      </button>
    ))}
  </div>
);

function inputCls(error?: string, extra = "") {
  return [
    "h-10 px-3 rounded-[var(--shape-xs)] bg-surface-container-highest text-on-surface body-medium border outline-none appearance-none",
    error ? "border-destructive focus:border-destructive" : "border-outline focus:border-primary",
    extra,
  ]
    .filter(Boolean)
    .join(" ");
}

export default Settings;
