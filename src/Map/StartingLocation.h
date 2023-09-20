#pragma once

#include "Common.h"
#include "Base.h"
#include "Choke.h"

class StartingLocation
{
public:
    Base *main;
    Base *natural;
    Choke *mainChoke;
    Choke *naturalChoke;
};
