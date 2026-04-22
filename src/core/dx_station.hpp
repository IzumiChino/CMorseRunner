#pragma once
#include "station.hpp"
#include "dx_oper.hpp"

class MyStation;  // forward

class DxStation : public Station {
public:
    DxOper oper;

    // Set by Contest after construction so DxStation can read operator messages
    static MyStation* myStation_;

    DxStation();
    void processEvent(StationEvent ev) override;
};
