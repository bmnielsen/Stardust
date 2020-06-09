#pragma once

#include "Common.h"
#include "Building.h"
#include "Unit.h"
#include "Block.h"
#include "Base.h"

/*
 * Block finding logic:
 *
 * - Start with our starting block, which provides 2 gateways, 2-4 tech buildings and 3-4 cannons
 * - Then progressively try to place smaller and smaller blocks
 *
 * Ideas for future improvement:
 * - Allow cutting off one of the corners of a block where applicable
 * - Try a few strategies for filling our main base and pick the best one
 *
 * Pylon placement logic:
 *
 * - First pylon is always in start block
 * - Subsequent pylons are ordered by their distance to the mineral line
 * - Producer takes first pylon that either provides required building locations or keeps a minimum number of build locations available
 */

namespace BuildingPlacement
{
    struct BuildLocation;

    struct BuildLocationCmp
    {
        bool operator()(const BuildLocation &a, const BuildLocation &b) const;
    };

    typedef std::set<BuildLocation, BuildLocationCmp> BuildLocationSet;

    // Small struct to hold data about a build location
    struct BuildLocation
    {
        Block::Location location;
        int builderFrames;         // Approximately how many frames the builder will take to get to this location
        int framesUntilPowered;    // Approximately how many frames will elapse before this position is powered
        int distanceToExit;        // Approximate ground distance from this build location to the neighbourhood exit
        BuildLocationSet powersMedium;          // For a pylon, what medium build locations would be powered by it
        BuildLocationSet powersLarge;           // For a pylon, what large build locations would be powered by it

        BuildLocation(Block::Location location, int builderFrames, int framesUntilPowered, int distanceToExit)
                : location(location)
                , builderFrames(builderFrames)
                , framesUntilPowered(framesUntilPowered)
                , distanceToExit(distanceToExit) {}
    };

    enum class Neighbourhood
    {
        MainBase
    }; // TODO: Add proxy, etc.

    void initialize();

    void onBuildingQueued(const Building *building);

    void onBuildingCancelled(const Building *building);

    void onUnitCreate(const Unit &unit);

    void onUnitDestroy(const Unit &unit);

    void update();

    std::map<Neighbourhood, std::map<int, BuildLocationSet>> &getBuildLocations();

    BuildLocationSet &availableGeysers();

    int builderFrames(Neighbourhood location, BWAPI::TilePosition tile, BWAPI::UnitType type);

    std::pair<BWAPI::TilePosition, std::vector<BWAPI::TilePosition>> &baseStaticDefenseLocations(Base *base);
}