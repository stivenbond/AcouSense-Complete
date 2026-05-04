import { defineConfig, loadEnv } from "vite";
import react from "@vitejs/plugin-react-swc";
import path from "path";
import { componentTagger } from "lovable-tagger";

// https://vitejs.dev/config/
export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), "");
  // FastAPI backend (acousense.api.main:app) — handles all non-auth /api/* calls
  const backendUrl = env.VITE_BACKEND_URL || "http://localhost:8000";
  // Express auth server (auth-server/index.js) — handles /api/auth/*
  const authUrl = env.VITE_AUTH_URL || "http://localhost:3030";

  return {
    server: {
      host: "::",
      port: 8080,
      hmr: {
        overlay: false,
      },
      // Order matters: more specific paths must come first so Vite matches
      // /api/auth/* before the catch-all /api/* rule.
      proxy: {
        "/api/auth": {
          target: authUrl,
          changeOrigin: true,
          secure: false,
        },
        "/api": {
          target: backendUrl,
          changeOrigin: true,
          secure: false,
        },
        "/health": {
          target: backendUrl,
          changeOrigin: true,
          secure: false,
        },
        "/ready": {
          target: backendUrl,
          changeOrigin: true,
          secure: false,
        },
      },
    },
    plugins: [react(), mode === "development" && componentTagger()].filter(Boolean),
    resolve: {
      alias: {
        "@": path.resolve(__dirname, "./src"),
      },
      dedupe: [
        "react",
        "react-dom",
        "react/jsx-runtime",
        "react/jsx-dev-runtime",
        "@tanstack/react-query",
        "@tanstack/query-core",
      ],
    },
  };
});
