import React, { useMemo, useRef, useState, useEffect } from "react";
import maplibregl from "maplibre-gl";
import "maplibre-gl/dist/maplibre-gl.css";
import { MapboxOverlay } from "@deck.gl/mapbox";
import { HeatmapLayer } from "@deck.gl/aggregation-layers";
import { ScatterplotLayer } from "@deck.gl/layers";
import type { PickingInfo } from "@deck.gl/core";
import { getMapStyle } from "@/lib/mapStyles";
import { useSettings } from "@/lib/settingsStore";
import type { SensorData } from "@/lib/sensorApi";

interface NoiseHeatmapProps {
  sensors: SensorData[];
}

interface PopupData {
  sensor: SensorData;
  lng: number;
  lat: number;
}

const dotColor = (db: number): [number, number, number, number] => {
  if (db < 55) return [57, 211, 83, 255];   // primary green #39d353
  if (db <= 75) return [255, 184, 28, 255]; // amber
  return [239, 68, 68, 255];                // destructive red
};

export const NoiseHeatmap: React.FC<NoiseHeatmapProps> = ({ sensors }) => {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const mapRef = useRef<maplibregl.Map | null>(null);
  const overlayRef = useRef<MapboxOverlay | null>(null);
  const popupRef = useRef<maplibregl.Popup | null>(null);
  const [popup, setPopup] = useState<PopupData | null>(null);
  const mapStyle = useSettings((s) => s.mapStyle);

  // Initial centre + zoom: average of sensor coordinates
  const initialView = useMemo(() => {
    if (sensors.length === 0) return { lng: -0.1, lat: 51.507, zoom: 13.5 };
    const lng = sensors.reduce((a, s) => a + s.lng, 0) / sensors.length;
    const lat = sensors.reduce((a, s) => a + s.lat, 0) / sensors.length;
    return { lng, lat, zoom: 13.5 };
  }, [sensors]);

  // ── Initialise the map ───────────────────────────────────────────────────
  useEffect(() => {
    if (!containerRef.current || mapRef.current) return;

    const map = new maplibregl.Map({
      container: containerRef.current,
      style: getMapStyle(mapStyle),
      center: [initialView.lng, initialView.lat],
      zoom: initialView.zoom,
      attributionControl: { compact: true },
    });
    mapRef.current = map;

    map.addControl(new maplibregl.NavigationControl({ showCompass: false }), "top-right");

    map.on("load", () => {
      const overlay = new MapboxOverlay({ interleaved: false, layers: [] });
      overlayRef.current = overlay;
      map.addControl(overlay as unknown as maplibregl.IControl);
    });

    return () => {
      popupRef.current?.remove();
      popupRef.current = null;
      overlayRef.current = null;
      map.remove();
      mapRef.current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // ── React to map style changes ───────────────────────────────────────────
  useEffect(() => {
    const map = mapRef.current;
    if (!map) return;
    map.setStyle(getMapStyle(mapStyle));
  }, [mapStyle]);

  // ── Update layers whenever sensors change ────────────────────────────────
  useEffect(() => {
    const overlay = overlayRef.current;
    if (!overlay) return;

    const heatmap = new HeatmapLayer<SensorData>({
      id: "noise-heatmap",
      data: sensors,
      getPosition: (s) => [s.lng, s.lat],
      getWeight: (s) => Math.max(0, s.db),
      aggregation: "MEAN",
      radiusPixels: 70,
      intensity: 1.4,
      threshold: 0.04,
      colorRange: [
        [26, 35, 126, 0],
        [0, 137, 123, 120],
        [67, 160, 71, 180],
        [253, 216, 53, 210],
        [251, 140, 0, 230],
        [229, 57, 53, 250],
      ],
    });

    const scatter = new ScatterplotLayer<SensorData>({
      id: "sensor-dots",
      data: sensors,
      pickable: true,
      stroked: true,
      filled: true,
      getPosition: (s) => [s.lng, s.lat],
      getFillColor: (s) => dotColor(s.db),
      getLineColor: [12, 17, 23, 220],
      getRadius: 7,
      getLineWidth: 2,
      radiusUnits: "pixels",
      lineWidthUnits: "pixels",
      onClick: (info: PickingInfo) => {
        const obj = info.object as SensorData | undefined;
        if (!obj) return false;
        setPopup({ sensor: obj, lng: obj.lng, lat: obj.lat });
        return true;
      },
    });

    overlay.setProps({ layers: [heatmap, scatter] });
  }, [sensors]);

  // ── Popup handling via maplibre-gl ───────────────────────────────────────
  useEffect(() => {
    const map = mapRef.current;
    if (!map) return;
    if (!popup) {
      popupRef.current?.remove();
      popupRef.current = null;
      return;
    }
    const html = `
      <div style="font-family:'Roboto Flex',sans-serif;min-width:200px">
        <div style="font-weight:600;font-size:14px;margin-bottom:6px">${escapeHtml(popup.sensor.name)}</div>
        <div style="font-size:11px;opacity:0.7;margin-bottom:8px">${escapeHtml(popup.sensor.id)}</div>
        <div style="display:flex;justify-content:space-between;font-size:13px;margin-bottom:4px">
          <span>Current</span>
          <span style="font-family:'Roboto Mono',monospace;color:${rgbCss(dotColor(popup.sensor.db))}">${popup.sensor.db.toFixed(1)} dB</span>
        </div>
        <div style="display:flex;justify-content:space-between;font-size:12px;margin-bottom:4px;opacity:0.85">
          <span>Threshold</span><span style="font-family:'Roboto Mono',monospace">${popup.sensor.threshold} dB</span>
        </div>
        <div style="display:flex;justify-content:space-between;font-size:12px;margin-bottom:4px;opacity:0.85">
          <span>Last update</span><span>${escapeHtml(popup.sensor.lastSeen)}</span>
        </div>
        <div style="display:flex;justify-content:space-between;font-size:12px;opacity:0.85">
          <span>Status</span>
          <span style="text-transform:uppercase;font-weight:600;color:${popup.sensor.db > popup.sensor.threshold ? "#ef4444" : "#39d353"}">${popup.sensor.db > popup.sensor.threshold ? "Alert" : popup.sensor.status}</span>
        </div>
      </div>`;

    if (!popupRef.current) {
      popupRef.current = new maplibregl.Popup({ closeButton: true, closeOnClick: false, offset: 12 });
      popupRef.current.on("close", () => setPopup(null));
    }
    popupRef.current
      .setLngLat([popup.lng, popup.lat])
      .setHTML(html)
      .addTo(map);
  }, [popup]);

  return (
    <div className="absolute inset-0">
      <div ref={containerRef} className="absolute inset-0" />
      <div className="absolute bottom-3 left-3 flex gap-3 bg-surface-container/85 backdrop-blur px-3 py-2 rounded-full pointer-events-none">
        {[
          { color: "#39d353", label: "< 55 dB" },
          { color: "#ffb81c", label: "55–75 dB" },
          { color: "#ef4444", label: "> 75 dB" },
        ].map((l) => (
          <span key={l.label} className="flex items-center gap-1.5 label-small text-on-surface-variant">
            <span className="w-2.5 h-2.5 rounded-full" style={{ background: l.color }} />
            {l.label}
          </span>
        ))}
      </div>
    </div>
  );
};

function escapeHtml(s: string): string {
  return s.replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]!));
}

function rgbCss([r, g, b]: [number, number, number, number]): string {
  return `rgb(${r}, ${g}, ${b})`;
}
