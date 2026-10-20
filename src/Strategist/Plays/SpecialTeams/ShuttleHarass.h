#pragma once

#include "Play.h"

class ShuttleHarass : public PlayItem
{
public:
    ShuttleHarass() : PlayItem("ShuttleHarass") {}

    void update() override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override
    {
        std::set<OwnedUnit> allUnits;
        for (auto &shuttleAndCargo : shuttlesAndCargo)
        {
            allUnits.insert(shuttleAndCargo.first);
            for (auto &cargo : shuttleAndCargo.second)
            {
                allUnits.insert(cargo);
            }
        }
        for (auto &cargoAndTarget : cargoAndTargets)
        {
            allUnits.insert(cargoAndTarget.first);
        }

        for (auto &gameUnit : allUnits)
        {
            movableUnitCallback(gameUnit);
        }
    }

    [[nodiscard]] bool canReassignUnit(const OwnedUnit &gameUnit) const override
    {
        /* Allow shuttles to be reassigned only if they are not carrying anything */
        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Shuttle)
        {
            return gameUnit->rawUnit->getLoadedUnits().empty();
        }

        /* Allow other units to be reassigned only if they are not being carried and don't have a target */
        if (gameUnit->rawUnit->isLoaded()) return false;
        auto iterRef = cargoAndTargets.find(gameUnit);
        return iterRef == cargoAndTargets.end() || iterRef->second == nullptr || !iterRef->second->exists();
    }

    void addUnit(const OwnedUnit &gameUnit) override
    {
        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Shuttle)
        {
            shuttlesAndCargo[gameUnit] = {};
        }
        else
        {
            /* This unit will be the cargo of the nearest shuttle not currently "full" */
            OwnedUnit closestShuttle = nullptr;
            int closestShuttleDist = INT_MAX;
            for (auto &shuttleAndCargo : shuttlesAndCargo)
            {
                if (shuttleAndCargo.second.size() >= 2) continue;

                int span = shuttleAndCargo.first->getDistance(gameUnit);
                if (span < closestShuttleDist)
                {
                    closestShuttleDist = span;
                    closestShuttle = shuttleAndCargo.first;
                }
            }

            if (closestShuttle)
            {
                shuttlesAndCargo[closestShuttle].insert(gameUnit);
            }
            else
            {
                AppLog::Get() << "Error: Tried to add " << (*gameUnit) << " to shuttle harass, but no valid shuttle available";
                status.removedUnits.emplace_back(gameUnit);
            }
        }
        PlayItem::addUnit(gameUnit);
    }

    void removeUnit(const OwnedUnit &gameUnit) override
    {
        /* Remove as cargo from any shuttles */
        for (auto &shuttleAndCargo : shuttlesAndCargo)
        {
            shuttleAndCargo.second.erase(gameUnit);
        }

        /* Remove shuttle or cargo */
        shuttlesAndCargo.erase(gameUnit);
        cargoAndTargets.erase(gameUnit);

        PlayItem::removeUnit(gameUnit);
    }

private:
    std::map<OwnedUnit, std::set<OwnedUnit>> shuttlesAndCargo;
    std::map<OwnedUnit, Unit> cargoAndTargets;
};
