#pragma once

#include "Common.h"
#include "Base.h"
#include "Choke.h"

class StartingLocation
{
public:
    Base *main;
    Base *natBase;
    Choke *mainChoke;
    Choke *naturalChoke;

    StartingLocation(
            Base *main,
            Base *natBase,
            Choke *mainChoke,
            Choke *naturalChoke) : main(main), natBase(natBase), mainChoke(mainChoke), naturalChoke(naturalChoke)
    {}
};
