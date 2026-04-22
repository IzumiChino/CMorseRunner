#include "volume_ctl.hpp"
#include <cmath>
#include <algorithm>

TVolumeControl::TVolumeControl() {
    // Pascal defaults: FMaxOut=20000, NoiseIn/OutDb=76, Attack/Hold=28
    setNoiseInDb(76.0f);
    setNoiseOutDb(76.0f);
    setAttackSamples(28);
    setHoldSamples(28);
}

void TVolumeControl::setNoiseInDb(float v) {
    noiseInDb = v;
    noiseIn_ = std::pow(10.0f, 0.05f * v);
    calcBeta();
}

void TVolumeControl::setNoiseOutDb(float v) {
    noiseOutDb = v;
    noiseOut_ = std::min(0.25f * maxOut, std::pow(10.0f, 0.05f * v));
    calcBeta();
}

void TVolumeControl::setAttackSamples(int v) {
    attackSamples = std::max(1, v);
    makeAttackShape();
}

void TVolumeControl::setHoldSamples(int v) {
    holdSamples = std::max(1, v);
    makeAttackShape();
}

void TVolumeControl::calcBeta() {
    beta_        = noiseIn_ / std::log(maxOut / (maxOut - noiseOut_));
    defaultGain_ = noiseOut_ / noiseIn_;
}

void TVolumeControl::makeAttackShape() {
    len_ = 2 * (attackSamples + holdSamples) + 1;
    attackShape_.resize(len_);
    for (int i = 0; i < attackSamples; ++i) {
        float v = std::log(0.5f - 0.5f * std::cos(
            static_cast<float>(i + 1) * 3.14159265f / (attackSamples + 1)));
        attackShape_[i]           = v;
        attackShape_[len_ - 1 - i] = v;
    }
    reset();
}

void TVolumeControl::reset() {
    realBuf_.assign(len_, 0.0f);
    magBuf_.assign(len_,  0.0f);
    bufIdx_ = 0;
}

float TVolumeControl::calcAgcGain() {
    float envel = 1e-10f;
    int di = bufIdx_;
    for (int wi = 0; wi < len_; ++wi) {
        float s = magBuf_[di] + attackShape_[wi];
        if (s > envel) envel = s;
        if (++di == len_) di = 0;
    }
    envelope_ = envel;
    float e = std::exp(envel);
    return maxOut * (1.0f - std::exp(-e / beta_)) / e;
}

float TVolumeControl::applyAgc(float v) {
    realBuf_[bufIdx_] = v;
    magBuf_[bufIdx_]  = std::log(std::abs(v) + 1e-10f);
    bufIdx_ = (bufIdx_ + 1) % len_;
    int mid = (bufIdx_ + len_ / 2) % len_;
    return realBuf_[mid] * calcAgcGain();
}

float TVolumeControl::applyDefaultGain(float v) {
    return std::clamp(v * defaultGain_, -maxOut, maxOut);
}

TSingleArray TVolumeControl::process(const TSingleArray& data) {
    TSingleArray result(data.size());
    if (agcEnabled)
        for (size_t i = 0; i < data.size(); ++i)
            result[i] = applyAgc(data[i]);
    else
        for (size_t i = 0; i < data.size(); ++i)
            result[i] = applyDefaultGain(data[i]);
    return result;
}
