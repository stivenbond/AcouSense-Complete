import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit
from sklearn.metrics import r2_score

# Your data
data = {
    'Windows_Volume': [60, 58, 56, 54, 52, 48, 46, 44, 42, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 6, 4, 2, 0],
    'dBZ': [99, 99, 98, 97, 96.5, 96, 95, 94, 93.5, 93, 92.5, 91.5, 90.5, 90, 89, 87.5, 86.5, 85, 84.2, 82, 81, 79, 76, 74, 71, 68, 63, 59, 55, 52],
    'ADC_P2P': [85, 85, 78, 74, 68, 64, 58, 54, 50, 45, 42, 40, 36, 33, 31, 28, 26, 22, 20, 18, 15, 15, 12, 12, 10, 9, 9, 9, 9, 9]
}

adc = np.array(data['ADC_P2P'])
db = np.array(data['dBZ'])

# ============================================================
# MODEL 1: Linear in log-space (standard for sound pressure)
# dB = A * log10(ADC) + B
# ============================================================
def db_from_adc_log(adc, A, B, C):
    """Logarithmic model: dB = A * log10(adc - C) + B"""
    # Add small epsilon to avoid log(0/negative)
    adj = np.maximum(adc - C, 0.1)
    return A * np.log10(adj) + B

# Initial guess: sound pressure ~ voltage, dB ~ 20*log10(ADC)
# Need to handle the noise floor (ADC values saturate at ~9)
popt_log, pcov_log = curve_fit(db_from_adc_log, adc, db,
                               p0=[20, 50, 8],  # C=8 is noise floor offset
                               maxfev=5000)
A_log, B_log, C_log = popt_log
db_fit_log = db_from_adc_log(adc, A_log, B_log, C_log)
r2_log = r2_score(db, db_fit_log)

# ============================================================
# MODEL 2: Polynomial (simpler for Arduino, but less physical)
# dB = a*ADC^2 + b*ADC + c
# ============================================================
poly_coeffs = np.polyfit(adc, db, 2)  # Quadratic fit
poly_fn = np.poly1d(poly_coeffs)
db_fit_poly = poly_fn(adc)
r2_poly = r2_score(db, db_fit_poly)

# ============================================================
# MODEL 3: Power law (alternative to log)
# ADC = k * (dB - dB0)^p  ->  dB = dB0 + (ADC/k)^(1/p)
# ============================================================
def db_from_adc_power(adc, dB0, k, p):
    return dB0 + ((adc - 9) / k) ** (1/p)  # subtract noise floor

# Filter out saturated low-end (ADC=9) for power law fit
mask = adc > 9
popt_power, _ = curve_fit(db_from_adc_power, adc[mask], db[mask],
                          p0=[50, 1, 0.5], maxfev=5000)
dB0_power, k_power, p_power = popt_power
db_fit_power = db_from_adc_power(adc, dB0_power, k_power, p_power)
r2_power = r2_score(db, db_fit_power)

# ============================================================
# Print results and select best model
# ============================================================
print("=" * 60)
print("FITTING RESULTS")
print("=" * 60)
print(f"\nMODEL 1 - Logarithmic: dB = A*log10(ADC - C) + B")
print(f"  A = {A_log:.4f}, B = {B_log:.4f}, C = {C_log:.4f}")
print(f"  R² = {r2_log:.6f}")
print(f"  Formula: dB = {A_log:.4f} * log10(ADC - {C_log:.2f}) + {B_log:.4f}")

print(f"\nMODEL 2 - Quadratic Polynomial: dB = a*ADC² + b*ADC + c")
print(f"  a = {poly_coeffs[0]:.6f}, b = {poly_coeffs[1]:.6f}, c = {poly_coeffs[2]:.6f}")
print(f"  R² = {r2_poly:.6f}")

print(f"\nMODEL 3 - Power Law: dB = dB0 + ((ADC - 9)/k)^(1/p)")
print(f"  dB0 = {dB0_power:.4f}, k = {k_power:.4f}, p = {p_power:.4f}")
print(f"  R² = {r2_power:.6f}")

# Select best model
models = {'Logarithmic': r2_log, 'Quadratic': r2_poly, 'Power Law': r2_power}
best_model = max(models, key=models.get)
print(f"\n✓ BEST MODEL: {best_model} (R² = {models[best_model]:.6f})")

# ============================================================
# Arduino-ready conversion formula (using best model)
# ============================================================
print("\n" + "=" * 60)
print("ARDUINO NANO FIRMWARE FORMULA")
print("=" * 60)

if best_model == 'Logarithmic':
    print(f"""
// Use this in your Arduino code:
// adc_pp = peak-to-peak ADC value (0-1023 for Nano, but yours seems 0-85 range)
// Note: Your ADC range appears 0-85, ensure reading matches

float adc_pp_raw = getPeakToPeak();  // Your function to get ADC peak-to-peak
float adc_corrected = adc_pp_raw - {C_log:.2f};
if (adc_corrected < 0.1) adc_corrected = 0.1;
float dB = {A_log:.4f} * log10(adc_corrected) + {B_log:.4f};

// If your ADC gives 0-1023 values, scale them first:
// float adc_scaled = adc_raw * (85.0 / 1023.0);
""")

elif best_model == 'Quadratic':
    print(f"""
// Use this in your Arduino code:
float adc_pp = getPeakToPeak();  // Your ADC peak-to-peak value
float dB = {poly_coeffs[0]:.6f} * adc_pp * adc_pp +
           {poly_coeffs[1]:.6f} * adc_pp +
           {poly_coeffs[2]:.6f};

// Note: This is fast but less physically accurate at extremes
""")

else:  # Power Law
    print(f"""
// Use this in your Arduino code:
float adc_pp = getPeakToPeak();  // Your ADC peak-to-peak value
float adc_corrected = adc_pp - 9.0;
if (adc_corrected < 0.1) adc_corrected = 0.1;
float dB = {dB0_power:.4f} + pow(adc_corrected / {k_power:.4f}, 1.0/{p_power:.4f});

// Note: Requires math.h for pow() function
""")

# ============================================================
# Generate calibration lookup table (optional, very fast on Nano)
# ============================================================
print("\n" + "=" * 60)
print("LOOKUP TABLE (if you prefer speed over RAM)")
print("=" * 60)
print("// ADC_P2P -> dB (for best model)")
print("// Generate this table in Arduino PROGMEM:")
print("const float db_lookup[86] PROGMEM = {")
print("  // ADC: 0 to 85")
for adc_val in range(0, 86):
    if best_model == 'Logarithmic':
        adj = max(adc_val - C_log, 0.1)
        db_val = A_log * np.log10(adj) + B_log
    elif best_model == 'Quadratic':
        db_val = poly_fn(adc_val)
    else:
        adj = max(adc_val - 9, 0.1)
        db_val = dB0_power + (adj / k_power) ** (1.0/p_power)

    if adc_val % 10 == 0:
        print(f"  {db_val:.2f},  // ADC={adc_val}")
print("};")

# ============================================================
# Plotting
# ============================================================
plt.figure(figsize=(12, 5))

plt.subplot(1, 2, 1)
plt.scatter(adc, db, color='red', label='Measured', s=50, zorder=5)
plt.plot(adc, db_fit_log, 'b-', label=f'Logarithmic (R²={r2_log:.4f})', linewidth=2)
plt.plot(adc, db_fit_poly, 'g--', label=f'Quadratic (R²={r2_poly:.4f})', linewidth=2)
plt.plot(adc, db_fit_power, 'm:', label=f'Power Law (R²={r2_power:.4f})', linewidth=2)
plt.xlabel('ADC Peak-to-Peak (raw)', fontsize=12)
plt.ylabel('dB(Z) from reference app', fontsize=12)
plt.title('Sensor Calibration: ADC vs Sound Level', fontsize=14)
plt.legend()
plt.grid(True, alpha=0.3)

plt.subplot(1, 2, 2)
residuals_log = db - db_fit_log
residuals_poly = db - db_fit_poly
residuals_power = db - db_fit_power
plt.scatter(db, residuals_log, label=f'Logarithmic (std={np.std(residuals_log):.2f} dB)', alpha=0.7)
plt.scatter(db, residuals_poly, label=f'Quadratic (std={np.std(residuals_poly):.2f} dB)', alpha=0.7)
plt.scatter(db, residuals_power, label=f'Power Law (std={np.std(residuals_power):.2f} dB)', alpha=0.7)
plt.axhline(y=0, color='black', linestyle='-', linewidth=0.8)
plt.xlabel('Measured dB(Z)', fontsize=12)
plt.ylabel('Residual (dB)', fontsize=12)
plt.title('Fit Residuals', fontsize=14)
plt.legend()
plt.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('ky038_calibration_fit.png', dpi=150)
plt.show()

# ============================================================
# Important notes about your data
# ============================================================
print("\n" + "=" * 60)
print("DATA QUALITY NOTES")
print("=" * 60)
print(f"• ADC saturation at low end: Values below ADC=10 all read 9")
print(f"  → Your sensor's noise floor is ~52 dB")
print(f"• Dynamic range: ~{max(db)-min(db):.0f} dB (from {min(db):.0f} to {max(db):.0f})")
print(f"• Recommend using LOG model (physically correct for sound pressure)")
print(f"• In Arduino: Use float math or lookup table for performance")
