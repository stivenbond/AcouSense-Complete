import React, { useState } from "react";
import { Link, useNavigate, useLocation } from "react-router-dom";
import { M3Card } from "@/components/M3Card";
import { M3Button } from "@/components/M3Button";
import { Icon } from "@/components/Icon";
import { authApi } from "@/lib/authApi";
import { useUser } from "@/lib/userStore";

const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

const SignIn: React.FC = () => {
  const navigate = useNavigate();
  const location = useLocation();
  const setUser = useUser((s) => s.setUser);

  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [showPassword, setShowPassword] = useState(false);
  const [emailError, setEmailError] = useState("");
  const [passwordError, setPasswordError] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const redirectTarget = (location.state as { from?: string } | null)?.from ?? "/";

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setEmailError("");
    setPasswordError("");
    setError("");

    let valid = true;
    if (!email) {
      setEmailError("Email is required");
      valid = false;
    } else if (!EMAIL_RE.test(email)) {
      setEmailError("Enter a valid email address");
      valid = false;
    }
    if (!password) {
      setPasswordError("Password is required");
      valid = false;
    }
    if (!valid) return;

    setLoading(true);
    const res = await authApi.login(email.trim(), password);
    setLoading(false);

    if (res.ok && res.user) {
      setUser(res.user);
      navigate(redirectTarget, { replace: true });
    } else {
      setError(res.error ?? "Something went wrong. Please try again.");
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center p-6" style={{ background: "hsl(var(--background))" }}>
      <M3Card variant="filled" className="w-full max-w-[420px] p-8">
        <div className="flex flex-col items-center gap-2 mb-8">
          <Icon name="sensors" size="lg" className="text-primary" />
          <h1 className="headline-medium text-on-surface">Sign in to AcouSense</h1>
          <p className="body-medium text-on-surface-variant">Monitor your urban noise environment</p>
        </div>

        <form className="flex flex-col gap-4" onSubmit={handleSubmit}>
          <div>
            <input
              type="email"
              autoComplete="email"
              placeholder="Email"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              className={`h-14 w-full px-4 rounded-[var(--shape-xs)] bg-surface-container-highest text-on-surface body-large border outline-none ${
                emailError ? "border-destructive focus:border-destructive" : "border-outline focus:border-primary"
              }`}
            />
            {emailError && <p className="label-small text-destructive mt-1 ml-1">{emailError}</p>}
          </div>

          <div>
            <div className="relative">
              <input
                type={showPassword ? "text" : "password"}
                autoComplete="current-password"
                placeholder="Password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                className={`h-14 w-full px-4 pr-12 rounded-[var(--shape-xs)] bg-surface-container-highest text-on-surface body-large border outline-none ${
                  passwordError ? "border-destructive focus:border-destructive" : "border-outline focus:border-primary"
                }`}
              />
              <button
                type="button"
                onClick={() => setShowPassword(!showPassword)}
                className="absolute right-3 top-1/2 -translate-y-1/2 text-on-surface-variant hover:text-on-surface"
                aria-label={showPassword ? "Hide password" : "Show password"}
              >
                <Icon name={showPassword ? "visibility_off" : "visibility"} size="sm" />
              </button>
            </div>
            {passwordError && <p className="label-small text-destructive mt-1 ml-1">{passwordError}</p>}
          </div>

          {error && (
            <div className="flex items-center gap-2 p-3 rounded-[var(--shape-md)] bg-destructive/10 border border-destructive/20">
              <Icon name="error" size="sm" className="text-destructive" />
              <span className="body-small text-destructive">{error}</span>
            </div>
          )}

          <M3Button type="submit" className="w-full justify-center mt-2" disabled={loading} icon={loading ? "progress_activity" : undefined}>
            {loading ? "Signing in…" : "Sign in"}
          </M3Button>

          <div className="flex justify-between mt-2">
            <Link to="/auth/forgot-password" className="label-medium text-primary hover:underline">
              Forgot password?
            </Link>
            <Link to="/auth/register" className="label-medium text-primary hover:underline">
              Create account
            </Link>
          </div>
        </form>
      </M3Card>
    </div>
  );
};

export default SignIn;
