#include "Block.h"

/* This start block is useful in mains with a geyser on left and minerals on right and top, like Outsider 1 o'clock */
class StartBottomHorizontal : public BuildBlock
{
public:
    explicit StartBottomHorizontal(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 14; }

    [[nodiscard]] int height() const override { return 4; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        /* For start blocks the provided tile is the nexus */
        auto blkTile = tilePos + BWAPI::TilePosition(-6, 2);

        /* Generate block tiles */
        std::vector<BWAPI::TilePosition> usedTiles = {
                blkTile + BWAPI::TilePosition(4, 0),
                blkTile + BWAPI::TilePosition(5, 0),
                blkTile + BWAPI::TilePosition(4, 1),
                blkTile + BWAPI::TilePosition(5, 1),

                blkTile + BWAPI::TilePosition(0, 2),
                blkTile + BWAPI::TilePosition(1, 2),
                blkTile + BWAPI::TilePosition(2, 2),
                blkTile + BWAPI::TilePosition(0, 3),
                blkTile + BWAPI::TilePosition(1, 3),
                blkTile + BWAPI::TilePosition(2, 3),

                blkTile + BWAPI::TilePosition(3, 2),
                blkTile + BWAPI::TilePosition(4, 2),
                blkTile + BWAPI::TilePosition(5, 2),
                blkTile + BWAPI::TilePosition(3, 3),
                blkTile + BWAPI::TilePosition(4, 3),
                blkTile + BWAPI::TilePosition(5, 3),

                blkTile + BWAPI::TilePosition(6, 1),
                blkTile + BWAPI::TilePosition(7, 1),
                blkTile + BWAPI::TilePosition(8, 1),
                blkTile + BWAPI::TilePosition(9, 1),
                blkTile + BWAPI::TilePosition(6, 2),
                blkTile + BWAPI::TilePosition(7, 2),
                blkTile + BWAPI::TilePosition(8, 2),
                blkTile + BWAPI::TilePosition(9, 2),
                blkTile + BWAPI::TilePosition(6, 3),
                blkTile + BWAPI::TilePosition(7, 3),
                blkTile + BWAPI::TilePosition(8, 3),
                blkTile + BWAPI::TilePosition(9, 3),

                blkTile + BWAPI::TilePosition(10, 1),
                blkTile + BWAPI::TilePosition(11, 1),
                blkTile + BWAPI::TilePosition(12, 1),
                blkTile + BWAPI::TilePosition(13, 1),
                blkTile + BWAPI::TilePosition(10, 2),
                blkTile + BWAPI::TilePosition(11, 2),
                blkTile + BWAPI::TilePosition(12, 2),
                blkTile + BWAPI::TilePosition(13, 2),
                blkTile + BWAPI::TilePosition(10, 3),
                blkTile + BWAPI::TilePosition(11, 3),
                blkTile + BWAPI::TilePosition(12, 3),
                blkTile + BWAPI::TilePosition(13, 3)
        };

        /* Generate surrounding tiles */
        std::vector<BWAPI::TilePosition> borderTiles = {
                blkTile + BWAPI::TilePosition(5, -1),
                blkTile + BWAPI::TilePosition(4, -1),
                blkTile + BWAPI::TilePosition(3, -1),
                blkTile + BWAPI::TilePosition(3, 0),
                blkTile + BWAPI::TilePosition(3, 1),
                blkTile + BWAPI::TilePosition(2, 1),
                blkTile + BWAPI::TilePosition(1, 1),
                blkTile + BWAPI::TilePosition(0, 1),
                blkTile + BWAPI::TilePosition(-1, 1),
                blkTile + BWAPI::TilePosition(-1, 2),
                blkTile + BWAPI::TilePosition(-1, 3),
                blkTile + BWAPI::TilePosition(-1, 4),
                blkTile + BWAPI::TilePosition(0, 4),
                blkTile + BWAPI::TilePosition(1, 4),
                blkTile + BWAPI::TilePosition(2, 4),
                blkTile + BWAPI::TilePosition(3, 4),
                blkTile + BWAPI::TilePosition(4, 4),
                blkTile + BWAPI::TilePosition(5, 4),
                blkTile + BWAPI::TilePosition(6, 4),
                blkTile + BWAPI::TilePosition(7, 4),
                blkTile + BWAPI::TilePosition(8, 4),
                blkTile + BWAPI::TilePosition(9, 4),
                blkTile + BWAPI::TilePosition(10, 4),
                blkTile + BWAPI::TilePosition(11, 4),
                blkTile + BWAPI::TilePosition(12, 4),
                blkTile + BWAPI::TilePosition(13, 4),
                blkTile + BWAPI::TilePosition(14, 4),
                blkTile + BWAPI::TilePosition(14, 3),
                blkTile + BWAPI::TilePosition(14, 2),
                blkTile + BWAPI::TilePosition(14, 1),
                blkTile + BWAPI::TilePosition(14, 0),
                blkTile + BWAPI::TilePosition(13, 0),
                blkTile + BWAPI::TilePosition(12, 0),
                blkTile + BWAPI::TilePosition(11, 0),
                blkTile + BWAPI::TilePosition(10, 0)
        };

        if (placeStartBlock(tilePos, usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartBottomHorizontal>(blkTile, blkTile + BWAPI::TilePosition(4, 0));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 0));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(3, 2));
        large.emplace_back(cornerTL + BWAPI::TilePosition(6, 1));
        large.emplace_back(cornerTL + BWAPI::TilePosition(10, 1));

        cannons.emplace_back(cornerTL + BWAPI::TilePosition(8, -4));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(4, -4));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(10, -1));
    }
};