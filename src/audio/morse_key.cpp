#include "morse_key.hpp"
#include "morse_table.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

TKeyer& keyer() {
    static TKeyer instance;
    return instance;
}

TKeyer::TKeyer() {
    loadMorseTable();
    makeRamp();
}

void TKeyer::loadMorseTable() {
    for (auto sv : MorseTable) {
        std::string s(sv);
        if (s.size() < 3 || s[1] != '[') continue;
        auto end = s.find(']');
        if (end == std::string::npos) continue;
        std::string code = s.substr(2, end - 2) + " ";
        // single char key
        char ch = s[0];
        morse_[ch] = code;
    }
}

void TKeyer::setRiseTime(float v) {
    riseTime_ = v;
    makeRamp();
}

float TKeyer::blackmanHarrisKernel(float x) const {
    constexpr float a0 = 0.35875f, a1 = 0.48829f, a2 = 0.14128f, a3 = 0.01168f;
    return a0 - a1*std::cos(2*TWO_PI/2*x) + a2*std::cos(4*TWO_PI/2*x) - a3*std::cos(6*TWO_PI/2*x);
}

TSingleArray TKeyer::blackmanHarrisStepResponse(int len) const {
    TSingleArray r(len);
    for (int i = 0; i < len; ++i)
        r[i] = blackmanHarrisKernel(static_cast<float>(i) / len);
    for (int i = 1; i < len; ++i)
        r[i] += r[i-1];
    float scale = 1.0f / r[len-1];
    for (auto& v : r) v *= scale;
    return r;
}

void TKeyer::makeRamp() {
    rampLen_ = static_cast<int>(std::round(2.7f * riseTime_ * Rate));
    if (rampLen_ < 1) rampLen_ = 1;
    rampOn_ = blackmanHarrisStepResponse(rampLen_);
    rampOff_.resize(rampLen_);
    for (int i = 0; i < rampLen_; ++i)
        rampOff_[rampLen_ - 1 - i] = rampOn_[i];
}

std::string TKeyer::encode(const std::string& txt) const {
    std::string result;
    for (char c : txt) {
        if (c == ' ' || c == '_') {
            result += ' ';
        } else {
            char uc = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            auto it = morse_.find(uc);
            if (it != morse_.end())
                result += it->second;
        }
    }
    if (!result.empty())
        result.back() = '~';
    return result;
}

TSingleArray TKeyer::getEnvelope() {
    // count units
    int unitCnt = 0;
    for (char c : MorseMsg) {
        switch (c) {
            case '.': unitCnt += 2; break;
            case '-': unitCnt += 4; break;
            case ' ': unitCnt += 2; break;
            case '~': unitCnt += 1; break;
        }
    }

    int samplesInUnit = static_cast<int>(std::round(0.1f * Rate * 12.0f / Wpm));
    TrueEnvelopeLen = unitCnt * samplesInUnit + rampLen_;
    int len = BufSize * static_cast<int>(std::ceil(static_cast<float>(TrueEnvelopeLen) / BufSize));
    TSingleArray result(len, 0.0f);

    int p = 0;
    auto addRampOn = [&]() {
        std::copy(rampOn_.begin(), rampOn_.end(), result.begin() + p);
        p += rampLen_;
    };
    auto addRampOff = [&]() {
        std::copy(rampOff_.begin(), rampOff_.end(), result.begin() + p);
        p += rampLen_;
    };
    auto addOn = [&](int dur) {
        int count = dur * samplesInUnit - rampLen_;
        std::fill(result.begin() + p, result.begin() + p + count, 1.0f);
        p += count;
    };
    auto addOff = [&](int dur) {
        p += dur * samplesInUnit - rampLen_;
    };

    for (char c : MorseMsg) {
        switch (c) {
            case '.': addRampOn(); addOn(1); addRampOff(); addOff(1); break;
            case '-': addRampOn(); addOn(3); addRampOff(); addOff(1); break;
            case ' ': addOff(2); break;
            case '~': addOff(1); break;
        }
    }
    return result;
}
