#include "contest.hpp"
#include "ini.hpp"
#include "call_list.hpp"
#include "log.hpp"
#include "dx_station.hpp"
#include "../audio/rnd_func.hpp"
#include <cstdlib>
#include <cmath>
#include <algorithm>

Contest::Contest() {
    auto& ini = Ini::instance();
    filter_.setSamplesInInput(ini.bufSize);
    filter_.setPoints(static_cast<int>(std::round(
        0.7f * DEFAULTRATE / ini.bandwidth)));
    filter_.setPasses(3);
    filter_.setGainDb(10.0f * std::log10(500.0f / ini.bandwidth));
    modulator_.setCarrierFreq(static_cast<float>(ini.pitch));
    modulator_.setSamplesPerSec(DEFAULTRATE);
    lastAppliedPitch_ = static_cast<float>(ini.pitch);
    agc_.maxOut = 20000.0f;
    agc_.setNoiseInDb(76.0f);
    agc_.setNoiseOutDb(76.0f);
    agc_.setAttackSamples(28);
    agc_.setHoldSamples(28);
    agc_.agcEnabled = true;
    qsb_.qsbLevel = ini.qsb ? 1.0f : 0.0f;
    ritHz_.store(ini.rit);
    pitchHz_.store(static_cast<float>(ini.pitch));
    monVol_.store(1.0f);

    // Wire MyStation events to DX station dispatcher
    myStation.onFinishedSending = [this]() {
        auto& sIni = Ini::instance();
        // Spawn new callers on CQ or TU (pileup mode)
        bool isCq = myStation.msg.count(StationMessage::CQ) > 0;
        bool isTu = myStation.msg.count(StationMessage::TU) > 0
                 && myStation.msg.count(StationMessage::HisCall) > 0;
        if (isCq || isTu) {
            int n = rndPoisson(static_cast<float>(sIni.activity) / 2.0f);
            for (int i = 0; i < n; ++i) stnColl.addDx();
        }
        // Tell all stations I finished sending
        stnColl.dispatchEvent(StationEvent::MeFinished);
    };
    myStation.onStartedSending = [this]() {
        stnColl.dispatchEvent(StationEvent::MeStarted);
    };
}

Contest::~Contest() { stop(); }

void Contest::start() {
    auto& ini = Ini::instance();
    clearLog();
    stnColl.clear();
    while (!macroQueue_.empty()) macroQueue_.pop();
    qsoCount   = 0;
    points     = 0;
    elapsedSec = 0.0;
    blockCount_      = 0;
    nextSpawnBlock_  = 0;
    durationFired_   = false;
    ritPhase_        = 0.0f;
    state = ContestState::Running;

    agc_.maxOut = 20000.0f;
    // Re-sync settings
    myStation.myCall = ini.call;
    myStation.wpm    = ini.wpm;
    myStation.pitch  = 0;
    myStation.setPitch(0);
    pitchHz_.store(static_cast<float>(ini.pitch));
    lastAppliedPitch_ = 0.0f;
    modulator_.setCarrierFreq(static_cast<float>(ini.pitch));
    lastAppliedPitch_ = static_cast<float>(ini.pitch);
    filter_.setPoints(static_cast<int>(std::round(
        0.7f * DEFAULTRATE / ini.bandwidth)));
    filter_.setGainDb(10.0f * std::log10(500.0f / ini.bandwidth));
    qsb_.qsbLevel = ini.qsb ? 1.0f : 0.0f;

    loadCallList("calls.bin");
    DxStation::myStation_ = &myStation;  // give DX stations access to operator state
    spawnStations();

    if (ini.saveWav)
        wav_.openWrite("contest.wav", DEFAULTRATE);
}

void Contest::stop() {
    state = ContestState::Stopped;
    stnColl.clear();
    myStation.envelope.clear();
    myStation.state = StationState::Listening;
    while (!macroQueue_.empty()) macroQueue_.pop();
    if (wav_.isOpen()) wav_.close();
}

void Contest::pause()  { state = ContestState::Paused; }
void Contest::resume() { state = ContestState::Running; }

// ── macro queue ───────────────────────────────────────────────────────────────

void Contest::queueMsg(StationMessage m) {
    macroQueue_.push(m);
    if (myStation.state != StationState::Sending)
        flushMacroQueue();
}

void Contest::flushMacroQueue() {
    if (macroQueue_.empty()) return;
    StationMessage m = macroQueue_.front();
    macroQueue_.pop();

    if (m == StationMessage::HisCall)
        myStation.sendHisCall(pendingHisCall_);
    else if (m == StationMessage::NR) {
        myStation.nr = pendingNr_;
        myStation.sendMsg(StationMessage::NR);
    } else {
        myStation.sendMsg(m);
    }
}

// ── user actions ──────────────────────────────────────────────────────────────

void Contest::userSentCq() {
    while (!macroQueue_.empty()) macroQueue_.pop();
    pendingHisCall_.clear();
    myStation.sendCq();
}

void Contest::userSentExchange(const std::string& hisCall, int hisNr) {
    pendingHisCall_ = hisCall;
    pendingNr_      = hisNr;

    // Log the QSO attempt
    Qso q;
    q.t    = elapsedSec / 86400.0;
    q.call = hisCall;
    q.rst  = 599;
    q.nr   = hisNr;

    // Find matching DX station for true values
    auto* dx = stnColl.findByCall(hisCall);
    if (dx) {
        q.trueCall = dx->myCall;
        q.trueRst  = dx->rst;
        q.trueNr   = dx->nr;
        // Don't call heardMyCall() — the DX station FSM handles this via evMeFinished
    }
    // Check dupe
    for (int i = 0; i + 1 < static_cast<int>(gQsoList.size()); ++i)
        if (gQsoList[i].call == hisCall && gQsoList[i].err == "   ")
            q.dupe = true;

    gQsoList.push_back(q);
    gCallSent = true;
    gNrSent   = true;

    // Queue: his call + NR (don't clear queue - allow chaining)
    while (!macroQueue_.empty()) macroQueue_.pop();
    macroQueue_.push(StationMessage::HisCall);
    macroQueue_.push(StationMessage::NR);
    if (myStation.state != StationState::Sending)
        flushMacroQueue();
}

void Contest::userSentTuAndLog() {
    // Validate and score the last QSO
    if (!gQsoList.empty()) {
        checkErr();
        auto& q = gQsoList.back();
        if (q.err == "   ") {
            ++qsoCount;
            ++points;
            if (onScoreChanged) onScoreChanged(qsoCount, points);
        }
        if (onQsoLogged) onQsoLogged(q);
    }

    pendingHisCall_.clear();
    gCallSent = false;
    gNrSent   = false;

    // Queue TU then auto-CQ
    while (!macroQueue_.empty()) macroQueue_.pop();
    macroQueue_.push(StationMessage::TU);
    macroQueue_.push(StationMessage::CQ);
    if (myStation.state != StationState::Sending)
        flushMacroQueue();
}

void Contest::userSentAgn() {
    while (!macroQueue_.empty()) macroQueue_.pop();
    myStation.sendAgn();
}

void Contest::userSentQm() {
    // Append ?  without clearing queue (allow chaining)
    macroQueue_.push(StationMessage::Qm);
    if (myStation.state != StationState::Sending)
        flushMacroQueue();
}

void Contest::userSentB4() {
    while (!macroQueue_.empty()) macroQueue_.pop();
    myStation.sendMsg(StationMessage::B4);
}

void Contest::userSentNil() {
    while (!macroQueue_.empty()) macroQueue_.pop();
    myStation.sendMsg(StationMessage::Nil);
}

void Contest::userSentHisCall(const std::string& hisCall) {
    if (!hisCall.empty()) pendingHisCall_ = hisCall;
    macroQueue_.push(StationMessage::HisCall);
    if (myStation.state != StationState::Sending)
        flushMacroQueue();
}

void Contest::userSentMyCall() {
    macroQueue_.push(StationMessage::MyCall);
    if (myStation.state != StationState::Sending)
        flushMacroQueue();
}

void Contest::userTyped(const std::string& text) {
    if (onCallDecoded) onCallDecoded(text);
}

// ── settings ──────────────────────────────────────────────────────────────────

void Contest::setRit(int hz) {
    auto& ini = Ini::instance();
    ini.rit = std::clamp(hz, -1000, 1000);
    // Audio thread reads ritHz_ atomically; modulator is updated inside getAudioBlock()
    ritHz_.store(ini.rit);
}

void Contest::setRunMode(RunMode m) {
    Ini::instance().runMode = m;
}

void Contest::applyBandwidth(int hz) {
    auto& ini = Ini::instance();
    ini.bandwidth = std::clamp(hz, 100, 2000);
    filter_.setPoints(static_cast<int>(std::round(
        0.7f * DEFAULTRATE / ini.bandwidth)));
    filter_.setGainDb(10.0f * std::log10(500.0f / ini.bandwidth));
}

void Contest::applyPitch(int hz) {
    auto& ini = Ini::instance();
    ini.pitch = std::clamp(hz, 300, 900);
    pitchHz_.store(static_cast<float>(ini.pitch));
    // modulator_.setCarrierFreq() will be called from audio thread on next block
}

void Contest::applyWpm(int wpm) {
    auto& ini = Ini::instance();
    ini.wpm = std::clamp(wpm, 10, 60);
    myStation.wpm = ini.wpm;
}

void Contest::setSelfMonVolume(int db) {
    // db range -30..+20, linear gain = 10^(db/20)
    monVol_.store(std::pow(10.0f, static_cast<float>(db) / 20.0f));
}

// ── audio ─────────────────────────────────────────────────────────────────────

void Contest::spawnStations() {
    auto& ini = Ini::instance();
    int target = 1 + ini.activity * 2;
    while (stnColl.activeDxCount() < target)
        stnColl.addDx();
    if (ini.qrn && (std::rand() % 8  == 0)) stnColl.addQrn();
    if (ini.qrm && (std::rand() % 12 == 0)) stnColl.addQrm();
}

TSingleArray Contest::getAudioBlock() {
    auto& ini = Ini::instance();
    if (state != ContestState::Running)
        return TSingleArray(ini.bufSize, 0.0f);

    // ── Update modulator carrier only when pitch setting genuinely changes ────
    // NOTE: RIT is handled entirely in IQ phase rotation; carrier = pitch only.
    // Do NOT compare against modulator_.carrierFreq (rounded actual) — it will
    // always differ from the requested pitch, resetting sampleNo_ every block.
    float requestedPitch = pitchHz_.load();
    if (std::fabs(requestedPitch - lastAppliedPitch_) > 0.5f) {
        modulator_.setCarrierFreq(requestedPitch);
        lastAppliedPitch_ = requestedPitch;
    }

    // ── Spawn new callers periodically ────────────────────────────────────────
    if (blockCount_ >= nextSpawnBlock_) {
        spawnStations();
        nextSpawnBlock_ = blockCount_ + static_cast<int>(
            rndGaussLim(5.0f, 3.0f) * DEFAULTRATE / ini.bufSize);
    }

    // ── Build IQ noise floor ───────────────────────────────────────────────────
    constexpr float NOISEAMP = 6000.0f;
    TReImArrays iq;
    setLengthReIm(iq, ini.bufSize);
    // rndUniform() is in [-1,1]; Pascal uses Random-0.5 ∈ [-0.5,0.5]
    for (int i = 0; i < ini.bufSize; ++i) {
        iq.Re[i] = 1.5f * NOISEAMP * rndUniform();
        iq.Im[i] = 1.5f * NOISEAMP * rndUniform();
    }

    // ── QRN impulse noise ────────────────────────────────────────────────────
    if (ini.qrn) {
        for (int i = 0; i < ini.bufSize; ++i)
            if (rndUniform() < 0.01f)
                iq.Re[i] = 60.0f * NOISEAMP * (rndUniform() - 0.5f);
    }

    // ── RX signal from DX stations (already in IQ) ───────────────────────────
    TReImArrays rx = stnColl.getBlock();
    for (int i = 0; i < ini.bufSize; ++i) {
        iq.Re[i] += rx.Re[i];
        iq.Im[i] += rx.Im[i];
    }

    // ── My TX monitor into IQ (BEFORE filter) ────────────────────────────────
    if (myStation.state == StationState::Sending && !myStation.envelope.empty()) {
        auto blk = myStation.getBlock();
        float monGain = monVol_.load();
        float rfg = 1.0f;  // QSK receive-gate (decaying)
        for (int i = 0; i < ini.bufSize; ++i) {
            float env = blk[i] / myStation.amplitude;  // 0..1
            if (ini.qsk) {
                // QSK: sidetone + gated RX (Pascal: Rfg decays with key)
                float target = 1.0f - env;
                if (rfg > target)
                    rfg = target;
                else
                    rfg = rfg * 0.997f + 0.003f;
                float smg = monGain;
                iq.Re[i] = smg * blk[i] + rfg * iq.Re[i];
                iq.Im[i] = smg * blk[i] + rfg * iq.Im[i];
            } else {
                // No QSK: mute RX, full sidetone
                iq.Re[i] = monGain * blk[i];
                iq.Im[i] = monGain * blk[i];
            }
        }
    }

    // ── Process IQ through filter / modulate / AGC ───────────────────────────
    TSingleArray processed = mixAndProcess(std::move(iq));

    // ── Tick all stations AFTER collecting audio (Pascal order) ────────────────
    stnColl.tick();
    myStation.tick();
    if (myStation.state == StationState::Listening && !macroQueue_.empty())
        flushMacroQueue();

    // ── Accounting ────────────────────────────────────────────────────────────
    elapsedSec += static_cast<double>(ini.bufSize) / DEFAULTRATE;
    ++blockCount_;

    if (onTimerTick) onTimerTick(elapsedSec);

    // ── Duration check (fire once) ────────────────────────────────────────────
    if (!durationFired_ && ini.duration > 0 &&
        elapsedSec >= static_cast<double>(ini.duration) * 60.0)
    {
        durationFired_ = true;
        state = ContestState::Paused;
        if (onDurationExpired) onDurationExpired();
    }

    return processed;
}

TSingleArray Contest::mixAndProcess(TReImArrays iq) {
    auto& ini = Ini::instance();

    // ── Apply QSB to IQ amplitude ────────────────────────────────────────────
    if (ini.qsb) {
        qsb_.applyTo(iq.Re);
        qsb_.applyTo(iq.Im);
    }

    // ── RIT phase rotation: rotate IQ by ritPhase_ ──────────────────────────
    // This shifts the received signal by RIT Hz relative to our pitch
    float rit  = static_cast<float>(ritHz_.load());
    float dPhi = TWO_PI * rit / DEFAULTRATE;
    for (size_t i = 0; i < iq.Re.size(); ++i) {
        float cs = std::cos(ritPhase_);
        float sn = std::sin(ritPhase_);
        float re = iq.Re[i] * cs - iq.Im[i] * sn;
        float im = iq.Re[i] * sn + iq.Im[i] * cs;
        iq.Re[i] = re;
        iq.Im[i] = im;
        ritPhase_ += dPhi;
    }
    while (ritPhase_ >  TWO_PI) ritPhase_ -= TWO_PI;
    while (ritPhase_ < -TWO_PI) ritPhase_ += TWO_PI;

    TReImArrays filtered = filter_.filter(iq);
    TSingleArray audio   = modulator_.modulate(filtered);
    TSingleArray result  = agc_.process(audio);

    if (wav_.isOpen())
        wav_.writeFrom(result.data(), static_cast<int>(result.size()));

    return result;
}

