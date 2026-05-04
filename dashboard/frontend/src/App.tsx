import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { BrowserRouter, Route, Routes } from "react-router-dom";
import { TooltipProvider } from "@/components/ui/tooltip";
import { AppShell } from "@/components/AppShell";
import { ToastHost } from "@/components/ToastHost";
import { AuthBootstrap, RequireAuth, RedirectIfAuthed } from "@/components/AuthGuards";
import Dashboard from "./pages/Dashboard";
import History from "./pages/History";
import Stats from "./pages/Stats";
import Devices from "./pages/Devices";
import Settings from "./pages/Settings";
import SignIn from "./pages/SignIn";
import Register from "./pages/Register";
import ForgotPassword from "./pages/ForgotPassword";
import NotFound from "./pages/NotFound";

const queryClient = new QueryClient();

const App = () => (
  <QueryClientProvider client={queryClient}>
    <TooltipProvider>
      <BrowserRouter>
        <AuthBootstrap>
          <Routes>
            {/* Auth routes — no shell, redirect to / when already signed in */}
            <Route path="/auth/signin" element={<RedirectIfAuthed><SignIn /></RedirectIfAuthed>} />
            <Route path="/auth/register" element={<RedirectIfAuthed><Register /></RedirectIfAuthed>} />
            <Route path="/auth/forgot-password" element={<RedirectIfAuthed><ForgotPassword /></RedirectIfAuthed>} />

            {/* Protected app routes — wrapped in shell, gated by RequireAuth */}
            <Route path="/" element={<RequireAuth><AppShell><Dashboard /></AppShell></RequireAuth>} />
            <Route path="/dashboard" element={<RequireAuth><AppShell><Dashboard /></AppShell></RequireAuth>} />
            <Route path="/history" element={<RequireAuth><AppShell><History /></AppShell></RequireAuth>} />
            <Route path="/stats" element={<RequireAuth><AppShell><Stats /></AppShell></RequireAuth>} />
            <Route path="/devices" element={<RequireAuth><AppShell><Devices /></AppShell></RequireAuth>} />
            <Route path="/settings" element={<RequireAuth><AppShell><Settings /></AppShell></RequireAuth>} />

            <Route path="*" element={<NotFound />} />
          </Routes>
          <ToastHost />
        </AuthBootstrap>
      </BrowserRouter>
    </TooltipProvider>
  </QueryClientProvider>
);

export default App;
