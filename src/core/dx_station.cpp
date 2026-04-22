#include "dx_station.hpp"
#include "my_station.hpp"
#include "call_list.hpp"
#include "ini.hpp"
#include "../audio/rnd_func.hpp"
#include <cstdlib>
#include <cmath>

MyStation *DxStation::myStation_ = nullptr;

DxStation::DxStation()
{
	auto &ini = Ini::instance();
	myCall = pickCall();
	hisCall = ini.call;
	rst = 599;
	wpm = ini.wpm;
	// pitch = signed Hz offset from carrier, gaussian centred at 0 (range ±300 Hz)
	pitch = static_cast<int>(std::round(rndGaussLim(0.0f, 300.0f)));
	amplitude = 9000.0f + 18000.0f * (1.0f + rndUShaped());
	setPitch(pitch);

	oper.myCall = myCall;
	oper.skills = 1 + std::rand() % 3;
	oper.setState(OperState::NeedPrevEnd); // wait for CQ before calling
	// nr will be overridden by Contest::spawnStations if needed; default to 1
	nr = 1;

	// Start silent: wait for evMeFinished (CQ) before transmitting
	// DX station waits silently until evMeFinished (CQ heard)
	state = StationState::Listening;
	timeOut = NEVER;
}

void DxStation::processEvent(StationEvent ev)
{
	if (oper.state == OperState::Done || oper.state == OperState::Failed)
		return;

	switch (ev) {
	case StationEvent::MsgSent:
		// DX station just finished its own transmission
		if (myStation_ && myStation_->state == StationState::Sending)
			timeOut = NEVER; // operator still sending — wait
		else
			timeOut = oper.getReplyTimeout(wpm);
		break;

	case StationEvent::MeFinished:
		// Operator just finished sending — parse what was said
		if (state == StationState::Sending)
			break; // we're busy transmitting
		if (myStation_) {
			oper.receivedMsg(myStation_->msg, myStation_->hisCall);
		}
		if (oper.state == OperState::Done ||
			oper.state == OperState::Failed)
			break;
		timeOut = oper.getSendDelay(wpm);
		state = StationState::Listening;
		break;

	case StationEvent::MeStarted:
		// Operator started sending — stop whatever we were doing
		if (state != StationState::Sending) {
			state = StationState::Listening;
			timeOut = NEVER;
		}
		break;

	case StationEvent::Timeout:
		if (state != StationState::Sending) {
			auto reply = oper.getReply();
			if (reply == StationMessage::None) {
				oper.state = OperState::Failed;
				return;
			}
			for (int i = 0; i < oper.repeatCnt; ++i)
				sendMsg(reply);
		}
		break;

	default:
		break;
	}
}
