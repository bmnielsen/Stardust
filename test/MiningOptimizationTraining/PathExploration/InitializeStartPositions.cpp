#include "ExploreStartPositionsModule.h"

#include <BWAPI/SimulateGatherPathOptions.h>
#include <BWAPI/SimulateGatherPathResult.h>

#include "MiningOptimizationTraining/DataModel/Configuration.h"

namespace MiningOptimizationTraining
{
    namespace
    {
        std::set<BWAPI::Position> getPatchMiningStartPositions(std::set<std::pair<int, int>> &blockedPositions,
                                                               BWAPI::TilePosition depotTile,
                                                               BWAPI::Unit patch)
        {
            std::set<BWAPI::Position> result;

            auto patchTile = patch->getTilePosition();

            // Compute some worker center positions around the patch
            auto patchTopLeft = patch->getPosition() + BWAPI::Position(-32, -16);
            auto topLeft = patchTopLeft + BWAPI::Position(-12, -12);
            auto topRight = patchTopLeft + BWAPI::Position(75, -12);
            auto bottomLeft = patchTopLeft + BWAPI::Position(-12, 43);
            auto bottomRight = patchTopLeft + BWAPI::Position(75, 43);

            // Determine where the patch is in relation to the depot
            bool left = (patchTile.x < depotTile.x);
            bool right = (patchTile.x > (depotTile.x + 2));
            bool hmid = (!left && !right);
            bool top = (patchTile.y < depotTile.y);
            bool bottom = (patchTile.y > (depotTile.y + 2));
            bool vmid = (!top && !bottom);

            int xUpperBound = BWAPI::Broodwar->mapWidth() * 32 - 12;
            int yUpperBound = BWAPI::Broodwar->mapHeight() * 32 - 12;

            auto addPosition = [&](int x, int y)
            {
                if (x < 11 || y < 11 || x > xUpperBound || y > yUpperBound) return;

                auto pos = BWAPI::Position(x, y);
                if (pos.isValid() && !blockedPositions.contains(std::make_pair(x, y)))
                {
                    result.insert(pos);
                }
            };

            // We might mine from the left side of the patch if is it to the right or middle of the depot
            if (right || hmid)
            {
                for (int y = topLeft.y; y <= bottomLeft.y; y++) addPosition(topLeft.x, y);
            }

            // We might mine from the right side of the patch if is it to the left or middle of the depot
            if (left || hmid)
            {
                for (int y = topRight.y; y <= bottomRight.y; y++) addPosition(topRight.x, y);
            }

            // We might mine from the top of the patch if is it to the bottom or middle of the depot
            if (bottom || vmid)
            {
                for (int x = topLeft.x; x < topRight.x; x++) addPosition(x, topLeft.y);
            }

            // We might mine from the bottom of the patch if is it to the top or middle of the depot
            if (top || vmid)
            {
                for (int x = bottomLeft.x; x < bottomRight.x; x++) addPosition(x, bottomRight.y);
            }

            return result;
        }
    }

    template <>
    void ExploreStartPositionsModule<InitializeStartPosition>::initializeStartPositions()
    {
        // At this stage we build a set of possible mining start positions around each patch that are oriented towards the depot

        // Build a set of blocked positions around all patches on the map
        std::set<std::pair<int, int>> blockedPositions;
        auto addBlockedAroundBox = [&blockedPositions](BWAPI::Position topLeft, BWAPI::Position size)
        {
            for (int x = topLeft.x - 11; x < topLeft.x + size.x + 11; x++)
            {
                for (int y = topLeft.y - 11; y < topLeft.y + size.y + 11; y++)
                {
                    blockedPositions.emplace(x, y);
                }
            }
        };
        for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (!unit->getType().isMineralField() && unit->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser) continue;

            auto topLeft = unit->getPosition() - (BWAPI::Position(unit->getType().tileSize()) / 2);
            addBlockedAroundBox(topLeft, BWAPI::Position(unit->getType().tileSize()));

            auto walkPos = BWAPI::WalkPosition(topLeft);
            auto walkSize = BWAPI::WalkPosition(unit->getType().tileSize());
            for (int walkX = walkPos.x - 3; walkX < walkPos.x + walkSize.x + 3; walkX++)
            {
                for (int walkY = walkPos.y - 3; walkY < walkPos.y + walkSize.y + 3; walkY++)
                {
                    auto hereWalk = BWAPI::WalkPosition(walkX, walkY);
                    if (!hereWalk.isValid()) continue;
                    if (BWAPI::Broodwar->isWalkable(hereWalk)) continue;
                    addBlockedAroundBox(BWAPI::Position(hereWalk), BWAPI::Position(8, 8));
                }
            }
        }

        // Empty the map data
        mapData.clear(BWAPI::Broodwar->mapHash());

        // Process each patch
        for (auto base : Map::allBases())
        {
            if (options.oneBase && base->getTilePosition() != options.oneBase) continue;

            for (auto &patch : base->mineralPatches())
            {
                if (options.onePatch && patch->tile != options.onePatch) continue;

                auto patchUnit = patch->getBwapiUnitIfVisible();
                if (!patchUnit)
                {
                    Log::Get() << "ERROR: Could not find unit for patch @ " << patch->tile;
                    return;
                }

                auto patchStartPositions = getPatchMiningStartPositions(blockedPositions, base->getTilePosition(), patchUnit);
                for (auto &startPosition : patchStartPositions)
                {
                    // The heading is always the direction between the center of the worker and the center of the patch
                    auto heading = (int8_t)Geo::BWDirection(patch->center - startPosition);

                    // Generate the exact positions to test
                    auto baseX = ((unsigned int)startPosition.x) << 8;
                    auto baseY = ((unsigned int)startPosition.y) << 8;
                    for (unsigned int subpixelX = 0; subpixelX < 256; subpixelX += (256 / START_POSITIONS_TO_EXPLORE_PER_AXIS))
                    {
                        for (unsigned int subpixelY = 0; subpixelY < 256; subpixelY += (256 / START_POSITIONS_TO_EXPLORE_PER_AXIS))
                        {
                            startPositions.emplace_back(InitializeStartPosition{
                                BWAPI::ExactPosition{baseX + subpixelX, baseY + subpixelY, heading, 0, 0},
                                patchUnit});
                        }
                    }
                }
            }
        }
    }

    template <>
    void ExploreStartPositionsModule<InitializeStartPosition>::explore(InitializeStartPosition &startPosition)
    {
        // At this point we have initialized a very liberal set of positions at a coarse subpixel granularity.
        // Now we simulate from each position purely to collect the set of start positions actually reached from a gather rotation.
        // We do not simulate with resends since we must assume the path may be starting from a position with no path data (for when a worker
        // is assigned to minerals after doing something else, and therefore reaches the patch at an abnormal start position).
        // We do however simulate both with the return action happening at and after arrival since both cases come up depending on order process timer
        // resets.
        // We also simulate with cannons placed to ensure we catch the few start positions that only arise when pathed around cannons.
        // When we do the training with resends, we then add in any additional start positions not seen in this simplified exploration.

        auto simulate = [&](
                bool forceReturn,
                const MiningOptimization::CannonPlacement &cannonPlacement,
                const BWAPI::StateCopy &initialState)
        {
            auto preparedReturnPath = prepareReturnPath(startPosition, initialState);
            if (!preparedReturnPath) return;

            auto returnResult = simWorker->simulateGatherPath(
                    BWAPI::SimulateGatherPathOptions({}, preparedReturnPath->returnPathState)
                        .setForceAction(forceReturn)
                        .setReturnStateAtStartOfNextPath());
            if (!returnResult)
            {
                // This is not necessarily a problem if the start position is behind another mineral field
                // (the 12 oclock base on Benzene has this problem)
                Log::Debug() << "Return path simulation did not succeed; patch @ " << startPosition.patch->getTilePosition()
                           << "; start position " << BWAPI::TilePosition(startPosition.pos.pos());
                return;
            }

            auto gatherResult = simWorker->simulateGatherPath(
                    BWAPI::SimulateGatherPathOptions({}, returnResult->stateAtStartOfNextPath)
                            .setForceAction(true));
            if (!gatherResult)
            {
                Log::Get() << "Gather path simulation did not succeed; patch @ " << startPosition.patch->getTilePosition()
                           << "; start position " << BWAPI::TilePosition(startPosition.pos.pos());
                return;
            }

            auto positionAndVelocity = PositionAndVelocity(gatherResult->nextPathStartPosition);
            auto [_, inserted] =
                    mapData.resourceToReturnPathStartPositions[TilePosition::fromBWAPI(startPosition.patch->getTilePosition())]
                            .insert(positionAndVelocity);
            if (!inserted) return;

            // This is the first time we are seeing this position, so generate the path data
            auto path = Path<ReturnArrivalData>{positionAndVelocity};
            path.populatePositionsToExplore();
            mapData.resourceToReturnPaths[TilePosition::fromBWAPI(startPosition.patch->getTilePosition())][positionAndVelocity] = std::move(path);
        };

        for (auto &[cannonPlacement, state]
                : patchToCannonsToStateCopy[startPosition.patch->getTilePosition()])
        {
            simulate(true, cannonPlacement, *state);
            simulate(false, cannonPlacement, *state);
        }
    }
}
