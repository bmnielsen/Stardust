#include "DefendBase.h"

#include "Strategist.h"
#include "General.h"
#include "Players.h"
#include "Map.h"
#include "Units.h"
#include "UnitUtil.h"
#include "PathFinding.h"
#include "BuildingPlacement.h"
#include "Builder.h"
#include "Opponent.h"

DefendBase::DefendBase(Base *baseLoc, int enemyValue)
        : PlayItem((std::ostringstream() << "Defend base @ " << baseLoc->getTilePosition()).str())
        , baseLoc(baseLoc)
        , enemyValue(enemyValue)
        , squad(std::make_shared<DefendBaseSquad>(baseLoc))
        , pylonLocation(BWAPI::TilePositions::Invalid)
        , pylon(nullptr)
{
    Marshal::addSquad(squad);

    /* Get the static defense locations for this base */
    auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(baseLoc);
    if (baseStaticDefenseLocations.isValid())
    {
        pylonLocation = baseStaticDefenseLocations.powerPylon;
        cannonLocations.assign(baseStaticDefenseLocations.workerDefenseCannons.begin(), baseStaticDefenseLocations.workerDefenseCannons.end());

        /* Get any existing units - maybe we have had a defend base squad for this base before */
        pylon = UnitMgr::myBuildingAt(pylonLocation);
        for (auto iterRef = cannonLocations.begin(); iterRef != cannonLocations.end();)
        {
            auto cannon = UnitMgr::myBuildingAt(*iterRef);
            if (cannon)
            {
                cannons.push_back(cannon);
                iterRef = cannonLocations.erase(iterRef);
            }
            else
            {
                iterRef++;
            }
        }
    }
}

void DefendBase::update()
{
    squad->enemyUnits.clear();
    squad->enemyUnits.insert(UnitMgr::enemyAtBase(baseLoc).begin(), UnitMgr::enemyAtBase(baseLoc).end());

    /* Clear dead static defense buildings */
    if (pylon && !pylon->exists()) pylon = nullptr;
    for (auto iterRef = cannons.begin(); iterRef != cannons.end();)
    {
        if ((*iterRef)->exists())
        {
            iterRef++;
        }
        else
        {
            cannonLocations.emplace_front((*iterRef)->getTilePosition());
            iterRef = cannons.erase(iterRef);
        }
    }

    /* Check for static defense buildings that have started */
    if (!pylon)
    {
        auto pendingBuilding = Constructor::pendingHere(pylonLocation);
        if (pendingBuilding) pylon = pendingBuilding->gameUnit;
    }
    for (auto iterRef = cannonLocations.begin(); iterRef != cannonLocations.end();)
    {
        auto pendingBuilding = Constructor::pendingHere(*iterRef);
        if (pendingBuilding && pendingBuilding->gameUnit)
        {
            cannons.push_back(pendingBuilding->gameUnit);
            iterRef = cannonLocations.erase(iterRef);
        }
        else
        {
            iterRef++;
        }
    }

    /* Update detection - release observers when no longer needed, request observers when needed */
    auto &detectors = squad->getDetectors();
    if (!squad->needsDetection() && !detectors.empty())
    {
        status.removedUnits.insert(status.removedUnits.end(), detectors.begin(), detectors.end());
    }
    else if (squad->needsDetection() && detectors.empty())
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());
    }

    /* Release any units in the squad if they are no longer required */
    if (enemyValue == 0 || (squad->needsDetection() && detectors.empty()))
    {
        status.removedUnits = squad->getUnits();
        return;
    }

    /* Don't add ground units to defend an island base */
    if (baseLoc->island) return;

    /* Otherwise reserve enough units to adequately defend the base */
    int ourValue = 0;
    for (auto &gameUnit : squad->getUnits())
    {
        ourValue += CombatSim::unitValue(gameUnit);
    }

    int requestedUnits = 0;
    while (ourValue < (enemyValue * 6) / 5)
    {
        requestedUnits++;
        ourValue += CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Zealot);
    }

    /* Handle early-game defense of the main or natural a bit differently */
    if ((baseLoc == Map::getMyMain() || baseLoc == Map::getMyNatural()) && frameNow < 10000)
    {
        /* Take all units that are very close to the base */
        auto gridNodePredicate = [](const NavGrid::GridNode &gridNode)
        {
            return gridNode.cost < 1200;
        };
        status.unitRequirements.emplace_back(requestedUnits,
                                             BWAPI::UnitTypes::Protoss_Zealot,
                                             baseLoc->getPosition(),
                                             true,
                                             gridNodePredicate);
        status.unitRequirements.emplace_back(requestedUnits,
                                             BWAPI::UnitTypes::Protoss_Dragoon,
                                             baseLoc->getPosition(),
                                             true,
                                             gridNodePredicate);

        /* Take zealots not in the vanguard cluster if we still need more units */
        if (requestedUnits > 0)
        {
            status.unitRequirements.emplace_back(requestedUnits,
                                                 BWAPI::UnitTypes::Protoss_Zealot,
                                                 baseLoc->getPosition(),
                                                 false);
        }

        return;
    }

    /* TODO: Request zealot or dragoon when we have that capability */
    if (requestedUnits > 0)
    {
        /* If we are under pressure, don't defend this base */
        auto pressure = Director::pressure();
        if (pressure > 0.6) return;

        /* Only reserve units that have a safe path to the base */
        auto gridNodePredicate = [](const NavGrid::GridNode &gridNode)
        {
            return gridNode.cost < 1200 || PlayerInfo::grid(BWAPI::Broodwar->enemy()).groundThreat(gridNode.midpoint()) == 0;
        };

        status.unitRequirements.emplace_back(requestedUnits,
                                             BWAPI::UnitTypes::Protoss_Dragoon,
                                             baseLoc->getPosition(),
                                             pressure < 0.4,
                                             gridNodePredicate);
    }
}

void DefendBase::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Always ensure the pylon is built */
    if (!pylon && pylonLocation.isValid() && !Constructor::pendingHere(pylonLocation))
    {
        auto buildLocation = BuildingPlacement::BuildLocation(BuildBlock::Location(pylonLocation),
                                                              BuildingPlacement::builderFrames(baseLoc->getPosition(),
                                                                                               pylonLocation,
                                                                                               BWAPI::UnitTypes::Protoss_Pylon),
                                                              0,
                                                              0);
        prioProdGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   label,
                                                                   BWAPI::UnitTypes::Protoss_Pylon,
                                                                   buildLocation);
    }

    /* Build cannons if necessary and possible */
    if (!cannonLocations.empty())
    {
        size_t neededCannons = desiredCannons() - cannons.size();
        for (int i = 0; i < neededCannons && i < cannonLocations.size(); i++)
        {
            /* Determine the "normal" and "low" priority levels */
            /* By default we use "main army" and "lowest", but bump them up if: */
            /* - it is the main or natural in the early game */
            /* - the enemy has a lot of mutalisks */
            int normalPriority = PRIORITY_MAINARMY;
            int lowPriority = PRIORITY_LOWEST;
            bool mutaThreat = UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) > 4;
            if ((baseLoc == Map::getMyMain() || baseLoc == Map::getMyNatural()) && frameNow < 12000)
            {
                if (mutaThreat)
                {
                    normalPriority = PRIORITY_BASEDEFENSE;
                    lowPriority = PRIORITY_MAINARMYBASEPRODUCTION;
                }
                else
                {
                    normalPriority = PRIORITY_MAINARMYBASEPRODUCTION;
                    lowPriority = PRIORITY_NORMAL;
                }
            }
            else if (mutaThreat)
            {
                normalPriority = PRIORITY_MAINARMYBASEPRODUCTION;
                lowPriority = PRIORITY_NORMAL;
            }

            /* Use the low priority for the last cannon until the others are completed */
            int priority = normalPriority;
            if (i == (neededCannons - 1))
            {
                if (i > 0)
                {
                    priority = lowPriority;
                }
                else
                {
                    for (const auto &cannon : cannons)
                    {
                        if (!cannon->completed)
                        {
                            priority = lowPriority;
                            break;
                        }
                    }
                }
            }

            if (!Constructor::pendingHere(cannonLocations[i]))
            {
                int framesUntilPowered = 0;
                if (!pylon || !pylon->completed)
                {
                    framesUntilPowered = Constructor::framesUntilCompleted(
                            pylonLocation,
                            UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Pylon) + 240);
                }

                auto buildLocation = BuildingPlacement::BuildLocation(
                        BuildBlock::Location(cannonLocations[i]),
                        BuildingPlacement::builderFrames(baseLoc->getPosition(),
                                                         cannonLocations[i],
                                                         BWAPI::UnitTypes::Protoss_Photon_Cannon),
                        framesUntilPowered,
                        0);
                prioProdGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                  label,
                                                                  BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                                  buildLocation);
            }
        }
    }

    /* Build an observer if we need one */
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}

int DefendBase::desiredCannons()
{
    /* Desire no cannons if the pylon is not yet complete */
    if (!pylon || !pylon->completed) return 0;

    int neededCannons = enemyAirThreatCannons(baseLoc);

    /* At expansions we always get cannons if the enemy is not contained */
    if (baseLoc != Map::getMyMain() && baseLoc != Map::getMyNatural())
    {
        neededCannons = std::min(2, neededCannons);
    }

    /* Always get one cannon outside our main if the enemy has at least one DT, they can quickly sneak in and kill our workers */
    if (baseLoc != Map::getMyMain() && (baseLoc != Map::getMyNatural() || Map::mapSpecificOverride()->hasBackdoorNatural())
        && UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0)
    {
        neededCannons = std::min(1, neededCannons);
    }

    return neededCannons;
}

int DefendBase::enemyAirThreatCannons(Base *baseToDefend)
{
    /* Count enemy air units we want to defend against */
    int enemyAirUnits =
            UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) +
            UnitMgr::countEnemy(BWAPI::UnitTypes::Terran_Wraith) +
            UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Scout);

    /* Could the enemy have air units? */

    /* First check if we have observed anything that indicates an air threat or possible air threat */
    bool enemyAirThreat =
            enemyAirUnits > 0 ||

            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Spire) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Mutalisk) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Scourge) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Greater_Spire) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Guardian) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Devourer) ||

            /* Starport and anything with starport as a prerequisite */
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Starport) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Dropship) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Wraith) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Valkyrie) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Science_Facility) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Science_Vessel) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Control_Tower) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Physics_Lab) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Battlecruiser) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Covert_Ops) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Ghost) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Nuclear_Silo) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Nuclear_Missile) ||

            /* Stargate and anything with stargate as a prerequisite */
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Stargate) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Corsair) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Scout) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Fleet_Beacon) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Carrier) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Arbiter);

    bool enemyDropThreat =
            PlayerInfo::upgradeLevel(BWAPI::Broodwar->enemy(), BWAPI::UpgradeTypes::Ventral_Sacs) > 0 ||

            /* Terran is handled by air threat */

            /* Robo and anything with robo as a prerequisite */
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Shuttle) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Reaver) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observatory) ||
            UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observer);

    /* Handle island expansions now */
    if (baseToDefend->island)
    {
        if (enemyAirUnits > 0) return 2;
        if (enemyAirThreat || enemyDropThreat) return 1;
        return 0;
    }

    int cannonBuildTime = UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
    if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Forge) < 1) cannonBuildTime += UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Forge);

    /* Next, if the enemy is Zerg, guard against mutas we haven't scouted */
    /* TODO: Do economic modelling to determine if the enemy could have mutas */
    auto enemyMain = Map::getEnemyStartingMain();
    if (!enemyAirThreat && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg && enemyMain)
    {
        /* We use our opponent model to determine the worst-case completion frame, unless we have never lost a game against this opponent, in */
        /* which case we assume the worst */
        int expectedMutaliskCompletionFrame = 8500;
        if (Opponent::winLossRatio(0.0, 200) < 0.99)
        {
            expectedMutaliskCompletionFrame = std::min(Opponent::minValueInPreviousGames("firstMutaliskCompleted", 8500, 15, 10), 20000);
        }

        int flightTime = Pathfinder::ExpectedTravelTime(baseToDefend->getPosition(), enemyMain->getPosition(), BWAPI::UnitTypes::Zerg_Mutalisk) - 50;

        if (frameNow > (expectedMutaliskCompletionFrame + flightTime - cannonBuildTime)) enemyAirThreat = true;
    }

    /* Main and natural are special cases, we only get cannons there to defend against air threats or sneak attacks */
    if (baseToDefend == Map::getMyMain() || baseToDefend == Map::getMyNatural())
    {
        if (enemyAirUnits > 6)
        {
            return std::max(1, 4 - (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Corsair) / 2));
        }
        if (enemyAirThreat)
        {
            return std::max(1, 3 - UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Corsair));
        }
        if (enemyDropThreat && frameNow > 8000) return 1;
        return 0;
    }

    if (enemyAirUnits > 0) return 2;
    if (enemyAirThreat || enemyDropThreat) return 1;
    return 0;
}
