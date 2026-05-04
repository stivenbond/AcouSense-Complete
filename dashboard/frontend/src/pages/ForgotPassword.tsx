import React, { useState } from "react";
import { Link } from "react-router-dom";
import { M3Card } from "@/components/M3Card";
import { M3Button } from "@/components/M3Button";
import { Icon } from "@/components/Icon";
import { authApi } from "@/lib/authApi";

const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

const ForgotPassword: React.FC = () => {
  const [email, setEmail] = useState("");
  const [emailError, setEmailError] = useState("");
  const [loading, setLoading] = useState(false);
  const [sent, setSent] = useState(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setEmailError("");
    if (!EMAIL_RE.test(email)) {
      setEmailError("Enter a valid email address");
      return;
    }
    setLoading(true);
    await authApi.forgotPassword(email.trim());
    setLoading(false);
    setSent(true);
  };

  return (
    <div className="min-h-screen flex items-center justify-center p-6" style={{ background: "hsl(var(--background))" }}>
      <M3Card variant="filled" className="w-full max-w-[420px] p-8">
        <div className="flex flex-col items-center gap-2 mb-8">
          <Icon name="lock_reset" size="lg" className="text-primary" />
          <h1 className="headline-medium text-on-surface">Reset password</h1>
          <p className="body-medium text-on-surface-variant text-center">
            Enter your email and we'll send you a reset link
          </p>
        </div>

        {sent ? (
          <div className="flex flex-col items-center gap-4 text-center">
            <Icon name="mark_email_read" size="lg" className="text-primary" />
            <p className="body-medium text-on-surface">
              If an account exists for that email, a reset link is on its way.
            </p>
            <p className="label-small text-on-surface-variant">
              In demo mode, check the auth-server console for the link.
            </p>
            <Link to="/auth/signin" className="label-medium text-primary hover:underline">
              Back to sign in
            </Link>
          </div>
        ) : (
          <form className="flex flex-col gap-4" onSubmit={handleSubmit}>
            <div>
              <input
                type="email"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                placeholder="Email"
                autoComplete="email"
                className={`h-14 w-full px-4 rounded-[var(--shape-xs)] bg-surface-container-highest text-on-surface body-large border outline-none ${
                  emailError ? "border-destructive focus:border-destructive" : "border-outline focus:border-primary"
                }`}
              />
              {emailError && <p className="label-small text-destructive mt-1 ml-1">{emailError}</p>}
            </div>
            <M3Button type="submit" className="w-full justify-center mt-2" disabled={loading}>
              {loading ? "Sending…" : "Send reset link"}
            </M3Button>
            <div className="text-center mt-2">
              <Link to="/auth/signin" className="label-medium text-primary hover:underline">
                Back to sign in
              </Link>
            </div>
          </form>
        )}
      </M3Card>
    </div>
  );
};

export default ForgotPassword;
