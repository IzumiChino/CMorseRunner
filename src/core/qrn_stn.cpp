#include "qrn_stn.hpp"
#include "ini.hpp"
#include "../audio/rnd_func.hpp"
#include <cstdlib>
#include <cmath>

QrnStation::QrnStation() {
    auto& ini = Ini::instance();
    int dur = secondsToBlocks(static_cast<float>(std::rand()) / RAND_MAX) * ini.bufSize;
    envelope.resize(dur, 0.0f);
    amplitude = 1e5f * std::pow(10.0f, 2.0f * (static_cast<float>(std::rand()) / RAND_MAX));
    for (auto& v : envelope)
        if ((static_cast<float>(std::rand()) / RAND_MAX) < 0.01f)
            v = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * amplitude;
    state = StationState::Sending;
}

void QrnStation::processEvent(StationEvent ev) {
    if (ev == StationEvent::MsgSent) done = true;
}
