#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"

namespace
{
    std::string direction(BWAPI::TilePosition baseTile, BWAPI::Position pos, bool fourDirections = false)
    {
        int left = BWAPI::Position(baseTile).x;
        int right = BWAPI::Position(baseTile + BWAPI::TilePosition(4, 0)).x;
        int top = BWAPI::Position(baseTile).y;
        int bottom = BWAPI::Position(baseTile + BWAPI::TilePosition(0, 3)).y;

        if (pos.x >= left && pos.x <= right && pos.y >= top && pos.y <= bottom)
        {
            return "error";
        }

        if (pos.x >= left && pos.x <= right)
        {
            if (pos.y < top)
            {
                return "top";
            }
            return "bottom";
        }
        if (pos.y >= top && pos.y <= bottom)
        {
            if (pos.x < left)
            {
                return "left";
            }
            return "right";
        }
        if (pos.x < left)
        {
            if (pos.y < top)
            {
                if (fourDirections)
                {
                    if ((top - pos.y) > (left - pos.x)) return "top";
                    return "left";
                }
                return "topleft";
            }
            if (fourDirections)
            {
                if ((pos.y - bottom) > (left - pos.x)) return "bottom";
                return "left";
            }
            return "bottomleft";
        }
        if (pos.y < top)
        {
            if (fourDirections)
            {
                if ((top - pos.y) > (pos.x - right)) return "top";
                return "right";
            }
            return "topright";
        }
        if (fourDirections)
        {
            if ((pos.y - bottom) > (pos.x - right)) return "bottom";
            return "right";
        }
        return "bottomright";
    }
}

TEST(NaturalLayouts, AnalyzeAll)
{
    std::set<std::string> layouts;

    auto run = [&](BWTest test)
    {
        test.frameLimit = 10;
        test.expectWin = false;
        test.writeReplay = false;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.myModule = []()
        {
            return new DoNothingModule();
        };

        test.onStartMine = [&]()
        {
            Map::initialize();

            auto bases = Map::unscoutedStartingLocations();
            bases.insert(Map::getMyMain());
            auto enemyMain = Map::getEnemyStartingMain();
            if (enemyMain)
            {
                bases.insert(enemyMain);
            }

            for (auto base : bases)
            {
                auto natural = Map::getStartingBaseNatural(base);
                if (!natural)
                {
                    std::cout << "No natural for starting base @ " << base->getTilePosition() << std::endl;
                    continue;
                }

                auto geyserTiles = natural->geyserLocations();
                if (geyserTiles.empty())
                {
                    continue;
                }

                auto chokes = Map::getStartingBaseChokes(base);

                std::cout << "Natural @ " << natural->getTilePosition() << std::endl;
                std::cout << "Geyser: " << direction(natural->getTilePosition(), BWAPI::Position(*geyserTiles.begin() + BWAPI::TilePosition(2, 1)), true) << std::endl;
                std::cout << "Minerals: " << direction(natural->getTilePosition(), natural->mineralLineCenter) << std::endl;
                std::cout << "Choke: " << direction(natural->getTilePosition(), chokes.second->center, true) << std::endl;

                std::ostringstream builder;
                builder << "min " << direction(natural->getTilePosition(), natural->mineralLineCenter);
                builder << "; ";
                builder << "gas " << direction(natural->getTilePosition(), BWAPI::Position(*geyserTiles.begin() + BWAPI::TilePosition(2, 1)), true);
                builder << "; ";
                builder << "choke " << direction(natural->getTilePosition(), chokes.second->center, true);

                layouts.insert(builder.str());
            }
        };

        test.run();
    };

    Maps::RunOnEach(Maps::Get("sscai"), run);
    Maps::RunOnEach(Maps::Get("aiide"), run);
    Maps::RunOnEach(Maps::Get("cog2022"), run);

    for (auto &layout : layouts)
    {
        std::cout << layout << std::endl;
    }
}
