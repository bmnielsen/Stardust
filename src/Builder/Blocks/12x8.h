#include "Block.h"

class Block12x8 : public BuildBlock
{
public:
    Block12x8(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 12; }

    [[nodiscard]] int height() const override { return 8; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowLeftEdge() const override { return false; }

    [[nodiscard]] bool allowRightEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            if (cornerTL.x == 0)
            {
                return std::make_shared<Block12x8>(tilePos, tilePos + BWAPI::TilePosition(4, 3));
            }
            else
            {
                return std::make_shared<Block12x8>(tilePos, tilePos + BWAPI::TilePosition(6, 3));
            }
        }

        return nullptr;
    }

    bool tilesUsed(BWAPI::TilePosition tilePos, BWAPI::TilePosition size) override
    {
        if (!BuildBlock::tilesUsed(tilePos, size)) return false;

        /* When the first medium location is taken to the left, it opens up the next one */
        if (tilePos == (cornerTL + BWAPI::TilePosition(3, 3)) && size.x == 3 && size.y == 2)
        {
            medium.emplace_back(cornerTL + BWAPI::TilePosition(0, 3));
            small.emplace_back(cornerTL + BWAPI::TilePosition(1, 3), true);
            removeUsed();
            return true;
        }

        /* When the first medium location is taken to the right, it opens up the next one */
        /* Only applies when on left map edge */
        if (tilePos == (cornerTL + BWAPI::TilePosition(6, 3)) && size.x == 3 && size.y == 2)
        {
            medium.emplace_back(cornerTL + BWAPI::TilePosition(9, 3));
            small.emplace_back(cornerTL + BWAPI::TilePosition(9, 3), true);
            removeUsed();
            return true;
        }

        /* If the first location on the left is instead taken with a converted pylon, it opens up the next two pylons */
        if (tilePos == (cornerTL + BWAPI::TilePosition(4, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(2, 3));
            removeUsed();
            return true;
        }
        if (tilePos == (cornerTL + BWAPI::TilePosition(2, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(0, 3));
            removeUsed();
            return true;
        }

        /* If the first location on the right is instead taken with a converted pylon, it opens up another pylon */
        if (tilePos == (cornerTL + BWAPI::TilePosition(8, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(10, 3));
            removeUsed();
            return true;
        }

        /* If the first location on the right (left edge version) is instead taken with a converted pylon, it opens up the next two pylons */
        if (tilePos == (cornerTL + BWAPI::TilePosition(6, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(8, 3));
            removeUsed();
            return true;
        }
        if (tilePos == (cornerTL + BWAPI::TilePosition(8, 3)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(10, 3));
            removeUsed();
            return true;
        }

        return true;
    }

protected:
    void placeLocations() override
    {
        if (cornerTL.x == 0)
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(4, 3));
            small.emplace_back(cornerTL + BWAPI::TilePosition(2, 3));
            small.emplace_back(cornerTL + BWAPI::TilePosition(0, 3));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(6, 3), false, false);
            small.emplace_back(cornerTL + BWAPI::TilePosition(6, 3), true);
        }
        else if (cornerTL.x + 12 == BWAPI::Broodwar->mapWidth())
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(6, 3));
            small.emplace_back(cornerTL + BWAPI::TilePosition(8, 3));
            small.emplace_back(cornerTL + BWAPI::TilePosition(10, 3));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(3, 3), false, false);
            small.emplace_back(cornerTL + BWAPI::TilePosition(4, 3), true);
        }
        else
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(6, 3));
            medium.emplace_back(cornerTL + BWAPI::TilePosition(3, 3), false, false);
            medium.emplace_back(cornerTL + BWAPI::TilePosition(8, 3));
            small.emplace_back(cornerTL + BWAPI::TilePosition(8, 3), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(4, 3), true);
        }

        large.emplace_back(cornerTL);
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 5));
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 5));
        large.emplace_back(cornerTL + BWAPI::TilePosition(8, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(8, 5));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(1, 1), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(1, 5), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 1), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 5), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(8, 1), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(8, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(8, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(10, 1), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(8, 5), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(10, 5), true);
    }
};