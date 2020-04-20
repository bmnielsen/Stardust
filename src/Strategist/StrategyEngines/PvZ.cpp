#include "PvZ.h"

#include "Units.h"
#include "Map.h"
#include "Builder.h"

#include "Plays/Defensive/DefendMainBase.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Offensive/RallyArmy.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"

void PvZ::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<DefendMainBase>());
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<RallyArmy>());
}

void PvZ::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    defaultExpansions(plays);
}

void PvZ::updateProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    handleNaturalExpansion(prioritizedProductionGoals);

    handleUpgrades(prioritizedProductionGoals);
    handleDetection(prioritizedProductionGoals);
}

void PvZ::handleNaturalExpansion(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been (or is being) taken
    auto natural = Map::getMyNatural();
    if (natural->ownedSince != -1) return;
    if (Builder::isPendingHere(natural->getTilePosition())) return;

    // Currently we do this as soon as we have at least 5 combat units and one of them is 2000 units of ground distance away from our main
    // TODO: More nuanced approach
    int army = Units::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon);
    if (army < 5) return;

    bool unitIsAwayFromHome = false;
    for (const auto &unit : Units::allMine())
    {
        if (unit->type != BWAPI::UnitTypes::Protoss_Zealot && unit->type != BWAPI::UnitTypes::Protoss_Dragoon) continue;
        if (!unit->completed) continue;
        if (!unit->exists()) continue;

        int dist = PathFinding::GetGroundDistance(unit->lastPosition, Map::getMyMain()->getPosition(), unit->type);
        if (dist > 2000)
        {
            unitIsAwayFromHome = true;
            break;
        }
    }
    if (!unitIsAwayFromHome) return;

    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(natural->getTilePosition()),
                                                          BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                           natural->getTilePosition(),
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0, 0);
    prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Nexus, buildLocation);
}

void PvZ::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, true);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier);

    defaultGroundUpgrades(prioritizedProductionGoals);

    // TODO: Air upgrades
}

void PvZ::handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // The main army play will reactively request mobile detection when it sees a cloaked enemy unit
    // The logic here is to look ahead to make sure we already have detection available when we need it

    // Break out if we already have an observer
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0 || Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) > 0)
    {
        return;
    }

    // TODO: Use scouting information

    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1)
    {
        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    }
}
