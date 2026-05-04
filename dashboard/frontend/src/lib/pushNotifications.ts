/**
 * Browser Web Push helpers — registers the service worker, requests permission,
 * subscribes, and posts the subscription to the AcouSense backend.
 *
 * In demo mode, posting is replaced with a console.log and a simulated push
 * notification fired locally via `setTimeout`.
 */

const SUBSCRIBE_ENDPOINT = "/api/notifications/push/subscribe";

function urlBase64ToUint8Array(base64String: string): Uint8Array {
  const padding = "=".repeat((4 - (base64String.length % 4)) % 4);
  const base64 = (base64String + padding).replace(/-/g, "+").replace(/_/g, "/");
  const raw = atob(base64);
  const out = new Uint8Array(raw.length);
  for (let i = 0; i < raw.length; ++i) out[i] = raw.charCodeAt(i);
  return out;
}

const VAPID_KEY = (import.meta.env.VITE_VAPID_PUBLIC_KEY as string | undefined) ?? "";

export async function ensureServiceWorker(): Promise<ServiceWorkerRegistration> {
  if (!("serviceWorker" in navigator)) throw new Error("Service workers unsupported");
  const existing = await navigator.serviceWorker.getRegistration("/sw.js");
  if (existing) return existing;
  return navigator.serviceWorker.register("/sw.js");
}

export interface EnablePushOptions {
  demoMode: boolean;
}

export type EnablePushResult =
  | { ok: true; subscription?: PushSubscription }
  | { ok: false; reason: "unsupported" | "denied" | "error"; error?: Error };

export async function enablePushNotifications({ demoMode }: EnablePushOptions): Promise<EnablePushResult> {
  if (typeof window === "undefined" || !("Notification" in window) || !("PushManager" in window)) {
    return { ok: false, reason: "unsupported" };
  }
  try {
    const permission = await Notification.requestPermission();
    if (permission !== "granted") {
      return { ok: false, reason: "denied" };
    }

    const reg = await ensureServiceWorker();
    await navigator.serviceWorker.ready;

    let subscription: PushSubscription | null = null;
    try {
      const subOpts: PushSubscriptionOptionsInit = { userVisibleOnly: true };
      if (VAPID_KEY) subOpts.applicationServerKey = urlBase64ToUint8Array(VAPID_KEY);
      subscription = await reg.pushManager.subscribe(subOpts);
    } catch (err) {
      // Browsers without a valid VAPID key cannot subscribe to a real push service.
      // In demo mode we still consider this successful because we'll simulate pushes.
      if (!demoMode) throw err;
    }

    if (demoMode) {
      console.log("[AcouSense demo] push subscription:", subscription?.toJSON?.() ?? null);
      window.setTimeout(() => {
        try {
          reg.showNotification("AcouSense — simulated alert", {
            body: "SNS-003 exceeded 75 dB (demo push fired 5s after enabling).",
            icon: "/icon-192.svg",
            badge: "/icon-192.svg",
            tag: "acousense-demo",
            data: { url: "/" },
          });
        } catch (e) {
          console.warn("Demo push notification failed", e);
        }
      }, 5000);
      return { ok: true, subscription: subscription ?? undefined };
    }

    if (subscription) {
      await fetch(SUBSCRIBE_ENDPOINT, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(subscription.toJSON()),
      }).catch((e) => console.warn("Failed to register push subscription", e));
    }

    return { ok: true, subscription: subscription ?? undefined };
  } catch (err) {
    return { ok: false, reason: "error", error: err instanceof Error ? err : new Error(String(err)) };
  }
}

export async function disablePushNotifications(): Promise<void> {
  if (!("serviceWorker" in navigator)) return;
  const reg = await navigator.serviceWorker.getRegistration("/sw.js");
  if (!reg) return;
  const sub = await reg.pushManager.getSubscription();
  if (sub) await sub.unsubscribe().catch(() => {});
}
