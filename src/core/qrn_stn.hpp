#pragma once
#include "station.hpp"

class QrnStation : public Station {
public:
    bool done{false};
    QrnStation();
    void processEvent(StationEvent ev) override;
};
