import React from "react";
import { cn } from "@/lib/utils";

interface M3CardProps {
  variant?: "elevated" | "filled" | "outlined";
  className?: string;
  children: React.ReactNode;
  style?: React.CSSProperties;
}

const variantStyles = {
  elevated: "bg-surface-container-low shadow-md",
  filled: "bg-surface-container",
  outlined: "bg-surface-container-low border border-outline-variant",
};

export const M3Card: React.FC<M3CardProps> = ({ variant = "elevated", className, children, style }) => (
  <div className={cn("rounded-[var(--shape-lg)] overflow-hidden", variantStyles[variant], className)} style={style}>
    {children}
  </div>
);
