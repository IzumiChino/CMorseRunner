#pragma once
#include "snd_types.hpp"
#include <vector>

class TMovingAverage {
public:
    TMovingAverage();

    void reset();
    TReImArrays filter(const TReImArrays& data);

    int   points{129};
    int   passes{3};
    int   samplesInInput{512};
    float gainDb{0.0f};

    void setPoints(int v);
    void setPasses(int v);
    void setSamplesInInput(int v);
    void setGainDb(float v);

private:
    using Buf2D = std::vector<TSingleArray>;
    Buf2D bufRe_, bufIm_;
    float norm_{1.0f};

    void calcScale();
    TSingleArray doFilter(const TSingleArray& data, Buf2D& buf);
    void pushArray(const TSingleArray& src, TSingleArray& dst);
    void shiftArray(TSingleArray& dst, int count);
    void pass(const TSingleArray& src, TSingleArray& dst);
    TSingleArray getResult(const TSingleArray& src);
};
