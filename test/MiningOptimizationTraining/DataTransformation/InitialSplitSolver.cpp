#include "InitialSplitSolver.h"

#include <ranges>

/*
 * The initial split solver takes the trained initial split data (all of the likely useful paths on the first two rotations from all starting position
 * headings) and finds the best solution for a given combination of patches.
 *
 * The solver has the following constraints to limit the exploration space:
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
 *
 * The solver is implemented such that each mining phase can be run piecewise, so that we can use the solver's logic to guide which positions we need
 * to explore during the path exploration.
 */

#define FIRST_GATHER_TOLERANCE 3
#define FIRST_RETURN_TOLERANCE 3
#define SECOND_GATHER_TOLERANCE 3

namespace MiningOptimizationTraining
{
    namespace
    {
        // Used as the order process timer reset values when we don't know what result we will get
        std::map<int, unsigned int> unknownOrderProcessTimerResetValues = {
            {0, 1},
            {1, 1},
            {2, 1},
            {3, 1},
            {4, 1},
            {5, 1},
            {6, 1},
            {7, 1}
        };

        // On the first return, we avoid action on frame 153 and 157 to avoid complications due to order process timer resets
        // For 153, if we switch to a different patch, the order process timer reset will happen during the gather command processing
        // For 157, the order process timer reset happens between delivery and initializing the move to the patch
        std::set<int> avoidActionFramesFirstReturn = {153, 157};

        // On the second return, we don't switch patches again so we only have to consider the second case from above
        std::set<int> avoidActionFramesSecondReturn = {307};

        template <typename ObservationType>
        struct QueuedNode
        {
            const InitialWorkerPathNode<ObservationType> *node;
            std::set<int> resends;
            int frame;
        };

        template <typename ObservationType>
        void getPaths(
                std::vector<InitialSplitSolver::PathResult> &results,
                int startFrame,
                std::optional<int> requireResendAfterFrame,
                std::optional<int> requireMiningEndBeforeFrame,
                std::optional<std::set<int>*> avoidActionAtFrames,
                bool pathStartsWithGatherCommand,
                std::optional<int> considerWaitingAtStartToResendAtFrame,
                const std::map<int, unsigned int> &orderProcessTimerResetValues,
                const InitialWorkerPathNode<ObservationType> &rootNode,
                const InitialSplitSolver::PathResult* previousPathResult)
        {
            std::deque<QueuedNode<ObservationType>> nodeQueue;

            auto addResult = [&](const ObservationType &arrivalData, const std::set<int> &resends, int frameDelay = 0)
            {
                // Don't add a gather path that isn't facing the patch
                if constexpr (std::is_same_v<ObservationType, InitialWorkerGatherArrivalData>)
                {
                    if (!arrivalData.facingPatch) return;
                }

                auto pathResults = arrivalData.computePathResult(
                        startFrame + frameDelay,
                        pathStartsWithGatherCommand,
                        resends.empty() ? std::nullopt : static_cast<std::optional<int>>(*resends.rbegin()),
                        std::views::keys(orderProcessTimerResetValues));

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

                if (avoidActionAtFrames)
                {
                    for (const auto &pathResult : pathResults)
                    {
                        if ((*avoidActionAtFrames)->contains(pathResult.actionFrame))
                        {
                            validResult = false;
                            break;
                        }
                    }
                }

                if (validResult)
                {
                    results.emplace_back(rootNode.pos, startFrame + frameDelay, frameDelay, std::move(pathResults), resends, previousPathResult);
                }
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
                    if (frame >= (startFrame + (3 * (pathStartsWithGatherCommand ? 2 : 1)) + 2) &&
                        (!requireResendAfterFrame || frame > (*requireResendAfterFrame)) &&
                        current->type != NodeType::PoorResendNode &&
                        !node.resends.contains(frame - 3))
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

                            if (considerWaitingAtStartToResendAtFrame && current->arrivalDataAfterResend->arrivalDelay == 10 &&
                                frame < *considerWaitingAtStartToResendAtFrame)
                            {
                                int frameDelay = *considerWaitingAtStartToResendAtFrame - frame;
                                if (frameDelay < 4)
                                {
                                    addResult(*current->arrivalDataAfterResend, {frame + frameDelay}, frameDelay);
                                }
                            }

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
        }

        void filterBestPaths(std::vector<InitialSplitSolver::PathResult> &pathResults,
                             int tolerance)
        {
            if (pathResults.empty()) return;

            // First score each result
            std::vector<std::pair<const InitialSplitSolver::PathResult*, int>> resultsAndScore;
            int bestScore = INT_MAX;
            for (const auto &pathResult : pathResults)
            {
                int score = pathResult.worstActionFrameAndDelay();
                resultsAndScore.emplace_back(&pathResult, score);
                bestScore = std::min(bestScore, score);
            }

            // Now move all of the best results into the result vector
            std::vector<InitialSplitSolver::PathResult> bestResults;
            for (auto &[result, score] : resultsAndScore)
            {
                if (score > (bestScore + tolerance)) continue;

                bool matchesExisting = false;
                for (const auto &existing : bestResults)
                {
                    if (result->equivalentTo(existing))
                    {
                        matchesExisting = true;
                        break;
                    }
                }

                if (!matchesExisting)
                {
                    // if (result->frameDelay > 0)
                    // {
                    //     std::cout << "Got a result with frame delay: delay " << result->frameDelay << "; resend at " << *result->resends.begin()
                    //               << "; results " << result->pathResults.size() << "; arrival frame " << result->pathResults.begin()->arrivalFrame
                    //               << "; action frame " << result->pathResults.begin()->actionFrame
                    //               << "; post action delay " << result->pathResults.begin()->postActionDelay
                    //     << std::endl;
                    //
                    //     for (auto &[otherResult, otherScore] : resultsAndScore)
                    //     {
                    //         if (otherResult->resends.size() == 1 && otherResult->frameDelay == 0 &&
                    //             *otherResult->resends.begin() == (*result->resends.begin() - result->frameDelay) &&
                    //             result->previousPathResult == otherResult->previousPathResult)
                    //         {
                    //             std::cout << "Corresponding non-delay result: delay " << otherResult->frameDelay << "; resend at " << *otherResult->resends.begin()
                    //                       << "; results " << otherResult->pathResults.size() << "; arrival frame " << otherResult->pathResults.begin()->arrivalFrame
                    //                       << "; action frame " << otherResult->pathResults.begin()->actionFrame
                    //                       << "; post action delay " << otherResult->pathResults.begin()->postActionDelay
                    //             << std::endl;
                    //         }
                    //     }
                    // }

                    bestResults.emplace_back(std::move(*result));
                }
            }

            pathResults = std::move(bestResults);
        }
    }

    std::optional<MiningOptimization::InitialSplitData> InitialSplitSolver::execute()
    {
        executeFirstGather();
        executeFirstReturn();
        executeSecondGather();
        executeSecondReturn();

        if (secondReturnPathResults.empty()) return std::nullopt;

        // Find the best solution
        int bestSolutionFrame = INT_MAX;
        std::pair<const PathResult*, std::map<int, std::pair<PathResult, PathResult>>> *bestSolution = nullptr;
        for (auto &solution : secondReturnPathResults)
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

        MiningOptimization::InitialSplitData result;
        result.firstRotation = bestSolution->first->toInitialSplitRotation(&orderProcessTimerResetValues);
        for (auto &[frame, solution] : bestSolution->second)
        {
            solution.second.previousPathResult = &solution.first; // Needed since we've moved stuff around since setting the original pointer
            result.firstRotationDeliveryToSecondRotation[frame] = solution.second.toInitialSplitRotation(nullptr);
        }

        return result;
    }

    void InitialSplitSolver::executeFirstGather()
    {
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToFirstGatherPath.contains(exactStartPosition))
                            << "No first gather path data for start position " << startPosition;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToFirstGatherPath.at(exactStartPosition).contains(firstPatch))
                            << "No first gather path data for start position " << startPosition << " and patch " << firstPatch;

        auto &firstGatherRootNode = mapData
                .startingWorkerPositionToPatchToFirstGatherPath
                .at(exactStartPosition)
                .at(firstPatch);
        getPaths(firstGatherPathResults,
                 1,
                 8,
                 158,
                 std::nullopt,
                 true,
                 std::nullopt,
                 orderProcessTimerResetValues,
                 firstGatherRootNode,
                 nullptr);

        filterBestPaths(firstGatherPathResults, FIRST_GATHER_TOLERANCE);
    }

    void InitialSplitSolver::executeFirstReturn()
    {
        if (firstGatherPathResults.empty()) return;

        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToReturnPaths.contains(exactStartPosition))
                            << "No return path data for start position " << startPosition;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToReturnPaths.at(exactStartPosition).contains(firstPatch))
                            << "No return path data for start position " << startPosition << " and patch " << firstPatch;

        auto &returnNodes = mapData
                        .startingWorkerPositionToPatchToReturnPaths
                        .at(exactStartPosition)
                        .at(firstPatch);

        for (const auto &firstGatherPathResult : firstGatherPathResults)
        {
            EXPECT_EQ(1, firstGatherPathResult.pathResults.size()) << "First gather has more than one result";
            auto &pathResult = *firstGatherPathResult.pathResults.begin();

            auto firstReturnRootNodeIt = returnNodes.find(pathResult.nextPathStartPosition);
            if (firstReturnRootNodeIt == returnNodes.end())
            {
                Log::Get() << "WARNING: " << startPosition << "-" << firstPatch << "-" << secondPatch
                           << ": Return nodes don't contain nextPathStartPosition from first gather " << pathResult.nextPathStartPosition;
                continue;
            }

            getPaths(firstReturnPathResults,
                     pathResult.actionFrame + 85,
                     std::nullopt,
                     std::nullopt,
                     &avoidActionFramesFirstReturn,
                     false,
                     157,
                     orderProcessTimerResetValues,
                     firstReturnRootNodeIt->second,
                     &firstGatherPathResult);
        }

        filterBestPaths(firstReturnPathResults, FIRST_RETURN_TOLERANCE);
    }

    void InitialSplitSolver::executeSecondGather()
    {
        if (firstReturnPathResults.empty()) return;

        auto patchPair = std::make_pair(firstPatch, secondPatch);
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchesToSecondGatherPaths.contains(exactStartPosition))
                    << "No second gather path data for start position " << startPosition;
        EXPECT_TRUE(mapData.startingWorkerPositionToPatchesToSecondGatherPaths.at(exactStartPosition).contains(patchPair))
                    << "No second gather path data for start position " << startPosition << " and patch " << firstPatch << " and " << secondPatch;

        auto &secondGatherRootNodes = mapData
                .startingWorkerPositionToPatchesToSecondGatherPaths
                .at(exactStartPosition)
                .at(patchPair);

        for (auto &firstReturnPathResult : firstReturnPathResults)
        {
            std::vector<std::vector<PathResult>> resultsForThisFirstRotation;
            for (auto &returnPathResult : firstReturnPathResult.pathResults)
            {
                auto secondGatherRootNodeIt = secondGatherRootNodes.find(returnPathResult.nextPathStartPosition);
                if (secondGatherRootNodeIt == secondGatherRootNodes.end())
                {
                    Log::Get() << "WARNING: " << startPosition << "-" << firstPatch << "-" << secondPatch
                               << ": Second gather nodes don't contain nextPathStartPosition from first return "
                               << returnPathResult.nextPathStartPosition;
                    break;
                }

                std::vector<PathResult> currentSecondGatherPathResults;
                getPaths(currentSecondGatherPathResults,
                         returnPathResult.actionFrame + 1,
                         158,
                         308,
                         std::nullopt,
                         firstPatch != secondPatch,
                         std::nullopt,
                         (returnPathResult.actionFrame > 158) ? unknownOrderProcessTimerResetValues : orderProcessTimerResetValues,
                         secondGatherRootNodeIt->second,
                         nullptr);
                filterBestPaths(currentSecondGatherPathResults, SECOND_GATHER_TOLERANCE);

                resultsForThisFirstRotation.emplace_back(std::move(currentSecondGatherPathResults));
            }

            secondGatherPathResults.emplace(&firstReturnPathResult, std::move(resultsForThisFirstRotation));
        }
    }

    void InitialSplitSolver::executeSecondReturn()
    {
        if (secondGatherPathResults.empty()) return;

        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToReturnPaths.at(exactStartPosition).contains(secondPatch))
                            << "No return path data for start position " << startPosition << " and patch " << secondPatch;

        auto &returnNodes = mapData
                        .startingWorkerPositionToPatchToReturnPaths
                        .at(exactStartPosition)
                        .at(firstPatch);

        for (auto &[firstReturnPathResult, resultsForThisFirstReturnPath] : secondGatherPathResults)
        {
            std::map<int, std::pair<PathResult, PathResult>> actionFrameToSecondRotation;
            for (size_t pathIndex = 0; pathIndex < resultsForThisFirstReturnPath.size(); pathIndex++)
            {
                auto &returnPathResult = firstReturnPathResult->pathResults[pathIndex];
                auto &currentSecondGatherPathResults = resultsForThisFirstReturnPath[pathIndex];

                std::vector<PathResult> currentSecondReturnPathResults;
                for (const auto &secondGatherPathResult : currentSecondGatherPathResults)
                {
                    EXPECT_EQ(1, secondGatherPathResult.pathResults.size()) << "Second gather has more than one result";
                    auto &gatherPathResult = *secondGatherPathResult.pathResults.begin();

                    auto secondReturnRootNodeIt = returnNodes.find(gatherPathResult.nextPathStartPosition);
                    if (secondReturnRootNodeIt == returnNodes.end())
                    {
                        Log::Get() << "WARNING: " << startPosition << "-" << firstPatch << "-" << secondPatch
                                   << ": Return nodes don't contain nextPathStartPosition from second gather "
                                   << gatherPathResult.nextPathStartPosition;
                        continue;
                    }

                    getPaths(currentSecondReturnPathResults,
                             gatherPathResult.actionFrame + 85,
                             std::nullopt,
                             std::nullopt,
                             &avoidActionFramesSecondReturn,
                             false,
                             std::nullopt,
                             ((gatherPathResult.actionFrame + 85) > 158) ? unknownOrderProcessTimerResetValues : orderProcessTimerResetValues,
                             secondReturnRootNodeIt->second,
                             &secondGatherPathResult);
                }

                filterBestPaths(currentSecondReturnPathResults, 0);

                if (currentSecondReturnPathResults.empty()) break;

                // Just pick the first of the best second return paths
                // TODO: Figure out if there is a more intelligent way to select this
                auto &bestSecondRotation = *currentSecondReturnPathResults.begin();

                // Move the results since they will go out of scope momentarily
                actionFrameToSecondRotation.try_emplace(returnPathResult.actionFrame,
                                                        std::move(*bestSecondRotation.previousPathResult),
                                                        std::move(bestSecondRotation));
            }
            if (actionFrameToSecondRotation.size() != firstReturnPathResult->pathResults.size()) continue;

            secondReturnPathResults.emplace_back(firstReturnPathResult, std::move(actionFrameToSecondRotation));
        }
    }

    [[nodiscard]] int InitialSplitSolver::PathResult::worstActionFrame() const
    {
        int worst = 0;
        for (const auto &result : pathResults)
        {
            worst = std::max(worst, result.actionFrame);
        }
        return worst;
    }

    [[nodiscard]] int InitialSplitSolver::PathResult::worstActionFrameAndDelay() const
    {
        int worst = 0;
        for (const auto &result : pathResults)
        {
            worst = std::max(worst, result.actionFrame + result.postActionDelay);
        }
        return worst;
    }

    [[nodiscard]] bool InitialSplitSolver::PathResult::equivalentTo(const PathResult &other) const
    {
        if (pathResults.size() != other.pathResults.size()) return false;
        for (const auto &pathResult : pathResults)
        {
            bool found = false;
            for (const auto &otherPathResult : other.pathResults)
            {
                if (pathResult.actionFrame == otherPathResult.actionFrame &&
                    pathResult.postActionDelay == otherPathResult.postActionDelay &&
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

    [[nodiscard]] MiningOptimization::InitialSplitRotation InitialSplitSolver::PathResult::toInitialSplitRotation(
        const std::map<int, unsigned int> *resetValueAndOccurrenceRate) const
    {
        EXPECT_NE(nullptr, previousPathResult) << "Trying to make an initial split rotation from a gather path result";
        EXPECT_EQ(1, previousPathResult->pathResults.size()) << "Gather path has multiple results";

        std::set<uint16_t> combinedResends;
        for (const auto resend : previousPathResult->resends) combinedResends.insert(resend + frameDelay);
        for (const auto resend : resends) combinedResends.insert(resend);

        int returnArrivalFrame = -1;
        std::map<uint16_t, uint8_t> returnActionFrames;
        for (const auto &returnPathResult : pathResults)
        {
            EXPECT_TRUE((returnArrivalFrame == -1) || (returnArrivalFrame == returnPathResult.arrivalFrame))
                    << "Inconsistent arrival frames on return paths";
            returnArrivalFrame = returnPathResult.arrivalFrame;
            if (!resetValueAndOccurrenceRate || !returnPathResult.orderProcessTimerResetValue)
            {
                returnActionFrames.emplace(returnPathResult.actionFrame, 1);
            }
            else
            {
                returnActionFrames.emplace(returnPathResult.actionFrame,
                                           resetValueAndOccurrenceRate->at(*returnPathResult.orderProcessTimerResetValue));
            }
        }

        return MiningOptimization::InitialSplitRotation
        {
            (uint8_t)frameDelay,
            std::move(combinedResends),
            (uint16_t)(previousPathResult->pathResults.begin()->arrivalFrame + frameDelay),
            (uint16_t)(previousPathResult->pathResults.begin()->actionFrame + frameDelay),
            (uint16_t)returnArrivalFrame,
            std::move(returnActionFrames)
        };
    }
}
