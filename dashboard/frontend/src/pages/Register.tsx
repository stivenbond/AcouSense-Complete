import React, { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { M3Card } from "@/components/M3Card";
import { M3Button } from "@/components/M3Button";
import { Icon } from "@/components/Icon";
import { authApi } from "@/lib/authApi";
import { useUser } from "@/lib/userStore";

const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

interface FormErrors {
  name?: string;
  email?: string;
  password?: string;
  confirm?: string;
}

const Register: React.FC = () => {
  const navigate = useNavigate();
  const setUser = useUser((s) => s.setUser);

  const [name, setName] = useState("");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [confirm, setConfirm] = useState("");
  const [errors, setErrors] = useState<FormErrors>({});
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    const next: FormErrors = {};
    if (!name.trim()) next.name = "Name is required";
    if (!EMAIL_RE.test(email)) next.email = "Enter a valid email address";
    if (password.length < 8) next.password = "Password must be at least 8 characters";
    if (password !== confirm) next.confirm = "Passwords do not match";
    setErrors(next);
    setError("");
    if (Object.keys(next).length) return;

    setLoading(true);
    const res = await authApi.register({
      name: name.trim(),
      email: email.trim(),
      password,
      healthConsent: false,
    });
    setLoading(false);
    if (res.ok && res.user) {
      setUser(res.user);
      navigate("/", { replace: true });
    } else {
      setError(res.error ?? "Something went wrong. Please try again.");
    }
  };

  return (
    <div className="min-h-screen flex items-center justify-center p-6" style={{ background: "hsl(var(--background))" }}>
      <M3Card variant="filled" className="w-full max-w-[420px] p-8">
        <div className="flex flex-col items-center gap-2 mb-8">
          <Icon name="sensors" size="lg" className="text-primary" />
          <h1 className="headline-medium text-on-surface">Create account</h1>
          <p className="body-medium text-on-surface-variant">Join the AcouSense network</p>
        </div>

        <form className="flex flex-col gap-4" onSubmit={handleSubmit}>
          <Field
            value={name}
            onChange={setName}
            placeholder="Full name"
            autoComplete="name"
            error={errors.name}
          />
          <Field
            type="email"
            value={email}
            onChange={setEmail}
            placeholder="Email"
            autoComplete="email"
            error={errors.email}
          />
          <Field
            type="password"
            value={password}
            onChange={setPassword}
            placeholder="Password (min. 8 characters)"
            autoComplete="new-password"
            error={errors.password}
          />
          <Field
            type="password"
            value={confirm}
            onChange={setConfirm}
            placeholder="Confirm password"
            autoComplete="new-password"
            error={errors.confirm}
          />

          {error && (
            <div className="flex items-center gap-2 p-3 rounded-[var(--shape-md)] bg-destructive/10 border border-destructive/20">
              <Icon name="error" size="sm" className="text-destructive" />
              <span className="body-small text-destructive">{error}</span>
            </div>
          )}

          <M3Button type="submit" className="w-full justify-center mt-2" disabled={loading}>
            {loading ? "Creating account…" : "Create account"}
          </M3Button>

          <div className="text-center mt-2">
            <Link to="/auth/signin" className="label-medium text-primary hover:underline">
              Already have an account? Sign in
            </Link>
          </div>
        </form>
      </M3Card>
    </div>
  );
};

interface FieldProps {
  value: string;
  onChange: (v: string) => void;
  type?: string;
  placeholder?: string;
  autoComplete?: string;
  error?: string;
}

const Field: React.FC<FieldProps> = ({ value, onChange, type = "text", placeholder, autoComplete, error }) => (
  <div>
    <input
      type={type}
      value={value}
      onChange={(e) => onChange(e.target.value)}
      placeholder={placeholder}
      autoComplete={autoComplete}
      className={`h-14 w-full px-4 rounded-[var(--shape-xs)] bg-surface-container-highest text-on-surface body-large border outline-none ${
        error ? "border-destructive focus:border-destructive" : "border-outline focus:border-primary"
      }`}
    />
    {error && <p className="label-small text-destructive mt-1 ml-1">{error}</p>}
  </div>
);

export default Register;
