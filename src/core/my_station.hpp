#pragma once
#include "station.hpp"
#include <functional>

class MyStation : public Station {
public:
    MyStation();
    void processEvent(StationEvent ev) override;

    void sendCq();
    void sendExchange(const std::string& hisCallStr, int hisNr);
    void sendTu();
    void sendAgn();
    void sendQm();
    void sendMyCall();
    void sendHisCall(const std::string& hisCallStr);

    bool isSending() const { return state == StationState::Sending; }

    // Fired when I finish sending (dispatch evMeFinished to DX stations)
    std::function<void()> onFinishedSending;
    // Fired when I start sending (dispatch evMeStarted to DX stations)
    std::function<void()> onStartedSending;

    // Override to trigger events; contest wires the callbacks
    void sendText(const std::string& text) override;
};
