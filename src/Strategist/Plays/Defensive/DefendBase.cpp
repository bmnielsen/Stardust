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

DefendBase::DefendBase(Base *base, int enemyValue)
        : Play((std::ostringstream() << "Defend base @ " << base->getTilePosition()).str())
        , base(base)
        , enemyValue(enemyValue)
        , squad(std::make_shared<DefendBaseSquad>(base))
        , pylonLocation(BWAPI::TilePositions::Invalid)
        , pylon(nullptr)
{
    General::addSquad(squad);

    // Get the static defense locations for this base
    auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
    if (baseStaticDefenseLocations.isValid())
    {
        pylonLocation = baseStaticDefenseLocations.powerPylon;
        cannonLocations.assign(baseStaticDefenseLocations.workerDefenseCannons.begin(), baseStaticDefenseLocations.workerDefenseCannons.end());

        // Get any existing units - maybe we have had a defend base squad for this base before
        pylon = Units::myBuildingAt(pylonLocation);
        for (auto it = cannonLocations.begin(); it != cannonLocations.end();)
        {
            auto cannon = Units::myBuildingAt(*it);
            if (cannon)
            {
                cannons.push_back(cannon);
                it = cannonLocations.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
}

void DefendBase::update()
{
    squad->enemyUnits.clear();
    squad->enemyUnits.insert(Units::enemyAtBase(base).begin(), Units::enemyAtBase(base).end());

    // Clear dead static defense buildings
    if (pylon && !pylon->exists()) pylon = nullptr;
    for (auto it = cannons.begin(); it != cannons.end();)
    {
        if ((*it)->exists())
        {
            it++;
        }
        else
        {
            cannonLocations.emplace_front((*it)->getTilePosition());
            it = cannons.erase(it);
        }
    }

    // Check for static defense buildings that have started
    if (!pylon)
    {
        auto pendingBuilding = Builder::pendingHere(pylonLocation);
        if (pendingBuilding) pylon = pendingBuilding->unit;
    }
    for (auto it = cannonLocations.begin(); it != cannonLocations.end();)
    {
        auto pendingBuilding = Builder::pendingHere(*it);
        if (pendingBuilding && pendingBuilding->unit)
        {
            cannons.push_back(pendingBuilding->unit);
            it = cannonLocations.erase(it);
        }
        else
        {
            it++;
        }
    }

    // Update detection - release observers when no longer needed, request observers when needed
    auto &detectors = squad->getDetectors();
    if (!squad->needsDetection() && !detectors.empty())
    {
        status.removedUnits.insert(status.removedUnits.end(), detectors.begin(), detectors.end());
    }
    else if (squad->needsDetection() && detectors.empty())
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());
    }

    // Release any units in the squad if they are no longer required
    if (enemyValue == 0 || (squad->needsDetection() && detectors.empty()))
    {
        status.removedUnits = squad->getUnits();
        return;
    }

    // Don't add ground units to defend an island base
    if (base->island) return;

    // Otherwise reserve enough units to adequately defend the base
    int ourValue = 0;
    for (auto &unit : squad->getUnits())
    {
        ourValue += CombatSim::unitValue(unit);
    }

    int requestedUnits = 0;
    while (ourValue < (enemyValue * 6) / 5)
    {
        requestedUnits++;
        ourValue += CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Zealot);
    }

    // Handle early-game defense of the main or natural a bit differently
    if ((base == Map::getMyMain() || base == Map::getMyNatural()) && currentFrame < 10000)
    {
        // Take all units that are very close to the base
        auto gridNodePredicate = [](const NavigationGrid::GridNode &gridNode)
        {
            return gridNode.cost < 1200;
        };
        status.unitRequirements.emplace_back(requestedUnits,
                                             BWAPI::UnitTypes::Protoss_Zealot,
                                             base->getPosition(),
                                             true,
                                             gridNodePredicate);
        status.unitRequirements.emplace_back(requestedUnits,
                                             BWAPI::UnitTypes::Protoss_Dragoon,
                                             base->getPosition(),
                                             true,
                                             gridNodePredicate);

        // Take zealots not in the vanguard cluster if we still need more units
        if (requestedUnits > 0)
        {
            status.unitRequirements.emplace_back(requestedUnits,
                                                 BWAPI::UnitTypes::Protoss_Zealot,
                                                 base->getPosition(),
                                                 false);
        }

        return;
    }

    // TODO: Request zealot or dragoon when we have that capability
    if (requestedUnits > 0)
    {
        // If we are under pressure, don't defend this base
        auto pressure = Strategist::pressure();
        if (pressure > 0.6) return;

        // Only reserve units that have a safe path to the base
        auto gridNodePredicate = [](const NavigationGrid::GridNode &gridNode)
        {
            return gridNode.cost < 1200 || Players::grid(BWAPI::Broodwar->enemy()).groundThreat(gridNode.center()) == 0;
        };

        status.unitRequirements.emplace_back(requestedUnits,
                                             BWAPI::UnitTypes::Protoss_Dragoon,
                                             base->getPosition(),
                                             pressure < 0.4,
                                             gridNodePredicate);
    }
}

void DefendBase::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Always ensure the pylon is built
    if (!pylon && pylonLocation.isValid() && !Builder::pendingHere(pylonLocation))
    {
        auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(pylonLocation), 0, 0, 0);
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   label,
                                                                   BWAPI::UnitTypes::Protoss_Pylon,
                                                                   buildLocation);
    }

    // Build cannons if necessary and possible
    if (!cannonLocations.empty())
    {
        int neededCannons = desiredCannons() - cannons.size();
        for (int i = 0; i < neededCannons && i < cannonLocations.size(); i++)
        {
            // Determine the "normal" and "low" priority levels
            // By default we use "main army" and "lowest", but bump them up if:
            // - it is the main or natural in the early game
            // - the enemy has a lot of mutalisks
            int normalPriority = PRIORITY_MAINARMY;
            int lowPriority = PRIORITY_LOWEST;
            bool mutaThreat = Units::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) > 4;
            if ((base == Map::getMyMain() || base == Map::getMyNatural()) && currentFrame < 12000)
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

            // Use the low priority for the last cannon until the others are completed
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

            if (!Builder::pendingHere(cannonLocations[i]))
            {
                auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(cannonLocations[i]), 0, 0, 0);
                prioritizedProductionGoals[priority].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                  label,
                                                                  BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                                  buildLocation);
            }
        }
    }

    // Build an observer if we need one
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}

int DefendBase::desiredCannons()
{
    // Desire no cannons if the pylon is not yet complete
    if (!pylon || !pylon->completed) return 0;

    int neededCannons = enemyAirThreatCannons(base);

    // At expansions we always get cannons if the enemy is not contained
    if (base != Map::getMyMain() && base != Map::getMyNatural())
    {
        neededCannons = std::min(2, neededCannons);
    }

    // Always get one cannon outside our main if the enemy has at least one DT, they can quickly sneak in and kill our workers
    if (base != Map::getMyMain() && Units::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0)
    {
        neededCannons = std::min(1, neededCannons);
    }

    return neededCannons;
}

int DefendBase::enemyAirThreatCannons(Base *baseToDefend)
{
    // Count enemy air units we want to defend against
    int enemyAirUnits =
            Units::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) +
            Units::countEnemy(BWAPI::UnitTypes::Terran_Wraith) +
            Units::countEnemy(BWAPI::UnitTypes::Protoss_Scout);

    // Could the enemy have air units?

    // First check if we have observed anything that indicates an air threat or possible air threat
    bool enemyAirThreat =
            enemyAirUnits > 0 ||

            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Spire) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Mutalisk) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Scourge) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Greater_Spire) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Guardian) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Devourer) ||

            // Starport and anything with starport as a prerequisite
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Starport) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Dropship) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Wraith) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Valkyrie) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Science_Facility) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Science_Vessel) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Control_Tower) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Physics_Lab) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Battlecruiser) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Covert_Ops) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Ghost) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Nuclear_Silo) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Nuclear_Missile) ||

            // Stargate and anything with stargate as a prerequisite
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Stargate) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Corsair) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Scout) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Fleet_Beacon) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Carrier) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Arbiter);

    bool enemyDropThreat =
            Players::upgradeLevel(BWAPI::Broodwar->enemy(), BWAPI::UpgradeTypes::Ventral_Sacs) > 0 ||

            // Terran is handled by air threat

            // Robo and anything with robo as a prerequisite
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Shuttle) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Reaver) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observatory) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observer);

    // Handle island expansions now
    if (baseToDefend->island)
    {
        if (enemyAirUnits > 0) return 2;
        if (enemyAirThreat || enemyDropThreat) return 1;
        return 0;
    }

    int cannonBuildTime = UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Forge) < 1) cannonBuildTime += UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Forge);

    // Next, if the enemy is Zerg, guard against mutas we haven't scouted
    // TODO: Do economic modelling to determine if the enemy could have mutas
    auto enemyMain = Map::getEnemyStartingMain();
    if (!enemyAirThreat && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg && enemyMain)
    {
        // We use our opponent model to determine the worst-case completion frame, unless we have never lost a game against this opponent, in
        // which case we assume the worst
        int expectedMutaliskCompletionFrame = 8500;
        if (Opponent::winLossRatio(0.0, 200) < 0.99)
        {
            expectedMutaliskCompletionFrame = Opponent::minValueInPreviousGames("firstMutaliskCompleted", 8500, 15, 10);
        }

        int flightTime = PathFinding::ExpectedTravelTime(baseToDefend->getPosition(), enemyMain->getPosition(), BWAPI::UnitTypes::Zerg_Mutalisk) - 50;

        if (currentFrame > (expectedMutaliskCompletionFrame + flightTime - cannonBuildTime)) enemyAirThreat = true;
    }

    // Main and natural are special cases, we only get cannons there to defend against air threats or sneak attacks
    if (baseToDefend == Map::getMyMain() || baseToDefend == Map::getMyNatural())
    {
        if (enemyAirUnits > 6)
        {
            return std::max(1, 4 - (Units::countAll(BWAPI::UnitTypes::Protoss_Corsair) / 2));
        }
        if (enemyAirThreat)
        {
            return std::max(1, 3 - Units::countAll(BWAPI::UnitTypes::Protoss_Corsair));
        }
        if (enemyDropThreat && currentFrame > 8000) return 1;
        return 0;
    }

    if (enemyAirUnits > 0) return 2;
    if (enemyAirThreat || enemyDropThreat) return 1;
    return 0;
}
