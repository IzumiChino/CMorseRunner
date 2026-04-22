#pragma once
#include "../audio/snd_types.hpp"
#include "../audio/morse_key.hpp"
#include "ini.hpp"
#include <string>
#include <set>

constexpr int NEVER = 0x7FFFFFFF;

enum class StationMessage {
    None, CQ, NR, TU, MyCall, HisCall, B4, Qm, Nil, Garbage,
    R_NR, R_NR2, DeMyCall1, DeMyCall2, DeMyCallNr1, DeMyCallNr2,
    NrQm, LongCQ, MyCallNr2, Qrl, Qrl2, Qsy, Agn
};
using MsgSet = std::set<StationMessage>;

enum class StationState  { Listening, Copying, PreparingToSend, Sending };
enum class StationEvent  { Timeout, MsgSent, MeStarted, MeFinished };

class Station {
public:
    float       amplitude{1.0f};
    int         wpm{30};
    TSingleArray envelope;
    StationState state{StationState::Listening};

    int         nr{1}, rst{599};
    std::string myCall, hisCall;
    MsgSet      msg;
    std::string msgText;

    // BFO phase state
    float bfo{0.0f};
    float dPhi{0.0f};

    int   pitch{600};
    int   timeOut{NEVER};
    bool  nrWithError{false};

    explicit Station() = default;
    virtual ~Station() = default;

    void tick();
    virtual TSingleArray getBlock();
    virtual void processEvent(StationEvent ev) = 0;

    void sendMsg(StationMessage m);
    virtual void sendText(const std::string& text);
    void sendMorse(const std::string& morse);

    void setPitch(int p);
    float nextBfo();

protected:
    int sendPos_{0};
    std::string nrAsText();
};
