import React, { useState } from "react";
import { useQuery } from "@tanstack/react-query";
import { M3Card } from "@/components/M3Card";
import { Icon } from "@/components/Icon";
import { sensors } from "@/lib/mockData";
import { fetchDeviceCards } from "@/lib/sensorApi";

const tabs = ["All", "Online", "Offline", "Warning"];

const Devices: React.FC = () => {
  const [activeTab, setActiveTab] = useState(0);
  const [search, setSearch] = useState("");
  const { data } = useQuery({
    queryKey: ["devices"],
    queryFn: fetchDeviceCards,
    retry: 1,
  });

  const deviceList = data && data.length > 0 ? data : sensors;

  const filtered = deviceList.filter(d => {
    const matchTab = activeTab === 0 || 
      (activeTab === 1 && d.status === "online") ||
      (activeTab === 2 && d.status === "offline") ||
      (activeTab === 3 && d.status === "warning");
    const matchSearch = d.name.toLowerCase().includes(search.toLowerCase()) || d.id.toLowerCase().includes(search.toLowerCase());
    return matchTab && matchSearch;
  });

  const statusColor = (s: string) => s === "online" ? "text-primary" : s === "warning" ? "text-tertiary" : "text-on-surface-variant";

  return (
    <div className="flex flex-col gap-4">
      {/* Tabs */}
      <div className="flex border-b border-outline-variant">
        {tabs.map((tab, i) => (
          <button
            key={tab}
            onClick={() => setActiveTab(i)}
            className={`px-6 py-3 label-large transition-colors relative ${
              i === activeTab ? "text-primary" : "text-on-surface-variant hover:text-on-surface"
            }`}
          >
            {tab}
            {i === activeTab && (
              <div className="absolute bottom-0 left-2 right-2 h-[3px] rounded-t-full bg-primary" />
            )}
          </button>
        ))}
      </div>

      {/* Search */}
      <div className="relative">
        <Icon name="search" size="sm" className="absolute left-4 top-1/2 -translate-y-1/2 text-on-surface-variant" />
        <input
          type="text"
          placeholder="Search devices..."
          value={search}
          onChange={e => setSearch(e.target.value)}
          className="w-full h-12 pl-12 pr-4 rounded-full bg-surface-container-highest text-on-surface body-large border-none outline-none focus:ring-2 focus:ring-primary/50"
        />
      </div>

      {/* Device Grid */}
      <div className="grid gap-4" style={{ gridTemplateColumns: "repeat(auto-fill, minmax(300px, 1fr))" }}>
        {filtered.map(device => (
          <M3Card
            key={device.id}
            variant={device.status === "warning" ? "elevated" : device.status === "offline" ? "filled" : "outlined"}
          >
            <div className="p-5 flex flex-col gap-4">
              {/* Header */}
              <div className="flex items-center justify-between">
                <div className="flex items-center gap-2">
                  <span className="title-medium text-on-surface">{device.id}</span>
                  <Icon name="circle" filled size="sm" className={`text-[8px] ${statusColor(device.status)}`} />
                </div>
                <button className="w-10 h-10 rounded-full flex items-center justify-center hover:bg-on-surface/[0.08]">
                  <Icon name="more_vert" className="text-on-surface-variant" />
                </button>
              </div>

              <p className="body-medium text-on-surface-variant">{device.name}</p>

              {/* Signal */}
              <div className="flex items-center justify-between">
                <span className="label-medium text-on-surface-variant">Signal</span>
                <div className="flex gap-[3px]">
                  {Array.from({ length: 5 }, (_, i) => (
                    <div
                      key={i}
                      className="w-1 rounded-full"
                      style={{
                        height: `${8 + i * 4}px`,
                        background: i < device.signal
                          ? "hsl(136, 76%, 67%)"
                          : "hsl(140, 6%, 20%)",
                      }}
                    />
                  ))}
                </div>
              </div>

              {/* Threshold */}
              <div className="flex items-center justify-between">
                <span className="label-medium text-on-surface-variant">Alert threshold</span>
                <span className="font-mono label-large text-on-surface">{device.threshold} dB</span>
              </div>

              {/* Footer */}
              <div className="pt-2 border-t border-outline-variant">
                <span className="label-small text-on-surface-variant">Last seen: {device.lastSeen}</span>
              </div>
            </div>
          </M3Card>
        ))}
      </div>
    </div>
  );
};

export default Devices;
