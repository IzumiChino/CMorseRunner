#pragma once
#include <string>
#include <array>

enum class RunMode {
	Stop,
	Pileup,
	Single,
	Wpx,
	Hst
};

struct Ini {
	// Station
	std::string call = "VE3NEA";
	std::string hamName;
	int wpm = 30;
	int bandwidth = 500;
	int pitch = 600;
	bool qsk = true;
	int rit = 0;
	int bufSize = 512;
	bool callsFromKeyer = false;
	bool saveWav = false;
	int selfMonVolume = 0; // -60..+20 dB encoded as (val-0.75)*80
	std::array<std::string, 8> macros{{"CQ $MYCALL TEST",
		"$EXCH",
		"TU",
		"AGN",
		"?",
		"QSO B4",
		"$HIS",
		"NIL"}};

	// Band conditions
	int activity = 2;
	bool qrn = true;
	bool qrm = true;
	bool qsb = true;
	bool flutter = true;
	bool lids = true;

	// Contest
	int duration = 30;
	int compDuration = 60;
	int hiScore = 0;
	RunMode runMode = RunMode::Stop;

	static Ini &instance();

	void load(const std::string &path);
	void save(const std::string &path) const;
};
