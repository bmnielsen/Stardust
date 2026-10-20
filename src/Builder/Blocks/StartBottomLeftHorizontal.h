#include "Block.h"

/* This start block is useful in mains with a geyser on left and minerals on right, like Tau Cross 6 o'clock */
class StartBottomLeftHorizontal : public BuildBlock
{
public:
    explicit StartBottomLeftHorizontal(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 11; }

    [[nodiscard]] int height() const override { return 6; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        /* For start blocks the provided tile is the nexus */
        auto blkTile = tilePos + BWAPI::TilePosition(-9, 1);

        /* Generate block tiles */
        std::vector<BWAPI::TilePosition> usedTiles = {
                blkTile + BWAPI::TilePosition(7, 0),
                blkTile + BWAPI::TilePosition(8, 0),
                blkTile + BWAPI::TilePosition(7, 1),
                blkTile + BWAPI::TilePosition(8, 1),
                blkTile + BWAPI::TilePosition(0, 2),
                blkTile + BWAPI::TilePosition(1, 2),
                blkTile + BWAPI::TilePosition(2, 2),
                blkTile + BWAPI::TilePosition(3, 2),
                blkTile + BWAPI::TilePosition(4, 2),
                blkTile + BWAPI::TilePosition(5, 2),
                blkTile + BWAPI::TilePosition(6, 2),
                blkTile + BWAPI::TilePosition(7, 2),
                blkTile + BWAPI::TilePosition(8, 2),
                blkTile + BWAPI::TilePosition(9, 2),
                blkTile + BWAPI::TilePosition(10, 2),
                blkTile + BWAPI::TilePosition(0, 3),
                blkTile + BWAPI::TilePosition(1, 3),
                blkTile + BWAPI::TilePosition(2, 3),
                blkTile + BWAPI::TilePosition(3, 3),
                blkTile + BWAPI::TilePosition(4, 3),
                blkTile + BWAPI::TilePosition(5, 3),
                blkTile + BWAPI::TilePosition(6, 3),
                blkTile + BWAPI::TilePosition(7, 3),
                blkTile + BWAPI::TilePosition(8, 3),
                blkTile + BWAPI::TilePosition(9, 3),
                blkTile + BWAPI::TilePosition(10, 3),
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
                blkTile + BWAPI::TilePosition(8, 5),
                blkTile + BWAPI::TilePosition(9, 5),
                blkTile + BWAPI::TilePosition(10, 5)
        };

        /* Generate surrounding tiles */
        std::vector<BWAPI::TilePosition> borderTiles = {
                blkTile + BWAPI::TilePosition(8, -1),
                blkTile + BWAPI::TilePosition(7, -1),
                blkTile + BWAPI::TilePosition(6, -1),
                blkTile + BWAPI::TilePosition(6, 0),
                blkTile + BWAPI::TilePosition(6, 1),
                blkTile + BWAPI::TilePosition(5, 1),
                blkTile + BWAPI::TilePosition(4, 1),
                blkTile + BWAPI::TilePosition(3, 1),
                blkTile + BWAPI::TilePosition(2, 1),
                blkTile + BWAPI::TilePosition(1, 1),
                blkTile + BWAPI::TilePosition(0, 1),
                blkTile + BWAPI::TilePosition(-1, 1),
                blkTile + BWAPI::TilePosition(-1, 2),
                blkTile + BWAPI::TilePosition(-1, 3),
                blkTile + BWAPI::TilePosition(-1, 4),
                blkTile + BWAPI::TilePosition(-1, 5),
                blkTile + BWAPI::TilePosition(0, 5),
                blkTile + BWAPI::TilePosition(1, 5),
                blkTile + BWAPI::TilePosition(2, 5),
                blkTile + BWAPI::TilePosition(3, 5),
                blkTile + BWAPI::TilePosition(4, 5),
                blkTile + BWAPI::TilePosition(5, 5),
                blkTile + BWAPI::TilePosition(6, 5),
                blkTile + BWAPI::TilePosition(7, 5),
                blkTile + BWAPI::TilePosition(7, 6),
                blkTile + BWAPI::TilePosition(8, 6),
                blkTile + BWAPI::TilePosition(9, 6),
                blkTile + BWAPI::TilePosition(10, 6),
                blkTile + BWAPI::TilePosition(11, 6),
                blkTile + BWAPI::TilePosition(11, 5),
                blkTile + BWAPI::TilePosition(11, 4),
                blkTile + BWAPI::TilePosition(11, 3),
                blkTile + BWAPI::TilePosition(11, 2)
        };

        if (placeStartBlock(tilePos, usedTiles, borderTiles, tileAvailability))
        {
            return std::make_shared<StartBottomLeftHorizontal>(blkTile, blkTile + BWAPI::TilePosition(7, 0));
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(7, 0));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(8, 2));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(8, 4));
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 2));

        cannons.emplace_back(cornerTL + BWAPI::TilePosition(13, 2));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(13, -3));
        cannons.emplace_back(cornerTL + BWAPI::TilePosition(9, -3));
    }
};