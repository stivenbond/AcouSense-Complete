import { create } from "zustand";
import { authApi, type AppUser } from "./authApi";

interface UserStore {
  user: AppUser | null;
  email: string | null;
  loading: boolean;
  bootstrapped: boolean;
  bootstrap: () => Promise<void>;
  setUser: (user: AppUser | null) => void;
  /** @deprecated kept for backwards compatibility — prefer setUser */
  setEmail: (email: string | null) => void;
  signOut: () => Promise<void>;
}

export const useUser = create<UserStore>((set) => ({
  user: null,
  email: null,
  loading: true,
  bootstrapped: false,
  async bootstrap() {
    set({ loading: true });
    const u = await authApi.me();
    set({ user: u, email: u?.email ?? null, loading: false, bootstrapped: true });
  },
  setUser(user) {
    set({ user, email: user?.email ?? null, loading: false, bootstrapped: true });
  },
  setEmail(email) {
    // Legacy shim: synthesize a minimal user when only the email is known.
    if (email) {
      set({
        user: { id: -1, email, name: null },
        email,
        loading: false,
        bootstrapped: true,
      });
    } else {
      set({ user: null, email: null, loading: false, bootstrapped: true });
    }
  },
  async signOut() {
    await authApi.logout();
    set({ user: null, email: null });
  },
}));
