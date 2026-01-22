#include "BWTest.h"

#include "Modules/InstrumentedDoNothingModule.h"

#include "Units.h"
#include "Map.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"
#include "MiningOptimizationTraining/DataModel/Configuration.h"

using namespace MiningOptimizationTraining;

namespace
{
    std::set<BWAPI::Position> getPatchMiningStartPositions(std::set<std::pair<int, int>> &blockedPositions,
                                                           BWAPI::TilePosition depotTile,
                                                           BWAPI::TilePosition patchTile)
    {
        std::set<BWAPI::Position> result;

        // Compute some worker center positions around the patch
        auto topLeft = BWAPI::Position(patchTile) + BWAPI::Position(-12, -12);
        auto topRight = BWAPI::Position(patchTile) + BWAPI::Position(75, -12);
        auto bottomLeft = BWAPI::Position(patchTile) + BWAPI::Position(-12, 43);
        auto bottomRight = BWAPI::Position(patchTile) + BWAPI::Position(75, 43);

        // Determine where the patch is in relation to the depot
        bool left = (patchTile.x < depotTile.x);
        bool right = (patchTile.x > (depotTile.x + 2));
        bool hmid = (!left && !right);
        bool top = (patchTile.y < depotTile.y);
        bool bottom = (patchTile.y > (depotTile.y + 2));
        bool vmid = (!top && !bottom);

        auto addPosition = [&](int x, int y)
        {
            auto pos = BWAPI::Position(x, y);
            if (pos.isValid() && !blockedPositions.contains(std::make_pair(x, y)))
            {
                result.insert(pos);
            }
        };

        // We might mine from the left side of the patch if is it to the right or middle of the depot
        if (right || hmid)
        {
            for (int y = topLeft.y; y <= bottomLeft.y; y++) addPosition(topLeft.x, y);
        }

        // We might mine from the right side of the patch if is it to the left or middle of the depot
        if (right || hmid)
        {
            for (int y = topRight.y; y <= bottomRight.y; y++) addPosition(topRight.x, y);
        }

        // We might mine from the top of the patch if is it to the bottom or middle of the depot
        if (bottom || vmid)
        {
            for (int x = topLeft.x; x < topRight.x; x++) addPosition(x, topLeft.y);
        }

        // We might mine from the bottom of the patch if is it to the top or middle of the depot
        if (top || vmid)
        {
            for (int x = bottomLeft.x; x < bottomRight.x; x++) addPosition(x, bottomRight.y);
        }

        return result;
    }

    class InitializeMapDataModule : public InstrumentedDoNothingModule
    {
    public:
        void onStart() override
        {
            InstrumentedDoNothingModule::onStart();

            // Analyze bases
            Units::initialize();
            Map::initialize();

            // Build a set of blocked positions around all patches on the map
            std::set<std::pair<int, int>> blockedPositions;
            auto addBlockedAroundBox = [&blockedPositions](BWAPI::Position topLeft, BWAPI::Position size)
            {
                for (int x = topLeft.x - 11; x < topLeft.x + size.x + 11; x++)
                {
                    for (int y = topLeft.y - 11; y < topLeft.y + size.y + 11; y++)
                    {
                        blockedPositions.emplace(x, y);
                    }
                }
            };
            for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                if (!unit->getType().isMineralField() && unit->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser) continue;

                addBlockedAroundBox(BWAPI::Position(unit->getInitialTilePosition()), BWAPI::Position(unit->getType().tileSize()));

                auto walkPos = BWAPI::WalkPosition(unit->getInitialTilePosition());
                auto walkSize = BWAPI::WalkPosition(unit->getType().tileSize());
                for (int walkX = walkPos.x - 3; walkX < walkPos.x + walkSize.x + 3; walkX++)
                {
                    for (int walkY = walkPos.y - 3; walkY < walkPos.y + walkSize.y + 3; walkY++)
                    {
                        auto hereWalk = BWAPI::WalkPosition(walkX, walkY);
                        if (!hereWalk.isValid()) continue;
                        if (BWAPI::Broodwar->isWalkable(hereWalk)) continue;
                        addBlockedAroundBox(BWAPI::Position(hereWalk), BWAPI::Position(8, 8));
                    }
                }
            }

            // Create the empty map data
            auto mapData = MapData{BWAPI::Broodwar->mapHash()};

            // Process each patch
            for (auto base : Map::allBases())
            {
                for (auto &patch : base->mineralPatches())
                {
                    auto startPositions = getPatchMiningStartPositions(blockedPositions, base->getTilePosition(), patch->tile);

                    auto &returnPaths = mapData.resourceToReturnPaths[TilePosition(patch->tile.x, patch->tile.y)];
                    for (auto &startPosition : startPositions)
                    {
                        // The heading is always the direction between the center of the worker and the center of the patch
                        auto heading = (int8_t)Geo::BWDirection(patch->center - startPosition);

                        // Create the position and velocity, where the velocity after mining completion is always 0
                        auto positionAndVelocity = PositionAndVelocity(startPosition.x, startPosition.y, heading, 0, 0);

                        // Create the path
                        auto path = Path<ReturnArrivalData>{positionAndVelocity};

                        // Generate the exact positions to test
                        auto baseX = ((unsigned int)startPosition.x) << 8;
                        auto baseY = ((unsigned int)startPosition.y) << 8;
                        for (int subpixelX = 0; subpixelX < 256; subpixelX += EXACT_POSITIONS_TO_EXPLORE_PER_AXIS)
                        {
                            for (int subpixelY = 0; subpixelY < 256; subpixelY += EXACT_POSITIONS_TO_EXPLORE_PER_AXIS)
                            {
                                path.positionsToExplore.emplace_back(baseX + subpixelX, baseY + subpixelY, heading, 0, 0);
                            }
                        }

                        returnPaths[positionAndVelocity] = std::move(path);
                    }
                }
            }

            // Save the map data
            Serialization::writeMapData(mapData);
        }
    };

    void run(BWTest &test)
    {
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.myModule = []()
        {
            return new InitializeMapDataModule();
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 10;
        test.run();
    }
}

TEST(InitializeMapData, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test);
}
