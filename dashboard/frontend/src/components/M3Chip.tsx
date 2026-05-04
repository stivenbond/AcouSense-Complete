import React from "react";
import { cn } from "@/lib/utils";

interface M3ChipProps {
  label: string;
  selected?: boolean;
  icon?: string;
  onClick?: () => void;
  variant?: "filter" | "assist" | "input";
}

export const M3Chip: React.FC<M3ChipProps> = ({ label, selected, icon, onClick, variant = "filter" }) => (
  <button
    onClick={onClick}
    className={cn(
      "inline-flex items-center gap-1.5 h-8 px-4 rounded-full label-large transition-all border",
      selected
        ? "bg-secondary-container text-secondary-container-foreground border-transparent"
        : "bg-transparent text-on-surface-variant border-outline hover:bg-on-surface/[0.08]"
    )}
  >
    {icon && <span className="material-symbols-rounded text-[18px]">{icon}</span>}
    {label}
  </button>
);
