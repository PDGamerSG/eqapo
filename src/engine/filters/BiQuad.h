#pragma once
#include <cfloat>
#include <cmath>

namespace eqapo {

// Second-order IIR section, RBJ cookbook coefficients, Equalizer APO parameterisation.
class BiQuad {
public:
    enum Type { LOW_PASS, HIGH_PASS, BAND_PASS, NOTCH, ALL_PASS, PEAKING, LOW_SHELF, HIGH_SHELF };

    BiQuad() = default;
    // bandwidthOrQOrS is Q when !isBandwidthOrS, otherwise bandwidth in octaves
    // (non-shelf types) or shelf slope S (shelf types).
    BiQuad(Type type, double dbGain, double freq, double srate, double bandwidthOrQOrS, bool isBandwidthOrS);

    inline void removeDenormals() {
        if (std::fabs(x1) < DBL_MIN) x1 = 0.0;
        if (std::fabs(x2) < DBL_MIN) x2 = 0.0;
        if (std::fabs(y1) < DBL_MIN) y1 = 0.0;
        if (std::fabs(y2) < DBL_MIN) y2 = 0.0;
    }

    inline double process(double sample) {
        double result = a0 * sample + a[1] * x2 + a[0] * x1 - a[3] * y2 - a[2] * y1;
        x2 = x1;
        x1 = sample;
        y2 = y1;
        y1 = result;
        return result;
    }

    // Sets normalised coefficients: b0, b1, b2, a1, a2 (a0 implied 1).
    void setCoefficients(double b0, double b1, double b2, double a1, double a2);
    // Magnitude response in dB at `freq` for sample rate `srate`.
    double gainAt(double freq, double srate) const;

    double b0() const { return a0; }
    double b1() const { return a[0]; }
    double b2() const { return a[1]; }
    double a1() const { return a[2]; }
    double a2() const { return a[3]; }

private:
    // a[0]=b1 a[1]=b2 a[2]=a1 a[3]=a2 (normalised), a0 = b0 (normalised)
    double a[4] = {0, 0, 0, 0};
    double a0 = 1.0;
    double x1 = 0, x2 = 0;
    double y1 = 0, y2 = 0;
};

}  // namespace eqapo
