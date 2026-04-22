#pragma once
#include "snd_types.hpp"

class TModulator {
public:
    TModulator();
    void setCarrierFreq(float freq);
    void setSamplesPerSec(int rate);
    TSingleArray modulate(const TReImArrays& data);

    float carrierFreq{600.0f};
    int   samplesPerSec{DEFAULTRATE};

private:
    int sampleNo_{0};
    TSingleArray sn_, cs_;
    void calcSinCos();
};
