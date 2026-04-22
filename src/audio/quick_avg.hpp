#pragma once
#include "snd_types.hpp"
#include <vector>

class TQuickAverage {
public:
    int passes{3};
    int points{128};

    TQuickAverage();
    void reset();

    float    filter(float v);
    TComplex filter(float re, float im);

private:
    using Buf2D = std::vector<std::vector<double>>;
    Buf2D reBufs_, imBufs_;
    int   idx_{0}, prevIdx_{0};
    float scale_{1.0f};

    double doFilter(float v, Buf2D& bufs);
};
