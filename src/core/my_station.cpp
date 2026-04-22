#include "my_station.hpp"
#include "ini.hpp"

MyStation::MyStation() {
    auto& ini = Ini::instance();
    myCall    = ini.call;
    wpm       = ini.wpm;
    // MyStation pitch = 0: user transmits exactly at carrier (0 Hz offset)
    pitch     = 0;
    amplitude = 300000.0f;
    setPitch(pitch);
}

void MyStation::processEvent(StationEvent ev) {
    if (ev == StationEvent::MsgSent) {
        if (onFinishedSending) onFinishedSending();
    }
}

void MyStation::sendText(const std::string& text) {
    bool wasSending = (state == StationState::Sending);
    Station::sendText(text);
    if (!wasSending && state == StationState::Sending) {
        if (onStartedSending) onStartedSending();
    }
}

void MyStation::sendCq() {
    hisCall.clear();
    sendMsg(StationMessage::CQ);
}

void MyStation::sendExchange(const std::string& hisCallStr, int hisNr) {
    hisCall = hisCallStr;
    nr      = hisNr;
    sendMsg(StationMessage::HisCall);
    sendMsg(StationMessage::NR);
}

void MyStation::sendTu() {
    sendMsg(StationMessage::TU);
}

void MyStation::sendAgn() {
    sendMsg(StationMessage::Agn);
}

void MyStation::sendQm() {
    sendMsg(StationMessage::Qm);
}

void MyStation::sendMyCall() {
    sendMsg(StationMessage::MyCall);
}

void MyStation::sendHisCall(const std::string& hisCallStr) {
    hisCall = hisCallStr;
    sendMsg(StationMessage::HisCall);
}
