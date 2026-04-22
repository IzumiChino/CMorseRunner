#include "rnd_func.hpp"
#include "snd_types.hpp"
#include "../core/ini.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

static float rnd01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

float rndNormal() {
    for (;;) {
        float u = rnd01();
        if (u > 0.0f)
            return std::sqrt(-2.0f * std::log(u)) * std::cos(6.28318530f * rnd01());
    }
}

float rndGaussLim(float mean, float lim) {
    float r = mean + rndNormal() * 0.5f * lim;
    return std::clamp(r, mean - lim, mean + lim);
}

float rndRayleigh(float mean) {
    float u1 = rnd01(), u2 = rnd01();
    if (u1 <= 0.0f) u1 = 1e-10f;
    if (u2 <= 0.0f) u2 = 1e-10f;
    return mean * std::sqrt(-std::log(u1) - std::log(u2));
}

float rndUniform() {
    return 2.0f * rnd01() - 1.0f;
}

float rndUShaped() {
    return std::sin(3.14159265f * (rnd01() - 0.5f));
}

int rndPoisson(float mean) {
    float g = std::exp(-mean);
    float t = 1.0f;
    for (int i = 0; i <= 30; ++i) {
        t *= rnd01();
        if (t <= g) return i;
    }
    return 30;
}

float blocksToSeconds(float blocks) {
    return blocks * Ini::instance().bufSize / static_cast<float>(DEFAULTRATE);
}

int secondsToBlocks(float sec) {
    return static_cast<int>(std::round(
        static_cast<float>(DEFAULTRATE) / Ini::instance().bufSize * sec));
}
