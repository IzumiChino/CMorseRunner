#pragma once
#include "snd_types.hpp"
#include <string>
#include <unordered_map>

class TKeyer {
public:
    int  Wpm{30};
    int  BufSize{DEFAULTBUFSIZE};
    int  Rate{DEFAULTRATE};
    std::string MorseMsg;
    int  TrueEnvelopeLen{0};

    TKeyer();

    std::string    encode(const std::string& txt) const;
    TSingleArray   getEnvelope();

    void setRiseTime(float v);
    float getRiseTime() const { return riseTime_; }

private:
    std::unordered_map<char, std::string> morse_;
    int          rampLen_{0};
    TSingleArray rampOn_, rampOff_;
    float        riseTime_{0.005f};

    void loadMorseTable();
    void makeRamp();
    float blackmanHarrisKernel(float x) const;
    TSingleArray blackmanHarrisStepResponse(int len) const;
};

// Global singleton (mirrors Pascal's global Keyer variable)
TKeyer& keyer();
