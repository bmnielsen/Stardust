#include "Block.h"

/* This start block is useful on maps with a narrow main, e.g. La Mancha 1 o'clock */
class StartCompactLeft : public BuildBlock
{
public:
    explicit StartCompactLeft(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 8; }

    [[nodiscard]] int height() const override { return 7; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        /* For start blocks the provided tile is the nexus */
        auto blkTile = tilePos + BWAPI::TilePosition(-5, -1);

        /* Generate block tiles */
        std::vector<BWAPI::TilePosition> usedTiles = {
                blkTile + BWAPI::TilePosition(0, 0),
                blkTile + BWAPI::TilePosition(1, 0),
                blkTile + BWAPI::TilePosition(2, 0),
                blkTile + BWAPI::TilePosition(3, 0),
                blkTile + BWAPI::TilePosition(4, 0),
                blkTile + BWAPI::TilePosition(0, 1),
                blkTile + BWAPI::TilePosition(1, 1),
                blkTile + BWAPI::TilePosition(2, 1),
                blkTile + BWAPI::TilePosition(3, 1),
                blkTile + BWAPI::TilePosition(4, 1),
                blkTile + BWAPI::TilePosition(0, 2),
                blkTile + BWAPI::TilePosition(1, 2),
                blkTile + BWAPI::TilePosition(2, 2),
                blkTile + BWAPI::TilePosition(3, 2),
                blkTile + BWAPI::TilePosition(4, 2),
                blkTile + BWAPI::TilePosition(0, 3),
                blkTile + BWAPI::TilePosition(1, 3),
                blkTile + BWAPI::TilePosition(2, 3),
                blkTile + BWAPI::TilePosition(3, 3),
                blkTile + BWAPI::TilePosition(4, 3),
                blkTile + BWAPI::TilePosition(0, 4),
                blkTile + BWAPI::TilePosition(1, 4),
                blkTile + BWAPI::TilePosition(2, 4),
                blkTile + BWAPI::TilePosition(3, 4),
                blkTile + BWAPI::TilePosition(4, 4),
                blkTile + BWAPI::TilePosition(5, 4),
                blkTile + BWAPI::TilePosition(6, 4),
                blkTile + BWAPI::TilePosition(7, 4),
                blkTile + BWAPI::TilePosition(0, 5),
                blkTile + BWAPI::TilePosition(1, 5),
                blkTile + BWAPI::TilePosition(2, 5),
                blkTile + BWAPI::TilePosition(3, 5),
                blkTile + BWAPI::TilePosition(4, 5),
                blkTile + BWAPI::TilePosition(5, 5),
                blkTile + BWAPI::TilePosition(6, 5),
                blkTile + BWAPI::TilePosition(7, 5),
                blkTile + BWAPI::TilePosition(0, 6),
                blkTile + BWAPI::TilePosition(1, 6),
                blkTile + BWAPI::TilePosition(2, 6),
                blkTile + BWAPI::TilePosition(3, 6),
                blkTile + BWAPI::TilePosition(4, 6),
                blkTile + BWAPI::TilePosition(5, 6),
                blkTile + BWAPI::TilePosition(6, 6),
                blkTile + BWAPI::TilePosition(7, 6)
        };

        /* Generate surrounding tiles */
        std::vector<BWAPI::TilePosition> borderTiles = {
                blkTile + BWAPI::TilePosition(4, -1),
                blkTile + BWAPI::TilePosition(3, -1),
                blkTile + BWAPI::TilePosition(2, -1),
                blkTile + BWAPI::TilePosition(1, -1),
                blkTile + BWAPI::TilePosition(0, -1),
                blkTile + BWAPI::TilePosition(-1, -1),
                blkTile + BWAPI::TilePosition(-1, 0),
                blkTile + BWAPI::TilePosition(-1, 1),
                blkTile + BWAPI::TilePosition(-1, 2),
                blkTile + BWAPI::TilePosition(-1, 3),
                blkTile + BWAPI::TilePosition(-1, 4),
                blkTile + BWAPI::TilePosition(-1, 5),
                blkTile + BWAPI::TilePosition(-1, 6),
                blkTile + BWAPI::TilePosition(-1, 7),
                blkTile + BWAPI::TilePosition(0, 7),
                blkTile + BWAPI::TilePosition(1, 7),
                blkTile + BWAPI::TilePosition(2, 7),
                blkTile + BWAPI::TilePosition(3, 7),
                blkTile + BWAPI::TilePosition(4, 7),
                blkTile + BWAPI::TilePosition(5, 7),
                blkTile + BWAPI::TilePosition(6, 7),
                blkTile + BWAPI::TilePosition(7, 7),
                blkTile + BWAPI::TilePosition(8, 7),
                blkTile + BWAPI::TilePosition(8, 6),
                blkTile + BWAPI::TilePosition(8, 5),
                blkTile + BWAPI::TilePosition(8, 4),
        };

        if (placeStartBlock(tilePos, usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartCompactLeft>(blkTile, blkTile + BWAPI::TilePosition(3, 2));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(3, 2));
        small.emplace_back(cornerTL + BWAPI::TilePosition(3, 0));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
        medium.emplace_back(cornerTL);
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 4));
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 4));

        cannons.emplace_back(cornerTL + BWAPI::TilePosition(9, 4));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(9, -1));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(5, -1));
    }
};