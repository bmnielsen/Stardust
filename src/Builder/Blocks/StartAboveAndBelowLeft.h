#include "Block.h"

/* This start block is useful in mains with a geyser on left and minerals on bottom and right, like Outsider 5 o'clock */
class StartAboveAndBelowLeft : public BuildBlock
{
public:
    explicit StartAboveAndBelowLeft(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 14; }

    [[nodiscard]] int height() const override { return 9; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        /* For start blocks the provided tile is the nexus */
        auto blkTile = tilePos + BWAPI::TilePosition(-10, -2);

        /* Generate block tiles */
        std::vector<BWAPI::TilePosition> usedTiles = {
                blkTile + BWAPI::TilePosition(8, 0),
                blkTile + BWAPI::TilePosition(9, 0),
                blkTile + BWAPI::TilePosition(10, 0),
                blkTile + BWAPI::TilePosition(8, 1),
                blkTile + BWAPI::TilePosition(9, 1),
                blkTile + BWAPI::TilePosition(10, 1),
                blkTile + BWAPI::TilePosition(11, 0),
                blkTile + BWAPI::TilePosition(12, 0),
                blkTile + BWAPI::TilePosition(13, 0),
                blkTile + BWAPI::TilePosition(11, 1),
                blkTile + BWAPI::TilePosition(12, 1),
                blkTile + BWAPI::TilePosition(13, 1),
                blkTile + BWAPI::TilePosition(8, 4),
                blkTile + BWAPI::TilePosition(9, 4),
                blkTile + BWAPI::TilePosition(8, 5),
                blkTile + BWAPI::TilePosition(9, 5),
                blkTile + BWAPI::TilePosition(0, 6),
                blkTile + BWAPI::TilePosition(1, 6),
                blkTile + BWAPI::TilePosition(2, 6),
                blkTile + BWAPI::TilePosition(3, 6),
                blkTile + BWAPI::TilePosition(4, 6),
                blkTile + BWAPI::TilePosition(5, 6),
                blkTile + BWAPI::TilePosition(6, 6),
                blkTile + BWAPI::TilePosition(7, 6),
                blkTile + BWAPI::TilePosition(0, 7),
                blkTile + BWAPI::TilePosition(1, 7),
                blkTile + BWAPI::TilePosition(2, 7),
                blkTile + BWAPI::TilePosition(3, 7),
                blkTile + BWAPI::TilePosition(4, 7),
                blkTile + BWAPI::TilePosition(5, 7),
                blkTile + BWAPI::TilePosition(6, 7),
                blkTile + BWAPI::TilePosition(7, 7),
                blkTile + BWAPI::TilePosition(0, 8),
                blkTile + BWAPI::TilePosition(1, 8),
                blkTile + BWAPI::TilePosition(2, 8),
                blkTile + BWAPI::TilePosition(3, 8),
                blkTile + BWAPI::TilePosition(4, 8),
                blkTile + BWAPI::TilePosition(5, 8),
                blkTile + BWAPI::TilePosition(6, 8),
                blkTile + BWAPI::TilePosition(7, 8),
        };

        /* Generate surrounding tiles */
        std::vector<BWAPI::TilePosition> borderTiles = {
                blkTile + BWAPI::TilePosition(14, 1),
                blkTile + BWAPI::TilePosition(14, 0),
                blkTile + BWAPI::TilePosition(14, -1),
                blkTile + BWAPI::TilePosition(13, -1),
                blkTile + BWAPI::TilePosition(12, -1),
                blkTile + BWAPI::TilePosition(11, -1),
                blkTile + BWAPI::TilePosition(10, -1),
                blkTile + BWAPI::TilePosition(9, -1),
                blkTile + BWAPI::TilePosition(8, -1),
                blkTile + BWAPI::TilePosition(7, -1),
                blkTile + BWAPI::TilePosition(7, 0),
                blkTile + BWAPI::TilePosition(7, 1),
                blkTile + BWAPI::TilePosition(7, 2),
                blkTile + BWAPI::TilePosition(8, 2),
                blkTile + BWAPI::TilePosition(9, 2),
                blkTile + BWAPI::TilePosition(9, 3),
                blkTile + BWAPI::TilePosition(8, 3),
                blkTile + BWAPI::TilePosition(7, 3),
                blkTile + BWAPI::TilePosition(7, 4),
                blkTile + BWAPI::TilePosition(7, 5),
                blkTile + BWAPI::TilePosition(6, 5),
                blkTile + BWAPI::TilePosition(5, 5),
                blkTile + BWAPI::TilePosition(4, 5),
                blkTile + BWAPI::TilePosition(3, 5),
                blkTile + BWAPI::TilePosition(2, 5),
                blkTile + BWAPI::TilePosition(1, 5),
                blkTile + BWAPI::TilePosition(0, 5),
                blkTile + BWAPI::TilePosition(-1, 5),
                blkTile + BWAPI::TilePosition(-1, 6),
                blkTile + BWAPI::TilePosition(-1, 7),
                blkTile + BWAPI::TilePosition(-1, 8),
                blkTile + BWAPI::TilePosition(-1, 9),
                blkTile + BWAPI::TilePosition(0, 9),
                blkTile + BWAPI::TilePosition(1, 9),
                blkTile + BWAPI::TilePosition(2, 9),
                blkTile + BWAPI::TilePosition(3, 9),
                blkTile + BWAPI::TilePosition(4, 9),
                blkTile + BWAPI::TilePosition(5, 9),
                blkTile + BWAPI::TilePosition(6, 9),
                blkTile + BWAPI::TilePosition(7, 9),
                blkTile + BWAPI::TilePosition(8, 9),
                blkTile + BWAPI::TilePosition(8, 8),
                blkTile + BWAPI::TilePosition(8, 7),
                blkTile + BWAPI::TilePosition(8, 6),
                blkTile + BWAPI::TilePosition(9, 6),
                blkTile + BWAPI::TilePosition(10, 6),
                blkTile + BWAPI::TilePosition(10, 5)
        };

        if (placeStartBlock(tilePos, usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartAboveAndBelowLeft>(blkTile, blkTile + BWAPI::TilePosition(8, 4));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(8, 4));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(8, 0));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(11, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 6));
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 6));

        cannons.emplace_back(cornerTL + BWAPI::TilePosition(14, 4));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(11, 5));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(14, 1));
    }
};