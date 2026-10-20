#pragma once

#include "Common.h"

/* Stores information about a block of build locations. */
class BuildBlock
{
public:
    struct Location
    {
        BWAPI::TilePosition tilePos;
        bool converted; // Whether this position is converted from a more-efficient position
        bool hasExit; // Whether this position has an exit (only applicable to medium locations)

        explicit Location(BWAPI::TilePosition tilePos, bool converted = false, bool hasExit = true)
                : tilePos(tilePos), converted(converted), hasExit(hasExit) {}
    };

    BWAPI::TilePosition cornerTL;

    BWAPI::TilePosition powerPylon;

    std::vector<Location> small;
    std::vector<Location> medium;
    std::vector<Location> large;

    std::vector<BWAPI::TilePosition> cannons; // Only applicable to start blocks

    BuildBlock(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : cornerTL(cornerTL), powerPylon(powerPylon) {}

    virtual ~BuildBlock() = default;

    [[nodiscard]] virtual int width() const = 0;

    [[nodiscard]] virtual int height() const = 0;

    [[nodiscard]] BWAPI::Position midpoint() const;

    [[nodiscard]] virtual bool allowTopEdge() const { return true; }

    [[nodiscard]] virtual bool allowLeftEdge() const { return true; }

    [[nodiscard]] virtual bool allowRightEdge() const { return true; }

    [[nodiscard]] virtual bool allowCorner() const { return true; }

    bool tilesReserved(BWAPI::TilePosition tilePos, BWAPI::TilePosition size, bool permanent = false);

    virtual bool tilesUsed(BWAPI::TilePosition tilePos, BWAPI::TilePosition size);

    bool tilesFreed(BWAPI::TilePosition tilePos, BWAPI::TilePosition size);

    virtual std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) = 0;

protected:
    bool place(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) const;

    bool placeStartBlock(BWAPI::TilePosition startPosition,
                         std::vector<BWAPI::TilePosition> &usedTiles,
                         std::vector<BWAPI::TilePosition> &borderTiles,
                         std::vector<unsigned int> &tileAvailability) const;

    virtual void placeLocations() = 0;

    void removeUsed();

private:
    std::vector<std::pair<BWAPI::TilePosition, BWAPI::TilePosition>> permanentTileReservations;
};
