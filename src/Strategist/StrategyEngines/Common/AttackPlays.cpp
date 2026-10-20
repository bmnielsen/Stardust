#include "StrategyEngine.h"

#include <Strategist/Plays/MainArmy/DefendMyMain.h>
#include <Strategist/Plays/MainArmy/ForgeFastExpand.h>
#include <Strategist/Plays/MainArmy/AttackEnemyBase.h>
#include <Strategist/Plays/MainArmy/MopUp.h>
#include <Strategist/Plays/Offensive/AttackExpansion.h>
#include <Strategist/Plays/Offensive/AttackIslandExpansion.h>
#include <Strategist/Plays/Scouting/ScoutEnemyExpos.h>

#include "Config.h"
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

void StrategyEngine::updateAttackPlays(std::vector<std::shared_ptr<PlayItem>> &playList, bool defendOurMain)
{
    auto mainArmyPlay = getPlay<MainArmyPlay>(playList);

    /* If we want to defend our main, cancel all attack plays and transition the main army */
    if (defendOurMain)
    {
        if (mainArmyPlay->isDefensive()) return;

        for (auto &play : playList)
        {
            if (auto attackExpansionPlay = std::dynamic_pointer_cast<AttackExpansion>(play))
            {
                attackExpansionPlay->status.complete = true;
            }
        }

        /* If we have built a wall, defend it */
        /* The ForgeFastExpand play will transition itself to DefendMyMain if it determines the wall is indefensible */
        if (BuildingPlacement::hasForgeGatewayWall())
        {
            auto &wall = BuildingPlacement::getForgeGatewayWall();
            auto forge = UnitMgr::myBuildingAt(wall.forge);
            if (forge && forge->type == BWAPI::UnitTypes::Protoss_Forge && forge->exists() && forge->rawUnit->isPowered())
            {
                setMainPlay<ForgeFastExpand>(mainArmyPlay);
                return;
            }
        }

        setMainPlay<DefendMyMain>(mainArmyPlay);
        return;
    }

    auto enemyStartingMain = Map::getEnemyStartingMain();
    auto enemyStartingNatural = Map::getEnemyStartingNatural();
    auto myNatural = Map::getMyNatural();
    auto attackEnemyBasePlay = dynamic_cast<AttackEnemyBase *>(mainArmyPlay);

    /* Get the current main army target base, if it is locked-in, to avoid indecision */
    auto getCurrentMainArmyTarget = [&]() -> Base*
    {
        /* Must be an attack base play against an enemy base */
        if (!attackEnemyBasePlay) return nullptr;
        if (attackEnemyBasePlay->baseLoc->owner != BWAPI::Broodwar->enemy()) return nullptr;

        /* Must have a vanguard cluster */
        auto vanguard = attackEnemyBasePlay->getSquad()->vanguardCluster();
        if (!vanguard) return nullptr;

        /* If the army is attacking the enemy main but has just discovered the enemy natural, switch to it */
        if (attackEnemyBasePlay->baseLoc == enemyStartingMain && enemyStartingNatural && enemyStartingNatural->owner == BWAPI::Broodwar->enemy())
        {
            /* Verify the cluster is closer to the natural than the main */
            if (vanguard->midpoint.getApproxDistance(enemyStartingNatural->getPosition())
                < vanguard->midpoint.getApproxDistance(enemyStartingMain->getPosition()))
            {
                return enemyStartingNatural;
            }
        }

        /* Stay locked in to the current base if the army is on the offensive */
        if (vanguard->currentActivity != UnitCluster::Activity::Regrouping) return attackEnemyBasePlay->baseLoc;

        /* Stay locked in to the current base if it is an expansion and we haven't been consistently regrouping for the past 5 seconds */
        if (attackEnemyBasePlay->baseLoc != enemyStartingMain && attackEnemyBasePlay->baseLoc != enemyStartingNatural
            && vanguard->lastActivityChange > (frameNow - 120))
        {
            return attackEnemyBasePlay->baseLoc;
        }

        return nullptr;
    };
    Base *mainArmyTarget = getCurrentMainArmyTarget();

    /* Now analyze all of the enemy bases to determine how we want to attack them */
    /* Main army target is either a fortified expansion, the enemy natural, or the enemy main */
    /* Exception is if the enemy took our natural early as a proxy */
    int lowestEnemyValue = INT_MAX;
    Base *lowestEnemyValueBase = nullptr;
    std::map<Base *, int> attackableExpansionsToEnemyUnitValue;
    std::set<Base *> islandExpansions;
    for (auto &baseLoc : Map::getEnemyBases())
    {
        /* Main and natural are default targets for our main army, so don't need to be analyzed */
        if (baseLoc == Map::getEnemyMain() || baseLoc == enemyStartingNatural) continue;

        /* Skip if the main army is already attacking this base */
        if (baseLoc == mainArmyTarget) continue;

        /* Handle islands separately */
        if (baseLoc->island)
        {
            islandExpansions.insert(baseLoc);
            continue;
        }

        /* If we haven't seen the depot and the base is in the starting area of the main, don't attack it */
        /* This is to handle maps like Andromeda where the enemy will often build buildings close to the min-only that are actually */
        /* logically part of their main */
        if (enemyStartingMain && enemyStartingMain->owner == BWAPI::Broodwar->enemy() && !baseLoc->resourceDepot)
        {
            auto enemyMainAreas = Map::getStartingBaseAreas(enemyStartingMain);
            if (enemyMainAreas.contains(baseLoc->getArea()))
            {
                continue;
            }
        }

        /* Gather enemy threats at the base */
        int enemyValue = 0;
        for (const auto &gameUnit : UnitMgr::enemyAtBase(baseLoc))
        {
            if (!UnitHelper::IsCombatUnit(gameUnit->type)) continue;
            enemyValue += CombatSim::unitValue(gameUnit);
        }

        /* Attack with a separate play if the unit value corresponds to three dragoons or less */
        /* Give up attacking an expansion in this way if it hasn't succeeded in 3000 frames (a bit over 2 minutes) */
        if (enemyValue <= 3 * CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon) &&
            baseLoc->ownedSince > (frameNow - 3000) &&
            baseLoc != myNatural)
        {
            attackableExpansionsToEnemyUnitValue[baseLoc] = enemyValue;
            continue;
        }

        /* Target the main army at this base if it does not already have a locked target and this base is most defended */
        if (enemyValue < lowestEnemyValue)
        {
            lowestEnemyValue = enemyValue;
            lowestEnemyValueBase = baseLoc;
        }
    }

    /* Choose the target for the main army */
    if (!mainArmyTarget) mainArmyTarget = lowestEnemyValueBase;
    if (!mainArmyTarget)
    {
        mainArmyTarget = enemyStartingNatural && enemyStartingNatural->owner == BWAPI::Broodwar->enemy()
                         ? enemyStartingNatural
                         : Map::getEnemyMain();
    }

    /* window_push (2026-10-08): once the one-shot push has fired, lock the main
     * army onto the enemy main and put the squad into all-in mode - no target
     * re-selection, no fight-or-flight, just press */
    if (Config::WindowPush::fired)
    {
        mainArmyTarget = Map::getEnemyMain();
    }

    /* Ensure the main army play is correctly set */
    if (mainArmyTarget)
    {
        if (attackEnemyBasePlay)
        {
            if (attackEnemyBasePlay->baseLoc != mainArmyTarget)
            {
                mainArmyPlay->status.transitionTo = std::make_shared<AttackEnemyBase>(mainArmyTarget);
            }

            if (Config::WindowPush::fired)
            {
                attackEnemyBasePlay->setAllIn(true);
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

    /* Remove AttackExpansion plays that are no longer needed */
    for (auto &play : playList)
    {
        auto attackIslandExpansionPlay = std::dynamic_pointer_cast<AttackIslandExpansion>(play);
        if (attackIslandExpansionPlay)
        {
            islandExpansions.erase(attackIslandExpansionPlay->baseLoc);
            continue;
        }

        auto attackExpansionPlay = std::dynamic_pointer_cast<AttackExpansion>(play);
        if (!attackExpansionPlay) continue;

        auto iterRef = attackableExpansionsToEnemyUnitValue.find(attackExpansionPlay->baseLoc);

        /* If we no longer need to attack this base, remove the play */
        if (iterRef == attackableExpansionsToEnemyUnitValue.end())
        {
            attackExpansionPlay->status.complete = true;
            continue;
        }

        attackExpansionPlay->enemyDefenseValue = iterRef->second;
        attackableExpansionsToEnemyUnitValue.erase(iterRef);
    }

    /* Add missing plays */
    for (auto &attackableExpansionAndEnemyUnitValue : attackableExpansionsToEnemyUnitValue)
    {
        playList.emplace(beforePlayIt<MainArmyPlay>(playList), std::make_shared<AttackExpansion>(
                attackableExpansionAndEnemyUnitValue.first,
                attackableExpansionAndEnemyUnitValue.second));
        Telemetry::log() << "Added attack expansion play for base @ "
                         << BWAPI::WalkPosition(attackableExpansionAndEnemyUnitValue.first->getPosition());
    }
    for (auto &islandExpansion : islandExpansions)
    {
        playList.emplace(beforePlayIt<MainArmyPlay>(playList), std::make_shared<AttackIslandExpansion>(islandExpansion));
        Telemetry::log() << "Added attack island expansion play for base @ "
                         << BWAPI::WalkPosition(islandExpansion->getPosition());
    }
}
