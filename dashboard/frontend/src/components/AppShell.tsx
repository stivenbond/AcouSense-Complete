import React, { useEffect, useMemo, useRef, useState } from "react";
import { Link, useLocation, useNavigate } from "react-router-dom";
import { Icon } from "./Icon";
import { cn } from "@/lib/utils";
import { useAlerts, type AlertItem } from "@/lib/alertsStore";
import { useUser } from "@/lib/userStore";

const navItems = [
  { href: "/", icon: "dashboard", label: "Dashboard" },
  { href: "/history", icon: "timeline", label: "History" },
  { href: "/stats", icon: "analytics", label: "Statistics" },
  { href: "/devices", icon: "sensors", label: "Devices" },
  { href: "/settings", icon: "settings", label: "Settings" },
];

export const AppShell: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const location = useLocation();
  const navigate = useNavigate();
  const currentLabel = navItems.find((n) => location.pathname === n.href)?.label ?? "AcouSense";

  const active = useAlerts((s) => s.active);
  const dismissed = useAlerts((s) => s.dismissed);
  const dismissAlert = useAlerts((s) => s.dismissAlert);
  const clearAll = useAlerts((s) => s.clearAll);
  const markAllRead = useAlerts((s) => s.markAllRead);
  const unreadCount = active.filter((a) => !a.read).length;

  const [bellOpen, setBellOpen] = useState(false);
  const [accountOpen, setAccountOpen] = useState(false);
  const [aboutOpen, setAboutOpen] = useState(false);
  const accountRef = useRef<HTMLDivElement | null>(null);

  const email = useUser((s) => s.email);
  const signOut = useUser((s) => s.signOut);

  useEffect(() => {
    if (bellOpen) markAllRead();
  }, [bellOpen, markAllRead]);

  // Close account dropdown on outside click
  useEffect(() => {
    if (!accountOpen) return;
    const handler = (e: MouseEvent) => {
      if (!accountRef.current?.contains(e.target as Node)) setAccountOpen(false);
    };
    document.addEventListener("mousedown", handler);
    return () => document.removeEventListener("mousedown", handler);
  }, [accountOpen]);

  return (
    <div className="app-shell">
      <nav className="nav-drawer">
        <div className="nav-drawer__header">
          <Icon name="sensors" className="text-primary" />
          <span className="title-large text-on-surface drawer-title">AcouSense</span>
        </div>
        {navItems.map((item) => {
          const isActive = location.pathname === item.href;
          return (
            <Link
              key={item.href}
              to={item.href}
              className={cn("nav-item", isActive && "nav-item--active")}
            >
              <Icon
                name={item.icon}
                filled={isActive}
                className={isActive ? "text-secondary-container-foreground" : "text-on-surface-variant"}
              />
              <span
                className={cn(
                  "label-large nav-item-label",
                  isActive ? "text-secondary-container-foreground" : "text-on-surface-variant",
                )}
              >
                {item.label}
              </span>
              {item.href === "/" && unreadCount > 0 && (
                <span className="ml-auto bg-destructive text-destructive-foreground text-[11px] font-medium rounded-full w-5 h-5 flex items-center justify-center">
                  {unreadCount}
                </span>
              )}
            </Link>
          );
        })}
      </nav>

      <div className="content-area">
        <header className="top-app-bar">
          <span className="title-large flex-1 text-on-surface">{currentLabel}</span>

          <button
            className="w-10 h-10 rounded-full flex items-center justify-center hover:bg-on-surface/[0.08] transition-colors relative focus:outline focus:outline-2 focus:outline-primary"
            onClick={() => setBellOpen(true)}
            aria-label={`Notifications (${unreadCount} unread)`}
          >
            <Icon name="notifications" className="text-on-surface-variant" />
            {unreadCount > 0 && (
              <span className="absolute -top-0.5 -right-0.5 min-w-[18px] h-[18px] px-1 rounded-full bg-destructive text-destructive-foreground text-[10px] font-bold flex items-center justify-center">
                {unreadCount}
              </span>
            )}
          </button>

          <div ref={accountRef} className="relative">
            <button
              className="w-10 h-10 rounded-full flex items-center justify-center hover:bg-on-surface/[0.08] transition-colors focus:outline focus:outline-2 focus:outline-primary"
              onClick={() => setAccountOpen((v) => !v)}
              aria-label="Account menu"
            >
              <Icon name="account_circle" className="text-on-surface-variant" />
            </button>
            {accountOpen && (
              <div className="absolute right-0 top-12 w-64 bg-surface-container-high border border-outline-variant rounded-[var(--shape-md)] shadow-xl py-1 z-50">
                <div className="px-4 py-3 border-b border-outline-variant">
                  <p className="label-small text-on-surface-variant">
                    {email ? "Signed in as" : "Not signed in"}
                  </p>
                  {email && <p className="body-medium text-on-surface truncate">{email}</p>}
                </div>
                {!email && (
                  <button
                    onClick={() => {
                      setAccountOpen(false);
                      navigate("/auth/signin");
                    }}
                    className="w-full text-left px-4 py-3 flex items-center gap-3 hover:bg-on-surface/[0.05] transition-colors body-medium text-on-surface"
                  >
                    <Icon name="login" size="sm" className="text-primary" />
                    Sign in
                  </button>
                )}
                <button
                  onClick={() => {
                    setAccountOpen(false);
                    navigate("/settings");
                  }}
                  className="w-full text-left px-4 py-3 flex items-center gap-3 hover:bg-on-surface/[0.05] transition-colors body-medium text-on-surface"
                >
                  <Icon name="settings" size="sm" className="text-on-surface-variant" />
                  Go to Settings
                </button>
                <button
                  onClick={() => {
                    setAccountOpen(false);
                    setAboutOpen(true);
                  }}
                  className="w-full text-left px-4 py-3 flex items-center gap-3 hover:bg-on-surface/[0.05] transition-colors body-medium text-on-surface"
                >
                  <Icon name="info" size="sm" className="text-on-surface-variant" />
                  About AcouSense
                </button>
                {email && (
                  <button
                    onClick={() => {
                      void signOut();
                      setAccountOpen(false);
                      navigate("/auth/signin");
                    }}
                    className="w-full text-left px-4 py-3 flex items-center gap-3 hover:bg-on-surface/[0.05] transition-colors body-medium text-destructive border-t border-outline-variant"
                  >
                    <Icon name="logout" size="sm" />
                    Sign out
                  </button>
                )}
              </div>
            )}
          </div>
        </header>
        <main className="main-content">{children}</main>
      </div>

      {/* Bottom toolbar (mobile) */}
      <nav className="bottom-toolbar">
        {navItems.slice(0, 5).map((item) => {
          const isActive = location.pathname === item.href;
          return (
            <Link
              key={item.href}
              to={item.href}
              className={cn("bottom-tab", isActive && "bottom-tab--active")}
            >
              <Icon name={item.icon} filled={isActive} size="sm" />
              <span className="label-small">{item.label}</span>
            </Link>
          );
        })}
      </nav>

      <NotificationDrawer
        open={bellOpen}
        onClose={() => setBellOpen(false)}
        active={active}
        dismissed={dismissed}
        onDismiss={dismissAlert}
        onClearAll={clearAll}
      />

      {aboutOpen && <AboutModal onClose={() => setAboutOpen(false)} />}

      <style>{`
        .app-shell {
          display: grid;
          grid-template-columns: 256px 1fr;
          grid-template-rows: 1fr;
          height: 100vh;
          overflow: hidden;
          background: hsl(var(--background));
        }
        .nav-drawer {
          grid-column: 1;
          grid-row: 1;
          width: 256px;
          height: 100vh;
          background: hsl(var(--surface-container-low));
          display: flex;
          flex-direction: column;
          gap: 2px;
          padding: 12px 0;
          overflow-y: auto;
        }
        .nav-drawer__header {
          display: flex;
          align-items: center;
          gap: 12px;
          padding: 16px 16px 20px;
        }
        .nav-item {
          position: relative;
          display: flex;
          align-items: center;
          gap: 12px;
          padding: 14px 24px 14px 16px;
          margin: 0 12px;
          border-radius: var(--shape-full);
          text-decoration: none;
          transition: background 100ms var(--motion-standard);
        }
        .nav-item--active {
          background: hsl(var(--secondary-container));
        }
        .nav-item:not(.nav-item--active):hover {
          background: hsl(var(--on-surface) / 0.08);
        }
        .nav-item:focus-visible {
          outline: 2px solid hsl(var(--primary));
          outline-offset: -2px;
        }
        .content-area {
          grid-column: 2;
          display: flex;
          flex-direction: column;
          height: 100vh;
          overflow: hidden;
        }
        .top-app-bar {
          height: 64px;
          display: flex;
          align-items: center;
          gap: 4px;
          padding: 0 16px 0 24px;
          background: hsl(var(--surface-container-low));
          border-bottom: 1px solid hsl(var(--outline-variant));
          flex-shrink: 0;
        }
        .main-content {
          flex: 1;
          overflow-y: auto;
          padding: 24px;
        }
        .bottom-toolbar {
          display: none;
          position: fixed;
          bottom: 0; left: 0; right: 0;
          height: 80px;
          background: hsl(var(--surface-container));
          border-top: 1px solid hsl(var(--outline-variant));
          flex-direction: row;
          align-items: center;
          justify-content: space-around;
          padding: 0 8px;
          z-index: 100;
        }
        .bottom-tab {
          display: flex;
          flex-direction: column;
          align-items: center;
          gap: 4px;
          padding: 8px 16px;
          border-radius: var(--shape-full);
          text-decoration: none;
          color: hsl(var(--on-surface-variant));
          transition: background 100ms var(--motion-standard);
        }
        .bottom-tab--active {
          color: hsl(var(--secondary-container-foreground));
          background: hsl(var(--secondary-container));
        }
        @media (max-width: 599px) {
          .app-shell { grid-template-columns: 1fr; }
          .nav-drawer { display: none; }
          .bottom-toolbar { display: flex; }
          .main-content { padding-bottom: 96px; }
        }
        @media (min-width: 600px) and (max-width: 1239px) {
          .app-shell { grid-template-columns: 80px 1fr; }
          .nav-drawer { width: 80px; align-items: center; padding: 12px 0; }
          .nav-drawer__header { padding: 16px 8px; flex-direction: column; gap: 4px; }
          .drawer-title { display: none; }
          .nav-item { flex-direction: column; padding: 12px 8px; gap: 4px; margin: 0 4px; }
          .nav-item-label { font-size: 10px; }
        }
      `}</style>
    </div>
  );
};

// ── Notification drawer ─────────────────────────────────────────────────────

const NotificationDrawer: React.FC<{
  open: boolean;
  onClose: () => void;
  active: AlertItem[];
  dismissed: AlertItem[];
  onDismiss: (id: string) => void;
  onClearAll: () => void;
}> = ({ open, onClose, active, dismissed, onDismiss, onClearAll }) => {
  return (
    <>
      <div
        className={cn(
          "fixed inset-0 bg-black/40 z-40 transition-opacity",
          open ? "opacity-100 pointer-events-auto" : "opacity-0 pointer-events-none",
        )}
        onClick={onClose}
        aria-hidden
      />
      <aside
        className={cn(
          "fixed top-0 right-0 h-full w-full max-w-md bg-surface-container border-l border-outline-variant z-50 flex flex-col transition-transform",
          open ? "translate-x-0" : "translate-x-full",
        )}
        style={{ transitionDuration: "240ms", transitionTimingFunction: "var(--motion-emphasized)" }}
        aria-hidden={!open}
        aria-label="Notifications"
      >
        <header className="flex items-center gap-2 px-5 py-4 border-b border-outline-variant">
          <Icon name="notifications" className="text-primary" />
          <span className="title-medium text-on-surface flex-1">Notifications</span>
          <button
            onClick={onClearAll}
            disabled={active.length === 0}
            className="label-medium text-primary hover:bg-primary/10 px-3 py-1 rounded-full transition-colors disabled:opacity-40"
          >
            Clear all
          </button>
          <button
            onClick={onClose}
            className="w-9 h-9 rounded-full hover:bg-on-surface/[0.08] flex items-center justify-center transition-colors"
            aria-label="Close notifications"
          >
            <Icon name="close" className="text-on-surface-variant" />
          </button>
        </header>
        <div className="flex-1 overflow-y-auto p-4 flex flex-col gap-2">
          <SectionHeading label={`Active (${active.length})`} />
          {active.length === 0 ? (
            <EmptyState message="No active alerts" icon="check_circle" />
          ) : (
            active.map((a) => (
              <AlertCard key={a.id} alert={a} onDismiss={() => onDismiss(a.id)} />
            ))
          )}

          <SectionHeading label={`Dismissed (${dismissed.length})`} className="mt-4" />
          {dismissed.length === 0 ? (
            <EmptyState message="No dismissed alerts" icon="history" />
          ) : (
            dismissed.map((a) => <AlertCard key={a.id} alert={a} dismissed />)
          )}
        </div>
      </aside>
    </>
  );
};

const SectionHeading: React.FC<{ label: string; className?: string }> = ({ label, className }) => (
  <h3 className={cn("label-medium text-on-surface-variant uppercase tracking-wider px-2 py-1", className)}>
    {label}
  </h3>
);

const EmptyState: React.FC<{ message: string; icon: string }> = ({ message, icon }) => (
  <div className="flex flex-col items-center justify-center py-6 gap-2 opacity-70">
    <Icon name={icon} size="lg" className="text-on-surface-variant" />
    <p className="body-small text-on-surface-variant">{message}</p>
  </div>
);

const AlertCard: React.FC<{ alert: AlertItem; onDismiss?: () => void; dismissed?: boolean }> = ({
  alert,
  onDismiss,
  dismissed,
}) => {
  const ageStr = useMemo(() => formatAge(alert.timestamp), [alert.timestamp]);
  const Icn = alert.level === "critical" ? "error" : "warning";
  const tone = alert.level === "critical" ? "text-destructive" : "text-tertiary";
  return (
    <article
      className={cn(
        "p-3 rounded-[var(--shape-md)] border bg-surface-container-low border-outline-variant",
        dismissed && "opacity-60",
      )}
    >
      <div className="flex items-start gap-3">
        <Icon name={Icn} className={tone} />
        <div className="flex-1 min-w-0">
          <p className="body-medium text-on-surface">{alert.message}</p>
          <p className="label-small text-on-surface-variant mt-1">
            {alert.sensor} · {alert.db > 0 ? `${alert.db.toFixed(1)} dB` : "—"}
            {alert.threshold > 0 && ` / ${alert.threshold} dB threshold`} · {ageStr}
          </p>
        </div>
        {!dismissed && (
          <button
            onClick={onDismiss}
            className="label-small text-on-surface-variant hover:text-on-surface px-2 py-1 rounded-full hover:bg-on-surface/[0.08] transition-colors"
          >
            Dismiss
          </button>
        )}
      </div>
    </article>
  );
};

function formatAge(ts: number): string {
  const seconds = Math.max(0, Math.floor((Date.now() - ts) / 1000));
  if (seconds < 60) return `${seconds}s ago`;
  const m = Math.floor(seconds / 60);
  if (m < 60) return `${m}m ago`;
  const h = Math.floor(m / 60);
  if (h < 24) return `${h}h ago`;
  return `${Math.floor(h / 24)}d ago`;
}

// ── About modal ─────────────────────────────────────────────────────────────

const AboutModal: React.FC<{ onClose: () => void }> = ({ onClose }) => (
  <div
    className="fixed inset-0 bg-black/50 z-50 flex items-center justify-center p-4"
    onClick={onClose}
    role="dialog"
    aria-modal
  >
    <div
      onClick={(e) => e.stopPropagation()}
      className="bg-surface-container-high border border-outline-variant rounded-[var(--shape-lg)] max-w-md w-full p-6 shadow-2xl"
    >
      <div className="flex items-center gap-3 mb-4">
        <Icon name="sensors" className="text-primary" size="lg" />
        <div>
          <h2 className="title-large text-on-surface">AcouSense</h2>
          <p className="label-small text-on-surface-variant">Urban noise monitor · v2.4.1</p>
        </div>
      </div>
      <p className="body-medium text-on-surface mb-2">
        Real-time acoustic environment intelligence for civic health.
      </p>
      <p className="body-small text-on-surface-variant mb-6">
        Licensed under the MIT License. Heatmap rendering powered by deck.gl + MapLibre.
      </p>
      <div className="flex justify-between items-center">
        <a
          href="https://github.com/acousense/dsp-noise-map"
          target="_blank"
          rel="noreferrer"
          className="label-medium text-primary hover:underline"
        >
          View project repository ↗
        </a>
        <button
          onClick={onClose}
          className="label-large text-on-surface-variant hover:bg-on-surface/[0.08] px-4 py-2 rounded-full transition-colors"
        >
          Close
        </button>
      </div>
    </div>
  </div>
);
