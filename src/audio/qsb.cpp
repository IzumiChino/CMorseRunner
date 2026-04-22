#include "qsb.hpp"
#include "rnd_func.hpp"
#include "../core/ini.hpp"
#include <cmath>

TQsb::TQsb() {
    filt_.passes = 3;
    setBandwidth(0.1f);
}

float TQsb::newGain() {
    auto c = filt_.filter(rndUniform(), rndUniform());
    float g = std::sqrt((c.Re * c.Re + c.Im * c.Im) * 3.0f * filt_.points);
    return g * qsbLevel + (1.0f - qsbLevel);
}

void TQsb::setBandwidth(float v) {
    bandwidth_ = v;
    int& bs = Ini::instance().bufSize;
    filt_.points = static_cast<int>(std::ceil(
        0.37f * DEFAULTRATE / (static_cast<float>(bs) / 4.0f * v)));
    filt_.reset();
    for (int i = 0; i < filt_.points * 3; ++i)
        gain_ = newGain();
}

void TQsb::applyTo(TSingleArray& arr) {
    int& bs = Ini::instance().bufSize;
    int subBlk = bs / 4;
    int blkCnt = static_cast<int>(arr.size()) / subBlk;

    for (int b = 0; b < blkCnt; ++b) {
        float nextGain = newGain();
        float dG = (nextGain - gain_) / subBlk;
        for (int i = 0; i < subBlk; ++i) {
            arr[b * subBlk + i] *= gain_;
            gain_ += dG;
        }
    }
}
