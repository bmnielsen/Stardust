#pragma once

#include "Play.h"

class ShuttleHarass : public Play
{
public:
    ShuttleHarass() : Play("ShuttleHarass") {}

    void update() override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override
    {
        std::set<MyUnit> allUnits;
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

        for (auto &unit : allUnits)
        {
            movableUnitCallback(unit);
        }
    }

    [[nodiscard]] bool canReassignUnit(const MyUnit &unit) const override
    {
        // Allow shuttles to be reassigned only if they are not carrying anything
        if (unit->type == BWAPI::UnitTypes::Protoss_Shuttle)
        {
            return unit->bwapiUnit->getLoadedUnits().empty();
        }

        // Allow other units to be reassigned only if they are not being carried and don't have a target
        if (unit->bwapiUnit->isLoaded()) return false;
        auto it = cargoAndTargets.find(unit);
        return it == cargoAndTargets.end() || it->second == nullptr || !it->second->exists();
    }

    void addUnit(const MyUnit &unit) override
    {
        if (unit->type == BWAPI::UnitTypes::Protoss_Shuttle)
        {
            shuttlesAndCargo[unit] = {};
        }
        else
        {
            // This unit will be the cargo of the nearest shuttle not currently "full"
            MyUnit closestShuttle = nullptr;
            int closestShuttleDist = INT_MAX;
            for (auto &shuttleAndCargo : shuttlesAndCargo)
            {
                if (shuttleAndCargo.second.size() >= 2) continue;

                int dist = shuttleAndCargo.first->getDistance(unit);
                if (dist < closestShuttleDist)
                {
                    closestShuttleDist = dist;
                    closestShuttle = shuttleAndCargo.first;
                }
            }

            if (closestShuttle)
            {
                shuttlesAndCargo[closestShuttle].insert(unit);
            }
            else
            {
                Log::Get() << "Error: Tried to add " << (*unit) << " to shuttle harass, but no valid shuttle available";
                status.removedUnits.emplace_back(unit);
            }
        }
        Play::addUnit(unit);
    }

    void removeUnit(const MyUnit &unit) override
    {
        // Remove as cargo from any shuttles
        for (auto &shuttleAndCargo : shuttlesAndCargo)
        {
            shuttleAndCargo.second.erase(unit);
        }

        // Remove shuttle or cargo
        shuttlesAndCargo.erase(unit);
        cargoAndTargets.erase(unit);

        Play::removeUnit(unit);
    }

private:
    std::map<MyUnit, std::set<MyUnit>> shuttlesAndCargo;
    std::map<MyUnit, Unit> cargoAndTargets;
};
