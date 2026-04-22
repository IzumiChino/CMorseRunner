#include "stn_coll.hpp"
#include "ini.hpp"
#include <algorithm>
#include <cmath>

void StnColl::clear() {
    dx.clear();
    qrn.clear();
    qrm.clear();
}

void StnColl::tick() {
    for (auto& s : dx)  s->tick();
    for (auto& s : qrn) s->tick();
    for (auto& s : qrm) s->tick();

    // Remove done/failed DX stations
    dx.erase(std::remove_if(dx.begin(), dx.end(),
        [](const auto& s){ return s->oper.state == OperState::Done ||
                                  s->oper.state == OperState::Failed; }),
        dx.end());

    // Remove finished noise/QRM stations
    qrn.erase(std::remove_if(qrn.begin(), qrn.end(),
        [](const auto& s){ return s->done; }), qrn.end());
    qrm.erase(std::remove_if(qrm.begin(), qrm.end(),
        [](const auto& s){ return s->done; }), qrm.end());
}

TReImArrays StnColl::getBlock() {
    auto& ini = Ini::instance();
    int bs = ini.bufSize;
    TReImArrays result;
    setLengthReIm(result, bs);

    auto mix = [&](Station& s) {
        if (s.state != StationState::Sending) return;
        auto blk = s.getBlock();
        for (int i = 0; i < bs; ++i) {
            float phi = s.nextBfo();
            result.Re[i] += blk[i] * std::cos(phi);
            result.Im[i] -= blk[i] * std::sin(phi);  // minus sin — Pascal convention
        }
    };

    for (auto& s : dx)  mix(*s);
    for (auto& s : qrn) mix(*s);
    for (auto& s : qrm) mix(*s);

    return result;
}

void StnColl::addDx(int count) {
    for (int i = 0; i < count; ++i)
        dx.push_back(std::make_unique<DxStation>());
}

void StnColl::addQrn() {
    qrn.push_back(std::make_unique<QrnStation>());
}

void StnColl::addQrm() {
    qrm.push_back(std::make_unique<QrmStation>());
}

int StnColl::activeDxCount() const {
    return static_cast<int>(dx.size());
}

DxStation* StnColl::findByCall(const std::string& call) {
    for (auto& s : dx)
        if (s->myCall == call) return s.get();
    return nullptr;
}

void StnColl::dispatchEvent(StationEvent ev) {
    // Iterate in reverse so stations that call Free() during event (osFailed/osDone) don't corrupt iteration
    for (int i = static_cast<int>(dx.size()) - 1; i >= 0; --i)
        dx[i]->processEvent(ev);
}
