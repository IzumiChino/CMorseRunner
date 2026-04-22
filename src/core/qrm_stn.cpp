#include "qrm_stn.hpp"
#include "call_list.hpp"
#include "ini.hpp"
#include "../audio/rnd_func.hpp"
#include <cstdlib>
#include <cmath>

QrmStation::QrmStation() {
    myCall    = pickCall();
    hisCall   = pickCall();
    nr        = 1 + std::rand() % 999;
    rst       = 599;
    wpm       = 20 + std::rand() % 20;
    // pitch = signed Hz offset from carrier
    pitch     = static_cast<int>(std::round(rndGaussLim(0.0f, 300.0f)));
    amplitude = 500.0f + 15000.0f * (static_cast<float>(std::rand()) / RAND_MAX);
    setPitch(pitch);

    // Send a random contest exchange
    int r = std::rand() % 4;
    switch (r) {
        case 0: sendMsg(StationMessage::DeMyCall1);   break;
        case 1: sendMsg(StationMessage::DeMyCallNr1); break;
        case 2: sendMsg(StationMessage::TU);          break;
        case 3: sendMsg(StationMessage::CQ);          break;
    }
}

void QrmStation::processEvent(StationEvent ev) {
    if (ev == StationEvent::MsgSent) done = true;
}
