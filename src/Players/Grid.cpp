#include "Grid.h"

#include "Geo.h"

// We add a buffer on detection and threat ranges
// Generally this is more useful as it forces our units to keep their distance
const int RANGE_BUFFER = 48;

#ifndef _DEBUG
namespace
{
#endif

    std::map<std::pair<BWAPI::UnitType, int>, std::set<BWAPI::WalkPosition>> positionsInRangeCache;

    std::set<BWAPI::WalkPosition> & getPositionsInRange(BWAPI::UnitType type, int range)
    {
        std::set<BWAPI::WalkPosition> & positions = positionsInRangeCache[std::make_pair(type, range)];

        if (positions.empty())
            for (int x = -type.dimensionLeft() - range; x <= type.dimensionRight() + range; x++)
                for (int y = -type.dimensionUp() - range; y <= type.dimensionDown() + range; y++)
                    if (Geo::EdgeToPointDistance(type, BWAPI::Positions::Origin, BWAPI::Position(x, y)) <= range)
                        positions.insert(BWAPI::WalkPosition(x >> 3, y >> 3));

        return positions;
    }

#ifndef _DEBUG
}
#endif

void Grid::GridData::add(BWAPI::UnitType type, int range, BWAPI::Position position, int delta)
{
    int startX = position.x >> 3;
    int startY = position.y >> 3;
    for (auto pos : getPositionsInRange(type, range))
    {
        int x = startX + pos.x;
        int y = startY + pos.y;
        if (x >= 0 && x < maxX && y >= 0 && y < maxY)
            data[x + y * maxX] += delta;
    }

    frameLastUpdated = BWAPI::Broodwar->getFrameCount();
}

void Grid::dumpHeatmapIfChanged(std::string heatmapName, const GridData & data) const
{
    if (data.frameLastDumped >= data.frameLastUpdated) return;

    CherryVis::addHeatmap(heatmapName, data.data, data.maxX, data.maxY);

    data.frameLastDumped = BWAPI::Broodwar->getFrameCount();
}

void Grid::unitCreated(BWAPI::UnitType type, BWAPI::Position position, bool completed)
{
    if (!type.isFlyer()) _collision.add(type, 0, position, 1);
    if (completed) unitCompleted(type, position);
}

void Grid::unitCompleted(BWAPI::UnitType type, BWAPI::Position position)
{
    if (type.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        _groundThreat.add(
            type,
            upgradeTracker->weaponRange(type.groundWeapon()) + RANGE_BUFFER,
            position,
            upgradeTracker->weaponDamage(type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor());

        // For sieged tanks, subtract the area close to the tank
        if (type.groundWeapon().minRange() > 0)
        {
            _groundThreat.add(
                type,
                type.groundWeapon().minRange() - RANGE_BUFFER,
                position,
                -upgradeTracker->weaponDamage(type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor());
        }
    }

    if (type.airWeapon() != BWAPI::WeaponTypes::None)
    {
        _airThreat.add(
            type,
            upgradeTracker->weaponRange(type.airWeapon()) + RANGE_BUFFER,
            position,
            upgradeTracker->weaponDamage(type.airWeapon()) * type.maxAirHits() * type.airWeapon().damageFactor());
    }

    if (type.isDetector())
    {
        _detection.add(type, (type.isBuilding() ? (7 * 32) : (11 * 32)) + RANGE_BUFFER, position, 1);
    }
}

void Grid::unitMoved(BWAPI::UnitType type, BWAPI::Position position, BWAPI::UnitType fromType, BWAPI::Position fromPosition)
{
    if (type == fromType && ((position.x >> 3) == (fromPosition.x >> 3)) && ((position.y >> 3) == (fromPosition.y >> 3))) return;

    unitDestroyed(fromType, fromPosition, true);
    unitCreated(type, position, true);
}

void Grid::unitDestroyed(BWAPI::UnitType type, BWAPI::Position position, bool completed)
{
    if (!type.isFlyer()) _collision.add(type, 0, position, -1);

    // If the unit was a building that was destroyed or cancelled before being completed, we only
    // need to update the collision grid
    if (!completed) return;

    if (type.groundWeapon() != BWAPI::WeaponTypes::None)
    {
        _groundThreat.add(
            type,
            upgradeTracker->weaponRange(type.groundWeapon()) + RANGE_BUFFER,
            position,
            -upgradeTracker->weaponDamage(type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor());

        // For sieged tanks, add back the area close to the tank
        if (type.groundWeapon().minRange() > 0)
        {
            _groundThreat.add(
                type,
                type.groundWeapon().minRange() - RANGE_BUFFER,
                position,
                upgradeTracker->weaponDamage(type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor());
        }
    }

    if (type.airWeapon() != BWAPI::WeaponTypes::None)
    {
        _airThreat.add(
            type,
            upgradeTracker->weaponRange(type.airWeapon()) + RANGE_BUFFER,
            position,
            -upgradeTracker->weaponDamage(type.airWeapon()) * type.maxAirHits() * type.airWeapon().damageFactor());
    }

    if (type.isDetector())
    {
        _detection.add(type, (type.isBuilding() ? (7 * 32) : (11 * 32)) + RANGE_BUFFER, position, -1);
    }
}

void Grid::unitWeaponDamageUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerDamage, int newDamage)
{
    if (weapon.targetsGround())
    {
        _groundThreat.add(
            type,
            upgradeTracker->weaponRange(type.groundWeapon()),
            position,
            newDamage - formerDamage);
    }

    if (weapon.targetsAir())
    {
        _airThreat.add(
            type,
            upgradeTracker->weaponRange(type.groundWeapon()),
            position,
            newDamage - formerDamage);
    }
}

void Grid::unitWeaponRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerRange, int newRange)
{
    // We don't need to worry about minimum range here, since tanks do not have range upgrades

    if (weapon.targetsGround())
    {
        _groundThreat.add(
            type,
            formerRange + RANGE_BUFFER,
            position,
            -upgradeTracker->weaponDamage(type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor());

        _groundThreat.add(
            type,
            newRange + RANGE_BUFFER,
            position,
            upgradeTracker->weaponDamage(type.groundWeapon()) * type.maxGroundHits() * type.groundWeapon().damageFactor());
    }

    if (weapon.targetsAir())
    {
        _airThreat.add(
            type,
            formerRange + RANGE_BUFFER,
            position,
            -upgradeTracker->weaponDamage(type.airWeapon()) * type.maxAirHits() * type.airWeapon().damageFactor());

        _airThreat.add(
            type,
            newRange + RANGE_BUFFER,
            position,
            upgradeTracker->weaponDamage(type.airWeapon()) * type.maxAirHits() * type.airWeapon().damageFactor());
    }
}
