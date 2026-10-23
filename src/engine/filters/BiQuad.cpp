#include "engine/filters/BiQuad.h"

namespace eqapo {

BiQuad::BiQuad(Type type, double dbGain, double freq, double srate, double bandwidthOrQOrS, bool isBandwidthOrS) {
    double A;
    if (type == PEAKING || type == LOW_SHELF || type == HIGH_SHELF) A = std::pow(10.0, dbGain / 40.0);
    else A = std::pow(10.0, dbGain / 20.0);
    double omega = 2.0 * M_PI * freq / srate;
    double sn = std::sin(omega);
    double cs = std::cos(omega);
    double alpha;

    if (!isBandwidthOrS)  // Q
        alpha = sn / (2.0 * bandwidthOrQOrS);
    else if (type == LOW_SHELF || type == HIGH_SHELF)  // shelf slope S
        alpha = sn / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / bandwidthOrQOrS - 1.0) + 2.0);
    else  // bandwidth in octaves
        alpha = sn * std::sinh(M_LN2 / 2.0 * bandwidthOrQOrS * omega / sn);

    double beta = 2.0 * std::sqrt(A) * alpha;

    double b0 = 1, b1 = 0, b2 = 0, a0n = 1, a1n = 0, a2n = 0;
    switch (type) {
        case LOW_PASS:
            b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2;
            a0n = 1 + alpha; a1n = -2 * cs; a2n = 1 - alpha;
            break;
        case HIGH_PASS:
            b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2;
            a0n = 1 + alpha; a1n = -2 * cs; a2n = 1 - alpha;
            break;
        case BAND_PASS:
            b0 = alpha; b1 = 0; b2 = -alpha;
            a0n = 1 + alpha; a1n = -2 * cs; a2n = 1 - alpha;
            break;
        case NOTCH:
            b0 = 1; b1 = -2 * cs; b2 = 1;
            a0n = 1 + alpha; a1n = -2 * cs; a2n = 1 - alpha;
            break;
        case ALL_PASS:
            b0 = 1 - alpha; b1 = -2 * cs; b2 = 1 + alpha;
            a0n = 1 + alpha; a1n = -2 * cs; a2n = 1 - alpha;
            break;
        case PEAKING:
            b0 = 1 + alpha * A; b1 = -2 * cs; b2 = 1 - alpha * A;
            a0n = 1 + alpha / A; a1n = -2 * cs; a2n = 1 - alpha / A;
            break;
        case LOW_SHELF:
            b0 = A * ((A + 1) - (A - 1) * cs + beta);
            b1 = 2 * A * ((A - 1) - (A + 1) * cs);
            b2 = A * ((A + 1) - (A - 1) * cs - beta);
            a0n = (A + 1) + (A - 1) * cs + beta;
            a1n = -2 * ((A - 1) + (A + 1) * cs);
            a2n = (A + 1) + (A - 1) * cs - beta;
            break;
        case HIGH_SHELF:
            b0 = A * ((A + 1) + (A - 1) * cs + beta);
            b1 = -2 * A * ((A - 1) + (A + 1) * cs);
            b2 = A * ((A + 1) + (A - 1) * cs - beta);
            a0n = (A + 1) - (A - 1) * cs + beta;
            a1n = 2 * ((A - 1) - (A + 1) * cs);
            a2n = (A + 1) - (A - 1) * cs - beta;
            break;
    }

    setCoefficients(b0 / a0n, b1 / a0n, b2 / a0n, a1n / a0n, a2n / a0n);
}

void BiQuad::setCoefficients(double nb0, double nb1, double nb2, double na1, double na2) {
    a0 = nb0;
    a[0] = nb1;
    a[1] = nb2;
    a[2] = na1;
    a[3] = na2;
    x1 = x2 = y1 = y2 = 0.0;
}

double BiQuad::gainAt(double freq, double srate) const {
    double omega = 2.0 * M_PI * freq / srate;
    double sn = std::sin(omega / 2.0);
    double phi = sn * sn;
    double nb0 = a0, nb1 = a[0], nb2 = a[1];
    double na0 = 1.0, na1 = a[2], na2 = a[3];
    double num = std::pow(nb0 + nb1 + nb2, 2) - 4 * (nb0 * nb1 + 4 * nb0 * nb2 + nb1 * nb2) * phi + 16 * nb0 * nb2 * phi * phi;
    double den = std::pow(na0 + na1 + na2, 2) - 4 * (na0 * na1 + 4 * na0 * na2 + na1 * na2) * phi + 16 * na0 * na2 * phi * phi;
    return 10 * std::log10(num) - 10 * std::log10(den);
}

}  // namespace eqapo
