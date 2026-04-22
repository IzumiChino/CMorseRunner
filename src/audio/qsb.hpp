#pragma once
#include "snd_types.hpp"
#include "quick_avg.hpp"

class TQsb {
public:
    float qsbLevel{1.0f};

    TQsb();
    void  applyTo(TSingleArray& arr);
    void  setBandwidth(float v);
    float getBandwidth() const { return bandwidth_; }

private:
    TQuickAverage filt_;
    float gain_{1.0f};
    float bandwidth_{0.1f};

    float newGain();
};
