#include "contest.hpp"
#include "ini.hpp"
#include "call_list.hpp"
#include "log.hpp"
#include "dx_station.hpp"
#include "../audio/rnd_func.hpp"
#include <cstdlib>
#include <cmath>
#include <algorithm>

static int randomDxWpm(int targetWpm)
{
	int delta = (std::rand() % 5) - 2;

	return std::clamp(targetWpm + delta, 10, 60);
}

Contest::Contest()
{
	auto &ini = Ini::instance();
	filter_.setSamplesInInput(ini.bufSize);
	filter_.setPoints(static_cast<int>(
		std::round(0.7f * DEFAULTRATE / ini.bandwidth)));
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
		auto &sIni = Ini::instance();
		// Spawn new callers on CQ or TU (pileup / competition style)
		bool isCq = myStation.msg.count(StationMessage::CQ) > 0;
		bool isTu = myStation.msg.count(StationMessage::TU) > 0;
		if (sIni.runMode != RunMode::Single &&
			sIni.runMode != RunMode::Hst && (isCq || isTu)) {
			int n = rndPoisson(
				static_cast<float>(sIni.activity) / 2.0f);
			if (isCq && n == 0 && sIni.activity > 0 &&
				stnColl.activeDxCount() == 0)
				n = 1;
			if (isTu && n == 0 && sIni.activity > 0)
				n = 1;
			for (int i = 0; i < n; ++i) {
				stnColl.addDx();
				configureNewDx(stnColl.dx.back().get());
			}
		}
		// Tell all stations I finished sending
		stnColl.dispatchEvent(StationEvent::MeFinished);
	};
	myStation.onStartedSending = [this]() {
		stnColl.dispatchEvent(StationEvent::MeStarted);
	};
}

Contest::~Contest()
{
	stop();
}

void Contest::start()
{
	auto &ini = Ini::instance();
	clearLog();
	stnColl.clear();
	while (!macroQueue_.empty())
		macroQueue_.pop();
	qsoCount = 0;
	points = 0;
	elapsedSec = 0.0;
	blockCount_ = 0;
	durationFired_ = false;
	ritPhase_ = 0.0f;
	state = ContestState::Running;

	agc_.maxOut = 20000.0f;
	// Re-sync settings
	myStation.myCall = ini.call;
	myStation.wpm = ini.wpm;
	myStation.pitch = 0;
	myStation.setPitch(0);
	pitchHz_.store(static_cast<float>(ini.pitch));
	lastAppliedPitch_ = 0.0f;
	modulator_.setCarrierFreq(static_cast<float>(ini.pitch));
	lastAppliedPitch_ = static_cast<float>(ini.pitch);
	filter_.setPoints(static_cast<int>(
		std::round(0.7f * DEFAULTRATE / ini.bandwidth)));
	filter_.setGainDb(10.0f * std::log10(500.0f / ini.bandwidth));
	qsb_.qsbLevel = ini.qsb ? 1.0f : 0.0f;

	loadCallList("calls.bin");
	DxStation::myStation_ =
		&myStation; // give DX stations access to operator state

	if (ini.saveWav)
		wav_.openWrite("contest.wav", DEFAULTRATE);
}

void Contest::stop()
{
	state = ContestState::Stopped;
	stnColl.clear();
	myStation.abortSend();
	myStation.envelope.clear();
	myStation.state = StationState::Listening;
	while (!macroQueue_.empty())
		macroQueue_.pop();
	if (wav_.isOpen())
		wav_.close();
}

void Contest::pause()
{
	state = ContestState::Paused;
}
void Contest::resume()
{
	state = ContestState::Running;
}

// ── macro queue ───────────────────────────────────────────────────────────────

void Contest::queueMsg(StationMessage m)
{
	macroQueue_.push(m);
	if (myStation.state != StationState::Sending)
		flushMacroQueue();
}

void Contest::flushMacroQueue()
{
	if (macroQueue_.empty())
		return;
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

void Contest::userSentCq()
{
	while (!macroQueue_.empty())
		macroQueue_.pop();
	pendingHisCall_.clear();
	myStation.sendCq();
}

void Contest::userSentCqText(const std::string &text)
{
	pendingHisCall_.clear();
	userSentText(text, MsgSet{StationMessage::CQ});
}

Qso Contest::makeQsoLogEntry(
	const std::string &hisCall, int copiedRst, int copiedNr)
{
	Qso q;
	q.t = elapsedSec / 86400.0;
	q.call = hisCall;
	q.call.erase(
		std::remove(q.call.begin(), q.call.end(), '?'), q.call.end());
	q.rst = copiedRst;
	q.nr = copiedNr;
	q.pfx = extractPrefix(q.call);

	auto *dx = stnColl.findByCall(q.call);
	if (dx) {
		q.trueCall = dx->myCall;
		q.trueRst = dx->rst;
		q.trueNr = dx->nr;
	}

	for (int i = 0; i + 1 < static_cast<int>(gQsoList.size()); ++i)
		if (gQsoList[i].call == q.call && gQsoList[i].err == "   ")
			q.dupe = true;

	return q;
}

void Contest::userSentExchange(const std::string &hisCall, int sentNr)
{
	pendingHisCall_ = hisCall;
	pendingNr_ = sentNr;

	gCallSent = true;
	gNrSent = true;

	// Queue: his call + NR (don't clear queue - allow chaining)
	while (!macroQueue_.empty())
		macroQueue_.pop();
	macroQueue_.push(StationMessage::HisCall);
	macroQueue_.push(StationMessage::NR);
	if (myStation.state != StationState::Sending)
		flushMacroQueue();
}

void Contest::userSentExchangeText(
	const std::string &text, const std::string &hisCall, int sentNr)
{
	pendingHisCall_ = hisCall;
	pendingNr_ = sentNr;
	myStation.hisCall = hisCall;
	myStation.nr = sentNr;
	gCallSent = true;
	gNrSent = true;

	userSentText(text, MsgSet{StationMessage::HisCall, StationMessage::NR});
}

void Contest::userSentTuAndLog(
	const std::string &hisCall, int copiedRst, int copiedNr)
{
	if (!hisCall.empty()) {
		gQsoList.push_back(
			makeQsoLogEntry(hisCall, copiedRst, copiedNr));
		checkErr();
		auto &q = gQsoList.back();
		if (q.err == "   ") {
			++qsoCount;
			++points;
			if (onScoreChanged)
				onScoreChanged(qsoCount, points);
		}
		if (onQsoLogged)
			onQsoLogged(q);
	}

	pendingHisCall_.clear();
	gCallSent = false;
	gNrSent = false;

	while (!macroQueue_.empty())
		macroQueue_.pop();
	macroQueue_.push(StationMessage::TU);
	if (myStation.state != StationState::Sending)
		flushMacroQueue();
}

void Contest::userSentTuAndLogText(const std::string &tuText,
	const std::string &cqText,
	const std::string &hisCall,
	int copiedRst,
	int copiedNr)
{
	if (!hisCall.empty()) {
		gQsoList.push_back(
			makeQsoLogEntry(hisCall, copiedRst, copiedNr));
		checkErr();
		auto &q = gQsoList.back();
		if (q.err == "   ") {
			++qsoCount;
			++points;
			if (onScoreChanged)
				onScoreChanged(qsoCount, points);
		}
		if (onQsoLogged)
			onQsoLogged(q);
	}

	pendingHisCall_.clear();
	gCallSent = false;
	gNrSent = false;

	while (!macroQueue_.empty())
		macroQueue_.pop();
	if (!myStation.isSending())
		myStation.msg.clear();
	myStation.msg.insert(StationMessage::TU);
	if (!tuText.empty())
		myStation.sendText(tuText);
	if (!cqText.empty()) {
		myStation.msg.insert(StationMessage::CQ);
		myStation.sendText(cqText);
	}
}

void Contest::userSentAgn()
{
	while (!macroQueue_.empty())
		macroQueue_.pop();
	myStation.sendAgn();
}

void Contest::userSentQm()
{
	// Append ?  without clearing queue (allow chaining)
	macroQueue_.push(StationMessage::Qm);
	if (myStation.state != StationState::Sending)
		flushMacroQueue();
}

void Contest::userSentB4()
{
	while (!macroQueue_.empty())
		macroQueue_.pop();
	myStation.sendMsg(StationMessage::B4);
}

void Contest::userSentNil()
{
	while (!macroQueue_.empty())
		macroQueue_.pop();
	myStation.sendMsg(StationMessage::Nil);
}

void Contest::userSentHisCall(const std::string &hisCall)
{
	if (!hisCall.empty())
		pendingHisCall_ = hisCall;
	macroQueue_.push(StationMessage::HisCall);
	if (myStation.state != StationState::Sending)
		flushMacroQueue();
}

void Contest::userSentMyCall()
{
	macroQueue_.push(StationMessage::MyCall);
	if (myStation.state != StationState::Sending)
		flushMacroQueue();
}

void Contest::userTyped(const std::string &text)
{
	if (onCallDecoded)
		onCallDecoded(text);
}

void Contest::userSentText(const std::string &text, const MsgSet &markers)
{
	if (text.empty())
		return;
	while (!macroQueue_.empty())
		macroQueue_.pop();
	if (!myStation.isSending())
		myStation.msg.clear();
	myStation.msg.insert(markers.begin(), markers.end());
	myStation.sendText(text);
}

void Contest::abortCurrentTransmit()
{
	while (!macroQueue_.empty())
		macroQueue_.pop();
	myStation.abortSend();
}

// ── settings ──────────────────────────────────────────────────────────────────

void Contest::setRit(int hz)
{
	auto &ini = Ini::instance();
	ini.rit = std::clamp(hz, -1000, 1000);
	// Audio thread reads ritHz_ atomically; modulator is updated inside getAudioBlock()
	ritHz_.store(ini.rit);
}

void Contest::setRunMode(RunMode m)
{
	Ini::instance().runMode = m;
}

void Contest::applyBandwidth(int hz)
{
	auto &ini = Ini::instance();
	ini.bandwidth = std::clamp(hz, 100, 2000);
	filter_.setPoints(static_cast<int>(
		std::round(0.7f * DEFAULTRATE / ini.bandwidth)));
	filter_.setGainDb(10.0f * std::log10(500.0f / ini.bandwidth));
}

void Contest::applyPitch(int hz)
{
	auto &ini = Ini::instance();
	ini.pitch = std::clamp(hz, 300, 900);
	pitchHz_.store(static_cast<float>(ini.pitch));
	// modulator_.setCarrierFreq() will be called from audio thread on next block
}

void Contest::applyWpm(int wpm)
{
	auto &ini = Ini::instance();
	ini.wpm = std::clamp(wpm, 10, 60);
	myStation.wpm = ini.wpm;
	for (auto &dx : stnColl.dx)
		dx->wpm = randomDxWpm(ini.wpm);
}

void Contest::setSelfMonVolume(int db)
{
	// db range -30..+20, linear gain = 10^(db/20)
	monVol_.store(std::pow(10.0f, static_cast<float>(db) / 20.0f));
}

void Contest::configureNewDx(DxStation *dx) const
{
	if (!dx)
		return;

	dx->wpm = randomDxWpm(Ini::instance().wpm);

	int minute = std::max(1, static_cast<int>(elapsedSec / 60.0) + 1);
	dx->nr = 1 +
		static_cast<int>(std::round(
			(static_cast<float>(std::rand()) / RAND_MAX) * minute *
			dx->oper.skills));
}

// ── audio ─────────────────────────────────────────────────────────────────────

void Contest::spawnStations()
{
	auto &ini = Ini::instance();
	if (ini.runMode == RunMode::Single && stnColl.activeDxCount() == 0) {
		stnColl.addDx();
		if (!stnColl.dx.empty()) {
			auto *dx = stnColl.dx.back().get();
			configureNewDx(dx);
			dx->oper.setState(OperState::NeedQso);
			dx->timeOut = dx->oper.getSendDelay(dx->wpm);
			dx->state = StationState::Listening;
		}
	} else if (ini.runMode == RunMode::Hst) {
		while (stnColl.activeDxCount() < ini.activity) {
			stnColl.addDx();
			auto *dx = stnColl.dx.back().get();
			configureNewDx(dx);
			dx->oper.setState(OperState::NeedQso);
			dx->timeOut = dx->oper.getSendDelay(dx->wpm);
			dx->state = StationState::Listening;
		}
	}
}

TSingleArray Contest::getAudioBlock()
{
	auto &ini = Ini::instance();
	if (state != ContestState::Running)
		return TSingleArray(ini.bufSize, 0.0f);

	// Match the original MorseRunner warm-up: keep the first few buffers
	// silent while the receive filter/AGC path fills and settles.
	if (blockCount_ < 5) {
		elapsedSec += static_cast<double>(ini.bufSize) / DEFAULTRATE;
		++blockCount_;
		if (onTimerTick)
			onTimerTick(elapsedSec);
		return TSingleArray(ini.bufSize, 0.0f);
	}

	// ── Update modulator carrier only when pitch setting genuinely changes ────
	// NOTE: RIT is handled entirely in IQ phase rotation; carrier = pitch only.
	// Do NOT compare against modulator_.carrierFreq (rounded actual) — it will
	// always differ from the requested pitch, resetting sampleNo_ every block.
	float requestedPitch = pitchHz_.load();
	if (std::fabs(requestedPitch - lastAppliedPitch_) > 0.5f) {
		modulator_.setCarrierFreq(requestedPitch);
		lastAppliedPitch_ = requestedPitch;
	}

	// ── Mode-specific caller maintenance ─────────────────────────────────────
	spawnStations();

	// ── Build IQ noise floor ───────────────────────────────────────────────────
	constexpr float NOISEAMP = 6000.0f;
	TReImArrays iq;
	setLengthReIm(iq, ini.bufSize);
	for (int i = 0; i < ini.bufSize; ++i) {
		iq.Re[i] = 1.5f * NOISEAMP * rndUniform();
		iq.Im[i] = 1.5f * NOISEAMP * rndUniform();
	}

	// ── QRN impulse noise ────────────────────────────────────────────────────
	if (ini.qrn) {
		TSingleArray qrnRe(ini.bufSize, 0.0f);
		TSingleArray qrnIm(ini.bufSize, 0.0f);
		float meanRe = 0.0f;
		float meanIm = 0.0f;

		for (int i = 0; i < ini.bufSize; ++i) {
			if (rndUniform() < 0.01f) {
				// Model atmospheric QRN as complex impulsive noise. Remove its
				// block DC afterwards so it cannot upconvert into a fixed tone
				// at the pitch frequency.
				qrnRe[i] = 30.0f * NOISEAMP * rndUniform();
				qrnIm[i] = 30.0f * NOISEAMP * rndUniform();
			}
			meanRe += qrnRe[i];
			meanIm += qrnIm[i];
		}

		meanRe /= ini.bufSize;
		meanIm /= ini.bufSize;
		for (int i = 0; i < ini.bufSize; ++i) {
			iq.Re[i] += qrnRe[i] - meanRe;
			iq.Im[i] += qrnIm[i] - meanIm;
		}

		// Original MorseRunner only spawns QRN burst stations occasionally
		// from the audio loop; do not create them as part of generic station
		// population maintenance.
		if (static_cast<float>(std::rand()) / RAND_MAX < 0.01f)
			stnColl.addQrn();
	}

	if (ini.qrm && static_cast<float>(std::rand()) / RAND_MAX < 0.0002f)
		stnColl.addQrm();

	// ── RX signal from DX stations (already in IQ) ───────────────────────────
	// Apply RIT only to received stations, not to the generated noise floor.
	// The original MorseRunner shifts station BFOs by RIT during mixing;
	// rotating the whole IQ block also shifts any residual DC/noise bias and
	// can leak a persistent carrier that tracks RIT.
	TReImArrays rx = stnColl.getBlock();
	float rit = static_cast<float>(ritHz_.load());
	float dPhi = TWO_PI * rit / DEFAULTRATE;
	for (int i = 0; i < ini.bufSize; ++i) {
		float cs = std::cos(ritPhase_);
		float sn = std::sin(ritPhase_);
		float re = rx.Re[i] * cs - rx.Im[i] * sn;
		float im = rx.Re[i] * sn + rx.Im[i] * cs;
		iq.Re[i] += re;
		iq.Im[i] += im;
		ritPhase_ += dPhi;
	}
	while (ritPhase_ > TWO_PI)
		ritPhase_ -= TWO_PI;
	while (ritPhase_ < -TWO_PI)
		ritPhase_ += TWO_PI;

	// ── Capture self-monitor block (injected AFTER RIT rotation inside mixAndProcess) ──
	TSingleArray monBlk;
	float monGain = monVol_.load();
	if (myStation.state == StationState::Sending &&
		!myStation.envelope.empty()) {
		monBlk = myStation.getBlock();
	}

	// ── Process IQ through filter / modulate / AGC ───────────────────────────
	TSingleArray processed = mixAndProcess(std::move(iq), monBlk, monGain);

	// ── Tick all stations AFTER collecting audio ──────────────────────────────
	stnColl.tick();
	myStation.tick();
	if (myStation.state == StationState::Listening && !macroQueue_.empty())
		flushMacroQueue();

	// ── Accounting ────────────────────────────────────────────────────────────
	elapsedSec += static_cast<double>(ini.bufSize) / DEFAULTRATE;
	++blockCount_;

	if (onTimerTick)
		onTimerTick(elapsedSec);

	// ── Duration check (fire once) ────────────────────────────────────────────
	if (!durationFired_ && ini.duration > 0 &&
		elapsedSec >= static_cast<double>(ini.duration) * 60.0) {
		durationFired_ = true;
		state = ContestState::Paused;
		if (onDurationExpired)
			onDurationExpired();
	}

	return processed;
}

TSingleArray Contest::mixAndProcess(
	TReImArrays iq, const TSingleArray &monBlk, float monGain)
{
	auto &ini = Ini::instance();

	// ── Apply QSB to IQ amplitude ────────────────────────────────────────────
	if (ini.qsb) {
		qsb_.applyTo(iq.Re);
		qsb_.applyTo(iq.Im);
	}

	// ── Inject self-monitor AFTER RIT rotation ───────────────────────────────
	// Injecting here ensures the sidetone is always heard at the pitch setting
	// regardless of RIT, because we bypass the RIT frequency shift.
	// The monitor is at 0 Hz baseband (= carrier = pitch Hz in the audio output).
	// Im must be 0 for a signal at 0 Hz baseband offset.
	if (!monBlk.empty()) {
		float rfg = 1.0f; // QSK receive-gate
		for (int i = 0; i < static_cast<int>(iq.Re.size()); ++i) {
			float env = monBlk[i] /
				myStation.amplitude;   // 0..1 envelope shape
			float m = monGain * monBlk[i]; // scaled sidetone sample
			if (ini.qsk) {
				// QSK: mix sidetone with gated received signal
				float target = 1.0f - env;
				if (rfg > target)
					rfg = target;
				else
					rfg = rfg * 0.997f + 0.003f;
				iq.Re[i] = m + rfg * iq.Re[i];
				iq.Im[i] = rfg *
					iq.Im[i]; // Im: only the gated RX, 0 from monitor
			} else {
				// No QSK: full sidetone, RX muted
				iq.Re[i] =
					m; // signal at 0 Hz baseband: Re = signal
				iq.Im[i] = 0.0f; // Im = 0 for 0 Hz baseband
			}
		}
	}

	TReImArrays filtered = filter_.filter(iq);
	TSingleArray audio = modulator_.modulate(filtered);
	TSingleArray result = agc_.process(audio);

	if (wav_.isOpen())
		wav_.writeFrom(result.data(), static_cast<int>(result.size()));

	return result;
}
