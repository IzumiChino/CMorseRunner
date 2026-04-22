#include "mixers.hpp"
#include <cmath>
#include <algorithm>

TModulator::TModulator() {
    calcSinCos();
}

void TModulator::setCarrierFreq(float freq) {
    carrierFreq = freq;
    sampleNo_ = 0;
    calcSinCos();
}

void TModulator::setSamplesPerSec(int rate) {
    samplesPerSec = rate;
    sampleNo_ = 0;
    calcSinCos();
}

void TModulator::calcSinCos() {
    int cnt = static_cast<int>(std::round(
        static_cast<float>(samplesPerSec) / carrierFreq));
    if (cnt < 1) cnt = 1;
    carrierFreq = static_cast<float>(samplesPerSec) / cnt;
    float dFi = TWO_PI / cnt;

    sn_.resize(cnt);
    cs_.resize(cnt);
    sn_[0] = 0.0f;
    cs_[0] = 1.0f;
    if (cnt > 1) {
        sn_[1] = std::sin(dFi);
        cs_[1] = std::cos(dFi);
    }
    for (int i = 2; i < cnt; ++i) {
        cs_[i] = cs_[1] * cs_[i-1] - sn_[1] * sn_[i-1];
        sn_[i] = cs_[1] * sn_[i-1] + sn_[1] * cs_[i-1];
    }
}

TSingleArray TModulator::modulate(const TReImArrays& data) {
    int n = static_cast<int>(data.Re.size());
    TSingleArray result(n);
    int cnt = static_cast<int>(sn_.size());
    for (int i = 0; i < n; ++i) {
        result[i] = data.Re[i] * sn_[sampleNo_] - data.Im[i] * cs_[sampleNo_];
        sampleNo_ = (sampleNo_ + 1) % cnt;
    }
    return result;
}
