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
                        orderProcessTimerResetValues);

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
                if (previousPathResult && previousPathResult->resends.contains(12) && previousPathResult->resends.size() == 1)
                {
                    Log::Get() << "Hey there!";
                }
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
                int score = 0;
                for (const auto &resetResult : pathResult.pathResults)
                {
                    score = std::max(score, resetResult.actionFrame + resetResult.postActionDelay);
                }
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

        // Pick the first return path result
        // TODO: Check if there is any reasonable way to pick the best one
        auto selectedFirstRotation = *getBestPaths(firstReturnPathResults).begin();
        auto firstRotation = selectedFirstRotation->toInitialSplitRotation();

        // TODO: Implement second rotation


        return MiningOptimization::InitialSplitData{
            firstRotation
        };
    }
}
