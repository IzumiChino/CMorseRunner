#include "log.hpp"
#include <algorithm>
#include <cctype>

std::vector<Qso> gQsoList;
bool gCallSent = false;
bool gNrSent   = false;

void clearLog() {
    gQsoList.clear();
    gCallSent = false;
    gNrSent   = false;
}

std::string extractPrefix(const std::string& callIn) {
    std::string call = callIn;

    // Strip common suffixes
    for (auto& sfx : {"/QRP", "/MM", "/M", "/P"}) {
        auto p = call.find(sfx);
        if (p != std::string::npos) call.erase(p);
    }
    // Collapse double slashes
    while (call.find("//") != std::string::npos)
        call.replace(call.find("//"), 2, "/");

    if (call.size() < 2) return "";

    auto isDigit  = [](char c){ return c >= '0' && c <= '9'; };

    std::string result, dig;
    auto slash = call.find('/');

    if (slash == std::string::npos) {
        result = call;
    } else if (slash == 0) {
        result = call.substr(1);
    } else if (slash == call.size() - 1) {
        result = call.substr(0, slash);
    } else {
        std::string s1 = call.substr(0, slash);
        std::string s2 = call.substr(slash + 1);
        if (s1.size() == 1 && isDigit(s1[0]))      { dig = s1; result = s2; }
        else if (s2.size() == 1 && isDigit(s2[0])) { dig = s2; result = s1; }
        else result = (s1.size() <= s2.size()) ? s1 : s2;
    }

    if (result.find('/') != std::string::npos) return "";

    // Delete trailing letters, keep at least 2 chars
    for (int p = static_cast<int>(result.size()) - 1; p >= 2; --p) {
        if (isDigit(result[p])) break;
        result.erase(p, 1);
    }

    // Ensure ends with digit
    if (!result.empty() && !isDigit(result.back()))
        result += '0';

    // Replace digit with override
    if (!dig.empty() && !result.empty())
        result.back() = dig[0];

    if (result.size() > 5) result = result.substr(0, 5);
    return result;
}

void checkErr() {
    if (gQsoList.empty()) return;
    auto& q = gQsoList.back();
    if (q.trueCall.empty())    q.err = "NIL";
    else if (q.dupe)           q.err = "DUP";
    else if (q.trueRst != q.rst) q.err = "RST";
    else if (q.trueNr  != q.nr)  q.err = "NR ";
    else                         q.err = "   ";
}
