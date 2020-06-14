#include <Strategist/Plays/MainArmy/DefendMyMain.h>
#include <Strategist/Plays/Defensive/DefendBase.h>
#include "StrategyEngine.h"

#include "Map.h"

void StrategyEngine::UpdateDefendBasePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    // First gather the list of bases we want to defend
    std::set<Base *> basesToDefend;
    for (auto &base : Map::getMyBases())
    {
        if (base == Map::getMyMain())
        {
            // Don't defend it with a DefendBase play if our main army is already defending the base
            auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
            if (mainArmyPlay && typeid(*mainArmyPlay) == typeid(DefendMyMain)) continue;
        }
        else if (base->mineralPatchCount() < 3)
        {
            continue;
        }

        basesToDefend.insert(base);
    }

    // Scan the plays and remove those that are no longer relevant
    for (auto &play : plays)
    {
        auto defendBasePlay = std::dynamic_pointer_cast<DefendBase>(play);
        if (!defendBasePlay) continue;

        auto it = basesToDefend.find(defendBasePlay->base);

        // If we no longer need to defend this base, remove the play
        if (it == basesToDefend.end())
        {
            defendBasePlay->status.complete = true;
            continue;
        }

        basesToDefend.erase(it);
    }

    // Add missing plays
    for (auto &baseToDefend : basesToDefend)
    {
        plays.emplace_back(std::make_shared<DefendBase>(baseToDefend));
        CherryVis::log() << "Added defend base play for base @ " << BWAPI::WalkPosition(baseToDefend->getPosition());
    }

    // TODO: Ordering?
}
