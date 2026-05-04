import React, { useEffect } from "react";
import { Navigate, useLocation } from "react-router-dom";
import { useUser } from "@/lib/userStore";

/** Calls `bootstrap()` once on mount so `useUser` reflects the cookie session. */
export const AuthBootstrap: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const bootstrapped = useUser((s) => s.bootstrapped);
  const bootstrap = useUser((s) => s.bootstrap);

  useEffect(() => {
    if (!bootstrapped) void bootstrap();
  }, [bootstrapped, bootstrap]);

  return <>{children}</>;
};

/** Renders children only if the user is signed in; otherwise redirects to /auth/signin. */
export const RequireAuth: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const user = useUser((s) => s.user);
  const loading = useUser((s) => s.loading);
  const bootstrapped = useUser((s) => s.bootstrapped);
  const location = useLocation();

  if (!bootstrapped || loading) return <BootScreen />;
  if (!user) {
    return <Navigate to="/auth/signin" replace state={{ from: location.pathname }} />;
  }
  return <>{children}</>;
};

/** Inverse of RequireAuth — sends signed-in users to the dashboard. */
export const RedirectIfAuthed: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const user = useUser((s) => s.user);
  const loading = useUser((s) => s.loading);
  const bootstrapped = useUser((s) => s.bootstrapped);

  if (!bootstrapped || loading) return <BootScreen />;
  if (user) return <Navigate to="/" replace />;
  return <>{children}</>;
};

const BootScreen: React.FC = () => (
  <div
    aria-busy
    style={{
      minHeight: "100vh",
      background: "hsl(var(--background))",
      color: "hsl(var(--on-surface-variant))",
      display: "flex",
      flexDirection: "column",
      alignItems: "center",
      justifyContent: "center",
      gap: 12,
    }}
  >
    <span
      className="material-symbols-rounded"
      style={{
        fontSize: 36,
        color: "hsl(var(--primary))",
        animation: "boot-pulse 1.4s ease-in-out infinite",
      }}
    >
      sensors
    </span>
    <span className="label-medium">Loading AcouSense…</span>
    <style>{`
      @keyframes boot-pulse {
        0%, 100% { opacity: 0.4; transform: scale(0.9); }
        50% { opacity: 1; transform: scale(1.05); }
      }
    `}</style>
  </div>
);
