#include "ExploreStartPositionsModule.h"

#include <BWAPI/SimulateGatherPathOptions.h>
#include <BWAPI/SimulateGatherPathResult.h>

#include "MiningOptimizationTraining/DataModel/Configuration.h"
#include "../DataTransformation/InitialSplitSolver.h"

#define RESULT_FRAME_THRESHOLD 0

namespace MiningOptimizationTraining
{
    namespace
    {
        // Container for keeping track of the relevant limits for the phase of exploration we are in (gather vs. return)
        struct Limits
        {
            int startOfExplorationWindow;
            int endOfExplorationWindow;
            int resends;
        };

        // Checks if two paths are equal, can be called with a vector or range
        bool pathsEqual(auto first, auto second)
        {
            std::input_or_output_iterator auto firstIt = first.begin();
            std::input_or_output_iterator auto secondIt = second.begin();
            while (firstIt != first.end() && secondIt != second.end())
            {
                if (*firstIt != *secondIt) return false;
                firstIt++;
                secondIt++;
            }
            return firstIt == first.end() && secondIt == second.end();
        }
    }

    template <>
    void ExploreStartPositionsModule<ExploreInitialWorkerStartPosition>::initializeStartPositions()
    {
        {
            auto positions = {
                PositionAndVelocity(264, 296, 32, 0, 0),
                PositionAndVelocity(312, 296, -32, 0, 0),
                PositionAndVelocity(240, 296, -16, 0, 0),
                PositionAndVelocity(288, 296, 32, 0, 0)
            };
            auto testBase = Map::baseNear(*positions.begin());
            for (auto patch : testBase->mineralPatches())
            {
                for (auto pos : positions)
                {
                    startPositions.emplace_back(ExploreInitialWorkerStartPosition{
                        BWAPI::ExactPosition{
                            (uint32_t)pos.x * 256,
                            (uint32_t)pos.y * 256,
                            pos.heading,
                            0,
                            0
                        },
                        patch->getBwapiUnitIfVisible()
                    });
                }
            }
            return;
        }

        initialWorkerMapData.startingWorkerPositionToPatchToFirstGatherPath.clear();
        initialWorkerMapData.startingWorkerPositionToPatchesToSecondGatherPaths.clear();
        initialWorkerMapData.startingWorkerPositionToPatchToReturnPaths.clear();

        for (const auto &[spawnPosition, _] : initialWorkerMapData.startingWorkerPositionToOrderProcessTimerReset)
        {
            auto base = Map::baseNear(spawnPosition);
            if (!base)
            {
                Log::Get() << "ERROR: Unexpectedly couldn't get a base for spawn position " << spawnPosition;
                return;
            }

            auto exactPosition = BWAPI::ExactPosition{
                    (uint32_t)spawnPosition.x * 256,
                    (uint32_t)spawnPosition.y * 256,
                    0, 0, 0
            };
            for (int heading = INT8_MIN; heading <= INT8_MAX; heading += 8)
            {
                exactPosition.heading = (int8_t)heading;

                for (auto &patch : base->mineralPatches())
                {
                    auto patchTile = TilePosition::fromBWAPI(patch->tile);
                    if (initialWorkerMapData.startingWorkerPositionToPatchToFirstGatherPath[exactPosition].contains(patchTile))
                    {
                        continue;
                    }

                    startPositions.emplace_back(ExploreInitialWorkerStartPosition{exactPosition, patch->getBwapiUnitIfVisible()});
                }
            }
        }
    }

    template <>
    void ExploreStartPositionsModule<ExploreInitialWorkerStartPosition>::explore(ExploreInitialWorkerStartPosition &startPosition)
    {
        // This is similar to the exploration done for normal gathering, but the scope is different because we know the subpixel positions and the
        // possible order process timer reset values the workers will encounter.

        // Find the base corresponding to this start position
        auto base = Map::baseNear(startPosition.pos.pos());
        if (!base)
        {
            Log::Get() << "ERROR: Unexpectedly couldn't get a base for start position " << startPosition.pos;
            return;
        }

        auto firstPatchPos = TilePosition::fromBWAPI(startPosition.patch->getTilePosition());

        // Initialize a solver for each second patch that we will use to guide which positions we need to explore at each step
        std::map<TilePosition, InitialSplitSolver> solvers;
        {
            auto startPositionAndVelocity = PositionAndVelocity(startPosition.pos.x >> 8,
                                                                startPosition.pos.y >> 8,
                                                                startPosition.pos.heading,
                                                                0,
                                                                0);
            for (const auto &secondPatch : base->mineralPatches())
            {
                auto secondPatchPos = TilePosition::fromBWAPI(secondPatch->tile);
                solvers.try_emplace(secondPatchPos,
                    initialWorkerMapData,
                    startPositionAndVelocity,
                    firstPatchPos,
                    secondPatchPos,
                    BWAPI::Races::Unknown);
            }
        }

        // Prepare the state so that the worker is at this start position
        auto prepareResult = simWorker->prepareGatherPath(
                BWAPI::PrepareGatherPathOptions(startPosition.pos, initialStateWithNoCannons.state));
        if (!prepareResult)
        {
            Log::Get() << "ERROR: Failed to prepare gather path for start position " << startPosition.pos;
            return;
        }

        auto makePathObservations = [&]<typename ObservationType>(
                const Limits &limits,
                InitialWorkerPathNode<ObservationType> &rootNode,
                int startFrame,
                std::unique_ptr<bwgame::state> &initialState,
                BWAPI::Unit gatherPatch = nullptr)
        {
            // Explores the path, recursively going down a level as appropriate
            auto explorePath = [&]( // NOLINT(*-no-recursion)
                    auto &explorePath,
                    int frame,
                    std::unique_ptr<InitialWorkerPathNode<ObservationType>> *nextNode,
                    std::set<int> resendFrames = {},
                    InitialWorkerPathNode<ObservationType> *resendNode = nullptr,
                    std::ranges::subrange<std::vector<BWAPI::ExactPosition>::iterator> noResendPath = {})
            {
                // Simulate the path
                auto simulate = [&](bool forceAction)
                {
                    auto simulateOptions =
                            BWAPI::SimulateGatherPathOptions(resendFrames, initialState)
                                    .setForceAction(forceAction);
                    if (gatherPatch)
                    {
                        simulateOptions.switchToPatch(gatherPatch->getBWIndex());
                    }
                    return simWorker->simulateGatherPath(simulateOptions);
                };

                std::unique_ptr<BWAPI::SimulateGatherPathResult> simulatedPathWithDeliveryAtArrivalResult = nullptr;
                std::unique_ptr<BWAPI::SimulateGatherPathResult> simulatedPathWithDeliveryAfterArrivalResult = nullptr;
                std::function<ObservationType()> createArrivalData;
                if constexpr (std::is_same_v<ObservationType, InitialWorkerGatherArrivalData>)
                {
                    createArrivalData = [&]()
                    {
                        simulatedPathWithDeliveryAtArrivalResult = simulate(true);
                        if (!simulatedPathWithDeliveryAtArrivalResult)
                        {
                            Log::Get() << "ERROR: Path could not be simulated";
                        }

                        return InitialWorkerGatherArrivalData::createFromSimulatedPath(
                            *simulatedPathWithDeliveryAtArrivalResult,
                            gatherPatch ? gatherPatch : startPosition.patch);
                    };
                }
                else
                {
                    createArrivalData = [&]()
                    {
                        simulatedPathWithDeliveryAtArrivalResult = simulate(true);
                        simulatedPathWithDeliveryAfterArrivalResult = simulate(false);
                        if (!simulatedPathWithDeliveryAtArrivalResult || !simulatedPathWithDeliveryAfterArrivalResult)
                        {
                            Log::Get() << "ERROR: Path could not be simulated";
                            simulatedPathWithDeliveryAtArrivalResult = simulate(true);
                            simulatedPathWithDeliveryAfterArrivalResult = simulate(false);
                        }

                        return InitialWorkerReturnArrivalData::createFromSimulatedPath(*simulatedPathWithDeliveryAtArrivalResult,
                                                                                       *simulatedPathWithDeliveryAfterArrivalResult);
                    };
                }

                ObservationType arrivalData = createArrivalData();
                auto &simulatedPath = simulatedPathWithDeliveryAtArrivalResult->positions;

                // If this is a resend node, check if the path changed
                if (resendNode)
                {
                    // If this node is uninitialized, set the node type
                    if (resendNode->type == NodeType::Uninitialized)
                    {
                        if (pathsEqual(noResendPath, simulatedPath))
                        {
                            resendNode->type = NodeType::StableNode;

                            // We can just jump out now, since stable nodes don't need to be explored for additional resends
                            return;
                        }

                        // Set the type of resend node
                        if (resendFrames.size() >= limits.resends)
                        {
                            resendNode->type = NodeType::FinalResendNode;
                        }
                        else if (noResendPath.size() > limits.startOfExplorationWindow && noResendPath.size() < simulatedPath.size())
                        {
                            resendNode->type = NodeType::PoorResendNode;
                        }
                        else
                        {
                            resendNode->type = NodeType::NonfinalResendNode;
                        }
                    }

                    resendNode->arrivalDataAfterResend = arrivalData;

                    // Jump out unless we need to explore more resends from here
                    if (resendNode->type != NodeType::NonfinalResendNode)
                    {
                        return;
                    }
                }

                // Loop through the path and create the nodes
                for (auto positionIt = simulatedPath.begin(); positionIt != simulatedPath.end(); positionIt++)
                {
                    // The arrival delay is the distance to the last position node, which is the arrival position
                    auto arrivalDelay = std::distance(positionIt, simulatedPath.end()) - 1;

                    // We skip adding the last position, since we don't need it for optimization
                    if (arrivalDelay == 0) break;

                    frame++;

                    auto &position = *positionIt;
                    if (*nextNode)
                    {
                        (*nextNode)->pos = position;
                    }
                    else
                    {
                        (*nextNode) = std::make_unique<InitialWorkerPathNode<ObservationType>>(position);
                    }
                    auto &currentNode = **nextNode;

                    // Set the type if we can already determine it here
                    if (arrivalDelay < limits.endOfExplorationWindow)
                    {
                        currentNode.type = NodeType::AfterExplorationWindow;
                    }
                    else if ((frame - startFrame) < BWAPI::Broodwar->getLatencyFrames()
                             || resendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames()))
                    {
                        currentNode.type = NodeType::ResendUnavailable;
                    }

                    // Make the observation on the node
                    arrivalData.arrivalDelay = arrivalDelay;
                    currentNode.arrivalData = arrivalData;

                    // Explore resends on nodes that haven't already been classified
                    if (currentNode.type == NodeType::Uninitialized)
                    {
                        std::set<int> nextResendFrames = resendFrames;
                        nextResendFrames.insert(frame);
                        explorePath(explorePath,
                                    frame,
                                    &currentNode.nextPositionAfterResend,
                                    std::move(nextResendFrames),
                                    &currentNode,
                                    std::ranges::subrange(positionIt + 1, simulatedPath.end()));
                    }

                    nextNode = &currentNode.nextPosition;
                }
            };

            // Start exploring the path from the worker's initial position
            std::unique_ptr<InitialWorkerPathNode<ObservationType>> rootNodePtr(&rootNode);
            explorePath(explorePath,
                        startFrame,
                        &rootNodePtr);
            rootNodePtr.release();
        };

        // Start by simulating the first gather path and gathering all results
        auto &firstGatherRootNode = initialWorkerMapData.startingWorkerPositionToPatchToFirstGatherPath[startPosition.pos]
                .emplace(firstPatchPos, startPosition.pos)
                .first->second;
        makePathObservations({GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END, GATHER_RESEND_LIMIT},
                             firstGatherRootNode,
                             prepareResult->startFrame,
                             prepareResult->state,
                             startPosition.patch);

        // Run the first gather of all of the solvers
        for (auto &[_, solver] : solvers)
        {
            solver.executeFirstGather();
        }

        auto &returnPaths = initialWorkerMapData.startingWorkerPositionToPatchToReturnPaths[startPosition.pos][firstPatchPos];
        auto observeReturn = [&](BWAPI::ExactPosition returnStartPosition, BWAPI::Unit patch)
        {
            // If this position has already been explored, skip it
            if (returnPaths.contains(returnStartPosition)) return;

            // Prepare the return path
            auto returnPrepareResult = simWorker->prepareGatherPath(
                    BWAPI::PrepareGatherPathOptions(returnStartPosition, initialStateWithNoCannons.state)
                    .prepareReturnFrom(patch->getBWIndex()));
            if (!returnPrepareResult)
            {
                Log::Get() << "ERROR: Failed to prepare return path for position " << returnStartPosition;
                return;
            }
            if (returnPrepareResult->startPosition != returnStartPosition)
            {
                Log::Get() << "ERROR: Prepared path is not at correct position; expected " << returnStartPosition
                           << " actual " << returnPrepareResult->startPosition;
                return;
            }

            auto &rootNode = returnPaths.emplace(returnStartPosition, returnStartPosition).first->second;
            makePathObservations({RETURN_EXPLORATION_WINDOW_START, RETURN_EXPLORATION_WINDOW_END, RETURN_RESEND_LIMIT},
                                 rootNode,
                                 returnPrepareResult->startFrame,
                                 returnPrepareResult->state);
        };

        // Explore the first return paths for each start position found by solving the first gather paths
        for (auto &firstGatherPathResult : solvers.begin()->second.firstGatherPathResults)
        {
            observeReturn(firstGatherPathResult.pathResults.begin()->nextPathStartPosition, startPosition.patch);
        }

        // Run the first return of all of the solvers
        for (auto &[_, solver] : solvers)
        {
            solver.executeFirstReturn();
        }

        // After the first rotation we explore switching to each patch
        for (const auto &secondPatchResource : base->mineralPatches())
        {
            auto secondPatch = secondPatchResource->getBwapiUnitIfVisible();
            auto secondPatchPos = TilePosition::fromBWAPI(secondPatchResource->tile);
            auto patchPair = std::make_pair(firstPatchPos, secondPatchPos);
            auto &solver = solvers.at(secondPatchPos);

            // Explore all of the start positions resulting from the first rotation
            for (auto &firstRotationResult : solver.firstReturnPathResults)
            {
                // Prepare the previous return path
                auto returnPrepareResult = simWorker->prepareGatherPath(
                    BWAPI::PrepareGatherPathOptions(firstRotationResult.startPosition, initialStateWithNoCannons.state)
                    .prepareReturnFrom(startPosition.patch->getBWIndex()));
                if (!returnPrepareResult)
                {
                    Log::Get() << "ERROR: Failed to prepare return path for position " << firstRotationResult.startPosition;
                    return;
                }
                if (returnPrepareResult->startPosition != firstRotationResult.startPosition)
                {
                    Log::Get() << "ERROR: Prepared path is not at correct position; expected " << firstRotationResult.startPosition
                            << " actual " << returnPrepareResult->startPosition;
                    return;
                }

                // The resends in the solver are relative to a normal game, so realign them to match our simulation
                std::set<int> resends;
                for (auto resend : firstRotationResult.resends)
                {
                    resends.insert(returnPrepareResult->startFrame + (resend - firstRotationResult.startFrame) + 1);
                }

                auto observeSecondGatherPath = [&](bool forceAction, BWAPI::ExactPosition pathStartPosition)
                {
                    auto gatherPrepareResult = simWorker->simulateGatherPath(
                            BWAPI::SimulateGatherPathOptions(resends, returnPrepareResult->state)
                                    .setForceAction(forceAction)
                                    .setReturnStateAtStartOfNextPath());
                    if (!gatherPrepareResult)
                    {
                        Log::Get() << "ERROR: Path could not be simulated";
                        return;
                    }
                    if (gatherPrepareResult->nextPathStartPosition != pathStartPosition)
                    {
                        Log::Get() << "ERROR: Path does not have proper start position";
                        return;
                    }

                    auto &secondGatherRootNode =
                            initialWorkerMapData.startingWorkerPositionToPatchesToSecondGatherPaths
                            [startPosition.pos]
                            [patchPair]
                            .emplace(pathStartPosition, pathStartPosition)
                            .first->second;
                    makePathObservations({GATHER_EXPLORATION_WINDOW_START, GATHER_EXPLORATION_WINDOW_END, GATHER_RESEND_LIMIT},
                                         secondGatherRootNode,
                                         gatherPrepareResult->actionFrame,
                                         gatherPrepareResult->stateAtStartOfNextPath,
                                         (secondPatch == startPosition.patch) ? nullptr : secondPatch);
                };

                // Simulate either once or twice depending on whether we have multiple possible outcomes
                bool deliveryAtArrival = false;
                bool deliveryAfterArrival = false;
                for (auto &result : firstRotationResult.pathResults)
                {
                    if (result.actionAtArrival)
                    {
                        if (!deliveryAtArrival)
                        {
                            observeSecondGatherPath(true, result.nextPathStartPosition);
                        }
                        deliveryAtArrival = true;
                    }
                    else
                    {
                        if (!deliveryAfterArrival)
                        {
                            observeSecondGatherPath(false, result.nextPathStartPosition);
                        }
                        deliveryAfterArrival = true;
                    }
                }
            }

            // Run the solver to get the best second gather paths
            solver.executeSecondGather();

            // Explore every second return path that comes up in the solver
            for (auto &[_, v1] : solver.secondGatherPathResults)
            {
                for (auto &v2 : v1)
                {
                    for (auto &result : v2)
                    {
                        observeReturn(result.pathResults.begin()->nextPathStartPosition, secondPatch);
                    }
                }
            }
        }
    }
}
