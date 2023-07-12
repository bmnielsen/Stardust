#include "McRave.h"

using namespace BWAPI;
using namespace std;

namespace McRave::Pylons
{
    namespace {
        map<TilePosition, int> smallLocations, mediumLocations, largeLocations;
        map<UnitSizeType, int> poweredPositions;

        void updatePowerGrid()
        {
            // Update power of every Pylon
            smallLocations.clear();
            mediumLocations.clear();
            largeLocations.clear();
            for (auto &u : Units::getUnits(PlayerState::Self)) {
                auto &unit = *u;
                if (unit.getType() != UnitTypes::Protoss_Pylon)
                    continue;

                int count = 1 + unit.unit()->isCompleted();
                for (int x = 0; x <= 15; x++) {
                    for (int y = 0; y <= 9; y++) {
                        const auto tile = TilePosition(x, y) + unit.getTilePosition() - TilePosition(8, 5);

                        if (!tile.isValid())
                            continue;

                        if (y == 0) {
                            if (x >= 4 && x <= 9)
                                largeLocations[tile] = max(largeLocations[tile], count);
                        }
                        else if (y == 1 || y == 8) {
                            if (x >= 2 && x <= 13) {
                                smallLocations[tile] = max(smallLocations[tile], count);
                                mediumLocations[tile] = max(mediumLocations[tile], count);
                            }
                            if (x >= 1 && x <= 12)
                                largeLocations[tile] = max(largeLocations[tile], count);
                        }
                        else if (y == 2 || y == 7) {
                            if (x >= 1 && x <= 14) {
                                smallLocations[tile] = max(smallLocations[tile], count);
                                mediumLocations[tile] = max(mediumLocations[tile], count);
                            }
                            if (x <= 13)
                                largeLocations[tile] = max(largeLocations[tile], count);
                        }
                        else if (y >= 3 && y <= 6) {
                            if (x >= 1 && x <= 14)
                                smallLocations[tile] = max(mediumLocations[tile], count);
                            if (x <= 15)
                                mediumLocations[tile] = max(mediumLocations[tile], count);
                            if (x <= 14)
                                largeLocations[tile] = max(largeLocations[tile], count);                            
                        }
                        else if (y == 9) {
                            if (x >= 5 && x <= 10) {
                                smallLocations[tile] = max(smallLocations[tile], count);
                                mediumLocations[tile] = max(mediumLocations[tile], count);
                            }
                            if (x >= 4 && x <= 9)
                                largeLocations[tile] = max(largeLocations[tile], count);
                        }
                    }
                }
            }
        }

        void updatePoweredPositions()
        {
            // Update power of every Block
            poweredPositions.clear();
            for (auto &block : BWEB::Blocks::getBlocks()) {
                for (auto &large : block.getLargeTiles()) {
                    if (largeLocations[large] > 0 && BWEB::Map::isUsed(large) == UnitTypes::None)
                        poweredPositions[UnitSizeTypes::Large]++;
                }
                for (auto &medium : block.getMediumTiles()) {
                    if (mediumLocations[medium] > 0 && BWEB::Map::isUsed(medium) == UnitTypes::None)
                        poweredPositions[UnitSizeTypes::Medium]++;
                }
                for (auto &small : block.getMediumTiles()) {
                    if (smallLocations[small] > 0 && BWEB::Map::isUsed(small) == UnitTypes::None)
                        poweredPositions[UnitSizeTypes::Small]++;
                }
            }
        }
    }

    void onFrame()
    {
        updatePowerGrid();
        updatePoweredPositions();
    }

    bool hasPowerNow(TilePosition here, UnitType building)
    {
        if (building.tileWidth() == 2)
            return smallLocations[here] == 2;
        else if (building.tileWidth() == 3)
            return mediumLocations[here] == 2;
        else if (building.tileWidth() == 4)
            return largeLocations[here] == 2;
        return false;
    }

    bool hasPowerSoon(TilePosition here, UnitType building)
    {
        if (building.tileWidth() == 2)
            return smallLocations[here] >= 1;
        else if (building.tileWidth() == 3)
            return mediumLocations[here] >= 1;
        else if (building.tileWidth() == 4)
            return largeLocations[here] >= 1;
        return false;
    }

    int countPoweredPositions(UnitType building) {
        if (building.tileWidth() == 2)
            return poweredPositions[UnitSizeTypes::Small];
        if (building.tileWidth() == 3)
            return poweredPositions[UnitSizeTypes::Medium];
        if (building.tileWidth() == 4)
            return poweredPositions[UnitSizeTypes::Large];
        return 0;
    }
}