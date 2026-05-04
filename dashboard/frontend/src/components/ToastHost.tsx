import React from "react";
import { useToast } from "@/lib/toastStore";
import { Icon } from "./Icon";

export const ToastHost: React.FC = () => {
  const message = useToast((s) => s.message);
  const variant = useToast((s) => s.variant);
  if (!message) return null;
  const icon = variant === "success" ? "check_circle" : "error";
  const style: React.CSSProperties =
    variant === "error"
      ? { background: "hsl(var(--destructive))", color: "hsl(var(--destructive-foreground))" }
      : {};
  return (
    <div className="acousense-toast" role="status" style={style}>
      <Icon name={icon} size="sm" />
      <span className="label-large">{message}</span>
    </div>
  );
};
