import type { AlertItem } from "./alertsStore";

const ENDPOINT = "/api/notifications/email";

export interface EmailAlertOptions {
  to: string;
  alert: AlertItem;
  demoMode: boolean;
}

export async function sendEmailAlert({ to, alert, demoMode }: EmailAlertOptions) {
  const payload = { to, alert };
  if (demoMode) {
    console.log("[AcouSense demo] email alert payload:", payload);
    return { ok: true, demo: true } as const;
  }
  try {
    const res = await fetch(ENDPOINT, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    return { ok: res.ok, status: res.status } as const;
  } catch (err) {
    console.warn("email alert failed", err);
    return { ok: false } as const;
  }
}
