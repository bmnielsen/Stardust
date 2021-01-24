#include "StrategyEngine.h"

#include <Strategist/Plays/MainArmy/DefendMyMain.h>
#include <Strategist/Plays/MainArmy/AttackEnemyBase.h>
#include <Strategist/Plays/MainArmy/MopUp.h>
#include <Strategist/Plays/Offensive/AttackExpansion.h>
#include <Strategist/Plays/Offensive/AttackIslandExpansion.h>
#include <Strategist/Plays/Scouting/ScoutEnemyExpos.h>

#include "Map.h"
#include "Units.h"
#include "UnitUtil.h"
#include "General.h"

namespace
{
    template<class T, class ...Args>
    void setMainPlay(MainArmyPlay *current, Args &&...args)
    {
        if (typeid(*current) == typeid(T)) return;
        current->status.transitionTo = std::make_shared<T>(std::forward<Args>(args)...);
    }
}

void StrategyEngine::updateAttackPlays(std::vector<std::shared_ptr<Play>> &plays, bool defendOurMain)
{
    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);

    // If we want to defend our main, cancel all attack plays and transition the main army
    if (defendOurMain)
    {
        for (auto &play : plays)
        {
            if (auto attackExpansionPlay = std::dynamic_pointer_cast<AttackExpansion>(play))
            {
                attackExpansionPlay->status.complete = true;
            }
        }

        setMainPlay<DefendMyMain>(mainArmyPlay);
        return;
    }

    // Get the current main army target base
    // To avoid indecision, we only allow the army to switch target bases in the following situations:
    // - it is attacking an expansion and has been on the attack in the past five seconds
    // - it is attacking the enemy main or natural and is still on the attack
    Base *mainArmyTarget = nullptr;
    auto attackEnemyBasePlay = dynamic_cast<AttackEnemyBase *>(mainArmyPlay);
    if (attackEnemyBasePlay && attackEnemyBasePlay->base->owner == BWAPI::Broodwar->enemy())
    {
        bool isMainOrNatural = attackEnemyBasePlay->base == Map::getEnemyStartingMain() ||
                               attackEnemyBasePlay->base == Map::getEnemyStartingNatural();

        auto vanguard = attackEnemyBasePlay->getSquad()->vanguardCluster();
        if (vanguard && (vanguard->currentActivity != UnitCluster::Activity::Regrouping
                         || (!isMainOrNatural && vanguard->lastActivityChange > (BWAPI::Broodwar->getFrameCount() - 120))))
        {
            mainArmyTarget = attackEnemyBasePlay->base;
        }
    }

    // Now analyze all of the enemy bases to determine how we want to attack them
    // Main army target is either a fortified expansion, the enemy natural, or the enemy main
    int lowestEnemyValue = INT_MAX;
    Base *lowestEnemyValueBase = nullptr;
    std::map<Base *, int> attackableExpansionsToEnemyUnitValue;
    std::set<Base *> islandExpansions;
    for (auto &base : Map::getEnemyBases())
    {
        // Main and natural are default targets for our main army, so don't need to be analyzed
        if (base == Map::getEnemyMain() || base == Map::getEnemyStartingNatural()) continue;

        // Skip if the main army is already attacking this base
        if (base == mainArmyTarget) continue;

        // Handle islands separately
        if (base->island)
        {
            islandExpansions.insert(base);
            continue;
        }

        // Gather enemy threats at the base
        int enemyValue = 0;
        for (const auto &unit : Units::enemyAtBase(base))
        {
            enemyValue += CombatSim::unitValue(unit);
        }

        // Attack with a separate play if the unit value corresponds to three dragoons or less
        if (enemyValue <= 3 * CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon))
        {
            attackableExpansionsToEnemyUnitValue[base] = enemyValue;
            continue;
        }

        // Target the main army at this base if it does not already have a locked target and this base is most defended
        if (enemyValue < lowestEnemyValue)
        {
            lowestEnemyValue = enemyValue;
            lowestEnemyValueBase = base;
        }
    }

    // Choose the target for the main army
    if (!mainArmyTarget) mainArmyTarget = lowestEnemyValueBase;
    if (!mainArmyTarget)
    {
        mainArmyTarget = Map::getEnemyStartingNatural() && Map::getEnemyStartingNatural()->owner == BWAPI::Broodwar->enemy()
                         ? Map::getEnemyStartingNatural()
                         : Map::getEnemyMain();
    }

    // Ensure the main army play is correctly set
    if (mainArmyTarget)
    {
        if (attackEnemyBasePlay)
        {
            if (attackEnemyBasePlay->base != mainArmyTarget)
            {
                mainArmyPlay->status.transitionTo = std::make_shared<AttackEnemyBase>(mainArmyTarget);
            }
        }
        else
        {
            setMainPlay<AttackEnemyBase>(mainArmyPlay, mainArmyTarget);
        }
    }
    else
    {
        setMainPlay<MopUp>(mainArmyPlay);
    }

    // Remove AttackExpansion plays that are no longer needed
    for (auto &play : plays)
    {
        auto attackIslandExpansionPlay = std::dynamic_pointer_cast<AttackIslandExpansion>(play);
        if (attackIslandExpansionPlay)
        {
            islandExpansions.erase(attackIslandExpansionPlay->base);
            continue;
        }

        auto attackExpansionPlay = std::dynamic_pointer_cast<AttackExpansion>(play);
        if (!attackExpansionPlay) continue;

        auto it = attackableExpansionsToEnemyUnitValue.find(attackExpansionPlay->base);

        // If we no longer need to defend this base, remove the play
        if (it == attackableExpansionsToEnemyUnitValue.end())
        {
            attackExpansionPlay->status.complete = true;
            continue;
        }

        attackExpansionPlay->enemyDefenseValue = it->second;
        attackableExpansionsToEnemyUnitValue.erase(it);
    }

    // Add missing plays
    auto beforeMainArmyIt = [&plays]()
    {
        auto it = plays.begin();
        for (; it != plays.end(); it++)
        {
            if (std::dynamic_pointer_cast<MainArmyPlay>(*it) != nullptr)
            {
                break;
            }
        }
        return it;
    };
    for (auto &attackableExpansionAndEnemyUnitValue : attackableExpansionsToEnemyUnitValue)
    {
        plays.emplace(beforeMainArmyIt(), std::make_shared<AttackExpansion>(
                attackableExpansionAndEnemyUnitValue.first,
                attackableExpansionAndEnemyUnitValue.second));
        CherryVis::log() << "Added attack expansion play for base @ "
                         << BWAPI::WalkPosition(attackableExpansionAndEnemyUnitValue.first->getPosition());
    }
    for (auto &islandExpansion : islandExpansions)
    {
        plays.emplace(beforeMainArmyIt(), std::make_shared<AttackIslandExpansion>(islandExpansion));
        CherryVis::log() << "Added attack island expansion play for base @ "
                         << BWAPI::WalkPosition(islandExpansion->getPosition());
    }
}
