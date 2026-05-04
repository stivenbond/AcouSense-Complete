import type { StyleSpecification } from "maplibre-gl";
import type { MapStyleId } from "./settingsStore";

const DARK_MATTER = "https://basemaps.cartocdn.com/gl/dark-matter-gl-style/style.json";
const POSITRON = "https://basemaps.cartocdn.com/gl/positron-gl-style/style.json";

const MAPTILER_KEY: string = (import.meta.env.VITE_MAPTILER_KEY as string | undefined) ?? "";

/**
 * Build a MapLibre style spec for a given AcouSense map style id.
 * Returns either a string URL (for hosted styles) or an inline StyleSpecification.
 */
export function getMapStyle(id: MapStyleId): string | StyleSpecification {
  switch (id) {
    case "dark":
      return DARK_MATTER;
    case "positron":
      return POSITRON;
    case "satellite":
      return {
        version: 8,
        sources: {
          esri: {
            type: "raster",
            tiles: [
              "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
            ],
            tileSize: 256,
            attribution:
              "Tiles &copy; Esri &mdash; Source: Esri, i-cubed, USDA, USGS, AEX, GeoEye, Getmapping, Aerogrid, IGN, IGP, UPR-EGP, and the GIS User Community",
          },
        },
        layers: [
          {
            id: "esri",
            type: "raster",
            source: "esri",
            minzoom: 0,
            maxzoom: 22,
          },
        ],
      };
    case "terrain": {
      if (MAPTILER_KEY) {
        return `https://api.maptiler.com/maps/landscape/style.json?key=${MAPTILER_KEY}`;
      }
      // Fallback to OpenTopoMap raster when no MapTiler key is configured.
      return {
        version: 8,
        sources: {
          opentopo: {
            type: "raster",
            tiles: [
              "https://a.tile.opentopomap.org/{z}/{x}/{y}.png",
              "https://b.tile.opentopomap.org/{z}/{x}/{y}.png",
              "https://c.tile.opentopomap.org/{z}/{x}/{y}.png",
            ],
            tileSize: 256,
            attribution:
              "Map data: &copy; OpenStreetMap contributors, SRTM | Style: &copy; OpenTopoMap (CC-BY-SA)",
          },
        },
        layers: [
          {
            id: "opentopo",
            type: "raster",
            source: "opentopo",
            minzoom: 0,
            maxzoom: 17,
          },
        ],
      };
    }
  }
}

export const MAP_STYLE_LABELS: Record<MapStyleId, string> = {
  dark: "Dark matter",
  positron: "Positron (light)",
  satellite: "Satellite",
  terrain: "Terrain",
};
