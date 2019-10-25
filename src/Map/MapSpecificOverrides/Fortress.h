#pragma once

#include "MapSpecificOverride.h"

class Fortress : public MapSpecificOverride
{
public:
    bool hasMineralWalking() override { return true; }

    void initializeChokes(std::map<const BWEM::ChokePoint *, Choke *> &chokes) override;
};
