#pragma once
#include <vector>

constexpr float TWO_PI   = 6.28318530717958647692f;
constexpr float FOUR_PI  = 12.5663706143591729538f;
constexpr float HALF_PI  = 1.57079632679489661923f;
constexpr float SMALL_FLOAT = 1e-12f;

constexpr int DEFAULTRATE     = 11025;
constexpr int DEFAULTBUFSIZE  = 512;
constexpr int DEFAULTBUFCOUNT = 8;

using TSingleArray = std::vector<float>;

struct TComplex {
    float Re{}, Im{};
};
using TComplexArray = std::vector<TComplex>;

struct TReImArrays {
    TSingleArray Re, Im;
};

inline void setLengthReIm(TReImArrays& arr, int len) {
    arr.Re.assign(len, 0.0f);
    arr.Im.assign(len, 0.0f);
}
