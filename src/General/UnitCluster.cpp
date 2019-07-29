#include "UnitCluster.h"
#include "PathFinding.h"

void UnitCluster::addUnit(BWAPI::Unit unit)
{
    if (units.find(unit) != units.end()) return;

    units.insert(unit);

    center = BWAPI::Position(
        ((center.x * (units.size() - 1)) + unit->getPosition().x) / units.size(),
        ((center.y * (units.size() - 1)) + unit->getPosition().y) / units.size());
}

void UnitCluster::removeUnit(BWAPI::Unit unit)
{
    if (units.find(unit) == units.end()) return;

    units.erase(unit);
    if (units.empty()) return;

    center = BWAPI::Position(
        ((center.x * (units.size() + 1)) - unit->getPosition().x) / units.size(),
        ((center.y * (units.size() + 1)) - unit->getPosition().y) / units.size());
}

void UnitCluster::update(BWAPI::Position targetPosition)
{
    int sumX = 0;
    int sumY = 0;
    BWAPI::Unit closestToTarget = nullptr;
    int closestToTargetDist = INT_MAX;
    for (auto unitIt = units.begin(); unitIt != units.end(); )
    {
        auto unit = *unitIt;

        if (!unit->exists())
        {
            unitIt = units.erase(unitIt);
            continue;
        }

        sumX += unit->getPosition().x;
        sumY += unit->getPosition().y;

        int dist = PathFinding::GetGroundDistance(targetPosition, unit->getPosition(), unit->getType());
        if (dist != -1 && dist < closestToTargetDist)
        {
            closestToTargetDist = dist;
            closestToTarget = unit;
        }

        unitIt++;
    }

    if (units.empty()) return;

    center = BWAPI::Position(sumX / units.size(), sumY / units.size());
    vanguard = closestToTarget;
}