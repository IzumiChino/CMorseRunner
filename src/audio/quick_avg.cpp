#include "quick_avg.hpp"
#include <cmath>

TQuickAverage::TQuickAverage() {
    reset();
}

void TQuickAverage::reset() {
    reBufs_.assign(passes + 1, std::vector<double>(points, 0.0));
    imBufs_.assign(passes + 1, std::vector<double>(points, 0.0));
    scale_ = static_cast<float>(std::pow(static_cast<double>(points), -passes));
    idx_     = 0;
    prevIdx_ = points - 1;
}

double TQuickAverage::doFilter(float v, Buf2D& bufs) {
    double result = v;
    for (int p = 1; p <= passes; ++p) {
        double vv = result;
        result = bufs[p][prevIdx_] - bufs[p-1][idx_] + vv;
        bufs[p-1][idx_] = vv;
    }
    bufs[passes][idx_] = result;
    return result * scale_;
}

float TQuickAverage::filter(float v) {
    float r = static_cast<float>(doFilter(v, reBufs_));
    prevIdx_ = idx_;
    idx_ = (idx_ + 1) % points;
    return r;
}

TComplex TQuickAverage::filter(float re, float im) {
    TComplex r;
    r.Re = static_cast<float>(doFilter(re, reBufs_));
    r.Im = static_cast<float>(doFilter(im, imBufs_));
    prevIdx_ = idx_;
    idx_ = (idx_ + 1) % points;
    return r;
}
