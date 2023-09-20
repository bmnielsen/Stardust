#pragma once

#include "MapSpecificOverride.h"

class JudgmentDay : public MapSpecificOverride
{
public:
    void modifyStartingLocation(std::unique_ptr<StartingLocation> &startingLocation) override;

    Base *naturalForWallPlacement(Base *main) override;
};
