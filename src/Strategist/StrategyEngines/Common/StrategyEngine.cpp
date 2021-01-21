#include <Strategist/Plays/MainArmy/DefendMyMain.h>
#include <Strategist/Plays/MainArmy/AttackEnemyBase.h>
#include <Strategist/Plays/MainArmy/MopUp.h>
#include <Strategist/Plays/Macro/CullArmy.h>
#include <Strategist/Plays/Defensive/DefendBase.h>
#include <Strategist/Plays/Scouting/ScoutEnemyExpos.h>
#include "StrategyEngine.h"

#include "Map.h"
#include "Builder.h"
#include "Units.h"
#include "UnitUtil.h"
#include "General.h"

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

void StrategyEngine::handleAntiRushProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                              int dragoonCount,
                                              int zealotCount,
                                              int zealotsRequired)
{
    // Get two zealots at highest priority
    if ((dragoonCount + zealotCount) < 2)
    {
        prioritizedProductionGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                    BWAPI::UnitTypes::Protoss_Zealot,
                                                                    -1,
                                                                    2);

        // Cancel a building cybernetics core unless it is close to being finished or we don't need the minerals
        // This handles cases where we queue the core shortly before scouting the rush
        for (const auto &core : Builder::pendingBuildingsOfType(BWAPI::UnitTypes::Protoss_Cybernetics_Core))
        {
            if (!core->isConstructionStarted() || (BWAPI::Broodwar->self()->minerals() < 150 && core->expectedFramesUntilCompletion() > 1000))
            {
                Log::Get() << "Cancelling " << core->type << "@" << core->tile << " because of recognized rush";
                Builder::cancel(core->tile);
            }
        }
    }
    else if (zealotsRequired > 0)
    {
        prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                      BWAPI::UnitTypes::Protoss_Zealot,
                                                                      zealotsRequired > 1 ? -1 : 1,
                                                                      -1);
    }

    // If the dragoon transition is just beginning, only order one so we keep producing zealots
    prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                               BWAPI::UnitTypes::Protoss_Dragoon,
                                                               dragoonCount == 0 ? 1 : -1,
                                                               -1);

    prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                               BWAPI::UnitTypes::Protoss_Zealot,
                                                               -1,
                                                               -1);

    // Upgrade goon range at 2 dragoons unless we are still behind in zealots
    if (zealotsRequired == 0 || zealotCount > 4)
    {
        upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);
    }
}

bool StrategyEngine::handleIslandExpansionProduction(std::vector<std::shared_ptr<Play>> &plays,
                                                     std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    auto setCulling = [&plays](int requiredSupply)
    {
#if CHERRYVIS_ENABLED
        CherryVis::setBoardValue("cull", (std::ostringstream() << std::max(0, requiredSupply)).str());
#endif

        // If we already have a cull army play, update the required supply
        // If it goes zero or negative, the play will complete itself
        auto cullArmy = getPlay<CullArmy>(plays);
        if (cullArmy)
        {
            cullArmy->supplyNeeded = requiredSupply;
            return;
        }

        // Nothing to do if we don't need supply
        if (requiredSupply <= 0) return;

        // Create the play
        plays.emplace(plays.begin(), std::make_shared<CullArmy>(requiredSupply));
    };

    // Only relevant when the main army is in mop-up mode
    auto mopUp = getPlay<MopUp>(plays);
    if (!mopUp)
    {
        setCulling(0);
        return false;
    }

    // Only relevant when the enemy has at least one island base
    bool enemyHasIslandBase = false;
    for (const auto &base : Map::getEnemyBases())
    {
        if (base->island)
        {
            enemyHasIslandBase = true;
            break;
        }
    }
    if (!enemyHasIslandBase)
    {
        setCulling(0);
        return false;
    }

    // Produce up to 10 carriers with relevant upgrades
    int requiredCarriers = 10 - Units::countAll(BWAPI::UnitTypes::Protoss_Carrier);
    int weaponLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons);
    if (weaponLevel < 3)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMYBASEPRODUCTION].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                                 BWAPI::UpgradeTypes::Protoss_Air_Weapons,
                                                                                 weaponLevel + 1,
                                                                                 1);
    }
    if (requiredCarriers > 0)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMYBASEPRODUCTION].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                                 BWAPI::UnitTypes::Protoss_Carrier,
                                                                                 requiredCarriers,
                                                                                 4);
        upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier, 0);

        // Cull our main army if we need the supply
        setCulling(
                requiredCarriers * BWAPI::UnitTypes::Protoss_Carrier.supplyRequired() - // Supply needed by the carriers
                (400 - BWAPI::Broodwar->self()->supplyUsed())); // Supply room
    }
    else
    {
        setCulling(0);
    }

    return true;
}

void StrategyEngine::oneGateCoreOpening(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                        int dragoonCount,
                                        int zealotCount,
                                        int desiredZealots)
{
    // If our core is done or we want no zealots just return dragoons
    if (desiredZealots == 0 || Units::countCompleted(BWAPI::UnitTypes::Protoss_Cybernetics_Core) > 0)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Dragoon,
                                                                   -1,
                                                                   -1);
        return;
    }

    // Ensure gas before zealot
    if (Units::countAll(BWAPI::UnitTypes::Protoss_Assimilator) == 0)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Dragoon,
                                                                   1,
                                                                   -1);
        return;
    }

    if (zealotCount == 0)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Zealot,
                                                                   1,
                                                                   1);
    }
    if (dragoonCount == 0)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Dragoon,
                                                                   1,
                                                                   1);
    }
    if (zealotCount < desiredZealots)
    {
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Zealot,
                                                                   desiredZealots - zealotCount,
                                                                   1);
    }

    prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                               BWAPI::UnitTypes::Protoss_Dragoon,
                                                               -1,
                                                               -1);
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
    std::unordered_map<Base *, int> basesToDefend;

    // Don't defend any bases if our main army play is defending our main
    if (mainArmyPlay && typeid(*mainArmyPlay) != typeid(DefendMyMain))
    {
        for (auto &base : Map::getMyBases())
        {
            if (base == Map::getMyMain() || base == Map::getMyNatural())
            {
                // Don't defend our main or natural with a DefendBase play if our main army is close to it
                if (typeid(*mainArmyPlay) == typeid(AttackEnemyBase))
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

            // Gather the enemy units threatening the base
            int enemyValue = 0;
            for (const auto &unit : Units::enemyAtBase(base))
            {
                enemyValue += CombatSim::unitValue(unit);
            }

            // If too many enemies are threatening the base, don't bother trying to defend it
            if (enemyValue > 5 * CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon))
            {
                continue;
            }

            basesToDefend[base] = enemyValue;
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

        defendBasePlay->enemyValue = it->second;

        basesToDefend.erase(it);
    }

    // Add missing plays
    for (auto &baseToDefend : basesToDefend)
    {
        plays.emplace(plays.begin(), std::make_shared<DefendBase>(
                baseToDefend.first,
                baseToDefend.second));
        CherryVis::log() << "Added defend base play for base @ " << BWAPI::WalkPosition(baseToDefend.first->getPosition());
    }
}

void StrategyEngine::scoutExpos(std::vector<std::shared_ptr<Play>> &plays, int startingFrame)
{
    if (BWAPI::Broodwar->getFrameCount() < startingFrame) return;
    if (getPlay<ScoutEnemyExpos>(plays) != nullptr) return;

    plays.emplace_back(std::make_shared<ScoutEnemyExpos>());
}

void StrategyEngine::reserveMineralsForExpansion(std::vector<std::pair<int, int>> &mineralReservations)
{
    // The idea here is to make sure we keep enough resources for an expansion if the total minerals left at our bases is low
    int totalMinerals = 0;
    for (const auto &base : Map::getMyBases())
    {
        totalMinerals += base->minerals();
    }

    if (totalMinerals < 800)
    {
        mineralReservations.emplace_back(std::make_pair(400, 0));
    }
}
