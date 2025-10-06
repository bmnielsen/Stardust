#include "Geo.h"

#include <set>
#include <array>

namespace
{
    const double pi = 3.14159265358979323846;

    struct radiusPosition
    {
        int radius;
        int x;
        int y;
    };
    std::vector<radiusPosition> radiusPositions;

    // Copied from openbw's bwgame.h, used for BW direction math
    const std::array<unsigned int, 64> tan_table = {
            7, 13, 19, 26, 32, 38, 45, 51, 58, 65, 71, 78, 85, 92,
            99, 107, 114, 122, 129, 137, 146, 154, 163, 172, 181,
            190, 200, 211, 221, 233, 244, 256, 269, 283, 297, 312,
            329, 346, 364, 384, 405, 428, 452, 479, 509, 542, 578,
            619, 664, 716, 775, 844, 926, 1023, 1141, 1287, 1476,
            1726, 2076, 2600, 3471, 5211, 10429, std::numeric_limits<unsigned int>::max()
    };
    
    struct xy
    {
        int x;
        int y;
    };
    const xy direction_table[256] = {
            {0,-256},{6,-256},{13,-256},{19,-255},{25,-255},{31,-254},{38,-253},{44,-252},
            {50,-251},{56,-250},{62,-248},{68,-247},{74,-245},{80,-243},{86,-241},{92,-239},
            {98,-237},{104,-234},{109,-231},{115,-229},{121,-226},{126,-223},{132,-220},{137,-216},
            {142,-213},{147,-209},{152,-206},{157,-202},{162,-198},{167,-194},{172,-190},{177,-185},
            {181,-181},{185,-177},{190,-172},{194,-167},{198,-162},{202,-157},{206,-152},{209,-147},
            {213,-142},{216,-137},{220,-132},{223,-126},{226,-121},{229,-115},{231,-109},{234,-104},
            {237,-98},{239,-92},{241,-86},{243,-80},{245,-74},{247,-68},{248,-62},{250,-56},
            {251,-50},{252,-44},{253,-38},{254,-31},{255,-25},{255,-19},{256,-13},{256,-6},
            {256,0},{256,6},{256,13},{255,19},{255,25},{254,31},{253,38},{252,44},
            {251,50},{250,56},{248,62},{247,68},{245,74},{243,80},{241,86},{239,92},
            {237,98},{234,104},{231,109},{229,115},{226,121},{223,126},{220,132},{216,137},
            {213,142},{209,147},{206,152},{202,157},{198,162},{194,167},{190,172},{185,177},
            {181,181},{177,185},{172,190},{167,194},{162,198},{157,202},{152,206},{147,209},
            {142,213},{137,216},{132,220},{126,223},{121,226},{115,229},{109,231},{104,234},
            {98,237},{92,239},{86,241},{80,243},{74,245},{68,247},{62,248},{56,250},
            {50,251},{44,252},{38,253},{31,254},{25,255},{19,255},{13,256},{6,256},
            {0,256},{-6,256},{-13,256},{-19,255},{-25,255},{-31,254},{-38,253},{-44,252},
            {-50,251},{-56,250},{-62,248},{-68,247},{-74,245},{-80,243},{-86,241},{-92,239},
            {-98,237},{-104,234},{-109,231},{-115,229},{-121,226},{-126,223},{-132,220},{-137,216},
            {-142,213},{-147,209},{-152,206},{-157,202},{-162,198},{-167,194},{-172,190},{-177,185},
            {-181,181},{-185,177},{-190,172},{-194,167},{-198,162},{-202,157},{-206,152},{-209,147},
            {-213,142},{-216,137},{-220,132},{-223,126},{-226,121},{-229,115},{-231,109},{-234,104},
            {-237,98},{-239,92},{-241,86},{-243,80},{-245,74},{-247,68},{-248,62},{-250,56},
            {-251,50},{-252,44},{-253,38},{-254,31},{-255,25},{-255,19},{-256,13},{-256,6},
            {-256,0},{-256,-6},{-256,-13},{-255,-19},{-255,-25},{-254,-31},{-253,-38},{-252,-44},
            {-251,-50},{-250,-56},{-248,-62},{-247,-68},{-245,-74},{-243,-80},{-241,-86},{-239,-92},
            {-237,-98},{-234,-104},{-231,-109},{-229,-115},{-226,-121},{-223,-126},{-220,-132},{-216,-137},
            {-213,-142},{-209,-147},{-206,-152},{-202,-157},{-198,-162},{-194,-167},{-190,-172},{-185,-177},
            {-181,-181},{-177,-185},{-172,-190},{-167,-194},{-162,-198},{-157,-202},{-152,-206},{-147,-209},
            {-142,-213},{-137,-216},{-132,-220},{-126,-223},{-121,-226},{-115,-229},{-109,-231},{-104,-234},
            {-98,-237},{-92,-239},{-86,-241},{-80,-243},{-74,-245},{-68,-247},{-62,-248},{-56,-250},
            {-50,-251},{-44,-252},{-38,-253},{-31,-254},{-25,-255},{-19,-255},{-13,-256},{-6,-256}
    };    
}

namespace Geo
{
    void initialize()
    {
        radiusPositions.clear();

        for (int x = -256; x <= 256; x++)
        {
            for (int y = -256; y <= 256; y++)
            {
                int dist = Geo::ApproximateDistance(0, x, 0, y);
                if (dist > 256) continue;
                radiusPositions.emplace_back(radiusPosition{dist, x, y});
            }
        }

        sort(radiusPositions.begin(), radiusPositions.end(),
             [](const radiusPosition &a, const radiusPosition &b) -> bool
             {
                 if (a.radius < b.radius) return true;
                 if (a.radius > b.radius) return false;
                 if (a.x < b.x) return true;
                 if (a.x > b.x) return false;
                 return (a.y < b.y);
             });
    }

    int ApproximateDistance(int x1, int x2, int y1, int y2)
    {
        unsigned int min = abs(x1 - x2);
        unsigned int max = abs(y1 - y2);
        if (max < min)
            std::swap(min, max);

        if (min <= (max >> 2U))
            return (int)max;

        unsigned int minCalc = (3 * min) >> 3U;
        return (int)((minCalc >> 5U) + minCalc + max - (max >> 4U) - (max >> 6U));
    }

    int EdgeToEdgeDistance(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter)
    {
        // Compute bounding boxes
        BWAPI::Position firstTopLeft = firstCenter + BWAPI::Position(-firstType.dimensionLeft(), -firstType.dimensionUp());
        BWAPI::Position firstBottomRight = firstCenter + BWAPI::Position(firstType.dimensionRight(), firstType.dimensionDown());
        BWAPI::Position secondTopLeft = secondCenter + BWAPI::Position(-secondType.dimensionLeft(), -secondType.dimensionUp());
        BWAPI::Position secondBottomRight = secondCenter + BWAPI::Position(secondType.dimensionRight(), secondType.dimensionDown());

        // Compute offsets
        int xDist = (std::max)({firstTopLeft.x - secondBottomRight.x - 1, secondTopLeft.x - firstBottomRight.x - 1, 0});
        int yDist = (std::max)({firstTopLeft.y - secondBottomRight.y - 1, secondTopLeft.y - firstBottomRight.y - 1, 0});

        // Compute distance
        return ApproximateDistance(xDist, 0, yDist, 0);
    }

    int EdgeToPointDistance(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point)
    {
        // Compute bounding box
        BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
        BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

        // Compute offsets
        int xDist = (std::max)({topLeft.x - point.x, point.x - bottomRight.x - 1, 0});
        int yDist = (std::max)({topLeft.y - point.y, point.y - bottomRight.y - 1, 0});

        // Compute distance
        return ApproximateDistance(xDist, 0, yDist, 0);
    }

    int EdgeToTileDistance(BWAPI::UnitType type, BWAPI::TilePosition topLeft, BWAPI::TilePosition tile)
    {
        BWAPI::TilePosition bottomRight = topLeft + type.tileSize();

        // Compute offsets
        int xDist = (std::max)({topLeft.x - tile.x - 1, tile.x - bottomRight.x - 1, 0});
        int yDist = (std::max)({topLeft.y - tile.y - 1, tile.y - bottomRight.y - 1, 0});

        // Compute distance
        return ApproximateDistance(xDist, 0, yDist, 0);
    }

    BWAPI::Position NearestPointOnEdge(BWAPI::Position point, BWAPI::UnitType type, BWAPI::Position center)
    {
        // Compute bounding box
        BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
        BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

        return {
                point.x < topLeft.x ? topLeft.x : (point.x > bottomRight.x ? bottomRight.x : point.x),
                point.y < topLeft.y ? topLeft.y : (point.y > bottomRight.y ? bottomRight.y : point.y)};
    }

    bool Overlaps(BWAPI::UnitType firstType, BWAPI::Position firstCenter, BWAPI::UnitType secondType, BWAPI::Position secondCenter)
    {
        // Compute bounding boxes
        BWAPI::Position firstTopLeft = firstCenter + BWAPI::Position(-firstType.dimensionLeft(), -firstType.dimensionUp());
        BWAPI::Position firstBottomRight = firstCenter + BWAPI::Position(firstType.dimensionRight(), firstType.dimensionDown());
        BWAPI::Position secondTopLeft = secondCenter + BWAPI::Position(-secondType.dimensionLeft(), -secondType.dimensionUp());
        BWAPI::Position secondBottomRight = secondCenter + BWAPI::Position(secondType.dimensionRight(), secondType.dimensionDown());

        return firstBottomRight.x >= secondTopLeft.x && secondBottomRight.x >= firstTopLeft.x &&
               firstBottomRight.y >= secondTopLeft.y && secondBottomRight.y >= firstTopLeft.y;
    }

    bool Overlaps(BWAPI::UnitType type, BWAPI::Position center, BWAPI::Position point)
    {
        // Compute bounding box
        BWAPI::Position topLeft = center + BWAPI::Position(-type.dimensionLeft(), -type.dimensionUp());
        BWAPI::Position bottomRight = center + BWAPI::Position(type.dimensionRight(), type.dimensionDown());

        return bottomRight.x >= point.x && point.x >= topLeft.x &&
               bottomRight.y >= point.y && point.y >= topLeft.y;
    }

    bool Overlaps(BWAPI::TilePosition firstTopLeft, int firstWidth, int firstHeight,
                  BWAPI::TilePosition secondtopLeft, int secondWidth, int secondHeight)
    {
        return firstTopLeft.x < secondtopLeft.x + secondWidth // first not right of second
               && firstTopLeft.y < secondtopLeft.y + secondHeight // first not below second
               && firstTopLeft.x + firstWidth > secondtopLeft.x // first not left of second
               && firstTopLeft.y + firstHeight > secondtopLeft.y; // first not above second
    }


    bool Walkable(BWAPI::UnitType type, BWAPI::Position center)
    {
        for (int x = center.x - type.dimensionLeft(); x <= center.x + type.dimensionRight(); x++)
        {
            for (int y = center.y - type.dimensionUp(); y <= center.y + type.dimensionDown(); y++)
            {
                if (!BWAPI::Broodwar->isWalkable(x / 8, y / 8))
                    return false;
            }
        }
        return true;
    }

    BWAPI::Position FindClosestUnwalkablePosition(BWAPI::Position start, int searchRadius, BWAPI::Position furtherFrom)
    {
        if (searchRadius > 256) return Geo::FindClosestUnwalkablePosition(start, start, searchRadius, furtherFrom);

        double furtherFromAngle = furtherFrom == BWAPI::Positions::Invalid ? 0.0 : atan2(start.y - furtherFrom.y, start.x - furtherFrom.x);
        for (auto &radiusPosition : radiusPositions)
        {
            // Stop the search when we exceed the radius
            if (radiusPosition.radius > searchRadius) return BWAPI::Positions::Invalid;

            auto here = BWAPI::Position(start.x + radiusPosition.x, start.y + radiusPosition.y);

            // Skip valid and walkable positions
            if (here.isValid() && BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(here))) continue;

            // If this is defined, we expect this position to be opposite the one we find here
            if (furtherFrom != BWAPI::Positions::Invalid)
            {
                double angle = atan2(here.y - start.y, here.x - start.x);
                if (std::abs(angle - furtherFromAngle) > (pi / 4.0)) continue;
            }

            return here;
        }

        return BWAPI::Positions::Invalid;
    }

    BWAPI::Position FindClosestUnwalkablePosition(BWAPI::Position start, BWAPI::Position closeTo, int searchRadius, BWAPI::Position furtherFrom)
    {
        BWAPI::Position bestPos = BWAPI::Positions::Invalid;
        int bestDist = INT_MAX;
        double furtherFromAngle = furtherFrom == BWAPI::Positions::Invalid ? 0.0 : atan2(start.y - furtherFrom.y, start.x - furtherFrom.x);
        for (int x = start.x - searchRadius; x <= start.x + searchRadius; x++)
        {
            for (int y = start.y - searchRadius; y <= start.y + searchRadius; y++)
            {
                BWAPI::Position current(x, y);
                if (current.isValid() && BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(current))) continue;

                int dist = current.getApproxDistance(closeTo);

                // If this is defined, we expect this position to be opposite the one we find here
                if (furtherFrom != BWAPI::Positions::Invalid)
                {
                    double angle = atan2(current.y - start.y, current.x - start.x);
                    if (std::abs(angle - furtherFromAngle) > (pi / 2.0)) continue;
                }

                if (dist < bestDist)
                {
                    bestPos = current;
                    bestDist = dist;
                }
            }
        }

        return bestPos;
    }

    void FindWalkablePositionsBetween(BWAPI::Position start, BWAPI::Position end, std::vector<BWAPI::Position> &result)
    {
        std::set<BWAPI::Position> added;
        int distTotal = start.getApproxDistance(end);
        int xdiff = end.x - start.x;
        int ydiff = end.y - start.y;
        for (int distStop = 0; distStop <= distTotal; distStop++)
        {
            BWAPI::Position pos(
                    start.x + (int) std::round(((double) distStop / distTotal) * xdiff),
                    start.y + (int) std::round(((double) distStop / distTotal) * ydiff));

            if (!pos.isValid()) continue;
            if (!BWAPI::Broodwar->isWalkable(BWAPI::WalkPosition(pos))) continue;
            if (added.contains(pos)) continue;

            result.push_back(pos);
            added.insert(pos);
        }
    }

    void FindTilesBetween(BWAPI::TilePosition start, BWAPI::TilePosition end, std::vector<BWAPI::TilePosition> &result)
    {
        std::set<BWAPI::TilePosition> added;
        int distTotal = start.getApproxDistance(end);
        int xdiff = end.x - start.x;
        int ydiff = end.y - start.y;
        for (int distStop = 0; distStop <= distTotal; distStop++)
        {
            BWAPI::TilePosition pos(
                    start.x + (int) std::round(((double) distStop / distTotal) * xdiff),
                    start.y + (int) std::round(((double) distStop / distTotal) * ydiff));

            if (!pos.isValid()) continue;
            if (added.contains(pos)) continue;

            result.push_back(pos);
            added.insert(pos);
        }
    }

    void FindWalkTilesBetween(BWAPI::WalkPosition start, BWAPI::WalkPosition end, std::vector<BWAPI::WalkPosition> &result)
    {
        std::set<BWAPI::WalkPosition> added;
        int distTotal = start.getApproxDistance(end);
        int xdiff = end.x - start.x;
        int ydiff = end.y - start.y;
        for (int distStop = 0; distStop <= distTotal; distStop++)
        {
            BWAPI::WalkPosition pos(
                    start.x + (int) std::round(((double) distStop / distTotal) * xdiff),
                    start.y + (int) std::round(((double) distStop / distTotal) * ydiff));

            if (!pos.isValid()) continue;
            if (added.contains(pos)) continue;

            result.push_back(pos);
            added.insert(pos);
        }
    }

    BWAPI::Position CenterOfUnit(BWAPI::TilePosition topLeft, BWAPI::UnitType type)
    {
        return CenterOfUnit(BWAPI::Position(topLeft), type);
    }

    BWAPI::Position CenterOfUnit(BWAPI::Position topLeft, BWAPI::UnitType type)
    {
        // For buildings we assume the top left is the top left of the tile placement, not the top left of the actual building dimensions
        if (type.isBuilding())
        {
            return {topLeft.x + type.tileWidth() * 16, topLeft.y + type.tileHeight() * 16};
        }

        return {topLeft.x + type.dimensionLeft() + 1, topLeft.y + type.dimensionUp() + 1};
    }

    BWAPI::Position ScaleVector(BWAPI::Position vector, int length)
    {
        auto magnitude = ApproximateDistance(vector.x, 0, vector.y, 0);
        if (magnitude == 0) return BWAPI::Positions::Invalid;

        float scale = (float) length / (float) magnitude;
        return {(int) ((float) vector.x * scale), (int) ((float) vector.y * scale)};
    }

    BWAPI::Position PerpendicularVector(BWAPI::Position vector, int length)
    {
        return ScaleVector({-vector.y, vector.x}, length);
    }

    std::vector<BWAPI::Position> TenDistancePositionsAroundPatch(BWAPI::Position patchCenter)
    {
        std::vector<BWAPI::Position> result = {{-54,-30},{-54,-29},{-54,-28},{-54,-27},{-54,-26},{-54,-25},{-54,-24},{-54,-23},{-54,-22},{-54,-21},{-54,-20},{-54,-19},{-54,-18},{-54,-17},{-54,-16},{-54,-15},{-54,-14},{-54,-13},{-54,-12},{-54,-11},{-54,-10},{-54,-9},{-54,-8},{-54,-7},{-54,-6},{-54,-5},{-54,-4},{-54,-3},{-54,-2},{-54,-1},{-54,0},{-54,1},{-54,2},{-54,3},{-54,4},{-54,5},{-54,6},{-54,7},{-54,8},{-54,9},{-54,10},{-54,11},{-54,12},{-54,13},{-54,14},{-54,15},{-54,16},{-54,17},{-54,18},{-54,19},{-54,20},{-54,21},{-54,22},{-54,23},{-54,24},{-54,25},{-54,26},{-54,27},{-54,28},{-54,29},{-53,-33},{-53,-32},{-53,-31},{-53,30},{-53,31},{-53,32},{-52,-35},{-52,-34},{-52,33},{-52,34},{-51,-36},{-51,35},{-50,-36},{-50,35},{-49,-37},{-49,36},{-48,-37},{-48,36},{-47,-37},{-47,36},{-46,-38},{-46,37},{-45,-38},{-45,37},{-44,-38},{-44,37},{-43,-38},{-43,37},{-42,-38},{-42,37},{-41,-38},{-41,37},{-40,-38},{-40,37},{-39,-38},{-39,37},{-38,-38},{-38,37},{-37,-38},{-37,37},{-36,-38},{-36,37},{-35,-38},{-35,37},{-34,-38},{-34,37},{-33,-38},{-33,37},{-32,-38},{-32,37},{-31,-38},{-31,37},{-30,-38},{-30,37},{-29,-38},{-29,37},{-28,-38},{-28,37},{-27,-38},{-27,37},{-26,-38},{-26,37},{-25,-38},{-25,37},{-24,-38},{-24,37},{-23,-38},{-23,37},{-22,-38},{-22,37},{-21,-38},{-21,37},{-20,-38},{-20,37},{-19,-38},{-19,37},{-18,-38},{-18,37},{-17,-38},{-17,37},{-16,-38},{-16,37},{-15,-38},{-15,37},{-14,-38},{-14,37},{-13,-38},{-13,37},{-12,-38},{-12,37},{-11,-38},{-11,37},{-10,-38},{-10,37},{-9,-38},{-9,37},{-8,-38},{-8,37},{-7,-38},{-7,37},{-6,-38},{-6,37},{-5,-38},{-5,37},{-4,-38},{-4,37},{-3,-38},{-3,37},{-2,-38},{-2,37},{-1,-38},{-1,37},{0,-38},{0,37},{1,-38},{1,37},{2,-38},{2,37},{3,-38},{3,37},{4,-38},{4,37},{5,-38},{5,37},{6,-38},{6,37},{7,-38},{7,37},{8,-38},{8,37},{9,-38},{9,37},{10,-38},{10,37},{11,-38},{11,37},{12,-38},{12,37},{13,-38},{13,37},{14,-38},{14,37},{15,-38},{15,37},{16,-38},{16,37},{17,-38},{17,37},{18,-38},{18,37},{19,-38},{19,37},{20,-38},{20,37},{21,-38},{21,37},{22,-38},{22,37},{23,-38},{23,37},{24,-38},{24,37},{25,-38},{25,37},{26,-38},{26,37},{27,-38},{27,37},{28,-38},{28,37},{29,-38},{29,37},{30,-38},{30,37},{31,-38},{31,37},{32,-38},{32,37},{33,-38},{33,37},{34,-38},{34,37},{35,-38},{35,37},{36,-38},{36,37},{37,-38},{37,37},{38,-38},{38,37},{39,-38},{39,37},{40,-38},{40,37},{41,-38},{41,37},{42,-38},{42,37},{43,-38},{43,37},{44,-38},{44,37},{45,-38},{45,37},{46,-37},{46,36},{47,-37},{47,36},{48,-37},{48,36},{49,-36},{49,35},{50,-36},{50,35},{51,-35},{51,-34},{51,33},{51,34},{52,-33},{52,-32},{52,-31},{52,30},{52,31},{52,32},{53,-30},{53,-29},{53,-28},{53,-27},{53,-26},{53,-25},{53,-24},{53,-23},{53,-22},{53,-21},{53,-20},{53,-19},{53,-18},{53,-17},{53,-16},{53,-15},{53,-14},{53,-13},{53,-12},{53,-11},{53,-10},{53,-9},{53,-8},{53,-7},{53,-6},{53,-5},{53,-4},{53,-3},{53,-2},{53,-1},{53,0},{53,1},{53,2},{53,3},{53,4},{53,5},{53,6},{53,7},{53,8},{53,9},{53,10},{53,11},{53,12},{53,13},{53,14},{53,15},{53,16},{53,17},{53,18},{53,19},{53,20},{53,21},{53,22},{53,23},{53,24},{53,25},{53,26},{53,27},{53,28},{53,29}};
        for (auto &pos : result)
        {
            pos.x += patchCenter.x;
            pos.y += patchCenter.y;
        }
        return result;
    }

    Direction directionFromBuilding(BWAPI::TilePosition tile, BWAPI::TilePosition size, BWAPI::Position pos, bool fourDirections)
    {
        int left = BWAPI::Position(tile).x;
        int right = BWAPI::Position(tile + BWAPI::TilePosition(size.x, 0)).x;
        int top = BWAPI::Position(tile).y;
        int bottom = BWAPI::Position(tile + BWAPI::TilePosition(0, size.y)).y;

        if (pos.x >= left && pos.x <= right && pos.y >= top && pos.y <= bottom)
        {
            return Direction::error;
        }

        if (pos.x >= left && pos.x <= right)
        {
            if (pos.y < top)
            {
                return Direction::up;
            }
            return Direction::down;
        }
        if (pos.y >= top && pos.y <= bottom)
        {
            if (pos.x < left)
            {
                return Direction::left;
            }
            return Direction::right;
        }
        if (pos.x < left)
        {
            if (pos.y < top)
            {
                if (fourDirections)
                {
                    if ((top - pos.y) > (left - pos.x)) return Direction::up;
                    return Direction::left;
                }
                return Direction::upleft;
            }
            if (fourDirections)
            {
                if ((pos.y - bottom) > (left - pos.x)) return Direction::down;
                return Direction::left;
            }
            return Direction::downleft;
        }
        if (pos.y < top)
        {
            if (fourDirections)
            {
                if ((top - pos.y) > (pos.x - right)) return Direction::up;
                return Direction::right;
            }
            return Direction::upright;
        }
        if (fourDirections)
        {
            if ((pos.y - bottom) > (pos.x - right)) return Direction::down;
            return Direction::right;
        }
        return Direction::downright;
    }

    int BWDirection(BWAPI::Position vector)
    {
        // Combination of the logic in openbw's xy_direction and atan

        if (vector.x == 0) return vector.y <= 0 ? 0 : -128;

        auto raw = (vector.y * 256) / vector.x;

        bool negative = raw < 0;
        if (negative) raw = -raw;

        size_t r = std::lower_bound(tan_table.begin(), tan_table.end(), (unsigned int)raw) - tan_table.begin();

        return int((vector.x > 0 ? 64 : -64) + (negative ? -r : r));
    }

    int BWAngleDiff(int a, int b)
    {
        int diff = a - b;
        if (diff > 127) diff -= 256;
        if (diff < -128) diff += 256;
        return abs(diff);
    }

    int BWAngleAdd(int a, int b)
    {
        int result = a + b;
        while (result > 127) result -= 256;
        while (result < -128) result += 256;
        return result;
    }

    void BWMovement(int &x, int &y, int &heading, int desiredHeading, int turnRate, int &speed, int acceleration, int topSpeed)
    {
        // BW updates position, then heading, then speed

        // Use the heading to update the position
        int dirHeading = (heading < 0) ? (heading + 256) : heading;
        x += (direction_table[dirHeading].x * speed) >> 8;
        y += (direction_table[dirHeading].y * speed) >> 8;

        // Update the heading
        int diff = BWAngleDiff(heading, desiredHeading);
        if (diff < turnRate)
        {
            heading = desiredHeading;
        }
        else
        {
            int first = BWAngleAdd(heading, turnRate);
            int second = BWAngleAdd(heading, -turnRate);
            if (BWAngleDiff(first, desiredHeading) < BWAngleDiff(second, desiredHeading))
            {
                heading = first;
            }
            else
            {
                heading = second;
            }
        }

        // Update the speed
        speed += acceleration;
        if (speed > topSpeed) speed = topSpeed;
        if (speed < 0) speed = 0;
    }
}
