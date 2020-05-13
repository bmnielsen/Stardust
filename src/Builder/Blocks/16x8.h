#include "Block.h"

class Block16x8 : public Block
{
public:
    Block16x8(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 16; }

    [[nodiscard]] int height() const override { return 8; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block16x8>(tile, tile + BWAPI::TilePosition(8, 3));
        }

        return nullptr;
    }

    bool tilesUsed(BWAPI::TilePosition tile, BWAPI::TilePosition size) override
    {
        if (!Block::tilesUsed(tile, size)) return false;

        // When the first medium location is taken to the right, it opens up the next one
        if (tile == (topLeft + BWAPI::TilePosition(10, 3)) && size.x == 3 && size.y == 2)
        {
            medium.emplace_back(topLeft + BWAPI::TilePosition(13, 3));
            small.emplace_back(topLeft + BWAPI::TilePosition(13, 3), true);
            removeUsed();
            return true;
        }

        // If the first location on the right is instead taken with a converted pylon, it opens up two more pylons
        if (tile == (topLeft + BWAPI::TilePosition(10, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(12, 3));
            removeUsed();
            return true;
        }
        if (tile == (topLeft + BWAPI::TilePosition(12, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(14, 3));
            removeUsed();
            return true;
        }

        // When the left pylon is taken, it opens up the first medium location to the left
        if (tile == (topLeft + BWAPI::TilePosition(6, 3)) && size.x == 2 && size.y == 2)
        {
            medium.emplace_back(topLeft + BWAPI::TilePosition(3, 3), false, false);
            small.emplace_back(topLeft + BWAPI::TilePosition(4, 3), true);
            removeUsed();
            return true;
        }

        // If the first medium location to the left is taken by a converted pylon, it opens up two more pylons
        if (tile == (topLeft + BWAPI::TilePosition(4, 4)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(2, 3));
            removeUsed();
            return true;
        }
        if (tile == (topLeft + BWAPI::TilePosition(2, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
            removeUsed();
            return true;
        }

        // When the first medium location to the left is taken, it opens up the last medium location to the left
        if (tile == (topLeft + BWAPI::TilePosition(3, 3)) && size.x == 3 && size.y == 2)
        {
            medium.emplace_back(topLeft + BWAPI::TilePosition(0, 3));
            removeUsed();
            return true;
        }

        return true;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 3));
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 3));
        medium.emplace_back(topLeft + BWAPI::TilePosition(10, 3), false, false);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 3), true);
        large.emplace_back(topLeft);
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 5));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 5));
        large.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(8, 5));
        large.emplace_back(topLeft + BWAPI::TilePosition(12, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(12, 5));
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 5), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 5), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 5), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(12, 1), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(12, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(12, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(14, 1), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(12, 5), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(14, 5), true);
    }
};