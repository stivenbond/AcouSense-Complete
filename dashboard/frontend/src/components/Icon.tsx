import React from "react";

interface IconProps {
  name: string;
  filled?: boolean;
  size?: "sm" | "md" | "lg";
  className?: string;
  style?: React.CSSProperties;
}

const sizeMap = {
  sm: "text-[20px]",
  md: "text-[24px]",
  lg: "text-[40px]",
};

export const Icon: React.FC<IconProps> = ({ name, filled, size = "md", className = "", style: customStyle }) => (
  <span
    className={`material-symbols-rounded ${filled ? "filled" : ""} ${sizeMap[size]} select-none leading-none inline-block ${className}`}
    style={{
      fontVariationSettings: filled
        ? "'FILL' 1, 'wght' 400, 'GRAD' 0, 'opsz' 24"
        : `'FILL' 0, 'wght' ${size === "lg" ? 300 : 400}, 'GRAD' 0, 'opsz' ${size === "sm" ? 20 : size === "lg" ? 40 : 24}`,
      ...customStyle,
    }}
  >
    {name}
  </span>
);
