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

DefendBase::DefendBase(Base *base)
        : Play((std::ostringstream() << "Defend base @ " << base->getTilePosition()).str())
        , base(base)
        , squad(std::make_shared<DefendBaseSquad>(base))
        , workerDefenseSquad(std::make_shared<WorkerDefenseSquad>(base))
        , pylonLocation(BWAPI::TilePositions::Invalid)
        , pylon(nullptr)
{
    General::addSquad(squad);

    // Get the static defense locations for this base
    auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
    if (baseStaticDefenseLocations.first != BWAPI::TilePositions::Invalid)
    {
        pylonLocation = baseStaticDefenseLocations.first;
        cannonLocations.assign(baseStaticDefenseLocations.second.begin(), baseStaticDefenseLocations.second.end());

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

    // Gather enemy units threatening the base
    int enemyValue = 0;
    bool requireDragoons = false;
    for (const auto &unit : Units::allEnemy())
    {
        if (!unit->lastPositionValid) continue;
        if (!UnitUtil::IsCombatUnit(unit->type) && unit->lastSeenAttacking < (BWAPI::Broodwar->getFrameCount() - 120)) continue;
        if (!unit->isTransport() && !UnitUtil::CanAttackGround(unit->type)) continue;
        if (!unit->type.isBuilding() && unit->lastSeen < (BWAPI::Broodwar->getFrameCount() - 240)) continue;

        int dist = unit->isFlying
                   ? unit->lastPosition.getApproxDistance(base->getPosition())
                   : PathFinding::GetGroundDistance(unit->lastPosition, base->getPosition(), unit->type);
        if (dist == -1 || dist > 1000) continue;
        if (dist > 500)
        {
            auto predictedPosition = unit->predictPosition(5);
            int predictedDist = unit->isFlying
                                ? predictedPosition.getApproxDistance(base->getPosition())
                                : PathFinding::GetGroundDistance(predictedPosition, base->getPosition(), unit->type);
            if (predictedDist > dist) continue;
        }

        squad->enemyUnits.insert(unit);
        enemyValue += CombatSim::unitValue(unit);

        // Require dragoons against certain enemy types
        requireDragoons = requireDragoons ||
                          (unit->isFlying ||
                           unit->type == BWAPI::UnitTypes::Protoss_Dragoon ||
                           unit->type == BWAPI::UnitTypes::Terran_Vulture);
    }

    // Release any units in the squad if they are no longer required
    if (enemyValue == 0)
    {
        status.removedUnits = squad->getUnits();
        return;
    }

    workerDefenseSquad->execute(squad->enemyUnits, squad);

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

    // TODO: Request zealot or dragoon when we have that capability
    if (requestedUnits > 0)
    {
        status.unitRequirements.emplace_back(requestedUnits, BWAPI::UnitTypes::Protoss_Dragoon, base->getPosition());
    }
}

void DefendBase::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Always ensure the pylon is built
    if (!pylon && pylonLocation.isValid() && !Builder::pendingHere(pylonLocation))
    {
        auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(pylonLocation), 0, 0, 0);
        prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                   BWAPI::UnitTypes::Protoss_Pylon,
                                                                   buildLocation);
    }

    // Build cannons if necessary and possible
    if (!cannonLocations.empty())
    {
        int neededCannons = desiredCannons() - cannons.size();
        if (neededCannons > 0)
        {
            auto location = *cannonLocations.begin();
            if (!Builder::pendingHere(location))
            {
                auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(*cannonLocations.begin()), 0, 0, 0);
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                                           buildLocation);
            }
        }
    }
}

void DefendBase::disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                           const std::function<void(const MyUnit &)> &movableUnitCallback)
{
    Play::disband(removedUnitCallback, movableUnitCallback);
    workerDefenseSquad->disband();
}

int DefendBase::desiredCannons()
{
    // Desire no cannons if the pylon is not yet complete
    if (!pylon || !pylon->completed) return 0;

    // Count enemy air units we want to defend against
    int enemyAirUnits =
            Units::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) +
            Units::countEnemy(BWAPI::UnitTypes::Terran_Wraith) +
            Units::countEnemy(BWAPI::UnitTypes::Terran_Dropship) +
            Units::countEnemy(BWAPI::UnitTypes::Protoss_Scout) +
            Units::countEnemy(BWAPI::UnitTypes::Protoss_Shuttle);

    // Could the enemy have air units?

    // First check if we have observed anything that indicates an air threat or possible air threat
    bool enemyAirThreat =
            enemyAirUnits > 0 ||

            Players::upgradeLevel(BWAPI::Broodwar->enemy(), BWAPI::UpgradeTypes::Ventral_Sacs) > 0 ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Spire) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Mutalisk) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Scourge) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Greater_Spire) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Guardian) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Devourer) ||

            Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Starport) ||
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

            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Stargate) || // Stargate and anything with stargate as a prerequisite
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Corsair) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Scout) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Fleet_Beacon) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Carrier) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Arbiter_Tribunal) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Arbiter) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility) || // Robo and anything with robo as a prerequisite
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Shuttle) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Support_Bay) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Reaver) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observatory) ||
            Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observer);

    // Next, if the enemy is Zerg, guard against mutas we haven't scouted
    // TODO: Consider value of seen units to determine if the enemy could have gone for mutas
    if (!enemyAirThreat && BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg && BWAPI::Broodwar->getFrameCount() > 10000)
    {
        auto enemyMain = Map::getEnemyStartingMain();
        if (enemyMain && enemyMain->owner == BWAPI::Broodwar->enemy()
            && enemyMain->lastScouted < (BWAPI::Broodwar->getFrameCount() - UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Spire)))
        {
            enemyAirThreat = true;
        }
    }

    // Main is a special case, we only get cannons there to defend against air threats
    if (base == Map::getMyMain())
    {
        if (enemyAirUnits > 6) return 4;
        if (enemyAirThreat) return 3;
        return 0;
    }

    // At expansions we get cannons if the enemy is not contained or has an air threat
    if (!Strategist::isEnemyContained() || enemyAirUnits > 0) return 2;
    if (enemyAirThreat) return 1;
    return 0;
}