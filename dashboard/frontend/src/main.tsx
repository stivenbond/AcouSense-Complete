import { createRoot } from "react-dom/client";
import App from "./App.tsx";
import "./index.css";
import { bootstrapTheme } from "./lib/settingsStore";

bootstrapTheme();

if ("serviceWorker" in navigator) {
  // Register the PWA / push service worker once the page is idle.
  window.addEventListener("load", () => {
    navigator.serviceWorker.register("/sw.js").catch((err) => {
      console.warn("Service worker registration failed", err);
    });
  });
}

createRoot(document.getElementById("root")!).render(<App />);
