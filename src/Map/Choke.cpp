#include "Choke.h"

#include "Geo.h"

namespace { auto & bwemMap = BWEM::Map::Instance(); }

namespace
{
    // Does a unit of the given type at the given position block the choke?
    bool blocksChoke(BWAPI::Position pos, BWAPI::UnitType type)
    {
        BWAPI::Position end1 = Geo::FindClosestUnwalkablePosition(pos, pos, 64);
        if (!end1.isValid()) return false;

        BWAPI::Position end2 = Geo::FindClosestUnwalkablePosition(BWAPI::Position(pos.x + pos.x - end1.x, pos.y + pos.y - end1.y), pos, 32);
        if (!end2.isValid()) return false;

        if (end1.getDistance(end2) < (end1.getDistance(pos) * 1.2)) return false;

        auto passable = [](BWAPI::UnitType ourUnit, BWAPI::UnitType enemyUnit, BWAPI::Position pos, BWAPI::Position wall)
        {
            BWAPI::Position topLeft = pos + BWAPI::Position(-ourUnit.dimensionLeft() - 1, -ourUnit.dimensionUp() - 1);
            BWAPI::Position bottomRight = pos + BWAPI::Position(ourUnit.dimensionRight() + 1, ourUnit.dimensionDown() + 1);

            std::vector<BWAPI::Position> positionsToCheck;

            if (wall.x < topLeft.x)
            {
                if (wall.y < topLeft.y)
                {
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown()));
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), pos.y));
                    positionsToCheck.push_back(BWAPI::Position(pos.x, topLeft.y - enemyUnit.dimensionDown()));
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp()));
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), pos.y));
                    positionsToCheck.push_back(BWAPI::Position(pos.x, bottomRight.y + enemyUnit.dimensionUp()));
                }
                else
                {
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown()));
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), pos.y));
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp()));
                }
            }
            else if (wall.x > bottomRight.x)
            {
                if (wall.y < topLeft.y)
                {
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown()));
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), pos.y));
                    positionsToCheck.push_back(BWAPI::Position(pos.x, topLeft.y - enemyUnit.dimensionDown()));
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp()));
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), pos.y));
                    positionsToCheck.push_back(BWAPI::Position(pos.x, bottomRight.y + enemyUnit.dimensionUp()));
                }
                else
                {
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown()));
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), pos.y));
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp()));
                }
            }
            else
            {
                if (wall.y < topLeft.y)
                {
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown()));
                    positionsToCheck.push_back(BWAPI::Position(pos.x, topLeft.y - enemyUnit.dimensionDown()));
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown()));
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.push_back(BWAPI::Position(pos.x, bottomRight.y + enemyUnit.dimensionUp()));
                    positionsToCheck.push_back(BWAPI::Position(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp()));
                    positionsToCheck.push_back(BWAPI::Position(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp()));
                }
                else
                {
                    return false;
                }
            }

            for (auto current : positionsToCheck)
                if (!Geo::Walkable(enemyUnit, current))
                    return false;

            return true;
        };

        return
            !passable(type, BWAPI::UnitTypes::Protoss_Probe, pos, end1) &&
            !passable(type, BWAPI::UnitTypes::Protoss_Probe, pos, end2);
    }
}

Choke::Choke(const BWEM::ChokePoint * _choke)
    : choke(_choke)
    , width(0)
    , isRamp(false)
    , highElevationTile(BWAPI::TilePositions::Invalid)
    , requiresMineralWalk(false)
    , firstAreaStartPosition(BWAPI::Positions::Invalid)
    , secondAreaStartPosition(BWAPI::Positions::Invalid)
{
    // Compute the choke width
    // Because the ends are themselves walkable tiles, we need to add a bit of padding to estimate the actual walkable width of the choke
    width = (int)BWAPI::Position(choke->Pos(choke->end1)).getDistance(BWAPI::Position(choke->Pos(choke->end2))) + 15;

    // BWEM tends to not set the endpoints of blocked chokes properly
    // So bump up the width in these cases
    // If there is a map with a narrow blocked choke it will break
    if (choke->Blocked() && width == 15) width = 32;

    // Check if the choke is a ramp
    int firstAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().first->Top()));
    int secondAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().second->Top()));
    if (firstAreaElevation != secondAreaElevation)
    {
        isRamp = true;

        // For narrow ramps, compute the tile nearest the center where the elevation
        // changes from low to high ground
        if (width < 128) computeRampHighGroundPosition();
    }

    // If the choke is narrow, generate positions where we can block the enemy worker scout
    if (width < 128)
    {
        // Initial center position using the BWEM data
        BWAPI::Position centerPoint = BWAPI::Position(choke->Center()) + BWAPI::Position(4, 4);
        BWAPI::Position end1 = Geo::FindClosestUnwalkablePosition(centerPoint, centerPoint, 64);
        BWAPI::Position end2 = Geo::FindClosestUnwalkablePosition(BWAPI::Position(centerPoint.x + centerPoint.x - end1.x, centerPoint.y + centerPoint.y - end1.y), centerPoint, 32);
        if (!end1.isValid() || !end2.isValid())
        {
            end1 = BWAPI::Position(choke->Pos(choke->end1)) + BWAPI::Position(4, 4);
            end2 = BWAPI::Position(choke->Pos(choke->end2)) + BWAPI::Position(4, 4);
        }

        // If the center is not really in the center, move it
        double end1Dist = end1.getDistance(centerPoint);
        double end2Dist = end2.getDistance(centerPoint);
        if (std::abs(end1Dist - end2Dist) > 2.0)
        {
            centerPoint = BWAPI::Position((end1.x + end2.x) / 2, (end1.y + end2.y) / 2);
            end1Dist = end1.getDistance(centerPoint);
            end2Dist = end2.getDistance(centerPoint);
        }

        computeScoutBlockingPositions(centerPoint, BWAPI::UnitTypes::Protoss_Probe, probeBlockScoutPositions);
        computeScoutBlockingPositions(centerPoint, BWAPI::UnitTypes::Protoss_Zealot, zealotBlockScoutPositions);
    }
}

void Choke::computeRampHighGroundPosition()
{
    BWAPI::Position chokeCenter = Center();

    int firstAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().first->Top()));
    int secondAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().second->Top()));
    int lowGroundElevation = std::min(firstAreaElevation, secondAreaElevation);
    int highGroundElevation = std::max(firstAreaElevation, secondAreaElevation);

    // Generate a set of low-ground tiles near the choke, ignoring "holes"
    std::set<BWAPI::TilePosition> lowGroundTiles;
    for (int x = -5; x <= 5; x++)
        for (int y = -5; y <= 5; y++)
        {
            BWAPI::TilePosition tile = BWAPI::TilePosition(choke->Center()) + BWAPI::TilePosition(x, y);
            if (!tile.isValid()) continue;
            if (BWAPI::Broodwar->getGroundHeight(tile) != lowGroundElevation) continue;
            if (BWAPI::Broodwar->getGroundHeight(tile + BWAPI::TilePosition(1, 0)) == lowGroundElevation ||
                BWAPI::Broodwar->getGroundHeight(tile + BWAPI::TilePosition(0, 1)) == lowGroundElevation ||
                BWAPI::Broodwar->getGroundHeight(tile + BWAPI::TilePosition(-1, 0)) == lowGroundElevation ||
                BWAPI::Broodwar->getGroundHeight(tile + BWAPI::TilePosition(0, -1)) == lowGroundElevation)
            {
                lowGroundTiles.insert(tile);
            }
        }

    const auto inChokeCenter = [this](BWAPI::Position pos) {
        BWAPI::Position end1 = Geo::FindClosestUnwalkablePosition(pos, pos, 64);
        if (!end1.isValid()) return false;

        BWAPI::Position end2 = Geo::FindClosestUnwalkablePosition(BWAPI::Position(pos.x + pos.x - end1.x, pos.y + pos.y - end1.y), pos, 32);
        if (!end2.isValid()) return false;

        if (end1.getDistance(end2) < end1.getDistance(pos)) return false;

        return std::abs(end1.getDistance(pos) - end2.getDistance(pos)) <= 2.0;
    };

    // Find the nearest position to the choke center that is on the high-ground border
    // This means that it is on the high ground and adjacent to one of the low-ground tiles found above
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;
    int bestDist = INT_MAX;
    for (int x = -64; x <= 64; x++)
        for (int y = -64; y <= 64; y++)
        {
            BWAPI::Position pos = chokeCenter + BWAPI::Position(x, y);
            if (!pos.isValid()) continue;
            if (pos.x % 32 > 0 && pos.x % 32 < 31 && pos.y % 32 > 0 && pos.y % 32 < 31) continue;
            if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))) continue;
            if (BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(pos)) != highGroundElevation) continue;
            if (lowGroundTiles.find(BWAPI::TilePosition(pos + BWAPI::Position(1, 0))) == lowGroundTiles.end() &&
                lowGroundTiles.find(BWAPI::TilePosition(pos + BWAPI::Position(-1, 0))) == lowGroundTiles.end() &&
                lowGroundTiles.find(BWAPI::TilePosition(pos + BWAPI::Position(0, 1))) == lowGroundTiles.end() &&
                lowGroundTiles.find(BWAPI::TilePosition(pos + BWAPI::Position(0, -1))) == lowGroundTiles.end())
                continue;
            if (!inChokeCenter(pos)) continue;

            int dist = pos.getApproxDistance(chokeCenter);
            if (dist < bestDist)
            {
                highElevationTile = BWAPI::TilePosition(pos);
                bestDist = dist;
                bestPos = pos;
            }
        }

    computeScoutBlockingPositions(bestPos, BWAPI::UnitTypes::Protoss_Probe, probeBlockScoutPositions);
    computeScoutBlockingPositions(bestPos, BWAPI::UnitTypes::Protoss_Zealot, zealotBlockScoutPositions);
}

void Choke::computeScoutBlockingPositions(BWAPI::Position center, BWAPI::UnitType type, std::set<BWAPI::Position> & result)
{
    if (!center.isValid()) return;
    if (result.size() == 1) return;

    int targetElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(center));

    // Search for a position in the immediate vicinity of the center that blocks the choke with one unit
    // Prefer at same elevation but return a lower elevation if that's all we have
    BWAPI::Position bestLowGround = BWAPI::Positions::Invalid;
    for (int x = 0; x <= 5; x++)
        for (int y = 0; y <= 5; y++)
            for (int xs = -1; xs <= (x == 0 ? 0 : 1); xs += 2)
                for (int ys = -1; ys <= (y == 0 ? 0 : 1); ys += 2)
                {
                    BWAPI::Position current = center + BWAPI::Position(x * xs, y * ys);
                    if (!blocksChoke(current, type)) continue;

                    // If this position is on the high-ground, return it
                    if (BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(current)) >= targetElevation)
                    {
                        if (!result.empty()) result.clear();
                        result.insert(current);
                        return;
                    }

                    // Otherwise set it as the best low-ground option if applicable
                    if (!bestLowGround.isValid())
                        bestLowGround = current;
                }

    if (bestLowGround.isValid())
    {
        if (!result.empty()) result.clear();
        result.insert(bestLowGround);
        return;
    }

    if (!result.empty()) return;

    // Try with two units instead

    // First grab the ends of the choke at the given center point
    BWAPI::Position end1 = Geo::FindClosestUnwalkablePosition(center, center, 64);
    if (!end1.isValid()) return;

    BWAPI::Position end2 = Geo::FindClosestUnwalkablePosition(BWAPI::Position(center.x + center.x - end1.x, center.y + center.y - end1.y), center, 32);
    if (!end2.isValid()) return;

    if (end1.getDistance(end2) < (end1.getDistance(center) * 1.2)) return;

    // Now find the positions between the ends
    std::vector<BWAPI::Position> toBlock;
    Geo::FindWalkablePositionsBetween(end1, end2, toBlock);
    if (toBlock.empty()) return;

    // Now we find two positions that block all of the positions in the vector

    // Step 1: remove positions on both ends that the enemy worker cannot stand on because of unwalkable terrain
    for (int i = 0; i < 2; i++)
    {
        BWAPI::Position start = *toBlock.begin();
        for (auto it = toBlock.begin(); it != toBlock.end(); it = toBlock.erase(it))
            if (Geo::Walkable(BWAPI::UnitTypes::Protoss_Probe, *it))
                break;

        std::reverse(toBlock.begin(), toBlock.end());
    }

    // Step 2: gather potential positions to place the unit that block the enemy unit locations at both ends
    std::vector<std::vector<BWAPI::Position>> candidatePositions = { std::vector<BWAPI::Position>(), std::vector<BWAPI::Position>() };
    for (int i = 0; i < 2; i++)
    {
        BWAPI::Position enemyPosition = *toBlock.begin();
        for (auto pos : toBlock)
        {
            // Is this a valid position for a probe?
            // We use a probe here because we sometimes mix probes and zealots and probes are larger
            if (!pos.isValid()) continue;
            if (!Geo::Walkable(BWAPI::UnitTypes::Protoss_Probe, pos)) continue;

            // Does it block the enemy position?
            if (!Geo::Overlaps(BWAPI::UnitTypes::Protoss_Probe, enemyPosition, type, pos))
                break;

            candidatePositions[i].push_back(pos);
        }

        std::reverse(toBlock.begin(), toBlock.end());
    }

    // Step 3: try to find a combination that blocks all positions
    // Prefer a combination that puts both units on the high ground
    // Prefer a combination that spaces out the units relatively evenly
    std::pair<BWAPI::Position, BWAPI::Position> bestPair = std::make_pair(BWAPI::Positions::Invalid, BWAPI::Positions::Invalid);
    std::pair<BWAPI::Position, BWAPI::Position> bestLowGroundPair = std::make_pair(BWAPI::Positions::Invalid, BWAPI::Positions::Invalid);
    int bestScore = INT_MAX;
    int bestLowGroundScore = INT_MAX;
    for (auto first : candidatePositions[0])
        for (auto second : candidatePositions[1])
        {
            int score;

            // Skip if the two units overlap
            // We use probes here because we sometimes mix probes and zealots and probes are larger
            if (Geo::Overlaps(BWAPI::UnitTypes::Protoss_Probe, first, BWAPI::UnitTypes::Protoss_Probe, second))
                continue;

            // Skip if any positions are not blocked by one of the units
            for (auto pos : toBlock)
                if (!Geo::Overlaps(type, first, BWAPI::UnitTypes::Protoss_Probe, pos) &&
                    !Geo::Overlaps(type, second, BWAPI::UnitTypes::Protoss_Probe, pos))
                {
                    goto nextCombination;
                }

            score = std::max({
                Geo::EdgeToPointDistance(type, first, end1),
                Geo::EdgeToPointDistance(type, second, end2),
                Geo::EdgeToEdgeDistance(type, first, type, second) });

            if (score < bestScore &&
                BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(first)) >= targetElevation &&
                BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(first)) >= targetElevation)
            {
                bestScore = score;
                bestPair = std::make_pair(first, second);
            }
            else if (score < bestLowGroundScore)
            {
                bestLowGroundScore = score;
                bestLowGroundPair = std::make_pair(first, second);
            }

        nextCombination:;
        }

    if (bestPair.first.isValid())
    {
        result.insert(bestPair.first);
        result.insert(bestPair.second);
    }
    else if (bestLowGroundPair.first.isValid())
    {
        result.insert(bestLowGroundPair.first);
        result.insert(bestLowGroundPair.second);
    }
}
