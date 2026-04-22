#include "ini.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

Ini &Ini::instance()
{
	static Ini inst;
	return inst;
}

static std::string trim(const std::string &s)
{
	auto b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos)
		return {};
	auto e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

static bool parseBool(const std::string &v)
{
	std::string lv = v;
	std::transform(lv.begin(), lv.end(), lv.begin(), ::tolower);
	return lv == "true" || lv == "1" || lv == "yes";
}

void Ini::load(const std::string &path)
{
	std::ifstream f(path);
	if (!f)
		return;

	std::string section, line;
	while (std::getline(f, line)) {
		line = trim(line);
		if (line.empty() || line[0] == ';')
			continue;
		if (line[0] == '[') {
			auto e = line.find(']');
			section = (e != std::string::npos)
				? line.substr(1, e - 1)
				: "";
			continue;
		}
		auto eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		std::string key = trim(line.substr(0, eq));
		std::string val = trim(line.substr(eq + 1));

		if (section == "Station") {
			if (key == "Call")
				call = val;
			else if (key == "Name")
				hamName = val;
			else if (key == "Wpm")
				wpm = std::stoi(val);
			else if (key == "Pitch")
				pitch = 300 + std::stoi(val) * 50;
			else if (key == "BandWidth")
				bandwidth = 100 + std::stoi(val) * 50;
			else if (key == "Qsk")
				qsk = parseBool(val);
			else if (key == "SelfMonVolume")
				selfMonVolume = std::stoi(val);
			else if (key == "SaveWav")
				saveWav = parseBool(val);
			else if (key == "CallsFromKeyer")
				callsFromKeyer = parseBool(val);
			else if (key.rfind("Macro", 0) == 0) {
				int idx = std::stoi(key.substr(5)) - 1;
				if (idx >= 0 &&
					idx < static_cast<int>(macros.size()))
					macros[idx] = val;
			}
		} else if (section == "Band") {
			if (key == "Activity")
				activity = std::stoi(val);
			else if (key == "Qrn")
				qrn = parseBool(val);
			else if (key == "Qrm")
				qrm = parseBool(val);
			else if (key == "Qsb")
				qsb = parseBool(val);
			else if (key == "Flutter")
				flutter = parseBool(val);
			else if (key == "Lids")
				lids = parseBool(val);
		} else if (section == "Contest") {
			if (key == "Duration")
				duration = std::stoi(val);
			else if (key == "CompetitionDuration")
				compDuration = std::stoi(val);
			else if (key == "HiScore")
				hiScore = std::stoi(val);
		} else if (section == "System") {
			if (key == "BufSize") {
				int v = std::stoi(val);
				if (v < 1)
					v = 3;
				if (v > 5)
					v = 5;
				bufSize = 64 << v;
			}
		}
	}

	// Migrate the previous default F2 macro to the standard contest form.
	// Keep user-customized content untouched.
	if (macros[1] == "$HIS $EXCH")
		macros[1] = "$EXCH";
}

void Ini::save(const std::string &path) const
{
	std::ofstream f(path);
	if (!f)
		return;

	f << "[Station]\n";
	f << "Call=" << call << "\n";
	f << "Name=" << hamName << "\n";
	f << "Pitch=" << (pitch - 300) / 50 << "\n";
	f << "BandWidth=" << (bandwidth - 100) / 50 << "\n";
	f << "Wpm=" << wpm << "\n";
	f << "Qsk=" << (qsk ? "true" : "false") << "\n";
	f << "SelfMonVolume=" << selfMonVolume << "\n";
	f << "SaveWav=" << (saveWav ? "true" : "false") << "\n";
	for (size_t i = 0; i < macros.size(); ++i)
		f << "Macro" << (i + 1) << "=" << macros[i] << "\n";

	f << "\n[Band]\n";
	f << "Activity=" << activity << "\n";
	f << "Qrn=" << (qrn ? "true" : "false") << "\n";
	f << "Qrm=" << (qrm ? "true" : "false") << "\n";
	f << "Qsb=" << (qsb ? "true" : "false") << "\n";
	f << "Flutter=" << (flutter ? "true" : "false") << "\n";
	f << "Lids=" << (lids ? "true" : "false") << "\n";

	f << "\n[Contest]\n";
	f << "Duration=" << duration << "\n";
	f << "CompetitionDuration=" << compDuration << "\n";
	f << "HiScore=" << hiScore << "\n";

	// BufSize: reverse-encode from bufSize = 64 << v
	int v = 3;
	for (int i = 1; i <= 5; ++i)
		if ((64 << i) == bufSize) {
			v = i;
			break;
		}
	f << "\n[System]\n";
	f << "BufSize=" << v << "\n";
}
