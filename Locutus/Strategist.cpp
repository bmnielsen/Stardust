#include "Strategist.h"

#include "Opening.h"
#include "Zealots.h"

namespace Strategist
{
    namespace
    {
        Opening* opening;
    }

    void chooseOpening()
    {
        opening = new Zealots();
    }

    std::vector<ProductionGoal>& currentProductionGoals()
    {
        return opening->goals();
    }
}
