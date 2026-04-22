#include "my_station.hpp"
#include "ini.hpp"
#include <algorithm>

MyStation::MyStation()
{
	auto &ini = Ini::instance();
	myCall = ini.call;
	wpm = ini.wpm;
	// MyStation pitch = 0: user transmits exactly at carrier (0 Hz offset)
	pitch = 0;
	amplitude = 25000.0f; // calibrated to DX station amplitude range
	setPitch(pitch);
}

void MyStation::processEvent(StationEvent ev)
{
	if (ev == StationEvent::MsgSent) {
		if (onFinishedSending)
			onFinishedSending();
	}
}

void MyStation::abortSend()
{
	envelope.clear();
	msg.clear();
	msg.insert(StationMessage::Garbage);
	msgText.clear();
	pieces_.clear();
	state = StationState::Listening;
	sendPos_ = 0;
	if (onFinishedSending)
		onFinishedSending();
}

void MyStation::addToPieces(const std::string &text)
{
	std::string remaining = text;
	size_t p = remaining.find("<his>");
	while (p != std::string::npos) {
		pieces_.push_back(remaining.substr(0, p));
		pieces_.push_back("@");
		remaining.erase(0, p + 5);
		p = remaining.find("<his>");
	}
	pieces_.push_back(remaining);
	pieces_.erase(std::remove_if(pieces_.begin(),
			      pieces_.end(),
			      [](const std::string &s) { return s.empty(); }),
		pieces_.end());
}

void MyStation::sendNextPiece()
{
	if (pieces_.empty())
		return;
	msgText.clear();
	if (pieces_.front() != "@")
		Station::sendText(pieces_.front());
	else if (Ini::instance().callsFromKeyer &&
		Ini::instance().runMode != RunMode::Hst &&
		Ini::instance().runMode != RunMode::Wpx)
		Station::sendText(" ");
	else
		Station::sendText(hisCall);
}

void MyStation::sendText(const std::string &text)
{
	addToPieces(text);
	if (state != StationState::Sending) {
		sendNextPiece();
		if (state == StationState::Sending && onStartedSending)
			onStartedSending();
	}
}

TSingleArray MyStation::getBlock()
{
	TSingleArray result = Station::getBlock();
	if (envelope.empty() && !pieces_.empty()) {
		pieces_.erase(pieces_.begin());
		if (!pieces_.empty())
			sendNextPiece();
	}
	return result;
}

void MyStation::sendCq()
{
	hisCall.clear();
	sendMsg(StationMessage::CQ);
}

void MyStation::sendExchange(const std::string &hisCallStr, int hisNr)
{
	hisCall = hisCallStr;
	nr = hisNr;
	sendMsg(StationMessage::HisCall);
	sendMsg(StationMessage::NR);
}

void MyStation::sendTu()
{
	sendMsg(StationMessage::TU);
}

void MyStation::sendAgn()
{
	sendMsg(StationMessage::Agn);
}

void MyStation::sendQm()
{
	sendMsg(StationMessage::Qm);
}

void MyStation::sendMyCall()
{
	sendMsg(StationMessage::MyCall);
}

void MyStation::sendHisCall(const std::string &hisCallStr)
{
	hisCall = hisCallStr;
	sendMsg(StationMessage::HisCall);
}
