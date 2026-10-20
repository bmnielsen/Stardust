#include "Block.h"

class StartCompactRightVertical : public BuildBlock
{
public:
    explicit StartCompactRightVertical(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 6; }

    [[nodiscard]] int height() const override { return 8; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        /* For start blocks the provided tile is the nexus */
        auto blkTile = tilePos + BWAPI::TilePosition(4, -3);

        /* Generate block tiles */
        std::vector<BWAPI::TilePosition> usedTiles = {
                blkTile + BWAPI::TilePosition(2, 0),
                blkTile + BWAPI::TilePosition(3, 0),
                blkTile + BWAPI::TilePosition(4, 0),
                blkTile + BWAPI::TilePosition(5, 0),
                blkTile + BWAPI::TilePosition(2, 1),
                blkTile + BWAPI::TilePosition(3, 1),
                blkTile + BWAPI::TilePosition(4, 1),
                blkTile + BWAPI::TilePosition(5, 1),
                blkTile + BWAPI::TilePosition(0, 2),
                blkTile + BWAPI::TilePosition(1, 2),
                blkTile + BWAPI::TilePosition(2, 2),
                blkTile + BWAPI::TilePosition(3, 2),
                blkTile + BWAPI::TilePosition(4, 2),
                blkTile + BWAPI::TilePosition(5, 2),
                blkTile + BWAPI::TilePosition(0, 3),
                blkTile + BWAPI::TilePosition(1, 3),
                blkTile + BWAPI::TilePosition(2, 3),
                blkTile + BWAPI::TilePosition(3, 3),
                blkTile + BWAPI::TilePosition(4, 3),
                blkTile + BWAPI::TilePosition(5, 3),
                blkTile + BWAPI::TilePosition(0, 4),
                blkTile + BWAPI::TilePosition(1, 4),
                blkTile + BWAPI::TilePosition(2, 4),
                blkTile + BWAPI::TilePosition(3, 4),
                blkTile + BWAPI::TilePosition(4, 4),
                blkTile + BWAPI::TilePosition(5, 4),
                blkTile + BWAPI::TilePosition(0, 5),
                blkTile + BWAPI::TilePosition(1, 5),
                blkTile + BWAPI::TilePosition(2, 5),
                blkTile + BWAPI::TilePosition(3, 5),
                blkTile + BWAPI::TilePosition(4, 5),
                blkTile + BWAPI::TilePosition(5, 5),
                blkTile + BWAPI::TilePosition(0, 6),
                blkTile + BWAPI::TilePosition(1, 6),
                blkTile + BWAPI::TilePosition(2, 6),
                blkTile + BWAPI::TilePosition(3, 6),
                blkTile + BWAPI::TilePosition(4, 6),
                blkTile + BWAPI::TilePosition(5, 6),
                blkTile + BWAPI::TilePosition(0, 7),
                blkTile + BWAPI::TilePosition(1, 7),
                blkTile + BWAPI::TilePosition(2, 7),
                blkTile + BWAPI::TilePosition(3, 7),
                blkTile + BWAPI::TilePosition(4, 7),
                blkTile + BWAPI::TilePosition(5, 7)
        };

        /* Generate surrounding tiles */
        std::vector<BWAPI::TilePosition> borderTiles = {
                blkTile + BWAPI::TilePosition(-1, 2),
                blkTile + BWAPI::TilePosition(-1, 1),
                blkTile + BWAPI::TilePosition(0, 1),
                blkTile + BWAPI::TilePosition(1, 1),
                blkTile + BWAPI::TilePosition(1, 0),
                blkTile + BWAPI::TilePosition(1, -1),
                blkTile + BWAPI::TilePosition(2, -1),
                blkTile + BWAPI::TilePosition(3, -1),
                blkTile + BWAPI::TilePosition(4, -1),
                blkTile + BWAPI::TilePosition(5, -1),
                blkTile + BWAPI::TilePosition(6, -1),
                blkTile + BWAPI::TilePosition(6, 0),
                blkTile + BWAPI::TilePosition(6, 1),
                blkTile + BWAPI::TilePosition(6, 2),
                blkTile + BWAPI::TilePosition(6, 3),
                blkTile + BWAPI::TilePosition(6, 4),
                blkTile + BWAPI::TilePosition(6, 5),
                blkTile + BWAPI::TilePosition(6, 6),
                blkTile + BWAPI::TilePosition(6, 7),
                blkTile + BWAPI::TilePosition(6, 8),
                blkTile + BWAPI::TilePosition(6, 7),
                blkTile + BWAPI::TilePosition(6, 6),
                blkTile + BWAPI::TilePosition(6, 5),
                blkTile + BWAPI::TilePosition(6, 4),
                blkTile + BWAPI::TilePosition(6, 3),
                blkTile + BWAPI::TilePosition(6, 2),
                blkTile + BWAPI::TilePosition(6, 1),
                blkTile + BWAPI::TilePosition(6, 0),
                blkTile + BWAPI::TilePosition(6, -1),
                blkTile + BWAPI::TilePosition(5, -1),
                blkTile + BWAPI::TilePosition(4, -1)

        };

        if (placeStartBlock(tilePos, usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartCompactRightVertical>(blkTile, blkTile + BWAPI::TilePosition(0, 4));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 4));
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 6));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(3, 6));
        large.emplace_back(cornerTL + BWAPI::TilePosition(2, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(2, 3));

        cannons.emplace_back(cornerTL + BWAPI::TilePosition(-6, 6));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(-6, 1));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(-2, 6));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(-2, 1));
    }
};