#pragma once
#include "snd_types.hpp"
#include <vector>

class TVolumeControl {
public:
    float noiseInDb{76.0f};
    float noiseOutDb{76.0f};
    int   attackSamples{155};
    int   holdSamples{155};
    bool  agcEnabled{true};
    float maxOut{20000.0f};   // Pascal default: FMaxOut := 20000

    TVolumeControl();
    void reset();
    TSingleArray process(const TSingleArray& data);

    void setNoiseInDb(float v);
    void setNoiseOutDb(float v);
    void setAttackSamples(int v);
    void setHoldSamples(int v);

private:
    float noiseIn_{1.0f}, noiseOut_{2000.0f};
    float beta_{1.0f}, defaultGain_{1.0f};
    float envelope_{0.0f};

    int          len_{0}, bufIdx_{0};
    TSingleArray realBuf_, magBuf_, attackShape_;

    void calcBeta();
    void makeAttackShape();
    float calcAgcGain();
    float applyAgc(float v);
    float applyDefaultGain(float v);
};
