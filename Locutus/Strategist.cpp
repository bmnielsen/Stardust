#include "Strategist.h"

#include "Opening.h"
#include "Zealots.h"
#include "Dragoons.h"
#include "DarkTemplar.h"

namespace Strategist
{
#ifndef _DEBUG
    namespace
    {
#endif
        Opening* opening;
#ifndef _DEBUG
    }
#endif

    void chooseOpening()
    {
        opening = new Dragoons();
    }

    std::vector<ProductionGoal *> & currentProductionGoals()
    {
        return opening->goals();
    }
}
