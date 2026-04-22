#pragma once
#include "station.hpp"

class QrmStation : public Station {
public:
    bool done{false};
    QrmStation();
    void processEvent(StationEvent ev) override;
};
