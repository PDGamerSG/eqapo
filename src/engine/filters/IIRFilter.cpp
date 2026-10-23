#include "engine/filters/IIRFilter.h"

#include <cfloat>
#include <cmath>

#include "engine/Log.h"

namespace eqapo {

IIRFilter::IIRFilter(const std::vector<double>& c) {
    order_ = static_cast<unsigned>(c.size() / 2 - 1);
    a_.resize(order_);
    b_.resize(order_);
    double a0 = c[order_ + 1];
    b0_ = c[0] / a0;
    for (unsigned i = 0; i < order_; i++) {
        b_[i] = c[i + 1] / a0;
        a_[i] = -c[i + order_ + 2] / a0;
    }
}

Channels IIRFilter::initialize(float, unsigned, const Channels& channelNames) {
    channelCount_ = channelNames.size();
    x_.assign(order_ * channelCount_, 0.0);
    y_.assign(order_ * channelCount_, 0.0);
    return channelNames;
}

void IIRFilter::process(float** output, float** input, unsigned frameCount) {
    const unsigned order = order_;
    for (size_t c = 0; c < channelCount_; c++) {
        const float* in = input[c];
        float* out = output[c];
        double* xo = x_.data() + c * order;
        double* yo = y_.data() + c * order;
        for (unsigned j = 0; j < frameCount; j++) {
            double sample = in[j];
            double sum = b0_ * sample;
            for (unsigned k = order - 1; k > 0; k--) {
                sum += b_[k] * xo[k];
                xo[k] = xo[k - 1];
            }
            sum += b_[0] * xo[0];
            for (unsigned k = order - 1; k > 0; k--) {
                sum += a_[k] * yo[k];
                yo[k] = yo[k - 1];
            }
            sum += a_[0] * yo[0];
            xo[0] = sample;
            yo[0] = sum;
            out[j] = static_cast<float>(sum);
        }
    }
    for (double& v : x_)
        if (std::fabs(v) < DBL_MIN) v = 0.0;
    for (double& v : y_)
        if (std::fabs(v) < DBL_MIN) v = 0.0;
}

std::string IIRFilter::describe() const { return format("IIR order %u", order_); }

}  // namespace eqapo
