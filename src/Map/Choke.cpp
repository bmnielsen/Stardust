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

    int squaredDistance(BWAPI::WalkPosition first, BWAPI::WalkPosition second)
    {
        int x = first.x - second.x;
        int y = first.y - second.y;
        return x * x + y * y;
    }

    int squaredDistance(BWAPI::Position first, BWAPI::Position second)
    {
        int x = first.x - second.x;
        int y = first.y - second.y;
        return x * x + y * y;
    }

    std::vector<BWAPI::Position> walkPositionBorder(BWAPI::WalkPosition pt)
    {
        BWAPI::Position initial(pt);
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

    /* Does a unit of the given type at the given position block the choke? */
    bool blocksChoke(BWAPI::Position pt, BWAPI::UnitType type)
    {
        BWAPI::Position end1 = Geom::FindClosestUnwalkablePosition(pt, 64);
        if (end1 == BWAPI::Positions::Invalid) return false;

        BWAPI::Position end2 = Geom::FindClosestUnwalkablePosition(BWAPI::Position(pt.x + pt.x - end1.x, pt.y + pt.y - end1.y), pt, 32);
        if (end2 == BWAPI::Positions::Invalid) return false;

        if (end1.getDistance(end2) < (end1.getDistance(pt) * 1.2)) return false;

        auto passable = [](BWAPI::UnitType ourUnit, BWAPI::UnitType enemyUnit, BWAPI::Position pt, BWAPI::Position wall)
        {
            BWAPI::Position cornerTL = pt + BWAPI::Position(-ourUnit.dimensionLeft() - 1, -ourUnit.dimensionUp() - 1);
            BWAPI::Position bottomRight = pt + BWAPI::Position(ourUnit.dimensionRight() + 1, ourUnit.dimensionDown() + 1);

            std::vector<BWAPI::Position> positionsToCheck;

            if (wall.x < cornerTL.x)
            {
                if (wall.y < cornerTL.y)
                {
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), cornerTL.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), pt.y);
                    positionsToCheck.emplace_back(pt.x, cornerTL.y - enemyUnit.dimensionDown());
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp());
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), pt.y);
                    positionsToCheck.emplace_back(pt.x, bottomRight.y + enemyUnit.dimensionUp());
                }
                else
                {
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), cornerTL.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), pt.y);
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp());
                }
            }
            else if (wall.x > bottomRight.x)
            {
                if (wall.y < cornerTL.y)
                {
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), cornerTL.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), pt.y);
                    positionsToCheck.emplace_back(pt.x, cornerTL.y - enemyUnit.dimensionDown());
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp());
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), pt.y);
                    positionsToCheck.emplace_back(pt.x, bottomRight.y + enemyUnit.dimensionUp());
                }
                else
                {
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), cornerTL.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), pt.y);
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp());
                }
            }
            else
            {
                if (wall.y < cornerTL.y)
                {
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), cornerTL.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(pt.x, cornerTL.y - enemyUnit.dimensionDown());
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), cornerTL.y - enemyUnit.dimensionDown());
                }
                else if (wall.y > bottomRight.y)
                {
                    positionsToCheck.emplace_back(pt.x, bottomRight.y + enemyUnit.dimensionUp());
                    positionsToCheck.emplace_back(bottomRight.x + enemyUnit.dimensionLeft(), bottomRight.y + enemyUnit.dimensionUp());
                    positionsToCheck.emplace_back(cornerTL.x - enemyUnit.dimensionRight(), bottomRight.y + enemyUnit.dimensionUp());
                }
                else
                {
                    return false;
                }
            }

            return std::all_of(positionsToCheck.begin(), positionsToCheck.end(), [&enemyUnit](const auto &positionToCheck)
            {
                return Geom::Walkable(enemyUnit, positionToCheck);
            });
        };

        return
                !passable(type, BWAPI::UnitTypes::Protoss_Probe, pt, end1) &&
                !passable(type, BWAPI::UnitTypes::Protoss_Probe, pt, end2);
    }
}

Choke::Choke(const BWEM::ChokePoint *_choke)
        : choke(_choke)
        , width(0)
        , midpoint(BWAPI::Position(choke->Center()) + BWAPI::Position(4, 4))
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
    /* Estimate the choke width */
    /* Because the ends are themselves walkable tiles, we need to add a bit of padding to estimate the actual walkable width of the choke */
    /* We do further refinement of narrow chokes later */
    width = (int) BWAPI::Position(choke->Pos(choke->end1)).getDistance(BWAPI::Position(choke->Pos(choke->end2))) + 15;

    /* Check if the choke is a ramp */
    /* TODO: This could technically fail if the "top" tile is at a different height than the end of the choke */
    int firstAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().first->Top()));
    int secondAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().second->Top()));
    if (firstAreaElevation != secondAreaElevation)
    {
        isRamp = true;
    }

    /* BWEM doesn't give us the information we need to properly fight at narrow chokes, so do some extra analysis */
    /* Outputs: */
    /* - Accurate choke width */
    /* - Actual middle of the choke (BWEM can put the center at weird places sometimes) */
    /* - Lengthwise ends of the choke (i.e. entrance and exit) */
    /* - Positions where we can block a worker scout from getting through the choke */
    /* - For ramps, position nearest the center where high ground begins */
    if (width < NARROW_CHOKE_THRESHOLD)
    {
        analyzeNarrowChoke();

        if (isNarrowChoke)
        {
            if (isRamp)
            {
                computeNarrowRampHighGroundPosition();
            }

            computeScoutBlockingPositions(midpoint, BWAPI::UnitTypes::Protoss_Probe, probeBlockScoutPositions);
            computeScoutBlockingPositions(midpoint, BWAPI::UnitTypes::Protoss_Zealot, zealotBlockScoutPositions);
        }
    }

    /* If the center is not walkable, find the nearest position that is */
    auto wpCenter = BWAPI::WalkPosition(midpoint);
    if (!BWAPI::Broodwar->isWalkable(wpCenter))
    {
        auto spiral = Geom::Spiral();
        while (wpCenter.isValid() && !BWAPI::Broodwar->isWalkable(wpCenter))
        {
            spiral.Next();
            wpCenter = BWAPI::WalkPosition(midpoint) + BWAPI::WalkPosition(spiral.x, spiral.y);
        }

        midpoint = BWAPI::Position(wpCenter) + BWAPI::Position(4, 4);
    }
}

void Choke::setAsMainChoke()
{
    /* Our main choke is always considered to be narrow regardless of its actual width */
    if (!isNarrowChoke)
    {
        analyzeNarrowChoke();

        if (!isNarrowChoke)
        {
            AppLog::Get() << "WARNING: Unable to analyze main choke as a narrow choke";
        }
    }
}

void Choke::analyzeNarrowChoke()
{
    /* Start by getting the sides closest to BWEM's center */
    BWAPI::Position side1 = Geom::FindClosestUnwalkablePosition(midpoint, 64 + width / 2);
    if (side1 == BWAPI::Positions::Invalid) return;

    BWAPI::Position side2 = Geom::FindClosestUnwalkablePosition(midpoint, 64 + width / 2, side1);
    if (side2 == BWAPI::Positions::Invalid) return;

#if DEBUG_NARROW_CHOKE_ANALYSIS
    AppLog::Get() << "Analyzing choke with initial center @ " << BWAPI::WalkPosition(midpoint);
#endif

    /* Compute the approximate angle of the choke */
    double angle = normalizeAngle(atan2(side1.y - side2.y, side1.x - side2.x) + (pi / 2.0));

    /* Now find the walls of the choke by tracing along each side, collecting walk positions that follow the approximate choke angle */
    auto traceChokeSide = [&angle, this](std::set<BWAPI::WalkPosition> &positions, BWAPI::WalkPosition initialPosition)
    {
#if DEBUG_NARROW_CHOKE_ANALYSIS
        AppLog::Debug() << "Tracing choke side from " << initialPosition;
#endif

        positions.insert(initialPosition);

        std::set<BWAPI::WalkPosition> visited;
        std::deque<std::pair<BWAPI::WalkPosition, int>> queue;

        int mapWalkWidth = BWAPI::Broodwar->mapWidth() * 8;
        int mapWalkHeight = BWAPI::Broodwar->mapHeight() * 8;
        auto isValidAndWalkable = [&mapWalkWidth, &mapWalkHeight](int walkX, int walkY)
        {
            if (walkX < 0) return false;
            if (walkY < 0) return false;
            if (walkX >= mapWalkWidth) return false;
            if (walkY >= mapWalkHeight) return false;
            return BWAPI::Broodwar->isWalkable(walkX, walkY);
        };

        auto visit = [&](std::pair<BWAPI::WalkPosition, int> current)
        {
            auto &pt = current.first;
            if (visited.contains(pt)) return;

            /* Skip nodes that haven't had a position added in 8 or more steps */
            /* But don't add to the visited set, as we might get here again from a valid path */
            if (current.second > 8) return;

            visited.insert(pt);

#if DEBUG_NARROW_CHOKE_ANALYSIS
            AppLog::Debug() << "Visiting " << pt;
#endif

            /* Find positions next to this one that has an unwalkable position somewhere around it */
            auto visitNeighbour = [&](int dx, int dy)
            {
                if (!isValidAndWalkable(pt.x + dx, pt.y + dy)) return;

                auto here = BWAPI::WalkPosition(pt.x + dx, pt.y + dy);
                if (visited.contains(here)) return;

                bool bordersUnwalkable = false;
                bool addedPosition = false;
                auto handleBorder = [&](int dx, int dy)
                {
                    /* First check if this is an unwalkable position */
                    if (isValidAndWalkable(here.x + dx, here.y + dy)) return;
                    bordersUnwalkable = true;

                    /* Now check if the position should be added to the set */
                    auto pt = BWAPI::WalkPosition(here.x + dx, here.y + dy);

                    /* Find the closest position to this one that is already part of the choke side */
                    int closestDist = INT_MAX;
                    BWAPI::WalkPosition closest = BWAPI::WalkPositions::Invalid;
                    for (auto other : positions)
                    {
                        auto span = other.getApproxDistance(pt);
                        if (span > 0 && span < closestDist)
                        {
                            closestDist = span;
                            closest = other;
                        }
                    }

                    /* Compare the angle of this position to the closest existing position to the expected angle of the choke */
                    if (closest != BWAPI::WalkPositions::Invalid)
                    {
                        double angleHere = normalizeAngle(atan2(pt.y - closest.y, pt.x - closest.x));
                        double diff = angleDiff(angleHere, angle);
                        if (diff < chokeSideAngleThreshold)
                        {
                            positions.insert(pt);
                            addedPosition = true;
#if DEBUG_NARROW_CHOKE_ANALYSIS
                            AppLog::Debug() << "Added part of choke side, angle diff vs. " << closest << " is " << (diff * 180.0 / pi);
#endif
                        }
                        else
                        {
#if DEBUG_NARROW_CHOKE_ANALYSIS
                            AppLog::Debug() << "Not part of choke side, angle diff vs. " << closest << " is " << (diff * 180.0 / pi);
#endif
                        }
                    }
                };

                handleBorder(1, - 1);
                handleBorder(1, 0);
                handleBorder(1, 1);
                handleBorder(- 1, - 1);
                handleBorder(- 1, 0);
                handleBorder(- 1, 1);
                handleBorder(0, - 1);
                handleBorder(0, 1);

                if (bordersUnwalkable)
                {
                    queue.emplace_back(here, addedPosition ? 0 : (current.second + 1));
                }
            };
            visitNeighbour(1, 0);
            visitNeighbour(-1, 0);
            visitNeighbour(0, 1);
            visitNeighbour(0, -1);
        };

        if (isValidAndWalkable(initialPosition.x, initialPosition.y - 1))
        {
            visit(std::make_pair(BWAPI::WalkPosition(initialPosition.x, initialPosition.y - 1), 0));
        }
        else if (isValidAndWalkable(initialPosition.x, initialPosition.y + 1))
        {
            visit(std::make_pair(BWAPI::WalkPosition(initialPosition.x, initialPosition.y + 1), 0));
        }
        else if (isValidAndWalkable(initialPosition.x - 1, initialPosition.y))
        {
            visit(std::make_pair(BWAPI::WalkPosition(initialPosition.x - 1, initialPosition.y), 0));
        }
        else if (isValidAndWalkable(initialPosition.x + 1, initialPosition.y))
        {
            visit(std::make_pair(BWAPI::WalkPosition(initialPosition.x + 1, initialPosition.y), 0));
        }
        else
        {
            AppLog::Get() << "ERROR: No walkable position bordering side position for choke @ " << BWAPI::WalkPosition(midpoint);
            return;
        }

        auto lastSize = positions.size();
        while (!queue.empty())
        {
            auto here = queue.front();
            queue.pop_front();

            visit(here);

            if (positions.size() > lastSize)
            {
                lastSize = positions.size();
            }
        }
    };
    std::set<BWAPI::WalkPosition> side1Positions, side2Positions;
    traceChokeSide(side1Positions, BWAPI::WalkPosition(side1.x >> 3, side1.y >> 3));
    traceChokeSide(side2Positions, BWAPI::WalkPosition(side2.x >> 3, side2.y >> 3));

#if DUMP_NARROW_CHOKE_HEATMAPS
    auto positionsHeatmap = [&side1Positions, &side2Positions, this](std::string label)
    {
        std::vector<long> data(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 16);
        for (auto pt : side1Positions)
        {
            data[pt.x + pt.y * BWAPI::Broodwar->mapWidth() * 4] = 100;
        }
        for (auto pt : side2Positions)
        {
            data[pt.x + pt.y * BWAPI::Broodwar->mapWidth() * 4] = 100;
        }
        Telemetry::addHeatmap((std::ostringstream() << "Choke" << label << "@" << BWAPI::WalkPosition(midpoint)).str(),
                              data,
                              BWAPI::Broodwar->mapWidth() * 4,
                              BWAPI::Broodwar->mapHeight() * 4);

        std::fill(data.begin(), data.end(), 0);
        for (auto pt : side1Positions)
        {
            data[pt.x + pt.y * BWAPI::Broodwar->mapWidth() * 4] = 100;
        }
        Telemetry::addHeatmap((std::ostringstream() << "Choke" << label << "Side1@" << BWAPI::WalkPosition(midpoint)).str(),
                              data,
                              BWAPI::Broodwar->mapWidth() * 4,
                              BWAPI::Broodwar->mapHeight() * 4);

        std::fill(data.begin(), data.end(), 0);
        for (auto pt : side2Positions)
        {
            data[pt.x + pt.y * BWAPI::Broodwar->mapWidth() * 4] = 100;
        }
        Telemetry::addHeatmap((std::ostringstream() << "Choke" << label << "Side2@" << BWAPI::WalkPosition(midpoint)).str(),
                              data,
                              BWAPI::Broodwar->mapWidth() * 4,
                              BWAPI::Broodwar->mapHeight() * 4);
    };
    positionsHeatmap("Trace");
#endif

    /* Get the choke squared width at each point on each side */
    auto addWidthIfSmaller = [](std::map<BWAPI::WalkPosition, std::pair<int, BWAPI::WalkPosition>> &widths,
                                BWAPI::WalkPosition pt,
                                BWAPI::WalkPosition other,
                                int widthHere)
    {
        auto iterRef = widths.find(pt);
        if (iterRef == widths.end() || iterRef->second.first > widthHere) widths[pt] = std::make_pair(widthHere, other);
    };
    std::map<BWAPI::WalkPosition, std::pair<int, BWAPI::WalkPosition>> side1SquaredWidth, side2SquaredWidth;
    int minSquaredWidth = INT_MAX;
    for (auto side1Pos : side1Positions)
    {
        for (auto side2Pos : side2Positions)
        {
            int squaredWidthHere = squaredDistance(side1Pos, side2Pos);
            addWidthIfSmaller(side1SquaredWidth, side1Pos, side2Pos, squaredWidthHere);
            addWidthIfSmaller(side2SquaredWidth, side2Pos, side1Pos, squaredWidthHere);
            if (squaredWidthHere < minSquaredWidth) minSquaredWidth = squaredWidthHere;
        }
    }

    /* Trim positions where the width is much longer than the minimum width */
    auto trimWidePositions = [&minSquaredWidth](std::map<BWAPI::WalkPosition, std::pair<int, BWAPI::WalkPosition>> &squaredWidths,
                                                std::set<BWAPI::WalkPosition> &positions)
    {
        int widthCutoff = minSquaredWidth * 2;
        for (auto iterRef = positions.begin(); iterRef != positions.end();)
        {
            if (squaredWidths[*iterRef].first > widthCutoff)
            {
#if DEBUG_NARROW_CHOKE_ANALYSIS
                AppLog::Debug() << "Removed " << *iterRef << "; width: " << widths[*iterRef].first << "; cutoff: " << widthCutoff;
#endif
                iterRef = positions.erase(iterRef);
            }
            else
            {
                iterRef++;
            }
        }
    };
    trimWidePositions(side1SquaredWidth, side1Positions);
    trimWidePositions(side2SquaredWidth, side2Positions);

#if DUMP_NARROW_CHOKE_HEATMAPS
    positionsHeatmap("TrimWide");
#endif

    if (side1Positions.empty() || side2Positions.empty())
    {
        AppLog::Get() << "ERROR: Trimmed all positions from choke @ " << BWAPI::WalkPosition(midpoint);
        return;
    }

    /* Now find the corners by getting the pairs of positions on each side that are furthest from each other */
    auto findFurthestPositions = [](std::set<BWAPI::WalkPosition> &positions, BWAPI::WalkPosition (&corners)[2])
    {
        int maxDist = -1;
        for (auto pt : positions)
        {
            for (auto other : positions)
            {
                auto span = squaredDistance(pt, other);
                if (span > maxDist)
                {
                    maxDist = span;
                    corners[0] = pt;
                    corners[1] = other;
                }
            }
        }
    };
    BWAPI::WalkPosition wpSide1Ends[2], wpSide2Ends[2];
    findFurthestPositions(side1Positions, wpSide1Ends);
    findFurthestPositions(side2Positions, wpSide2Ends);

    /* Align the ends */
    /* Do this by matching the corners that give the most parallel angles */
    double currentEnd1Angle = normalizeAngle(atan2(wpSide1Ends[0].y - wpSide2Ends[0].y, wpSide1Ends[0].x - wpSide2Ends[0].x));
    double currentEnd2Angle = normalizeAngle(atan2(wpSide1Ends[1].y - wpSide2Ends[1].y, wpSide1Ends[1].x - wpSide2Ends[1].x));
    double oppositeEnd1Angle = normalizeAngle(atan2(wpSide1Ends[0].y - wpSide2Ends[1].y, wpSide1Ends[0].x - wpSide2Ends[1].x));
    double oppositeEnd2Angle = normalizeAngle(atan2(wpSide1Ends[1].y - wpSide2Ends[0].y, wpSide1Ends[1].x - wpSide2Ends[0].x));
    if (angleDiff(oppositeEnd1Angle, oppositeEnd2Angle) < angleDiff(currentEnd1Angle, currentEnd2Angle))
    {
        BWAPI::WalkPosition tmp = wpSide2Ends[0];
        wpSide2Ends[0] = wpSide2Ends[1];
        wpSide2Ends[1] = tmp;
    }

    /* Convert to positions by taking the closest points */
    auto findClosestPositions = [](BWAPI::WalkPosition wpFirst, BWAPI::WalkPosition wpSecond, BWAPI::Position *first, BWAPI::Position *second)
    {
        int minDist = INT_MAX;
        for (auto firstPos : walkPositionBorder(wpFirst))
        {
            for (auto secondPos : walkPositionBorder(wpSecond))
            {
                int span = squaredDistance(firstPos, secondPos);
                if (span < minDist)
                {
                    minDist = span;
                    *first = firstPos;
                    *second = secondPos;
                }
            }
        }
    };
    BWAPI::Position side1Ends[2], side2Ends[2];
    findClosestPositions(wpSide1Ends[0], wpSide2Ends[0], &side1Ends[0], &side2Ends[0]);
    findClosestPositions(wpSide1Ends[1], wpSide2Ends[1], &side1Ends[1], &side2Ends[1]);

    /* Now that we have the corners, compute the center at each end */
    end1Center = (side1Ends[0] + side2Ends[0]) / 2;
    end2Center = (side1Ends[1] + side2Ends[1]) / 2;

    /* Compute the "exits", a position one tile further than the ends */
    end1Exit = end1Center + Geom::ScaleVector(end1Center - end2Center, 32);
    end2Exit = end2Center + Geom::ScaleVector(end2Center - end1Center, 32);

    /* In order to allow analysis of chokes along the map edges, we allow the sides and corners to be off the map */
    /* If any of the computed results are invalid though, we bail out now */
    if (!end1Center.isValid() || !end2Center.isValid() || !end1Exit.isValid() || !end2Exit.isValid())
    {
        AppLog::Get() << "WARNING: Choke positions out of bounds for choke @ " << BWAPI::WalkPosition(midpoint);
        return;
    }

    /* We can now officially mark this as an analyzed narrow choke */
    isNarrowChoke = true;

    /* The minimum width computed earlier is done with walk tiles, so adjust it a bit to approximate the actual width */
    width = (int) (sqrt(minSquaredWidth) * 8.0) - 8;

    /* Compute the length */
    length = (int) end1Center.getDistance(end2Center);

    /* Initialize the center as the centroid of the corners, then pull it to the center of the choke at that location */
    midpoint = (side1Ends[0] + side1Ends[1] + side2Ends[0] + side2Ends[1]) / 4;
    BWAPI::Position centerSide1 = Geom::FindClosestUnwalkablePosition(midpoint, 64);
    if (centerSide1 != BWAPI::Positions::Invalid)
    {
        BWAPI::Position centerSide2 = Geom::FindClosestUnwalkablePosition(BWAPI::Position(midpoint.x + midpoint.x - centerSide1.x,
                                                                                         midpoint.y + midpoint.y - centerSide1.y),
                                                                         midpoint,
                                                                         64,
                                                                         centerSide1);
        if (centerSide2 != BWAPI::Positions::Invalid) midpoint = (centerSide1 + centerSide2) / 2;
    }

    /* Now calculate the choke tiles */
    tileSide.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4);
    std::vector<bool> visited(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4);

    /* We do this at half-tile resolution, as that is what the collision grid in the combat sim uses */
    struct HalfTile
    {
        HalfTile(unsigned int x, unsigned int y) : x(x), y(y) {}

        explicit HalfTile(BWAPI::Position pt) : x(pt.x >> 4U), y(pt.y >> 4U) {}

        unsigned int x;
        unsigned int y;

        [[nodiscard]] bool isValid() const
        {
            return x < (BWAPI::Broodwar->mapWidth() * 2) && y < (BWAPI::Broodwar->mapHeight() * 2);
        }

        [[nodiscard]] bool isWalkable() const
        {
            return BWAPI::Broodwar->isWalkable((int)(x << 1U), (int)(y << 1U)) &&
                   BWAPI::Broodwar->isWalkable((int)(x << 1U) + 1, (int)(y << 1U)) &&
                   BWAPI::Broodwar->isWalkable((int)(x << 1U), (int)(y << 1U) + 1) &&
                   BWAPI::Broodwar->isWalkable((int)(x << 1U) + 1, (int)(y << 1U) + 1);
        }

        [[nodiscard]] unsigned int index() const
        {
            return x + y * BWAPI::Broodwar->mapWidth() * 2;
        }

        [[nodiscard]] BWAPI::TilePosition toTilePosition() const
        {
            return {(int)(x >> 1U), (int)(y >> 1U)};
        }
    };

    /* Initialize the queue by tracing each end of the choke */
    std::deque<std::pair<signed char, HalfTile>> queue;
    std::deque<std::pair<signed char, HalfTile>> unwalkableQueue;
    auto addHalfTilesBetween = [&](BWAPI::Position start, BWAPI::Position end, signed char side)
    {
        auto addPosition = [&](BWAPI::Position pt)
        {
            if (!pt.isValid()) return;

            auto tilePos = HalfTile(pt.x >> 4U, pt.y >> 4U);
            tileSide[tilePos.index()] = side;

            if (visited[tilePos.index()]) return;

            visited[tilePos.index()] = true;
            (tilePos.isWalkable() ? queue : unwalkableQueue).emplace_back(side, tilePos);
            chokeTiles.insert(tilePos.toTilePosition());
        };

        addPosition(start);
        BWAPI::Position diff = end - start;
        int span = start.getApproxDistance(end);
        for (int d = 8; d < span; d += 8)
        {
            auto v = Geom::ScaleVector(diff, d);
            if (v == BWAPI::Positions::Invalid) continue;
            addPosition(start + v);
        }
        addPosition(end);
    };
    addHalfTilesBetween(side1Ends[0], end1Exit, -2);
    addHalfTilesBetween(end1Exit, side2Ends[0], -2);
    addHalfTilesBetween(side1Ends[1], end2Exit, 1);
    addHalfTilesBetween(end2Exit, side2Ends[1], 1);

    /* Queue the center of the choke to be set to 0, unless the center is on one of the traced ends */
    auto centerTile = HalfTile(midpoint);
    if (!visited[centerTile.index()])
    {
        queue.emplace_front(0, centerTile);
    }

    /* Now flood-fill */
    auto visit = [&](std::pair<signed char, HalfTile> &tilePos, int x, int y)
    {
        auto next = HalfTile(tilePos.second.x + x, tilePos.second.y + y);

        if (!next.isValid() || visited[next.index()]) return;

        tileSide[next.index()] = tilePos.first;
        visited[next.index()] = true;
        if (tilePos.first == 0)
        {
            if (next.isWalkable())
            {
                queue.emplace_front(tilePos.first, next);
            }
        }
        else
        {
            ((tilePos.second.isWalkable() && next.isWalkable()) ? queue : unwalkableQueue).emplace_back(tilePos.first, next);
        }

        if (tileSide[tilePos.second.index()] == 0)
        {
            chokeTiles.insert(next.toTilePosition());
        }
    };
    while (!queue.empty() || !unwalkableQueue.empty())
    {
        auto tilePos = (queue.empty() ? unwalkableQueue : queue).front();
        (queue.empty() ? unwalkableQueue : queue).pop_front();

        visit(tilePos, 1, 0);
        visit(tilePos, -1, 0);
        visit(tilePos, 0, 1);
        visit(tilePos, 0, -1);
    }

    /* Finally re-trace the ends to set them to 0 */
    addHalfTilesBetween(side1Ends[0], side2Ends[0], 0);
    addHalfTilesBetween(side1Ends[1], side2Ends[1], 0);

#if DUMP_NARROW_CHOKE_HEATMAPS
    std::vector<long> chokeData(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 16);

    /* Sides */
    for (auto pt : side1Positions)
    {
        chokeData[pt.x + pt.y * BWAPI::Broodwar->mapWidth() * 4] = 1;
    }
    for (auto pt : side2Positions)
    {
        chokeData[pt.x + pt.y * BWAPI::Broodwar->mapWidth() * 4] = 1;
    }

    /* Ends */
    chokeData[wpSide1Ends[0].x + wpSide1Ends[0].y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpSide1Ends[1].x + wpSide1Ends[1].y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpSide2Ends[0].x + wpSide2Ends[0].y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpSide2Ends[1].x + wpSide2Ends[1].y * BWAPI::Broodwar->mapWidth() * 4] = 100;

    /* Centers */
    BWAPI::WalkPosition wpCenter(midpoint);
    BWAPI::WalkPosition wpEnd1Center(end1Center);
    BWAPI::WalkPosition wpEnd2Center(end2Center);
    BWAPI::WalkPosition wpEnd1Exit(end1Exit);
    BWAPI::WalkPosition wpEnd2Exit(end2Exit);
    chokeData[wpCenter.x + wpCenter.y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpEnd1Exit.x + wpEnd1Exit.y * BWAPI::Broodwar->mapWidth() * 4] = 75;
    chokeData[wpEnd2Exit.x + wpEnd2Exit.y * BWAPI::Broodwar->mapWidth() * 4] = 75;
    chokeData[wpEnd1Center.x + wpEnd1Center.y * BWAPI::Broodwar->mapWidth() * 4] = 100;
    chokeData[wpEnd2Center.x + wpEnd2Center.y * BWAPI::Broodwar->mapWidth() * 4] = 100;

    Telemetry::addHeatmap((std::ostringstream() << "Choke@" << wpCenter).str(),
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

    Telemetry::addHeatmap((std::ostringstream() << "ChokeSides@" << wpCenter).str(),
                          chokeSideData,
                          BWAPI::Broodwar->mapWidth() * 2,
                          BWAPI::Broodwar->mapHeight() * 2);

    std::vector<long> chokeTileData(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
    for (const auto &chokeTile : chokeTiles)
    {
        chokeTileData[chokeTile.x + chokeTile.y * BWAPI::Broodwar->mapWidth()] = 100;
    }
    Telemetry::addHeatmap((std::ostringstream() << "ChokeTiles@" << wpCenter).str(),
                          chokeTileData,
                          BWAPI::Broodwar->mapWidth(),
                          BWAPI::Broodwar->mapHeight());
#endif
}

void Choke::computeNarrowRampHighGroundPosition()
{
    BWAPI::Position chokeCenter = midpoint;

    int firstAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().first->Top()));
    int secondAreaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(choke->GetAreas().second->Top()));
    int lowGroundElevation = std::min(firstAreaElevation, secondAreaElevation);
    int highGroundElevation = std::max(firstAreaElevation, secondAreaElevation);

    /* Generate a set of low-ground tiles near the choke, ignoring "holes" */
    std::set<BWAPI::TilePosition> lowGroundTiles;
    for (int x = -5; x <= 5; x++)
    {
        for (int y = -5; y <= 5; y++)
        {
            BWAPI::TilePosition tilePos = BWAPI::TilePosition(choke->Center()) + BWAPI::TilePosition(x, y);
            if (!tilePos.isValid()) continue;
            if (BWAPI::Broodwar->getGroundHeight(tilePos) != lowGroundElevation) continue;
            if (BWAPI::Broodwar->getGroundHeight(tilePos + BWAPI::TilePosition(1, 0)) == lowGroundElevation ||
                BWAPI::Broodwar->getGroundHeight(tilePos + BWAPI::TilePosition(0, 1)) == lowGroundElevation ||
                BWAPI::Broodwar->getGroundHeight(tilePos + BWAPI::TilePosition(-1, 0)) == lowGroundElevation ||
                BWAPI::Broodwar->getGroundHeight(tilePos + BWAPI::TilePosition(0, -1)) == lowGroundElevation)
            {
                lowGroundTiles.insert(tilePos);
            }
        }
    }

    const auto inChokeCenter = [](BWAPI::Position pt)
    {
        BWAPI::Position end1 = Geom::FindClosestUnwalkablePosition(pt, 64);
        if (end1 == BWAPI::Positions::Invalid) return false;

        BWAPI::Position end2 = Geom::FindClosestUnwalkablePosition(BWAPI::Position(pt.x + pt.x - end1.x, pt.y + pt.y - end1.y), pt, 32);
        if (end2 == BWAPI::Positions::Invalid) return false;

        if (end1.getDistance(end2) < end1.getDistance(pt)) return false;

        return std::abs(end1.getDistance(pt) - end2.getDistance(pt)) <= 2.0;
    };

    /* Find the nearest position to the choke center that is on the high-ground border */
    /* This means that it is on the high ground and adjacent to one of the low-ground tiles found above */
    BWAPI::Position bestPos = BWAPI::Positions::Invalid;
    int bestDist = INT_MAX;
    for (int x = -64; x <= 64; x++)
    {
        for (int y = -64; y <= 64; y++)
        {
            BWAPI::Position pt = chokeCenter + BWAPI::Position(x, y);
            if (!pt.isValid()) continue;
            if (pt.x % 32 > 0 && pt.x % 32 < 31 && pt.y % 32 > 0 && pt.y % 32 < 31) continue;
            if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pt))) continue;
            if (BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(pt)) != highGroundElevation) continue;
            if (!lowGroundTiles.contains(BWAPI::TilePosition(pt + BWAPI::Position(1, 0))) &&
                !lowGroundTiles.contains(BWAPI::TilePosition(pt + BWAPI::Position(-1, 0))) &&
                !lowGroundTiles.contains(BWAPI::TilePosition(pt + BWAPI::Position(0, 1))) &&
                !lowGroundTiles.contains(BWAPI::TilePosition(pt + BWAPI::Position(0, -1))))
                continue;
            if (!inChokeCenter(pt)) continue;

            int span = pt.getApproxDistance(chokeCenter);
            if (span < bestDist)
            {
                highElevationTile = BWAPI::TilePosition(pt);
                bestDist = span;
                bestPos = pt;
            }
        }
    }

    computeScoutBlockingPositions(bestPos, BWAPI::UnitTypes::Protoss_Probe, probeBlockScoutPositions);
    computeScoutBlockingPositions(bestPos, BWAPI::UnitTypes::Protoss_Zealot, zealotBlockScoutPositions);
}

void Choke::computeScoutBlockingPositions(BWAPI::Position midpoint, BWAPI::UnitType type, std::set<BWAPI::Position> &outcome)
{
    if (!midpoint.isValid()) return;
    if (outcome.size() == 1) return;

    int targetElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(midpoint));

    /* Search for a position in the immediate vicinity of the center that blocks the choke with one unit */
    /* Prefer at same elevation but return a lower elevation if that's all we have */
    BWAPI::Position bestLowGround = BWAPI::Positions::Invalid;
    for (int x = 0; x <= 5; x++)
    {
        for (int y = 0; y <= 5; y++)
        {
            for (int xs = -1; xs <= (x == 0 ? 0 : 1); xs += 2)
            {
                for (int ys = -1; ys <= (y == 0 ? 0 : 1); ys += 2)
                {
                    BWAPI::Position current = midpoint + BWAPI::Position(x * xs, y * ys);
                    if (!blocksChoke(current, type)) continue;

                    /* If this position is on the high-ground, return it */
                    if (BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(current)) >= targetElevation)
                    {
                        if (!outcome.empty()) outcome.clear();
                        outcome.insert(current);
                        return;
                    }

                    /* Otherwise set it as the best low-ground option if applicable */
                    if (!bestLowGround.isValid())
                        bestLowGround = current;
                }
            }
        }
    }

    if (bestLowGround.isValid())
    {
        if (!outcome.empty()) outcome.clear();
        outcome.insert(bestLowGround);
        return;
    }

    if (!outcome.empty()) return;

    /* Try with two units instead */

    /* First grab the ends of the choke at the given center point */
    BWAPI::Position end1 = Geom::FindClosestUnwalkablePosition(midpoint, 64);
    if (end1 == BWAPI::Positions::Invalid) return;

    BWAPI::Position end2 = Geom::FindClosestUnwalkablePosition(BWAPI::Position(midpoint.x + midpoint.x - end1.x, midpoint.y + midpoint.y - end1.y),
                                                              midpoint,
                                                              32);
    if (end2 == BWAPI::Positions::Invalid) return;

    if (end1.getDistance(end2) < (end1.getDistance(midpoint) * 1.2)) return;

    /* Now find the positions between the ends */
    std::vector<BWAPI::Position> toBlock;
    Geom::FindWalkablePositionsBetween(end1, end2, toBlock);
    if (toBlock.empty()) return;

    /* Now we find two positions that block all of the positions in the vector */

    /* Step 1: remove positions on both ends that the enemy worker cannot stand on because of unwalkable terrain */
    for (int i = 0; i < 2; i++)
    {
        for (auto iterRef = toBlock.begin(); iterRef != toBlock.end(); iterRef = toBlock.erase(iterRef))
        {
            if (Geom::Walkable(BWAPI::UnitTypes::Protoss_Probe, *iterRef))
                break;
        }

        std::reverse(toBlock.begin(), toBlock.end());
    }

    /* Step 2: gather potential positions to place the unit that block the enemy unit locations at both ends */
    std::vector<std::vector<BWAPI::Position>> candidatePositions = {std::vector<BWAPI::Position>(), std::vector<BWAPI::Position>()};
    for (int i = 0; i < 2; i++)
    {
        BWAPI::Position enemyPosition = *toBlock.begin();
        for (auto pt : toBlock)
        {
            /* Is this a valid position for a probe? */
            /* We use a probe here because we sometimes mix probes and zealots and probes are larger */
            if (!pt.isValid()) continue;
            if (!Geom::Walkable(BWAPI::UnitTypes::Protoss_Probe, pt)) continue;

            /* Does it block the enemy position? */
            if (!Geom::Overlaps(BWAPI::UnitTypes::Protoss_Probe, enemyPosition, type, pt))
                break;

            candidatePositions[i].push_back(pt);
        }

        std::reverse(toBlock.begin(), toBlock.end());
    }

    /* Step 3: try to find a combination that blocks all positions */
    /* Prefer a combination that puts both units on the high ground */
    /* Prefer a combination that spaces out the units relatively evenly */
    std::pair<BWAPI::Position, BWAPI::Position> bestPair = std::make_pair(BWAPI::Positions::Invalid, BWAPI::Positions::Invalid);
    std::pair<BWAPI::Position, BWAPI::Position> bestLowGroundPair = std::make_pair(BWAPI::Positions::Invalid, BWAPI::Positions::Invalid);
    int bestScore = INT_MAX;
    int bestLowGroundScore = INT_MAX;
    for (auto first : candidatePositions[0])
    {
        for (auto second : candidatePositions[1])
        {
            int score;

            /* Skip if the two units overlap */
            /* We use probes here because we sometimes mix probes and zealots and probes are larger */
            if (Geom::Overlaps(BWAPI::UnitTypes::Protoss_Probe, first, BWAPI::UnitTypes::Protoss_Probe, second))
                continue;

            /* Skip if any positions are not blocked by one of the units */
            for (auto pt : toBlock)
            {
                if (!Geom::Overlaps(type, first, BWAPI::UnitTypes::Protoss_Probe, pt) &&
                    !Geom::Overlaps(type, second, BWAPI::UnitTypes::Protoss_Probe, pt))
                {
                    goto nextCombination;
                }
            }

            score = std::max({
                                     Geom::EdgeToPointDistance(type, first, end1),
                                     Geom::EdgeToPointDistance(type, second, end2),
                                     Geom::EdgeToEdgeDistance(type, first, type, second)});

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
        outcome.insert(bestPair.first);
        outcome.insert(bestPair.second);
    }
    else if (bestLowGroundPair.first.isValid())
    {
        outcome.insert(bestLowGroundPair.first);
        outcome.insert(bestLowGroundPair.second);
    }
}
