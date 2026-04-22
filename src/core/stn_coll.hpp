#pragma once
#include "dx_station.hpp"
#include "qrn_stn.hpp"
#include "qrm_stn.hpp"
#include "../audio/snd_types.hpp"
#include <vector>
#include <memory>

class StnColl {
public:
    StnColl() = default;
    ~StnColl() = default;

    void clear();
    void tick();
    TReImArrays  getBlock();

    void addDx(int count = 1);
    void addQrn();
    void addQrm();

    int  activeDxCount() const;
    DxStation* findByCall(const std::string& call);
    void dispatchEvent(StationEvent ev);  // broadcast to all DX stations

    std::vector<std::unique_ptr<DxStation>>  dx;
    std::vector<std::unique_ptr<QrnStation>> qrn;
    std::vector<std::unique_ptr<QrmStation>> qrm;
};
