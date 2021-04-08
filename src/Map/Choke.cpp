#include "Choke.h"

#include "Geo.h"

#if INSTRUMENTATION_ENABLED
#define DUMP_NARROW_CHOKE_HEATMAPS false
#define DEBUG_NARROW_CHOKE_ANALYSIS false
#endif

#define NARROW_CHOKE_THRESHOLD 128

namespace
{
    const double pi = 3.14159265358979323846;
    const double chokeSideAngleThreshold = pi / 4.0; // 45 degrees
    const double chokeCrossAngleThreshold = pi / 12.0; // 15 degrees

    double normalizeAngle(double angle)
    {
        while (angle < 0)
        {
            angle += pi;
        }

        while (angle > pi)
        {
            angle -= pi;
        }

        return angle;
    }

    double angleDiff(double first, double second)
    {
        double diff = std::abs(first - second);
        return std::min(diff, pi - diff);
    }

    std::vector<BWAPI::Position> walkPositionBorder(BWAPI::WalkPosition pos)
    {
        BWAPI::Position initial(pos);
        return std::vector<BWAPI::Position>{
                initial,
                initial + BWAPI::Position(1, 0),
                initial + BWAPI::Position(2, 0),
                initial + BWAPI::Position(3, 0),
                initial + BWAPI::Position(3, 1),
                initial + BWAPI::Position(3, 2),
                initial + BWAPI::Position(3, 3),
                initial + BWAPI::Position(2, 3),
                initial + BWAPI::Position(1, 3),
                initial + BWAPI::Position(0, 3),
                initial + BWAPI::Position(0, 2),
                initial + BWAPI::Position(0, 1)
        };
    }

    // Does a unit of the given type at the given position block the choke?
    bool blocksChoke(BWAPI::Position pos, BWAPI::UnitType type)
    {
        BWAPI::Position end1 = Geo::FindClosestUnwalkablePosition(pos, pos, 64);
        if (end1 == BWAPI::Positions::Invalid) return false;

        BWAPI::Position end2 = Geo::FindClosestUnwalkablePosition(BWAPI::Position(pos.x + pos.x - end1.x, pos.y + pos.y - end1.y), pos, 32);
        if (end2 == BWAPI::Positions::Invalid) return false;

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
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), pos.y);
                    positionsToCheck.emplace_back(pos.x, topLeft.y - enemyUnit.dimensionDown());
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp());
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), pos.y);
                    positionsToCheck.emplace_back(pos.x, bottomRight.y + enemyUnit.dimensionUp());
                }
                else
                {
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), pos.y);
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp());
                }
            }
            else if (wall.x > bottomRight.x)
            {
                if (wall.y < topLeft.y)
                {
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), pos.y);
                    positionsToCheck.emplace_back(pos.x, topLeft.y - enemyUnit.dimensionDown());
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp());
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), pos.y);
                    positionsToCheck.emplace_back(pos.x, bottomRight.y + enemyUnit.dimensionUp());
                }
                else
                {
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), pos.y);
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp());
                }
            }
            else
            {
                if (wall.y < topLeft.y)
                {
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), topLeft.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(pos.x, topLeft.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), topLeft.y - enemyUnit.dimensionDown());
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.emplace_back(pos.x, bottomRight.y + enemyUnit.dimensionUp());
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp());
                    positionsToCheck.emplace_back(topLeft.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp());
                }
                else
                {
                    return false;
                }
            }

            for (auto current : positionsToCheck)
            {
                if (!Geo::Walkable(enemyUnit, current))
                    return false;
            }

            return true;
        };

        return
                !passable(type, BWAPI::UnitTypes::Protoss_Probe, pos, end1) &&
                !passable(type, BWAPI::UnitTypes::Protoss_Probe, pos, end2);
    }
}

Choke::Choke(const BWEM::ChokePoint *_choke)
        : choke(_choke)
        , width(0)
        , center(BWAPI::Position(choke->Center()) + BWAPI::Position(4, 4))
        , isNarrowChoke(false)
        , length(false)
        , end1Center(BWAPI::Positions::Invalid)
        , end2Center(BWAPI::Positions::Invalid)
        , end1Exit(BWAPI::Positions::Invalid)
        , end2Exit(BWAPI::Positions::Invalid)
        , isRamp(false)
        , highElevationTile(BWAPI::TilePositions::Invalid)
        , requiresMineralWalk(false)
        , firstAreaMineralPatch(nullptr)
        , firstAreaStartPosition(BWAPI::Positions::Invalid)
        , secondAreaMineralPatch(nullptr)
        , secondAreaStartPosition(BWAPI::Positions::Invalid)
{
    // Estimate the choke width
    // Because the ends are themselves walkable tiles, we need to add a bit of padding to estimate the actual walkable width of the choke
    // We do further refinement of narrow chokes later
    width = (int) BWAPI::Position(choke->Pos(choke->end1)).getDistance(BWAPI::Position(choke->Pos(choke->end2))) + 15;

    // Check if the choke is a ramp
    // TODO: This could technically fail if the "top" tile is at a different height than the end of the choke
    int firstAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().first->Top()));
    int secondAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().second->Top()));
    if (firstAreaElevation != secondAreaElevation)
    {
        isRamp = true;
    }

    // BWEM doesn't give us the information we need to properly fight at narrow chokes, so do some extra analysis
    // Outputs:
    // - Accurate choke width
    // - Actual middle of the choke (BWEM can put the center at weird places sometimes)
    // - Lengthwise ends of the choke (i.e. entrance and exit)
    // - Positions where we can block a worker scout from getting through the choke
    // - For ramps, position nearest the center where high ground begins
    if (width < NARROW_CHOKE_THRESHOLD)
    {
        analyzeNarrowChoke();

        if (isNarrowChoke)
        {
            if (isRamp)
            {
                computeNarrowRampHighGroundPosition();
            }

            computeScoutBlockingPositions(center, BWAPI::UnitTypes::Protoss_Probe, probeBlockScoutPositions);
            computeScoutBlockingPositions(center, BWAPI::UnitTypes::Protoss_Zealot, zealotBlockScoutPositions);
        }
    }

    // If the center is not walkable, find the nearest position that is
    auto wpCenter = BWAPI::WalkPosition(center);
    if (!BWAPI::Broodwar->isWalkable(wpCenter))
    {
        auto spiral = Geo::Spiral();
        while (wpCenter.isValid() && !BWAPI::Broodwar->isWalkable(wpCenter))
        {
            spiral.Next();
            wpCenter = BWAPI::WalkPosition(center) + BWAPI::WalkPosition(spiral.x, spiral.y);
        }

        center = BWAPI::Position(wpCenter) + BWAPI::Position(4, 4);
    }
}

void Choke::setAsMainChoke()
{
    // Our main choke is always considered to be narrow regardless of its actual width
    if (!isNarrowChoke)
    {
        analyzeNarrowChoke();

        if (!isNarrowChoke)
        {
            Log::Get() << "WARNING: Unable to analyze main choke as a narrow choke";
        }
    }
}

void Choke::analyzeNarrowChoke()
{
    // TODO: This approach does not work for chokes where one of the sides is a map edge

    // Start by getting the sides closest to BWEM's center
    BWAPI::Position side1 = Geo::FindClosestUnwalkablePosition(center, center, 64 + width / 2);
    if (side1 == BWAPI::Positions::Invalid) return;

    BWAPI::Position side2 = Geo::FindClosestUnwalkablePosition(center, center, 64 + width / 2, side1);
    if (side2 == BWAPI::Positions::Invalid) return;

#if DEBUG_NARROW_CHOKE_ANALYSIS
    Log::Get() << "Analyzing choke with initial center @ " << BWAPI::WalkPosition(center);
#endif

    // Compute the approximate angle of the choke
    double angle = normalizeAngle(atan2(side1.y - side2.y, side1.x - side2.x) + (pi / 2.0));

    // Now find the walls of the choke by tracing along each side, collecting walk positions that follow the approximate choke angle
    auto traceChokeSide = [&angle](std::set<BWAPI::WalkPosition> &positions, BWAPI::WalkPosition initialPosition)
    {
#if DEBUG_NARROW_CHOKE_ANALYSIS
        Log::Debug() << "Tracing choke side from " << initialPosition;
#endif
        positions.insert(initialPosition);

        std::set<BWAPI::WalkPosition> visited;
        std::deque<BWAPI::WalkPosition> queue;

        auto visit = [&](BWAPI::WalkPosition pos)
        {
            if (visited.find(pos) != visited.end()) return;
            visited.insert(pos);

#if DEBUG_NARROW_CHOKE_ANALYSIS
            Log::Debug() << "Visiting " << pos;
#endif

            // Find the closest position to this one that is already part of the choke side
            int closestDist = INT_MAX;
            BWAPI::WalkPosition closest = BWAPI::WalkPositions::Invalid;
            for (auto other : positions)
            {
                auto dist = other.getApproxDistance(pos);
                if (dist > 0 && dist < closestDist)
                {
                    closestDist = dist;
                    closest = other;
                }
            }

            // Compare the angle of this position to the closest existing position to the expected angle of the choke
            if (closest != BWAPI::WalkPositions::Invalid)
            {
                double angleHere = normalizeAngle(atan2(pos.y - closest.y, pos.x - closest.x));
                double diff = angleDiff(angleHere, angle);
                if (diff < chokeSideAngleThreshold)
                {
                    positions.insert(pos);
#if DEBUG_NARROW_CHOKE_ANALYSIS
                    Log::Debug() << "Added part of choke side, angle diff vs. " << closest << " is " << (diff * 180.0 / pi);
#endif
                }
                else
                {
#if DEBUG_NARROW_CHOKE_ANALYSIS
                    Log::Debug() << "Not part of choke side, angle diff vs. " << closest << " is " << (diff * 180.0 / pi);
#endif
                }
            }

            // Add potential next positions depending on what neighbouring positions are walkable
            // We go around all positions around this one and add the unwalkable positions that are next to a walkable position
            auto createNeighbour = [&pos](int x, int y)
            {
                BWAPI::WalkPosition here(pos.x + x, pos.y + y);
                return std::make_pair(here, here.isValid() && BWAPI::Broodwar->isWalkable(here));
            };

            std::pair<BWAPI::WalkPosition, bool> neighbours[] = {
                    createNeighbour(1, 0),
                    createNeighbour(1, 1),
                    createNeighbour(0, 1),
                    createNeighbour(-1, 1),
                    createNeighbour(-1, 0),
                    createNeighbour(-1, -1),
                    createNeighbour(0, -1),
                    createNeighbour(1, -1),
                    createNeighbour(1, 0),
                    createNeighbour(1, 1)
            };

            for (int i = 1; i < 9; i++)
            {
                if (neighbours[i].second) continue;
                if (!neighbours[i - 1].second && !neighbours[i + 1].second) continue;
                queue.push_back(neighbours[i].first);
#if DEBUG_NARROW_CHOKE_ANALYSIS
                Log::Debug() << "Pushed to queue " << neighbours[i].first;
#endif
            }
        };

        visit(initialPosition);

        int lastSize = positions.size();
        int visitsSinceLastSizeChange = 0;
        while (!queue.empty())
        {
            visitsSinceLastSizeChange++;

            auto here = queue.front();
            queue.pop_front();

            visit(here);

            if (positions.size() > lastSize)
            {
                lastSize = positions.size();
                visitsSinceLastSizeChange = 0;
            }

            if (visitsSinceLastSizeChange > 20) break;
        }
    };
    std::set<BWAPI::WalkPosition> side1Positions, side2Positions;
    traceChokeSide(side1Positions, BWAPI::WalkPosition(side1.x >> 3, side1.y >> 3));
    traceChokeSide(side2Positions, BWAPI::WalkPosition(side2.x >> 3, side2.y >> 3));

    // Get the choke width at each point on each side
    auto addWidthIfSmaller = [](std::map<BWAPI::WalkPosition, std::pair<int, BWAPI::WalkPosition>> &widths,
                                BWAPI::WalkPosition pos,
                                BWAPI::WalkPosition other,
                                int widthHere)
    {
        auto it = widths.find(pos);
        if (it == widths.end() || it->second.first > widthHere) widths[pos] = std::make_pair(widthHere, other);
    };
    std::map<BWAPI::WalkPosition, std::pair<int, BWAPI::WalkPosition>> side1Width, side2Width;
    std::map<BWAPI::WalkPosition, double> side1Angle, side2Angle;
    int minWidth = INT_MAX;
    for (auto side1Pos : side1Positions)
    {
        for (auto side2Pos : side2Positions)
        {
            int widthHere = (int) BWAPI::Position(side1Pos).getDistance(BWAPI::Position(side2Pos));
            addWidthIfSmaller(side1Width, side1Pos, side2Pos, widthHere);
            addWidthIfSmaller(side2Width, side2Pos, side1Pos, widthHere);
            if (widthHere < minWidth) minWidth = widthHere;
        }
    }

    // Trim positions where the width is much longer than the minimum width
    auto trimWidePositions = [&minWidth](std::map<BWAPI::WalkPosition, std::pair<int, BWAPI::WalkPosition>> &widths,
                                         std::set<BWAPI::WalkPosition> &positions)
    {
        int widthCutoff = (int) ((double) minWidth * 1.3);
        for (auto it = positions.begin(); it != positions.end();)
        {
            if (widths[*it].first > widthCutoff)
            {
#if DEBUG_NARROW_CHOKE_ANALYSIS
                Log::Debug() << "Removed " << *it << "; width: " << widths[*it].first << "; cutoff: " << widthCutoff;
#endif
                it = positions.erase(it);
            }
            else
            {
                it++;
            }
        }
    };
    trimWidePositions(side1Width, side1Positions);
    trimWidePositions(side2Width, side2Positions);

    // Find the angle at the minimum width
    double angleX = 0.0;
    double angleY = 0.0;
    int angleCount = 0;
    int widthCutoff = (int) ((double) minWidth * 1.05);
    for (auto posAndWidthAndOther : side1Width)
    {
        if (posAndWidthAndOther.second.first <= widthCutoff)
        {

            auto thisAngle = atan2(posAndWidthAndOther.first.y - posAndWidthAndOther.second.second.y,
                                   posAndWidthAndOther.first.x - posAndWidthAndOther.second.second.x);
            angleX += sin(thisAngle);
            angleY += cos(thisAngle);
            angleCount++;

#if DEBUG_NARROW_CHOKE_ANALYSIS
            Log::Debug() << "Min width angle from " << posAndWidthAndOther.first << " to " << posAndWidthAndOther.second.second
                       << "; angle: " << (thisAngle * 180.0 / pi);
#endif
        }
    }

    if (angleCount == 0)
    {
        Log::Get() << "ERROR: Could not compute angle of choke @ " << BWAPI::WalkPosition(center);
        return;
    }

    angle = normalizeAngle(atan2(angleX / (double) angleCount, angleY / (double) angleCount));

    // Trim positions where the angle is much different than the minimum width angle
    auto trimCrookedPositions = [&angle](std::map<BWAPI::WalkPosition, std::pair<int, BWAPI::WalkPosition>> &widths,
                                         std::set<BWAPI::WalkPosition> &positions)
    {
        for (auto it = positions.begin(); it != positions.end();)
        {
            auto other = widths[*it].second;
            double angleHere = normalizeAngle(atan2(it->y - other.y, it->x - other.x));
            if (angleDiff(angle, angleHere) > chokeCrossAngleThreshold)
            {
#if DEBUG_NARROW_CHOKE_ANALYSIS
                Log::Debug() << "Removed " << *it << " to " << other
                             << "; angle: " << (angle * 180.0 / pi)
                             << "; angleHere: " << (angleHere * 180.0 / pi)
                             << "; angleDiff: " << (angleDiff(angle, angleHere) * 180.0 / pi);
#endif
                it = positions.erase(it);
            }
            else
            {
                it++;
            }
        }
    };
    trimCrookedPositions(side1Width, side1Positions);
    trimCrookedPositions(side2Width, side2Positions);

    if (side1Positions.empty() || side2Positions.empty())
    {
        Log::Get() << "ERROR: Trimmed all positions from choke @ " << BWAPI::WalkPosition(center);
        return;
    }

    // Now find the corners by getting the pairs of positions on each side that are furthest from each other
    auto findFurthestPositions = [](std::set<BWAPI::WalkPosition> &positions, BWAPI::WalkPosition (&corners)[2])
    {
        int maxDist = -1;
        for (auto pos : positions)
        {
            for (auto other : positions)
            {
                auto dist = pos.getApproxDistance(other);
                if (dist > maxDist)
                {
                    maxDist = dist;
                    corners[0] = pos;
                    corners[1] = other;
                }
            }
        }
    };
    BWAPI::WalkPosition wpSide1Ends[2], wpSide2Ends[2];
    findFurthestPositions(side1Positions, wpSide1Ends);
    findFurthestPositions(side2Positions, wpSide2Ends);

    // Align the ends
    double end1Angle = normalizeAngle(atan2(wpSide1Ends[0].y - wpSide2Ends[0].y, wpSide1Ends[0].x - wpSide2Ends[0].x));
    double end2Angle = normalizeAngle(atan2(wpSide1Ends[0].y - wpSide2Ends[1].y, wpSide1Ends[0].x - wpSide2Ends[1].x));
    if (angleDiff(angle, end1Angle) > angleDiff(angle, end2Angle))
    {
        BWAPI::WalkPosition tmp = wpSide2Ends[0];
        wpSide2Ends[0] = wpSide2Ends[1];
        wpSide2Ends[1] = tmp;
    }

    // Convert to positions by taking the closest points
    auto findClosestPositions = [](BWAPI::WalkPosition wpFirst, BWAPI::WalkPosition wpSecond, BWAPI::Position *first, BWAPI::Position *second)
    {
        int minDist = INT_MAX;
        for (auto firstPos : walkPositionBorder(wpFirst))
        {
            for (auto secondPos : walkPositionBorder(wpSecond))
            {
                int dist = firstPos.getApproxDistance(secondPos);
                if (dist < minDist)
                {
                    minDist = dist;
                    *first = firstPos;
                    *second = secondPos;
                }
            }
        }
    };
    BWAPI::Position side1Ends[2], side2Ends[2];
    findClosestPositions(wpSide1Ends[0], wpSide2Ends[0], &side1Ends[0], &side2Ends[0]);
    findClosestPositions(wpSide1Ends[1], wpSide2Ends[1], &side1Ends[1], &side2Ends[1]);

    // Now that we have the corners, compute the center at each end
    end1Center = (side1Ends[0] + side2Ends[0]) / 2;
    end2Center = (side1Ends[1] + side2Ends[1]) / 2;

    // Compute the "exits", a position one tile further than the ends
    end1Exit = end1Center + Geo::ScaleVector(end1Center - end2Center, 32);
    end2Exit = end2Center + Geo::ScaleVector(end2Center - end1Center, 32);

    // In order to allow analysis of chokes along the map edges, we allow the sides and corners to be off the map
    // If any of the computed results are invalid though, we bail out now
    if (!end1Center.isValid() || !end2Center.isValid() || !end1Exit.isValid() || !end2Exit.isValid())
    {
        Log::Get() << "WARNING: Choke positions out of bounds for choke @ " << BWAPI::WalkPosition(center);
        return;
    }

    // We can now officially mark this as an analyzed narrow choke
    isNarrowChoke = true;

    // The minimum width computed earlier is done with walk tiles, so adjust it a bit to approximate the actual width
    width = minWidth - 8;

    // Compute the length
    length = (int) end1Center.getDistance(end2Center);

    // Initialize the center as the centroid of the corners, then pull it to the center of the choke at that location
    center = (side1Ends[0] + side1Ends[1] + side2Ends[0] + side2Ends[1]) / 4;
    BWAPI::Position centerSide1 = Geo::FindClosestUnwalkablePosition(center, center, 64);
    if (centerSide1 != BWAPI::Positions::Invalid)
    {
        BWAPI::Position centerSide2 = Geo::FindClosestUnwalkablePosition(BWAPI::Position(center.x + center.x - centerSide1.x,
                                                                                         center.y + center.y - centerSide1.y),
                                                                         center,
                                                                         64,
                                                                         centerSide1);
        if (centerSide2 != BWAPI::Positions::Invalid) center = (centerSide1 + centerSide2) / 2;
    }

    // Now calculate the choke tiles
    tileSide.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4);
    std::vector<bool> visited(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4);

    // We do this at half-tile resolution, as that is what the collision grid in the combat sim uses
    struct HalfTile
    {
        HalfTile(unsigned int x, unsigned int y) : x(x), y(y) {}

        explicit HalfTile(BWAPI::Position pos) : x(pos.x >> 4U), y(pos.y >> 4U) {}

        unsigned int x;
        unsigned int y;

        [[nodiscard]] bool isValid() const
        {
            return x < (BWAPI::Broodwar->mapWidth() * 2) && y < (BWAPI::Broodwar->mapHeight() * 2);
        }

        [[nodiscard]] bool isWalkable() const
        {
            return BWAPI::Broodwar->isWalkable((x << 1U), (y << 1U)) &&
                   BWAPI::Broodwar->isWalkable((x << 1U) + 1, (y << 1U)) &&
                   BWAPI::Broodwar->isWalkable((x << 1U), (y << 1U) + 1) &&
                   BWAPI::Broodwar->isWalkable((x << 1U) + 1, (y << 1U) + 1);
        }

        [[nodiscard]] unsigned int index() const
        {
            return x + y * BWAPI::Broodwar->mapWidth() * 2;
        }

        [[nodiscard]] BWAPI::TilePosition toTilePosition() const
        {
            return BWAPI::TilePosition(x >> 1U, y >> 1U);
        }
    };

    // Initialize the queue by tracing each end of the choke
    std::deque<std::pair<int, HalfTile>> queue;
    std::deque<std::pair<int, HalfTile>> unwalkableQueue;
    auto addHalfTilesBetween = [&](BWAPI::Position start, BWAPI::Position end, int side)
    {
        auto addPosition = [&](BWAPI::Position pos)
        {
            if (!pos.isValid()) return;

            auto tile = HalfTile(pos.x >> 4U, pos.y >> 4U);
            tileSide[tile.index()] = side;

            if (visited[tile.index()]) return;

            visited[tile.index()] = true;
            (tile.isWalkable() ? queue : unwalkableQueue).emplace_back(std::make_pair(side, tile));
            chokeTiles.insert(tile.toTilePosition());
        };

        addPosition(start);
        BWAPI::Position diff = end - start;
        int dist = start.getApproxDistance(end);
        for (int d = 8; d < dist; d += 8)
        {
            auto v = Geo::ScaleVector(diff, d);
            if (v == BWAPI::Positions::Invalid) continue;
            addPosition(start + v);
        }
        addPosition(end);
    };
    addHalfTilesBetween(side1Ends[0], side2Ends[0], -2);
    addHalfTilesBetween(side1Ends[1], side2Ends[1], 1);

    // Queue the center of the choke to be set to 0, unless the center is on one of the traced ends
    auto centerTile = HalfTile(center);
    if (!visited[centerTile.index()])
    {
        queue.emplace_front(std::make_pair(0, centerTile));
    }

    // Now flood-fill
    auto visit = [&](std::pair<int, HalfTile> &tile, int x, int y)
    {
        auto next = HalfTile(tile.second.x + x, tile.second.y + y);

        if (!next.isValid() || visited[next.index()]) return;

        tileSide[next.index()] = tile.first;
        visited[next.index()] = true;
        if (tile.first == 0)
        {
            if (next.isWalkable())
            {
                queue.emplace_front(std::make_pair(tile.first, next));
            }
        }
        else
        {
            ((tile.second.isWalkable() && next.isWalkable()) ? queue : unwalkableQueue).emplace_back(std::make_pair(tile.first, next));
        }

        if (tileSide[tile.second.index()] == 0)
        {
            chokeTiles.insert(next.toTilePosition());
        }
    };
    while (!queue.empty() || !unwalkableQueue.empty())
    {
        auto tile = (queue.empty() ? unwalkableQueue : queue).front();
        (queue.empty() ? unwalkableQueue : queue).pop_front();

        visit(tile, 1, 0);
        visit(tile, -1, 0);
        visit(tile, 0, 1);
        visit(tile, 0, -1);
    }

    // Finally re-trace the ends to set them to 0
    addHalfTilesBetween(side1Ends[0], side2Ends[0], 0);
    addHalfTilesBetween(side1Ends[1], side2Ends[1], 0);

#if DUMP_NARROW_CHOKE_HEATMAPS
    std::vector<long> chokeData(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 16);

    // Sides
    for (auto pos : side1Positions)
    {
        chokeData[pos.x + pos.y * BWAPI::Broodwar->mapWidth() * 4] = 1;
    }
    for (auto pos : side2Positions)
    {
        chokeData[pos.x + pos.y * BWAPI::Broodwar->mapWidth() * 4] = 1;
    }

    // Ends
    chokeData[wpSide1Ends[0].x + wpSide1Ends[0].y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpSide1Ends[1].x + wpSide1Ends[1].y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpSide2Ends[0].x + wpSide2Ends[0].y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpSide2Ends[1].x + wpSide2Ends[1].y * BWAPI::Broodwar->mapWidth() * 4] = 100;

    // Centers
    BWAPI::WalkPosition wpCenter(center);
    BWAPI::WalkPosition wpEnd1Center(end1Center);
    BWAPI::WalkPosition wpEnd2Center(end2Center);
    chokeData[wpCenter.x + wpCenter.y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpEnd1Center.x + wpEnd1Center.y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpEnd2Center.x + wpEnd2Center.y * BWAPI::Broodwar->mapWidth() * 4] = 100;

    CherryVis::addHeatmap((std::ostringstream() << "Choke@" << wpCenter).str(),
                          chokeData,
                          BWAPI::Broodwar->mapWidth() * 4,
                          BWAPI::Broodwar->mapHeight() * 4);

    std::vector<long> chokeSideData(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4);
    for (int y = 0; y < BWAPI::Broodwar->mapHeight() * 2; y++)
    {
        for (int x = 0; x < BWAPI::Broodwar->mapWidth() * 2; x++)
        {
            chokeSideData[x + y * (BWAPI::Broodwar->mapWidth() * 2)] = tileSide[x + y * (BWAPI::Broodwar->mapWidth() * 2)];
        }
    }

    CherryVis::addHeatmap((std::ostringstream() << "ChokeSides@" << wpCenter).str(),
                          chokeSideData,
                          BWAPI::Broodwar->mapWidth() * 2,
                          BWAPI::Broodwar->mapHeight() * 2);

    std::vector<long> chokeTileData(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
    for (const auto &chokeTile : chokeTiles)
    {
        chokeTileData[chokeTile.x + chokeTile.y * BWAPI::Broodwar->mapWidth()] = 100;
    }
    CherryVis::addHeatmap((std::ostringstream() << "ChokeTiles@" << wpCenter).str(),
                          chokeTileData,
                          BWAPI::Broodwar->mapWidth(),
                          BWAPI::Broodwar->mapHeight());
#endif
}

void Choke::computeNarrowRampHighGroundPosition()
{
    BWAPI::Position chokeCenter = center;

    int firstAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().first->Top()));
    int secondAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().second->Top()));
    int lowGroundElevation = std::min(firstAreaElevation, secondAreaElevation);
    int highGroundElevation = std::max(firstAreaElevation, secondAreaElevation);

    // Generate a set of low-ground tiles near the choke, ignoring "holes"
    std::set<BWAPI::TilePosition> lowGroundTiles;
    for (int x = -5; x <= 5; x++)
    {
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
    }

    const auto inChokeCenter = [](BWAPI::Position pos)
    {
        BWAPI::Position end1 = Geo::FindClosestUnwalkablePosition(pos, pos, 64);
        if (end1 == BWAPI::Positions::Invalid) return false;

        BWAPI::Position end2 = Geo::FindClosestUnwalkablePosition(BWAPI::Position(pos.x + pos.x - end1.x, pos.y + pos.y - end1.y), pos, 32);
        if (end2 == BWAPI::Positions::Invalid) return false;

        if (end1.getDistance(end2) < end1.getDistance(pos)) return false;

        return std::abs(end1.getDistance(pos) - end2.getDistance(pos)) <= 2.0;
    };

    // Find the nearest position to the choke center that is on the high-ground border
    // This means that it is on the high ground and adjacent to one of the low-ground tiles found above
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;
    int bestDist = INT_MAX;
    for (int x = -64; x <= 64; x++)
    {
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
    }

    computeScoutBlockingPositions(bestPos, BWAPI::UnitTypes::Protoss_Probe, probeBlockScoutPositions);
    computeScoutBlockingPositions(bestPos, BWAPI::UnitTypes::Protoss_Zealot, zealotBlockScoutPositions);
}

void Choke::computeScoutBlockingPositions(BWAPI::Position center, BWAPI::UnitType type, std::set<BWAPI::Position> &result)
{
    if (!center.isValid()) return;
    if (result.size() == 1) return;

    int targetElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(center));

    // Search for a position in the immediate vicinity of the center that blocks the choke with one unit
    // Prefer at same elevation but return a lower elevation if that's all we have
    BWAPI::Position bestLowGround = BWAPI::Positions::Invalid;
    for (int x = 0; x <= 5; x++)
    {
        for (int y = 0; y <= 5; y++)
        {
            for (int xs = -1; xs <= (x == 0 ? 0 : 1); xs += 2)
            {
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
            }
        }
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
    if (end1 == BWAPI::Positions::Invalid) return;

    BWAPI::Position end2 = Geo::FindClosestUnwalkablePosition(BWAPI::Position(center.x + center.x - end1.x, center.y + center.y - end1.y),
                                                              center,
                                                              32);
    if (end2 == BWAPI::Positions::Invalid) return;

    if (end1.getDistance(end2) < (end1.getDistance(center) * 1.2)) return;

    // Now find the positions between the ends
    std::vector<BWAPI::Position> toBlock;
    Geo::FindWalkablePositionsBetween(end1, end2, toBlock);
    if (toBlock.empty()) return;

    // Now we find two positions that block all of the positions in the vector

    // Step 1: remove positions on both ends that the enemy worker cannot stand on because of unwalkable terrain
    for (int i = 0; i < 2; i++)
    {
        for (auto it = toBlock.begin(); it != toBlock.end(); it = toBlock.erase(it))
        {
            if (Geo::Walkable(BWAPI::UnitTypes::Protoss_Probe, *it))
                break;
        }

        std::reverse(toBlock.begin(), toBlock.end());
    }

    // Step 2: gather potential positions to place the unit that block the enemy unit locations at both ends
    std::vector<std::vector<BWAPI::Position>> candidatePositions = {std::vector<BWAPI::Position>(), std::vector<BWAPI::Position>()};
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
    {
        for (auto second : candidatePositions[1])
        {
            int score;

            // Skip if the two units overlap
            // We use probes here because we sometimes mix probes and zealots and probes are larger
            if (Geo::Overlaps(BWAPI::UnitTypes::Protoss_Probe, first, BWAPI::UnitTypes::Protoss_Probe, second))
                continue;

            // Skip if any positions are not blocked by one of the units
            for (auto pos : toBlock)
            {
                if (!Geo::Overlaps(type, first, BWAPI::UnitTypes::Protoss_Probe, pos) &&
                    !Geo::Overlaps(type, second, BWAPI::UnitTypes::Protoss_Probe, pos))
                {
                    goto nextCombination;
                }
            }

            score = std::max({
                                     Geo::EdgeToPointDistance(type, first, end1),
                                     Geo::EdgeToPointDistance(type, second, end2),
                                     Geo::EdgeToEdgeDistance(type, first, type, second)});

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
