#include "mov_avg.hpp"
#include <cmath>
#include <algorithm>

TMovingAverage::TMovingAverage() {
    reset();
}

void TMovingAverage::setPoints(int v)         { points = v; reset(); }
void TMovingAverage::setPasses(int v)         { passes = v; reset(); }
void TMovingAverage::setSamplesInInput(int v) { samplesInInput = v; reset(); }
void TMovingAverage::setGainDb(float v)       { gainDb = v; calcScale(); }

void TMovingAverage::calcScale() {
    norm_ = std::pow(10.0f, 0.05f * gainDb) *
            std::pow(static_cast<float>(points), -static_cast<float>(passes));
}

void TMovingAverage::reset() {
    int bufLen = samplesInInput + points;
    bufRe_.assign(passes + 1, TSingleArray(bufLen, 0.0f));
    bufIm_.assign(passes + 1, TSingleArray(bufLen, 0.0f));
    calcScale();
}

void TMovingAverage::pushArray(const TSingleArray& src, TSingleArray& dst) {
    int srcLen = static_cast<int>(src.size());
    int dstLen = static_cast<int>(dst.size());
    int keep   = dstLen - srcLen;
    std::move(dst.begin() + srcLen, dst.end(), dst.begin());
    std::copy(src.begin(), src.end(), dst.begin() + keep);
}

void TMovingAverage::shiftArray(TSingleArray& dst, int count) {
    std::move(dst.begin() + count, dst.end(), dst.begin());
}

void TMovingAverage::pass(const TSingleArray& src, TSingleArray& dst) {
    shiftArray(dst, samplesInInput);
    int hi = static_cast<int>(src.size()) - 1;
    for (int i = points; i <= hi; ++i)
        dst[i] = dst[i-1] - src[i - points] + src[i];
}

TSingleArray TMovingAverage::getResult(const TSingleArray& src) {
    TSingleArray result(samplesInInput);
    for (int i = 0; i < samplesInInput; ++i)
        result[i] = src[points + i] * norm_;
    return result;
}

TSingleArray TMovingAverage::doFilter(const TSingleArray& data, Buf2D& buf) {
    pushArray(data, buf[0]);
    for (int i = 1; i <= passes; ++i)
        pass(buf[i-1], buf[i]);
    return getResult(buf[passes]);
}

TReImArrays TMovingAverage::filter(const TReImArrays& data) {
    TReImArrays result;
    result.Re = doFilter(data.Re, bufRe_);
    result.Im = doFilter(data.Im, bufIm_);
    return result;
}
