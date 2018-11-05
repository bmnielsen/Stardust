#pragma once

#include "MapSpecificOverride.h"

class Fortress : public MapSpecificOverride
{
public:
    bool hasMineralWalking() { return true; }
    void initializeChokes(std::map<const BWEM::ChokePoint *, Choke> & chokes);
};
