#include "station.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

void Station::setPitch(int p)
{
	pitch = p;
	dPhi = TWO_PI * p / DEFAULTRATE;
}

float Station::nextBfo()
{
	float r = bfo;
	bfo += dPhi;
	if (bfo > TWO_PI)
		bfo -= TWO_PI;
	return r;
}

TSingleArray Station::getBlock()
{
	auto &ini = Ini::instance();
	int bs = ini.bufSize;
	TSingleArray result(bs, 0.0f);

	if (sendPos_ < static_cast<int>(envelope.size())) {
		int available = static_cast<int>(envelope.size()) - sendPos_;
		int count = std::min(bs, available);
		std::copy_n(envelope.begin() + sendPos_, count, result.begin());
		sendPos_ += count;
	}

	if (sendPos_ >= static_cast<int>(envelope.size()))
		envelope.clear();

	return result;
}

void Station::tick()
{
	if (state == StationState::Sending && envelope.empty()) {
		msgText.clear();
		state = StationState::Listening;
		processEvent(StationEvent::MsgSent);
	} else if (state != StationState::Sending) {
		if (timeOut > -1)
			--timeOut;
		if (timeOut == 0)
			processEvent(StationEvent::Timeout);
	}
}

void Station::sendMsg(StationMessage m)
{
	if (envelope.empty())
		msg.clear();
	if (m == StationMessage::None) {
		state = StationState::Listening;
		return;
	}
	msg.insert(m);

	switch (m) {
	case StationMessage::CQ:
		sendText("CQ <my> TEST");
		break;
	case StationMessage::NR:
		sendText("<#>");
		break;
	case StationMessage::TU:
		sendText("TU");
		break;
	case StationMessage::MyCall:
		sendText("<my>");
		break;
	case StationMessage::HisCall:
		sendText("<his>");
		break;
	case StationMessage::B4:
		sendText("QSO B4");
		break;
	case StationMessage::Qm:
		sendText("?");
		break;
	case StationMessage::Nil:
		sendText("NIL");
		break;
	case StationMessage::R_NR:
		sendText("R <#>");
		break;
	case StationMessage::R_NR2:
		sendText("R <#> <#>");
		break;
	case StationMessage::DeMyCall1:
		sendText("DE <my>");
		break;
	case StationMessage::DeMyCall2:
		sendText("DE <my> <my>");
		break;
	case StationMessage::DeMyCallNr1:
		sendText("DE <my> <#>");
		break;
	case StationMessage::DeMyCallNr2:
		sendText("DE <my> <my> <#>");
		break;
	case StationMessage::MyCallNr2:
		sendText("<my> <my> <#>");
		break;
	case StationMessage::NrQm:
		sendText("NR?");
		break;
	case StationMessage::LongCQ:
		sendText("CQ CQ TEST <my> <my> TEST");
		break;
	case StationMessage::Qrl:
		sendText("QRL?");
		break;
	case StationMessage::Qrl2:
		sendText("QRL?   QRL?");
		break;
	case StationMessage::Qsy:
		sendText("<his>  QSY QSY");
		break;
	case StationMessage::Agn:
		sendText("AGN");
		break;
	default:
		break;
	}
}

void Station::sendText(const std::string &text)
{
	std::string t = text;

	// Replace <#> (first with error, rest without)
	auto replaceFirst = [&](const std::string &tok,
				    const std::string &rep) {
		auto p = t.find(tok);
		if (p != std::string::npos)
			t.replace(p, tok.size(), rep);
	};
	auto replaceAll = [&](const std::string &tok, const std::string &rep) {
		size_t p = 0;
		while ((p = t.find(tok, p)) != std::string::npos) {
			t.replace(p, tok.size(), rep);
			p += rep.size();
		}
	};

	replaceFirst("<#>", nrAsText());
	replaceAll("<#>", nrAsText());
	replaceAll("<my>", myCall);
	replaceAll("<his>",
		hisCall); // replace here; subclasses set hisCall before calling sendText

	if (!msgText.empty())
		msgText += " ";
	msgText += t;
	sendMorse(keyer().encode(msgText));
}

void Station::sendMorse(const std::string &morse)
{
	if (envelope.empty()) {
		sendPos_ = 0;
		bfo = 0.0f;
	}
	auto &k = keyer();
	k.Wpm = wpm;
	k.MorseMsg = morse;
	TSingleArray env = k.getEnvelope();
	for (auto &v : env)
		v *= amplitude;
	envelope = std::move(env);
	state = StationState::Sending;
	timeOut = NEVER;
}

std::string Station::nrAsText()
{
	// Format RST+NR, apply abbreviations
	char buf[16];
	std::snprintf(buf, sizeof(buf), "%03d%03d", rst, nr);
	std::string s = buf;

	if (nrWithError) {
		// flip one digit
		int idx = static_cast<int>(s.size()) - 1;
		while (idx >= 0 && (s[idx] < '2' || s[idx] > '7'))
			--idx;
		if (idx >= 0) {
			if (std::rand() % 2 == 0)
				--s[idx];
			else
				++s[idx];
			char ebuf[16];
			std::snprintf(ebuf, sizeof(ebuf), "EEEEE %03d", nr);
			s += ebuf;
		}
		nrWithError = false;
	}

	// Replace 599 -> 5NN
	auto replAll = [&](const std::string &from, const std::string &to) {
		size_t p = 0;
		while ((p = s.find(from, p)) != std::string::npos) {
			s.replace(p, from.size(), to);
			p += to.size();
		}
	};
	replAll("599", "5NN");

	if (Ini::instance().runMode != RunMode::Hst) {
		replAll("000", "TTT");
		replAll("00", "TT");
		float r = static_cast<float>(std::rand()) / RAND_MAX;
		if (r < 0.4f)
			replAll("0", "O");
		else if (r < 0.97f)
			replAll("0", "T");
		if (static_cast<float>(std::rand()) / RAND_MAX < 0.97f)
			replAll("9", "N");
	}
	return s;
}
