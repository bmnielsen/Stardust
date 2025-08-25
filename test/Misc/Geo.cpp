#include "BWTest.h"

#include "Geo.h"

TEST(Geo, Find10DistancePositionsFromMineralPatch)
{
    BWAPI::Position patchLocation(100, 100);
    std::set<BWAPI::Position> result;

    for (int x = (100 - 32 - 10 - 12); x <= (100 + 32 + 10 + 12); x++)
    {
        for (int y = (100 - 16 - 10 - 12); y <= (100 + 16 + 10 + 12); y++)
        {
            BWAPI::Position pos(x, y);
            if (Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Resource_Mineral_Field, patchLocation, BWAPI::UnitTypes::Protoss_Probe, pos) == 10)
            {
                result.insert(pos - patchLocation);
            }
        }
    }

    std::string sep;
    std::cout << "{";
    for (const auto &pos : result)
    {
        std::cout << sep << "{" << pos.x << "," << pos.y << "}";
        sep = ",";
    }
    std::cout << "}" << std::endl;
}
