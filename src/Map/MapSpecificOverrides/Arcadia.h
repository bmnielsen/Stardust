#pragma once

#include "MapSpecificOverride.h"

class Arcadia : public MapSpecificOverride
{
public:
    void modifyBases(std::vector<Base *> &bases) override;

    void modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation) override;

    Base *naturalForWallPlacement(Base *main) override;
};
