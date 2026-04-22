#pragma once
#include <string>
#include <vector>
#include <ctime>

struct Qso {
    double      t{0.0};          // time as fraction of day
    std::string call, trueCall;
    int         rst{599},  trueRst{0};
    int         nr{0},     trueNr{0};
    std::string pfx;
    bool        dupe{false};
    std::string err;             // "   " = ok, "NIL", "DUP", "RST", "NR "
};

extern std::vector<Qso> gQsoList;
extern bool gCallSent;
extern bool gNrSent;

std::string extractPrefix(const std::string& call);
void        checkErr();
void        clearLog();
