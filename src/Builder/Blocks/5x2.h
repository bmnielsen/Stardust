#include "Block.h"

class Block5x2 : public BuildBlock
{
public:
    Block5x2(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 5; }

    [[nodiscard]] int height() const override { return 2; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block5x2>(tilePos, tilePos);
        }

        return nullptr;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(2, 0));
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 0), true);
    }
};