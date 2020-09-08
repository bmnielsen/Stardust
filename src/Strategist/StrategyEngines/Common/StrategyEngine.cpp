#include <Strategist/Plays/MainArmy/DefendMyMain.h>
#include <Strategist/Plays/MainArmy/AttackEnemyMain.h>
#include <Strategist/Plays/Defensive/DefendBase.h>
#include <Strategist/Plays/Offensive/AttackExpansion.h>
#include <Strategist/Plays/Scouting/ScoutEnemyExpos.h>
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

void StrategyEngine::mainArmyProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                               BWAPI::UnitType unitType,
                               int count,
                               int &highPriorityCount)
{
    if (count == -1)
    {
        if (highPriorityCount > 0)
        {
            prioritizedProductionGoals[PRIORITY_MAINARMYBASEPRODUCTION].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                                     unitType,
                                                                                     std::max(highPriorityCount, count),
                                                                                     -1);
            highPriorityCount = 0;
        }
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   unitType,
                                                                   -1,
                                                                   -1);
        return;
    }

    if (highPriorityCount > 0)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMYBASEPRODUCTION].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                                 unitType,
                                                                                 std::max(highPriorityCount, count),
                                                                                 -1);
        if (highPriorityCount < count)
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       unitType,
                                                                       count - highPriorityCount,
                                                                       -1);
            highPriorityCount = 0;
        }
        else
        {
            highPriorityCount -= count;
        }
    }
}

void StrategyEngine::updateDefendBasePlays(std::vector<std::shared_ptr<Play>> &plays)
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
        plays.emplace(plays.begin(), std::make_shared<DefendBase>(baseToDefend));
        CherryVis::log() << "Added defend base play for base @ " << BWAPI::WalkPosition(baseToDefend->getPosition());
    }
}

void StrategyEngine::updateAttackExpansionPlays(std::vector<std::shared_ptr<Play>> &plays)
{
    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);

    // First gather the list of bases we want to attack
    std::set<Base *> basesToAttack;

    // Don't attack any bases if our main army play is defending our main
    if (mainArmyPlay && typeid(*mainArmyPlay) != typeid(DefendMyMain))
    {
        for (auto &base : Map::getEnemyBases())
        {
            // Main is attacked by our main army
            if (base == Map::getEnemyMain()) continue;

            // Natural is attacked along the way to the main unless it has already been destroyed
            if (base == Map::getEnemyStartingNatural() && Map::getEnemyStartingMain()
                && Map::getEnemyStartingMain()->owner == BWAPI::Broodwar->enemy())
            {
                continue;
            }

            // Bases without many resources left are not a priority
            if (base->mineralPatchCount() < 3) continue;

            basesToAttack.insert(base);
        }
    }

    // Scan the plays and remove those that are no longer relevant
    for (auto &play : plays)
    {
        auto attackExpansionPlay = std::dynamic_pointer_cast<AttackExpansion>(play);
        if (!attackExpansionPlay) continue;

        auto it = basesToAttack.find(attackExpansionPlay->base);

        // If we no longer need to defend this base, remove the play
        if (it == basesToAttack.end())
        {
            attackExpansionPlay->status.complete = true;
            continue;
        }

        basesToAttack.erase(it);
    }

    // Add missing plays
    for (auto &baseToAttack : basesToAttack)
    {
        // Insert them before the main army play
        auto it = plays.begin();
        for (; it != plays.end(); it++)
        {
            if (auto match = std::dynamic_pointer_cast<MainArmyPlay>(*it))
            {
                break;
            }
        }

        plays.emplace(it, std::make_shared<AttackExpansion>(baseToAttack));
        CherryVis::log() << "Added attack expansion play for base @ " << BWAPI::WalkPosition(baseToAttack->getPosition());
    }
}

void StrategyEngine::scoutExpos(std::vector<std::shared_ptr<Play>> &plays, int startingFrame)
{
    if (BWAPI::Broodwar->getFrameCount() < startingFrame) return;
    if (getPlay<ScoutEnemyExpos>(plays) != nullptr) return;

    plays.emplace_back(std::make_shared<ScoutEnemyExpos>());
}
