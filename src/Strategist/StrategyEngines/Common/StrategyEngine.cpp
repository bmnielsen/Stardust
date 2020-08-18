#include <Strategist/Plays/MainArmy/DefendMyMain.h>
#include <Strategist/Plays/MainArmy/AttackEnemyMain.h>
#include <Strategist/Plays/Defensive/DefendBase.h>
#include "StrategyEngine.h"

#include "Map.h"
#include "Units.h"

bool StrategyEngine::hasEnemyStolenOurGas()
{
    return Units::countAll(BWAPI::UnitTypes::Protoss_Assimilator) == 0 &&
           Map::getMyMain()->geysers().empty();
}

void StrategyEngine::handleGasStealProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                              int &zealotCount)
{
    // Hop out now if we know it isn't a gas steal
    if (!hasEnemyStolenOurGas() || zealotCount > 0)
    {
        return;
    }

    // Ensure we have a zealot
    prioritizedProductionGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                               BWAPI::UnitTypes::Protoss_Zealot,
                                                               1,
                                                               1);
    zealotCount = 1;
}

void StrategyEngine::UpdateDefendBasePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);

    // First gather the list of bases we want to defend
    std::set<Base *> basesToDefend;

    // Don't defend any bases if our main army play is defending our main
    if (mainArmyPlay && typeid(*mainArmyPlay) != typeid(DefendMyMain))
    {
        for (auto &base : Map::getMyBases())
        {
            if (base == Map::getMyMain() || base == Map::getMyNatural())
            {
                // Don't defend our main or natural with a DefendBase play if our main army is close to it
                if (typeid(*mainArmyPlay) == typeid(AttackEnemyMain))
                {
                    auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                    if (vanguard && vanguard->vanguard)
                    {
                        int vanguardDist = PathFinding::GetGroundDistance(vanguard->vanguard->lastPosition,
                                                                          base->getPosition(),
                                                                          BWAPI::UnitTypes::Protoss_Zealot);
                        if (vanguardDist != -1 && vanguardDist < 1500) continue;
                    }
                }
            }
            else if (base->mineralPatchCount() < 3)
            {
                continue;
            }

            basesToDefend.insert(base);
        }
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
