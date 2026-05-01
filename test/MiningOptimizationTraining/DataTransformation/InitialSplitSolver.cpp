#include "InitialSplitSolver.h"

#include "OrderProcessTimer.h"

/*
 * The initial split solver takes the trained initial split data (all of the likely useful paths on the first two rotations from all starting position
 * headings) and finds the best solution for a given combination of patches.
 *
 * The solver has the following constraints:
 * - The first gather always has a resend taking effect after the initial order process timer reset
 * - The first gather will always have mining finish before the second order process timer reset
 * - If the first return occurs before the second order process timer reset, the second gather will have a resend taking effect after the second
 *   order process timer reset
 *
 * The result of this is that the only uncertainties are during the first return (if the worker cannot deliver resources before the second order
 * process timer reset) and the second return (if the worker cannot deliver resources before the third order process timer reset).
 *
 * For the second order process timer reset at frame 158, we know the values the order process timer can reset to, since the visible unit list cannot
 * change during this time unless the opponent does something very strange (e.g. a Zerg that morphs a drone and cancels it).
 *
 * For the third order process timer reset at frame 308, we expect new units to have been created in the meantime, so we don't try to predict
 * the reset values. As we try to plan to return resources before this reset anyway, this is considered acceptable. In the future we could gather
 * data on reset values given the opponent behaves normally.
 */
namespace MiningOptimizationTraining
{
    namespace
    {
        std::set<int> allOrderProcessTimerResetValues = {0, 1, 2, 3, 4, 5, 6, 7};

        template <typename ObservationType>
        struct QueuedNode
        {
            const InitialWorkerPathNode<ObservationType> *node;
            std::set<int> resends;
            int frame;
        };

        struct PathResult
        {
            std::vector<InitialWorkerComputePathResult> pathResults;
            std::set<int> resends;
            const PathResult* previousPathResult;

            [[nodiscard]] int worstActionFrame() const
            {
                int worst = 0;
                for (const auto &result : pathResults)
                {
                    worst = std::max(worst, result.actionFrame);
                }
                return worst;
            }

            [[nodiscard]] int worstActionFrameAndDelay() const
            {
                int worst = 0;
                for (const auto &result : pathResults)
                {
                    worst = std::max(worst, result.actionFrame + result.postActionDelay);
                }
                return worst;
            }

            [[nodiscard]] bool equivalentTo(const PathResult &other) const
            {
                if (pathResults.size() != other.pathResults.size()) return false;
                for (const auto &pathResult : pathResults)
                {
                    bool found = false;
                    for (const auto &otherPathResult : other.pathResults)
                    {
                        if (pathResult.actionFrame == otherPathResult.actionFrame ||
                            pathResult.postActionDelay == otherPathResult.postActionDelay ||
                            pathResult.nextPathStartPosition == otherPathResult.nextPathStartPosition)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found) return false;
                }
                return true;
            }

            [[nodiscard]] MiningOptimization::InitialSplitRotation toInitialSplitRotation() const
            {
                EXPECT_NE(nullptr, previousPathResult) << "Trying to make an initial split rotation from a gather path result";
                EXPECT_EQ(1, previousPathResult->pathResults.size()) << "Gather path has multiple results";

                std::set<uint16_t> combinedResends;
                for (const auto resend : previousPathResult->resends) combinedResends.insert(resend);
                for (const auto resend : resends) combinedResends.insert(resend);

                int returnArrivalFrame = -1;
                std::set<uint16_t> returnActionFrames;
                for (const auto &returnPathResult : pathResults)
                {
                    EXPECT_TRUE((returnArrivalFrame == -1) || (returnArrivalFrame == returnPathResult.arrivalFrame))
                        << "Inconsistent arrival frames on return paths";
                    returnArrivalFrame = returnPathResult.arrivalFrame;
                    returnActionFrames.insert(returnPathResult.actionFrame);
                }

                return MiningOptimization::InitialSplitRotation
                {
                    std::move(combinedResends),
                    (uint16_t)previousPathResult->pathResults.begin()->arrivalFrame,
                    (uint16_t)previousPathResult->pathResults.begin()->actionFrame,
                    (uint16_t)returnArrivalFrame,
                    std::move(returnActionFrames)
                };
            }
        };
    }

    std::optional<MiningOptimization::InitialSplitData> InitialSplitSolver::execute()
    {
        // This method creates the best plan for the given combination of patches for this worker
        // The best plan is the one that gets the earliest second delivery
        // If there is variance in the possible order process timer resets, we use the worst case
        // TODO: Test if it is better to use the average case

        auto exactStartPosition = BWAPI::ExactPosition((uint32_t)startPosition.x * 256,
                                                       (uint32_t)startPosition.y * 256,
                                                       startPosition.heading,
                                                       0,
                                                       0);

        EXPECT_TRUE(mapData.startingWorkerPositionToOrderProcessTimerReset.contains(startPosition))
                            << "No order process timer reset value data for start position " << startPosition;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToFirstGatherPath.contains(exactStartPosition))
                            << "No first gather path data for start position " << startPosition;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToFirstGatherPath.at(exactStartPosition).contains(firstPatch))
                            << "No first gather path data for start position " << startPosition << " and patch " << firstPatch;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToReturnPaths.contains(exactStartPosition))
                            << "No return path data for start position " << startPosition;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToReturnPaths.at(exactStartPosition).contains(firstPatch))
                            << "No return path data for start position " << startPosition << " and patch " << firstPatch;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToReturnPaths.at(exactStartPosition).contains(secondPatch))
                            << "No return path data for start position " << startPosition << " and patch " << secondPatch;

        auto patchPair = std::make_pair(firstPatch, secondPatch);
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchesToSecondGatherPaths.contains(exactStartPosition))
                    << "No second gather path data for start position " << startPosition;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchesToSecondGatherPaths.at(exactStartPosition).contains(patchPair))
                    << "No second gather path data for start position " << startPosition << " and patch " << firstPatch << " and " << secondPatch;

        // Determine the possible order process timer reset values
        std::set<int> orderProcessTimerResetValues;
        {
            for (const auto &resetData : mapData.startingWorkerPositionToOrderProcessTimerReset.at(startPosition))
            {
                if (resetData.opponentIsZerg && (opponentRace == BWAPI::Races::Protoss || opponentRace == BWAPI::Races::Terran)) continue;
                if (!resetData.opponentIsZerg && (opponentRace == BWAPI::Races::Zerg)) continue;

                orderProcessTimerResetValues.insert(resetData.value);
            }
        }

        auto getPaths = [&]<typename ObservationType>(
                std::vector<PathResult> &results,
                int startFrame,
                std::optional<int> requireResendAfterFrame,
                std::optional<int> requireMiningEndBeforeFrame,
                bool pathStartsWithGatherCommand,
                const InitialWorkerPathNode<ObservationType> &rootNode,
                const PathResult* previousPathResult)
        {
            std::deque<QueuedNode<ObservationType>> nodeQueue;

            auto addResult = [&](const ObservationType &arrivalData, std::set<int> resends)
            {
                auto pathResults = arrivalData.computePathResult(
                        startFrame,
                        pathStartsWithGatherCommand,
                        resends.empty() ? std::nullopt : static_cast<std::optional<int>>(*resends.rbegin()),
                        (startFrame > 158) ? allOrderProcessTimerResetValues : orderProcessTimerResetValues);

                bool validResult = true;

                if (requireMiningEndBeforeFrame)
                {
                    for (const auto &pathResult : pathResults)
                    {
                        if ((pathResult.actionFrame + 84) >= *requireMiningEndBeforeFrame)
                        {
                            validResult = false;
                            break;
                        }
                    }
                }

                if (validResult) results.emplace_back(std::move(pathResults), std::move(resends), previousPathResult);
            };

            // Add the no-resend result
            if (!requireResendAfterFrame || startFrame > (*requireResendAfterFrame))
            {
                addResult(rootNode.arrivalData, {});
            }

            // Push the root node onto the queue and start iterating over the paths
            nodeQueue.emplace_back(
                    &rootNode,
                    std::set<int>{},
                    startFrame);

            while (!nodeQueue.empty())
            {
                auto &node = *nodeQueue.begin();

                // Follow the nodes, register resend results, and queue resend nodes
                auto current = node.node;
                int frame = node.frame;
                while (current)
                {
                    if (frame >= (startFrame + BWAPI::Broodwar->getLatencyFrames() + 2) &&
                        (!requireResendAfterFrame || frame > (*requireResendAfterFrame)) &&
                        current->type != NodeType::PoorResendNode &&
                        !node.resends.contains(frame - BWAPI::Broodwar->getLatencyFrames()) &&
                        !OrderProcessTimer::isResetFrame(frame + 3))
                    {
                        std::set<int> resends = node.resends;
                        resends.insert(frame);

                        if (current->type == NodeType::StableNode)
                        {
                            addResult(current->arrivalData, resends);
                        }
                        else if (current->arrivalDataAfterResend)
                        {
                            addResult(*current->arrivalDataAfterResend, resends);
                            if (current->nextPositionAfterResend)
                            {
                                nodeQueue.emplace_back(current->nextPositionAfterResend.get(), resends, frame + 1);
                            }
                        }
                    }

                    current = current->nextPosition.get();
                    frame++;
                }

                nodeQueue.pop_front();
            }
        };

        auto getBestPaths = [](std::vector<PathResult> &pathResults)
        {
            std::vector<std::pair<const PathResult*, int>> resultsAndScore;

            // First score each result
            int bestScore = INT_MAX;
            for (const auto &pathResult : pathResults)
            {
                int score = pathResult.worstActionFrameAndDelay();
                resultsAndScore.emplace_back(&pathResult, score);
                bestScore = std::min(bestScore, score);
            }

            // Then return all the unique best results (we don't care about results that have the same next path start positions)
            std::vector<const PathResult*> bestResults;
            for (const auto &[result, score] : resultsAndScore)
            {
                if (score > bestScore) continue;

                bool matchesExisting = false;
                for (const auto &existing : bestResults)
                {
                    if (result->equivalentTo(*existing))
                    {
                        matchesExisting = true;
                        break;
                    }
                }

                if (!matchesExisting) bestResults.emplace_back(result);
            }

            return bestResults;
        };

        // Get the first gather path results
        auto &firstGatherRootNode = mapData
                .startingWorkerPositionToPatchToFirstGatherPath
                .at(exactStartPosition)
                .at(firstPatch);
        std::vector<PathResult> firstGatherPathResults;
        getPaths(firstGatherPathResults,
                 0,
                 8,
                 158,
                 true,
                 firstGatherRootNode,
                 nullptr);
        if (firstGatherPathResults.empty()) return std::nullopt;

        auto &returnNodes = mapData
                .startingWorkerPositionToPatchToReturnPaths
                .at(exactStartPosition)
                .at(firstPatch);

        // Plan first resend path for each best result
        std::vector<PathResult> firstReturnPathResults;
        for (const auto &firstGatherPathResult : getBestPaths(firstGatherPathResults))
        {
            EXPECT_EQ(1, firstGatherPathResult->pathResults.size()) << "First gather has more than one result";
            auto &pathResult = *firstGatherPathResult->pathResults.begin();

            auto firstReturnRootNodeIt = returnNodes.find(pathResult.nextPathStartPosition);
            if (firstReturnRootNodeIt == returnNodes.end())
            {
                Log::Get() << "WARNING: Return nodes don't contain nextPathStartPosition from first gather " << pathResult.nextPathStartPosition;
                continue;
            }

            getPaths(firstReturnPathResults,
                     pathResult.actionFrame + 85,
                     std::nullopt,
                     std::nullopt,
                     false,
                     firstReturnRootNodeIt->second,
                     firstGatherPathResult);
        }
        if (firstReturnPathResults.empty()) return std::nullopt;

        // We now have a set of best results for the first rotation, which may have several action frames depending on our knowledge of the order
        // process timer resets
        // For each of these possibilities, plan the best second rotation

        auto &secondGatherRootNodes = mapData
                .startingWorkerPositionToPatchesToSecondGatherPaths
                .at(exactStartPosition)
                .at(patchPair);

        std::vector<std::pair<const PathResult*, std::map<int, std::pair<PathResult, PathResult>>>> firstRotationAndActionFrameToSecondRotation;
        for (const auto &firstReturnPathResult : getBestPaths(firstReturnPathResults))
        {
            std::map<int, std::pair<PathResult, PathResult>> actionFrameToSecondRotation;
            for (const auto &returnPathResult : firstReturnPathResult->pathResults)
            {
                auto secondGatherRootNodeIt = secondGatherRootNodes.find(returnPathResult.nextPathStartPosition);
                if (secondGatherRootNodeIt == secondGatherRootNodes.end())
                {
                    Log::Get() << "WARNING: Second gather nodes don't contain nextPathStartPosition from first return "
                               << returnPathResult.nextPathStartPosition;
                    break;
                }

                std::vector<PathResult> secondGatherPathResults;
                getPaths(secondGatherPathResults,
                         returnPathResult.actionFrame,
                         158,
                         std::nullopt,
                         firstPatch != secondPatch,
                         secondGatherRootNodeIt->second,
                         nullptr);
                if (secondGatherPathResults.empty()) break;

                std::vector<PathResult> secondReturnPathResults;
                for (const auto &secondGatherPathResult : getBestPaths(secondGatherPathResults))
                {
                    EXPECT_EQ(1, secondGatherPathResult->pathResults.size()) << "Second gather has more than one result";
                    auto &gatherPathResult = *secondGatherPathResult->pathResults.begin();

                    auto secondReturnRootNodeIt = returnNodes.find(gatherPathResult.nextPathStartPosition);
                    if (secondReturnRootNodeIt == returnNodes.end())
                    {
                        Log::Get() << "WARNING: Return nodes don't contain nextPathStartPosition from second gather "
                                   << gatherPathResult.nextPathStartPosition;
                        continue;
                    }

                    getPaths(secondReturnPathResults,
                             gatherPathResult.actionFrame + 85,
                             std::nullopt,
                             std::nullopt,
                             false,
                             secondReturnRootNodeIt->second,
                             secondGatherPathResult);
                }
                if (secondReturnPathResults.empty()) break;

                // Just pick the first of the best second return paths
                // TODO: Figure out if there is a more intelligent way to select this
                auto bestSecondRotation = *getBestPaths(secondReturnPathResults).begin();

                // Move the results since they will go out of scope momentarily
                actionFrameToSecondRotation.try_emplace(returnPathResult.actionFrame,
                                                        std::move(*bestSecondRotation->previousPathResult),
                                                        std::move(*bestSecondRotation));
            }
            if (actionFrameToSecondRotation.size() != firstReturnPathResult->pathResults.size()) continue;

            firstRotationAndActionFrameToSecondRotation.emplace_back(firstReturnPathResult, std::move(actionFrameToSecondRotation));
        }
        if (firstRotationAndActionFrameToSecondRotation.empty()) return std::nullopt;

        // Find the best solution
        int bestSolutionFrame = INT_MAX;
        std::pair<const PathResult*, std::map<int, std::pair<PathResult, PathResult>>> *bestSolution = nullptr;
        for (auto &solution : firstRotationAndActionFrameToSecondRotation)
        {
            int worstSolution = 0;
            for (const auto &[_, secondRotation] : solution.second)
            {
                worstSolution = std::max(worstSolution, secondRotation.second.worstActionFrame());
            }
            if (worstSolution < bestSolutionFrame)
            {
                bestSolutionFrame = worstSolution;
                bestSolution = &solution;
            }
        }

        MiningOptimization::InitialSplitData result{
            bestSolution->first->toInitialSplitRotation()
        };
        for (auto &[frame, solution] : bestSolution->second)
        {
            solution.second.previousPathResult = &solution.first; // Needed since we've moved stuff around since setting the original pointer
            result.firstRotationDeliveryToSecondRotation[frame] = solution.second.toInitialSplitRotation();
        }

        return result;
    }
}
