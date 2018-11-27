#include "Strategist.h"

#include "Opening.h"
#include "Zealots.h"
#include "Dragoons.h"

namespace Strategist
{
    namespace
    {
        Opening* opening;
    }

    void chooseOpening()
    {
        opening = new Dragoons();
    }

    std::vector<ProductionGoal *> & currentProductionGoals()
    {
        return opening->goals();
    }
}
