#include "MiningOptimization.h"

#include "DataModel/Serialization.h"
#include "PathOptimizer.h"

#include "DebugFlag_MiningOptimization.h"
#include "Map.h"
#include "Units.h"

#if OUTPUT_STATISTICS
#include "PathStatistics.h"
#endif

namespace MiningOptimization
{
    namespace
    {
        MapData mapData;

        std::unique_ptr<PathOptimizer<GatherArrivalData>> gatherOptimizer;
        std::unique_ptr<PathOptimizer<ReturnArrivalData>> returnOptimizer;

#if OUTPUT_STATISTICS
        PathStatistics gatherPathStatistics;
        PathStatistics returnPathStatistics;
#endif
    }

    void initialize()
    {
        Serialization::setGameParameters(BWAPI::Broodwar->mapHash());
        Serialization::readMapData(mapData);

        gatherOptimizer = std::make_unique<PathOptimizer<GatherArrivalData>>(mapData, mapData.resourceToSerializedGatherPaths);
        returnOptimizer = std::make_unique<PathOptimizer<ReturnArrivalData>>(mapData, mapData.resourceToSerializedReturnPaths);

#if OUTPUT_STATISTICS
        gatherPathStatistics.reset();
        returnPathStatistics.reset();
#endif
    }

    void update()
    {
        gatherOptimizer->clearDeadWorkers();
        returnOptimizer->clearDeadWorkers();

#if OUTPUT_STATISTICS
        gatherOptimizer->updateStatistics(gatherPathStatistics);
        returnOptimizer->updateStatistics(returnPathStatistics);
#endif
    }

    void gameEnd()
    {
#if OUTPUT_STATISTICS
        auto outputStatistics = [](const PathStatistics &pathStatistics)
        {
            if (pathStatistics.count == 0) return (std::string)"No Data";

            std::ostringstream out;
            out << std::fixed << std::setprecision(1)
                << pathStatistics.count << " collections, "
                << pathStatistics.withPath << " with path data"
                << " (" << ((double)pathStatistics.withPath * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withPathFollowedToStableResult << " with path followed to stable"
                << " (" << ((double)pathStatistics.withPathFollowedToStableResult * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withPathFollowedToCompletion << " with path followed to completion"
                << " (" << ((double)pathStatistics.withPathFollowedToCompletion * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withExpectedArrivalFrame << " with expected arrival frame"
                << " (" << ((double)pathStatistics.withExpectedArrivalFrame * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withExpectedActionFrame << " with expected action frame"
                << " (" << ((double)pathStatistics.withExpectedActionFrame * 100.0 / (double)pathStatistics.count) << "%)";
            if (pathStatistics.withTakeover > 0)
            {
                out << ", " << pathStatistics.withTakeover << " takeover collections, "
                    << pathStatistics.patchSwitches << " with patch switch"
                    << " (" << ((double)pathStatistics.patchSwitches * 100.0 / (double)pathStatistics.withTakeover) << "%), "
                    << pathStatistics.withPlannedPatchLock << " with planned patch lock"
                    << " (" << ((double)pathStatistics.withPlannedPatchLock * 100.0 / (double)pathStatistics.withTakeover) << "%)";
                if (pathStatistics.withPlannedPatchLock > 0)
                {
                    out << ", " << pathStatistics.withExpectedPatchLockFrame << " with expected patch lock frame"
                        << " (" << ((double)pathStatistics.withExpectedPatchLockFrame * 100.0 / (double)pathStatistics.withPlannedPatchLock) << "%)";
                }
            }
            return out.str();
        };
        Log::Get() << "Gather path statistics: " << outputStatistics(gatherPathStatistics);
        Log::Get() << "Return path statistics: " << outputStatistics(returnPathStatistics);
#endif
    }

    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        gatherOptimizer->forWorker(worker, depot, resource).optimize();
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        returnOptimizer->forWorker(worker, depot, resource).optimize();
    }

    std::map<MyWorker, std::tuple<Resource, Resource, std::optional<InitialSplitData>>> initialWorkerSplit()
    {
        // Gather the patches and workers
        std::vector<Resource> patches = Map::getMyMain()->mineralPatches();
        std::vector<MyWorker> workers;
        for (auto unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
        {
            workers.emplace_back(std::static_pointer_cast<MyWorkerImpl>(unit));
        }

        // Sort the patches and workers by position to ensure stability between games
        std::sort(patches.begin(), patches.end(), [](const Resource &first, const Resource &second)
        {
            return first->tile < second->tile;
        });
        std::sort(workers.begin(), workers.end(), [](const MyWorker &first, const MyWorker &second)
        {
            return first->lastPosition < second->lastPosition;
        });

        if (workers.size() != 4)
        {
            Log::Get() << "ERROR: Don't have 4 workers when trying to run initial split";
            return {};
        }

        // Generate the PositionAndVelocity for each worker
        std::vector<PositionAndVelocity> workerPositionAndVelocity;
        for (const auto &worker : workers)
        {
            workerPositionAndVelocity.emplace_back(worker);
        }

        // Reference the appropriate initial split data depending on what we know about the opponent's race
        auto getInitialSplitDataForRace = [](BWAPI::Race opponentRace)
            -> std::unordered_map<PositionAndVelocity, std::map<std::pair<TilePosition, TilePosition>, InitialSplitData>>&
        {
            if (opponentRace == BWAPI::Races::Zerg)
            {
                return mapData.startLocationToPatchPairToInitialSplitDataZerg;
            }
            if (opponentRace == BWAPI::Races::Protoss || opponentRace == BWAPI::Races::Terran)
            {
                return mapData.startLocationToPatchPairToInitialSplitDataNotZerg;
            }
            return mapData.startLocationToPatchPairToInitialSplitDataUnknown;
        };
        auto &initialSplitData = getInitialSplitDataForRace(BWAPI::Broodwar->enemy()->getRace());

        // Generate combinations for all four workers to find the best solution
        uint16_t bestSeventhDelivery = UINT16_MAX;
        uint16_t bestEighthDelivery = UINT16_MAX;
        std::array<std::pair<TilePosition, TilePosition>, 4> bestSolution;
        for (auto &[firstWorkerAssignment, firstWorkerResult]
                : initialSplitData[workerPositionAndVelocity[0]])
        {
            for (auto &[secondWorkerAssignment, secondWorkerResult]
                    : initialSplitData[workerPositionAndVelocity[1]])
            {
                if (secondWorkerAssignment.first == firstWorkerAssignment.first) continue;
                if (secondWorkerAssignment.second == firstWorkerAssignment.second) continue;

                for (auto &[thirdWorkerAssignment, thirdWorkerResult]
                        : initialSplitData[workerPositionAndVelocity[2]])
                {
                    if (thirdWorkerAssignment.first == firstWorkerAssignment.first) continue;
                    if (thirdWorkerAssignment.second == firstWorkerAssignment.second) continue;
                    if (thirdWorkerAssignment.first == secondWorkerAssignment.first) continue;
                    if (thirdWorkerAssignment.second == secondWorkerAssignment.second) continue;

                    for (auto &[fourthWorkerAssignment, fourthWorkerResult]
                            : initialSplitData[workerPositionAndVelocity[3]])
                    {
                        if (fourthWorkerAssignment.first == firstWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == firstWorkerAssignment.second) continue;
                        if (fourthWorkerAssignment.first == secondWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == secondWorkerAssignment.second) continue;
                        if (fourthWorkerAssignment.first == thirdWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == thirdWorkerAssignment.second) continue;

                        std::multiset<uint16_t> result = {
                            firstWorkerResult.worstSecondRotationActionFrame(),
                            secondWorkerResult.worstSecondRotationActionFrame(),
                            thirdWorkerResult.worstSecondRotationActionFrame(),
                            fourthWorkerResult.worstSecondRotationActionFrame()
                        };

                        uint16_t seventhDelivery = *(std::prev(result.end(), 2));
                        uint16_t eighthDelivery = *result.rbegin();
                        if (seventhDelivery < bestSeventhDelivery || (seventhDelivery == bestSeventhDelivery && eighthDelivery < bestEighthDelivery))
                        {
                            bestSeventhDelivery = seventhDelivery;
                            bestEighthDelivery = eighthDelivery;
                            bestSolution = {
                                firstWorkerAssignment,
                                secondWorkerAssignment,
                                thirdWorkerAssignment,
                                fourthWorkerAssignment,
                            };
                        }
                    }
                }
            }
        }

        if (bestSeventhDelivery == INT16_MAX) return {};

        std::map<TilePosition, Resource> tileToPatch;
        for (auto &patch : patches)
        {
            tileToPatch[TilePosition::fromBWAPI(patch->tile)] = patch;
        }

        auto assignmentToTuple = [&](size_t workerIndex, std::pair<TilePosition, TilePosition> patchPair)
        {
            return std::make_tuple(
                tileToPatch[patchPair.first],
                tileToPatch[patchPair.second],
                initialSplitData[workerPositionAndVelocity[workerIndex]][patchPair]);
        };

        std::map<MyWorker, std::tuple<Resource, Resource, std::optional<InitialSplitData>>> result;
        for (size_t workerIndex = 0; workerIndex < 4; workerIndex++)
        {
            result.emplace(workers[workerIndex], assignmentToTuple(workerIndex, bestSolution[workerIndex]));
        }
        return result;
    }
}
