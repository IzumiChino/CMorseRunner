#pragma once
#include "stn_coll.hpp"
#include "my_station.hpp"
#include "log.hpp"
#include "../audio/mixers.hpp"
#include "../audio/mov_avg.hpp"
#include "../audio/volume_ctl.hpp"
#include "../audio/qsb.hpp"
#include "../audio/wav_file.hpp"
#include <functional>
#include <string>
#include <queue>
#include <atomic>
#include <cmath>

enum class ContestState { Stopped, Running, Paused };

class Contest {
public:
    Contest();
    ~Contest();

    void start();
    void stop();
    void pause();
    void resume();

    // Called by audio thread each buffer — returns mixed RX+monitor audio
    TSingleArray getAudioBlock();

    // Queue a macro message (can be called multiple times to chain)
    void queueMsg(StationMessage m);

    // Convenience wrappers
    void userSentCq();
    // hisCall: the call typed in the UI entry box
    void userSentExchange(const std::string& hisCall, int hisNr);
    void userSentTuAndLog();   // TU + save QSO + auto CQ
    void userSentAgn();
    void userSentQm();
    void userSentB4();
    void userSentNil();
    // Pass current call text so HisCall macro always has the right callsign
    void userSentHisCall(const std::string& hisCall);
    void userSentMyCall();
    void userTyped(const std::string& text);

    void setRit(int hz);
    void setRunMode(RunMode m);
    void applyBandwidth(int hz);
    void applyPitch(int hz);
    void applyWpm(int wpm);
    void setSelfMonVolume(int db);

    // Callbacks for UI
    std::function<void(int qso, int pts)>        onScoreChanged;
    std::function<void(double elapsed)>          onTimerTick;
    std::function<void(const std::string&)>      onCallDecoded;
    std::function<void(const Qso&)>              onQsoLogged;   // fires after TU+log
    std::function<void()>                        onDurationExpired;

    ContestState state{ContestState::Stopped};
    int          qsoCount{0};
    int          points{0};
    double       elapsedSec{0.0};

    StnColl      stnColl;
    MyStation    myStation;

private:
    TModulator     modulator_;
    TMovingAverage filter_;
    TVolumeControl agc_;
    TQsb           qsb_;
    WavFile        wav_;

    int  blockCount_{0};
    int  nextSpawnBlock_{0};
    bool durationFired_{false};

    // Pending macro messages to send after current TX finishes
    std::queue<StationMessage> macroQueue_;
    std::string pendingHisCall_;
    int         pendingNr_{0};

    // Atomics for thread-safe UI→audio thread communication
    std::atomic<int>   ritHz_{0};
    std::atomic<float> pitchHz_{600.0f};    // ini.pitch, read by audio thread
    std::atomic<float> monVol_{1.0f};       // self-monitor linear gain

    // Continuous RIT phase accumulator (audio thread only, not an atomic)
    float ritPhase_{0.0f};
    // Last pitch given to modulator (compare vs requested to avoid spurious resets)
    float lastAppliedPitch_{0.0f};

    void spawnStations();
    void flushMacroQueue();
    TSingleArray mixAndProcess(TReImArrays iq);
};
