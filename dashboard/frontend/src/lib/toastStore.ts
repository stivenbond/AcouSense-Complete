import { create } from "zustand";

interface ToastState {
  message: string | null;
  variant: "success" | "error";
  show: (message: string, variant?: "success" | "error") => void;
  hide: () => void;
}

export const useToast = create<ToastState>((set, get) => ({
  message: null,
  variant: "success",
  show: (message, variant = "success") => {
    set({ message, variant });
    window.setTimeout(() => {
      if (get().message === message) set({ message: null });
    }, 3000);
  },
  hide: () => set({ message: null }),
}));
