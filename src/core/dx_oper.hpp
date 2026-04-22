#pragma once
#include "station.hpp"
#include <string>
#include <set>

// Mirrors Pascal TOperatorState
enum class OperState {
    NeedPrevEnd,   // osNeedPrevEnd — waiting for previous CQ/TU to end
    NeedQso,       // osNeedQso     — ready to call in response to CQ
    NeedNr,        // osNeedNr      — heard my call, waiting for exchange NR
    NeedCall,      // osNeedCall    — heard NR but call was garbled
    NeedCallNr,    // osNeedCallNr  — neither call nor NR heard correctly
    NeedEnd,       // osNeedEnd     — sent R NR, waiting for TU
    Done,
    Failed
};

struct DxOper {
    OperState   state{OperState::NeedPrevEnd};
    std::string myCall;   // DX station's own callsign
    int         skills{2};
    int         patience{5};
    int         repeatCnt{1};

    // Set state + reset patience
    void setState(OperState s);

    // Returns send delay in blocks (before replying)
    int getSendDelay(int wpm) const;

    // Returns reply timeout in blocks (how long to wait for operator reply)
    int getReplyTimeout(int wpm) const;

    // Returns the message to send given current state
    StationMessage getReply() const;

    // Process messages received from the operator.
    // msgs:        StationMessages in operator's last transmission
    // sentHisCall: the callsign operator addressed (may differ from myCall)
    void receivedMsg(const std::set<StationMessage>& msgs, const std::string& sentHisCall);

private:
    void decPatience();
    // 0=no match, 1=partial/garbled, 2=exact
    int checkMyCall(const std::string& sentCall) const;
};
