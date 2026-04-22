#pragma once
#include "station.hpp"
#include <functional>
#include <vector>

class MyStation : public Station
{
      public:
	MyStation();
	void processEvent(StationEvent ev) override;

	void sendCq();
	void sendExchange(const std::string &hisCallStr, int hisNr);
	void sendTu();
	void sendAgn();
	void sendQm();
	void sendMyCall();
	void sendHisCall(const std::string &hisCallStr);
	void abortSend();

	bool isSending() const
	{
		return state == StationState::Sending;
	}

	// Fired when I finish sending (dispatch evMeFinished to DX stations)
	std::function<void()> onFinishedSending;
	// Fired when I start sending (dispatch evMeStarted to DX stations)
	std::function<void()> onStartedSending;

	TSingleArray getBlock() override;
	void sendText(const std::string &text) override;

      private:
	std::vector<std::string> pieces_;

	void addToPieces(const std::string &text);
	void sendNextPiece();
};
