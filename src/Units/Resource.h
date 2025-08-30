#pragma once

#include "Common.h"
#include "Unit.h"

// The forecast horizon for predicting whether a patch is going to be mined from in future frames
// Note that with our current logic, this must be less than the time one worker takes to mine a patch
#define GATHER_FORECAST_FRAMES 50

class ResourceImpl;

typedef std::shared_ptr<ResourceImpl> Resource;

class ResourceImpl : public std::enable_shared_from_this<ResourceImpl>
{
public:
    int id;
    bool isMinerals;

    BWAPI::TilePosition tile;
    BWAPI::Position center;

    int initialAmount;
    int currentAmount;

    bool seenLastFrame; // For tracking when mineral fields are mined out and removed
    bool destroyed; // For mineral fields, when they are mined out and removed
    Unit refinery;  // For geysers, the refinery unit a player has built on it

    // For mineral fields, the other mineral fields that workers might try to switch to
    std::set<Resource> resourcesInSwitchPatchRange;

    explicit ResourceImpl(BWAPI::Unit unit);

    [[nodiscard]] bool hasMyCompletedRefinery() const;

    [[nodiscard]] BWAPI::Unit getBwapiUnitIfVisible() const;

    [[nodiscard]] int getDistance(const Unit &unit) const;

    [[nodiscard]] int getDistance(BWAPI::Position pos) const;

    [[nodiscard]] int getDistance(const Resource &other) const;

    [[nodiscard]] int getDistance(BWAPI::UnitType type, BWAPI::Position center) const;

    [[nodiscard]] std::array<double, GATHER_FORECAST_FRAMES> &getAllOtherPatchesGatheredProbabilityForecast();

    void update();

private:
    mutable BWAPI::Unit bwapiUnit;

    // Probability of this resource being gathered from for the next GATHER_FORECAST_FRAMES frames, where index 0 is the next frame
    std::array<double, GATHER_FORECAST_FRAMES> gatherProbabilityForecast;

    // Frame when gatherProbabilityForecast was last updated
    int gatherProbabilityForecastUpdated;

    // Similar to the above, the combined probability that all of the other patches in the switch patch range are being gathered
    std::array<double, GATHER_FORECAST_FRAMES> allOtherPatchesGatheredProbabilityForecast;

    // Frame when gatherProbabilityForecast was last updated
    int allOtherPatchesGatheredProbabilityForecastUpdated;

    [[nodiscard]] std::array<double, GATHER_FORECAST_FRAMES> &getGatherProbabilityForecast();
};

std::ostream &operator<<(std::ostream &os, const ResourceImpl &resource);
