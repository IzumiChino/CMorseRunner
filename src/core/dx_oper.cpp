#include "dx_oper.hpp"
#include "station.hpp"
#include "../audio/rnd_func.hpp"
#include "../audio/snd_types.hpp"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <vector>

static constexpr int FULL_PATIENCE = 5;

void DxOper::setState(OperState s) {
    state = s;
    if (s == OperState::NeedQso)
        patience = static_cast<int>(std::round(rndRayleigh(4.0f)));
    else
        patience = FULL_PATIENCE;
    repeatCnt = (s == OperState::NeedQso && (static_cast<float>(std::rand()) / RAND_MAX) < 0.1f) ? 2 : 1;
}

void DxOper::decPatience() {
    if (state == OperState::Done) return;
    --patience;
    if (patience < 1) state = OperState::Failed;
}

// Simple edit-distance based call check
// Returns 2=exact, 1=close, 0=no match
int DxOper::checkMyCall(const std::string& sentCall) const {
    const std::string& c0 = myCall;
    const std::string& c  = sentCall;
    if (c.empty() || c.size() < 2) return 0;

    // Exact match
    if (c == c0) return 2;

    // '?' wildcard match
    if (c.size() == c0.size()) {
        bool ok = true;
        for (size_t i = 0; i < c.size(); ++i)
            if (c[i] != '?' && c[i] != c0[i]) { ok = false; break; }
        if (ok) return 1;  // wildcard match = almost
    }

    // Compute edit distance
    int n = static_cast<int>(c.size()), m = static_cast<int>(c0.size());
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = 0; i <= n; ++i) dp[i][0] = i * 2;
    for (int j = 0; j <= m; ++j) dp[0][j] = 0;
    for (int i = 1; i <= n; ++i)
        for (int j = 1; j <= m; ++j) {
            int t = dp[i][j-1] + (i < n ? 2 : 0);  // skip c0 char
            int l = dp[i-1][j] + (c[i-1] != '?' ? 2 : 0);
            int d = dp[i-1][j-1] + (c[i-1] != c0[j-1] && c[i-1] != '?' ? 2 : 0);
            dp[i][j] = std::min({t, l, d});
        }
    int penalty = dp[n][m];
    if (penalty == 0) return 1;  // structural zero but sizes differ → almost
    if (penalty <= 2) return 1;  // 1 char difference → almost
    return 0;
}

int DxOper::getSendDelay(int wpm) const {
    if (state == OperState::NeedPrevEnd) return NEVER;
    // ~0.1-0.6s jitter
    float base = 0.1f + 0.5f * (static_cast<float>(std::rand()) / RAND_MAX);
    return secondsToBlocks(base);
}

int DxOper::getReplyTimeout(int wpm) const {
    float base = 6.0f - static_cast<float>(skills);
    base = rndGaussLim(base, base / 2.0f);
    return secondsToBlocks(base);
}

StationMessage DxOper::getReply() const {
    switch (state) {
        case OperState::NeedPrevEnd:
        case OperState::Done:
        case OperState::Failed:
            return StationMessage::None;

        case OperState::NeedQso:
            return StationMessage::MyCall;

        case OperState::NeedNr:
            return (patience == FULL_PATIENCE - 1 || (static_cast<float>(std::rand()) / RAND_MAX) < 0.3f)
                   ? StationMessage::NrQm : StationMessage::Agn;

        case OperState::NeedCall:
            if ((static_cast<float>(std::rand()) / RAND_MAX) > 0.5f)
                return StationMessage::DeMyCallNr1;
            return ((static_cast<float>(std::rand()) / RAND_MAX) > 0.25f)
                   ? StationMessage::DeMyCallNr2 : StationMessage::MyCallNr2;

        case OperState::NeedCallNr:
            return ((static_cast<float>(std::rand()) / RAND_MAX) > 0.5f)
                   ? StationMessage::DeMyCall1 : StationMessage::DeMyCall2;

        case OperState::NeedEnd:
            if (patience < FULL_PATIENCE - 1) return StationMessage::NR;
            return ((static_cast<float>(std::rand()) / RAND_MAX) < 0.9f)
                   ? StationMessage::R_NR : StationMessage::R_NR2;
    }
    return StationMessage::None;
}

void DxOper::receivedMsg(const std::set<StationMessage>& msgs, const std::string& sentHisCall) {
    bool hasCq      = msgs.count(StationMessage::CQ)      > 0;
    bool hasTu      = msgs.count(StationMessage::TU)      > 0;
    bool hasNil     = msgs.count(StationMessage::Nil)     > 0;
    bool hasHisCall = msgs.count(StationMessage::HisCall) > 0;
    bool hasNr      = msgs.count(StationMessage::NR)      > 0;
    bool hasB4      = msgs.count(StationMessage::B4)      > 0;
    bool isGarbage  = msgs.empty();

    // --- CQ ---
    if (hasCq) {
        switch (state) {
            case OperState::NeedPrevEnd: setState(OperState::NeedQso); break;
            case OperState::NeedQso:    decPatience(); break;
            case OperState::NeedNr:
            case OperState::NeedCall:
            case OperState::NeedCallNr: state = OperState::Failed; break;
            case OperState::NeedEnd:    state = OperState::Done; break;
            default: break;
        }
        return;
    }

    // --- NIL ---
    if (hasNil) {
        switch (state) {
            case OperState::NeedPrevEnd: setState(OperState::NeedQso); break;
            case OperState::NeedQso:    decPatience(); break;
            case OperState::NeedNr:
            case OperState::NeedCall:
            case OperState::NeedCallNr:
            case OperState::NeedEnd:    state = OperState::Failed; break;
            default: break;
        }
        return;
    }

    // --- HisCall (operator addressed someone) ---
    if (hasHisCall) {
        int callMatch = checkMyCall(sentHisCall);
        switch (callMatch) {
            case 2: // exact
                if (state == OperState::NeedPrevEnd || state == OperState::NeedQso)
                    setState(OperState::NeedNr);
                else if (state == OperState::NeedCallNr)
                    setState(OperState::NeedNr);
                else if (state == OperState::NeedCall)
                    setState(OperState::NeedEnd);
                break;
            case 1: // partial/garbled
                if (state == OperState::NeedPrevEnd || state == OperState::NeedQso)
                    setState(OperState::NeedCallNr);
                else if (state == OperState::NeedNr)
                    setState(OperState::NeedCallNr);
                else if (state == OperState::NeedEnd)
                    setState(OperState::NeedCall);
                break;
            case 0: // wrong call
                if (state == OperState::NeedQso)
                    state = OperState::NeedPrevEnd;
                else if (state == OperState::NeedNr || state == OperState::NeedCall || state == OperState::NeedCallNr)
                    state = OperState::Failed;
                else if (state == OperState::NeedEnd)
                    state = OperState::Done;
                break;
        }
    }

    // --- B4 ---
    if (hasB4) {
        switch (state) {
            case OperState::NeedPrevEnd:
            case OperState::NeedQso:    setState(OperState::NeedQso); break;
            case OperState::NeedNr:
            case OperState::NeedEnd:    state = OperState::Failed; break;
            case OperState::NeedCall:
            case OperState::NeedCallNr: break; // same state
            default: break;
        }
    }

    // --- NR ---
    if (hasNr) {
        switch (state) {
            case OperState::NeedPrevEnd: break;
            case OperState::NeedQso:    state = OperState::NeedPrevEnd; break;
            case OperState::NeedNr:
                if ((static_cast<float>(std::rand()) / RAND_MAX) < 0.9f)
                    setState(OperState::NeedEnd);
                break;
            case OperState::NeedCall:   break;
            case OperState::NeedCallNr:
                if ((static_cast<float>(std::rand()) / RAND_MAX) < 0.9f)
                    setState(OperState::NeedCall);
                break;
            case OperState::NeedEnd:    break;
            default: break;
        }
    }

    // --- TU ---
    if (hasTu) {
        switch (state) {
            case OperState::NeedPrevEnd: setState(OperState::NeedQso); break;
            case OperState::NeedEnd:     state = OperState::Done; break;
            default: break;
        }
    }

    // Garbage clears state back to NeedPrevEnd
    if (isGarbage)
        state = OperState::NeedPrevEnd;

    if (state != OperState::NeedPrevEnd)
        decPatience();
}

