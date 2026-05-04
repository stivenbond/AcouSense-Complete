import React from "react";
import { cn } from "@/lib/utils";

interface M3ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: "filled" | "outlined" | "tonal" | "text" | "elevated";
  icon?: string;
  children: React.ReactNode;
}

const variants = {
  filled: "bg-primary text-primary-foreground hover:brightness-110",
  outlined: "border border-outline text-primary hover:bg-primary/10",
  tonal: "bg-secondary-container text-secondary-container-foreground hover:brightness-110",
  text: "text-primary hover:bg-primary/10",
  elevated: "bg-surface-container-low text-primary shadow-md hover:shadow-lg",
};

export const M3Button: React.FC<M3ButtonProps> = ({ variant = "filled", icon, children, className, ...props }) => (
  <button
    className={cn(
      "inline-flex items-center gap-2 px-6 h-10 rounded-full label-large transition-all",
      "disabled:opacity-40 disabled:pointer-events-none",
      variants[variant],
      className
    )}
    {...props}
  >
    {icon && <span className="material-symbols-rounded text-[18px]">{icon}</span>}
    {children}
  </button>
);
