#include "Grid.h"
#include <utility>
#include "Geo.h"
#include "UnitUtil.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define LOG_NEGATIVE_VALUES true
#define ASSERT_NEGATIVE_VALUES false
#endif

namespace
{
    const int STASIS_RANGE = 44;

    std::map<std::pair<BWAPI::UnitType, int>, std::set<BWAPI::WalkPosition>> positionsInRangeCache;

    std::set<BWAPI::WalkPosition> &getPositionsInRange(BWAPI::UnitType type, int range)
    {
        std::set<BWAPI::WalkPosition> &positions = positionsInRangeCache[std::make_pair(type, range)];

        if (positions.empty())
            for (int x = -type.dimensionLeft() - range; x <= type.dimensionRight() + range; x++)
            {
                for (int y = -type.dimensionUp() - range; y <= type.dimensionDown() + range; y++)
                {
                    if (Geo::EdgeToPointDistance(type, BWAPI::Positions::Origin, BWAPI::Position(x, y)) <= range)
                        positions.insert(BWAPI::WalkPosition(x >> 3U, y >> 3U));
                }
            }

        return positions;
    }
}

void Grid::GridData::add(BWAPI::UnitType type, int range, BWAPI::Position position, int delta)
{
    int startX = position.x >> 3U;
    int startY = position.y >> 3U;
    for (auto &pos : getPositionsInRange(type, range))
    {
        int x = startX + pos.x;
        int y = startY + pos.y;
        if (x >= 0 && x < maxX && y >= 0 && y < maxY)
        {
            data[x * maxY + y] += delta;

#if LOG_NEGATIVE_VALUES
            if (data[x * maxY + y] < 0)
            {
                Log::Get() << "Negative grid value @ " << BWAPI::Position(x, y) << "\n"
                           << "start=" << BWAPI::WalkPosition(position)
                           << ";type=" << type
                           << ";range=" << range
                           << ";delta=" << delta
                           << ";value=" << data[x * maxY + y];

                Log::Debug() << "Negative grid value @ " << BWAPI::Position(x, y) << "\n"
                             << "start=" << BWAPI::WalkPosition(position)
                             << ";type=" << type
                             << ";range=" << range
                             << ";delta=" << delta
                             << ";value=" << data[x * maxY + y];

#if ASSERT_NEGATIVE_VALUES
                BWAPI::Broodwar->leaveGame();
#endif
            }
#endif
        }
    }

    frameLastUpdated = currentFrame;
}

void Grid::dumpHeatmapIfChanged(const std::string &heatmapName, const GridData &data)
{
    if (data.frameLastDumped >= data.frameLastUpdated) return;

    // Transpose the vector
    std::vector<long> heatmapData(data.maxX * data.maxY);
    for (int x = 0; x < data.maxX; x++)
    {
        for (int y = 0; y < data.maxY; y++)
        {
            heatmapData[x + y * data.maxX] = data.data[x * data.maxY + y];
        }
    }

    CherryVis::addHeatmap(heatmapName, heatmapData, data.maxX, data.maxY);

    data.frameLastDumped = currentFrame;
}

void Grid::unitCreated(BWAPI::UnitType type, BWAPI::Position position, bool completed, bool burrowed, bool immobile)
{
    if (!type.isFlyer() && !burrowed) _collision.add(type, 0, position, 1);
    if (!immobile && (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
        type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode))
    {
        _stasisRange.add(type, STASIS_RANGE, position, 1);
    }
    if (completed) unitCompleted(type, position, burrowed, immobile);
}

void Grid::unitCompleted(BWAPI::UnitType type, BWAPI::Position position, bool burrowed, bool immobile)
{
    auto weaponUnitType = type;
    if (type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;
    if (type == BWAPI::UnitTypes::Protoss_Carrier) weaponUnitType = BWAPI::UnitTypes::Protoss_Interceptor;
    if (type == BWAPI::UnitTypes::Protoss_Reaver) weaponUnitType = BWAPI::UnitTypes::Protoss_Scarab;

    if (weaponUnitType.groundWeapon() != BWAPI::WeaponTypes::None && !immobile &&
        ((burrowed && type == BWAPI::UnitTypes::Zerg_Lurker) || (!burrowed && type != BWAPI::UnitTypes::Zerg_Lurker)))
    {
        _groundThreat.add(
                type,
                upgradeTracker->weaponRange(weaponUnitType.groundWeapon()) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                upgradeTracker->weaponDamage(weaponUnitType.groundWeapon()) * weaponUnitType.maxGroundHits()
                * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));

        if (UnitUtil::IsStationaryAttacker(type))
        {
            _staticGroundThreat.add(
                    type,
                    upgradeTracker->weaponRange(weaponUnitType.groundWeapon()) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                    position,
                    upgradeTracker->weaponDamage(weaponUnitType.groundWeapon()) * weaponUnitType.maxGroundHits()
                    * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));
        }

        // For sieged tanks, subtract the area close to the tank
        if (weaponUnitType.groundWeapon().minRange() > 0)
        {
            _groundThreat.add(
                    type,
                    weaponUnitType.groundWeapon().minRange() - rangeBuffer,
                    position,
                    -upgradeTracker->weaponDamage(weaponUnitType.groundWeapon()) * weaponUnitType.maxGroundHits());

            _staticGroundThreat.add(
                    type,
                    weaponUnitType.groundWeapon().minRange() - rangeBuffer,
                    position,
                    -upgradeTracker->weaponDamage(weaponUnitType.groundWeapon()) * weaponUnitType.maxGroundHits());
        }
    }

    if (weaponUnitType.airWeapon() != BWAPI::WeaponTypes::None && !immobile && !burrowed)
    {
        _airThreat.add(
                type,
                upgradeTracker->weaponRange(weaponUnitType.airWeapon()) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                upgradeTracker->weaponDamage(weaponUnitType.airWeapon()) * weaponUnitType.maxAirHits()
                * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));
    }

    if (type.isDetector() && !immobile)
    {
        if (type.isBuilding())
        {
            _detection.add(type, 7 * 32 + rangeBuffer, position, 1);
        }
        else
        {
            _detection.add(type, upgradeTracker->unitSightRange(type) + rangeBuffer, position, 1);
        }
    }
}

void Grid::unitMoved(BWAPI::UnitType type,
                     BWAPI::Position position,
                     bool burrowed,
                     bool immobile,
                     BWAPI::UnitType fromType,
                     BWAPI::Position fromPosition,
                     bool fromBurrowed,
                     bool fromImmobile)
{
    if (type == fromType && burrowed == fromBurrowed && immobile == fromImmobile &&
        ((position.x >> 3U) == (fromPosition.x >> 3U)) && ((position.y >> 3U) == (fromPosition.y >> 3U)))
        return;

    unitDestroyed(fromType, fromPosition, true, fromBurrowed, fromImmobile);
    unitCreated(type, position, true, burrowed, immobile);
}

void Grid::unitDestroyed(BWAPI::UnitType type, BWAPI::Position position, bool completed, bool burrowed, bool immobile)
{
    if (!type.isFlyer() && !burrowed) _collision.add(type, 0, position, -1);
    if (!immobile && (type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
        type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode))
    {
        _stasisRange.add(type, STASIS_RANGE, position, -1);
    }

    // If the unit was a building that was destroyed or cancelled before being completed, we only
    // need to update the collision grid
    if (!completed) return;

    auto weaponUnitType = type;
    if (type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;
    if (type == BWAPI::UnitTypes::Protoss_Carrier) weaponUnitType = BWAPI::UnitTypes::Protoss_Interceptor;
    if (type == BWAPI::UnitTypes::Protoss_Reaver) weaponUnitType = BWAPI::UnitTypes::Protoss_Scarab;

    if (weaponUnitType.groundWeapon() != BWAPI::WeaponTypes::None && !immobile &&
        ((burrowed && type == BWAPI::UnitTypes::Zerg_Lurker) || (!burrowed && type != BWAPI::UnitTypes::Zerg_Lurker)))
    {
        // For sieged tanks, add the area close to the tank before removing the entire area
        // We need to do it in this order to avoid triggering the negative values check
        if (weaponUnitType.groundWeapon().minRange() > 0)
        {
            _groundThreat.add(
                    type,
                    weaponUnitType.groundWeapon().minRange() - rangeBuffer,
                    position,
                    upgradeTracker->weaponDamage(weaponUnitType.groundWeapon()) * weaponUnitType.maxGroundHits());

            _staticGroundThreat.add(
                    type,
                    weaponUnitType.groundWeapon().minRange() - rangeBuffer,
                    position,
                    upgradeTracker->weaponDamage(weaponUnitType.groundWeapon()) * weaponUnitType.maxGroundHits());
        }

        _groundThreat.add(
                type,
                upgradeTracker->weaponRange(weaponUnitType.groundWeapon()) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                -upgradeTracker->weaponDamage(weaponUnitType.groundWeapon()) * weaponUnitType.maxGroundHits()
                * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));

        if (UnitUtil::IsStationaryAttacker(type))
        {
            _staticGroundThreat.add(
                    type,
                    upgradeTracker->weaponRange(weaponUnitType.groundWeapon()) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                    position,
                    -upgradeTracker->weaponDamage(weaponUnitType.groundWeapon()) * weaponUnitType.maxGroundHits()
                    * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));
        }
    }

    if (weaponUnitType.airWeapon() != BWAPI::WeaponTypes::None && !immobile && !burrowed)
    {
        _airThreat.add(
                type,
                upgradeTracker->weaponRange(weaponUnitType.airWeapon()) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                -upgradeTracker->weaponDamage(weaponUnitType.airWeapon()) * weaponUnitType.maxAirHits()
                * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));
    }

    if (type.isDetector() && !immobile)
    {
        if (type.isBuilding())
        {
            _detection.add(type, 7 * 32 + rangeBuffer, position, -1);
        }
        else
        {
            _detection.add(type, upgradeTracker->unitSightRange(type) + rangeBuffer, position, -1);
        }
    }
}

void Grid::unitWeaponDamageUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerDamage, int newDamage)
{
    auto weaponUnitType = type;
    if (type == BWAPI::UnitTypes::Terran_Bunker) weaponUnitType = BWAPI::UnitTypes::Terran_Marine;
    if (type == BWAPI::UnitTypes::Protoss_Carrier) weaponUnitType = BWAPI::UnitTypes::Protoss_Interceptor;
    if (type == BWAPI::UnitTypes::Protoss_Reaver) weaponUnitType = BWAPI::UnitTypes::Protoss_Scarab;

    if (weapon.targetsGround())
    {
        _groundThreat.add(
                type,
                upgradeTracker->weaponRange(weapon) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                (newDamage - formerDamage) * weaponUnitType.maxGroundHits() * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));

        if (UnitUtil::IsStationaryAttacker(type))
        {
            _staticGroundThreat.add(
                    type,
                    upgradeTracker->weaponRange(weapon) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                    position,
                    (newDamage - formerDamage) * weaponUnitType.maxGroundHits() * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));
        }
    }

    if (weapon.targetsAir())
    {
        _airThreat.add(
                type,
                upgradeTracker->weaponRange(weapon) + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                (newDamage - formerDamage) * weaponUnitType.maxAirHits() * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));
    }
}

void Grid::unitWeaponRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, BWAPI::WeaponType weapon, int formerRange, int newRange)
{
    // We don't need to worry about minimum range here, since tanks do not have range upgrades
    // Also don't need to worry about carriers and reavers

    if (weapon.targetsGround())
    {
        _groundThreat.add(
                type,
                formerRange + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                -upgradeTracker->weaponDamage(weapon) * type.maxGroundHits()
                * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));

        _groundThreat.add(
                type,
                newRange + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                upgradeTracker->weaponDamage(weapon) * type.maxGroundHits()
                * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));

        if (UnitUtil::IsStationaryAttacker(type))
        {
            _staticGroundThreat.add(
                    type,
                    formerRange + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                    position,
                    -upgradeTracker->weaponDamage(weapon) * type.maxGroundHits()
                    * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));

            _staticGroundThreat.add(
                    type,
                    newRange + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                    position,
                    upgradeTracker->weaponDamage(weapon) * type.maxGroundHits()
                    * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));
        }
    }

    if (weapon.targetsAir())
    {
        _airThreat.add(
                type,
                formerRange + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                -upgradeTracker->weaponDamage(weapon) * type.maxAirHits()
                * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));

        _airThreat.add(
                type,
                newRange + rangeBuffer + (type == BWAPI::UnitTypes::Terran_Bunker ? 48 : 0),
                position,
                upgradeTracker->weaponDamage(weapon) * type.maxAirHits()
                * (type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 1));
    }
}

void Grid::unitSightRangeUpgraded(BWAPI::UnitType type, BWAPI::Position position, int formerRange, int newRange)
{
    // Only mobile detectors are handled here
    if (!type.isDetector() || type.isBuilding()) return;

    _detection.add(type, formerRange + rangeBuffer, position, -1);
    _detection.add(type, newRange + rangeBuffer, position, 1);
}
