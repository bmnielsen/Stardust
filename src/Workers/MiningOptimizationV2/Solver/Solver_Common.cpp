#include "Solver.h"

#include "OrderProcessTimer.h"

#include "../DataModel/MapData.h"

/*
 * This file contains the main logic for the path solver.
 *
 * The solver has the following outputs:
 * - The best actions to be taken for each possible branch in the path
 * - The expected results (arrival frame, mining/delivery frame, patch lock frame, penalty for collision or not facing patch) with their probabilities
 * - When doing takeover optimization, what other potential patch locking frames can be optimized for, so the solver can be re-run if the patch
 *   locking probabilities change later
 *
 * This allows our mining optimization logic to call the solver, then follow the worker's path in the solver result, issuing the desired actions and
 * gradually becoming more confident of the result as any path branches are taken.
 *
 * For takeover optimization, we try to achieve patch locking as the top priority. As patch locking depends on whether all the other patches are
 * being mined, the state of this may change after the initial run of the solver. To help the mining optimization logic determine if this is the
 * case, the solver returns both the patch locking frame(s) it is planning to reach (so the assumptions about viability can be reconsidered later),
 * and what other patch locking frames could be reached if they become viable later. If the mining optimization logic thinks the situation might have
 * changed significantly, it can re-run the solver to get an updated result.
 *
 * Constraints considered by the solver:
 * - Gather resends cannot be issued LF from each other (Unit_Busy)
 * - Gather resends cannot be issued LF+1 frames before an order process timer reset, as this usually puts the worker in a weird state (it will
 *   stay in the ResetCollision order for more than the usual single frame, unless its order process timer resets to 0)
 * - Gather takeover takes the takeover frame into consideration (i.e. the latest frame the patch will be free) unless patch locking can be achieved
 *
 * The algorithm works in the following way:
 * - We start by recursively exploring the next node(s) on the no-resend path
 * - We then peek LF ahead to see if a resend is viable from the node
 * - If a resend is viable, we recursively explore as above, but with the resend
 * - We then weight and compare the results and pick the best one, updating our path branch structure accordingly
 * - We then return up the tree, until we have built the full decision tree with weighted results at each node
 */
namespace MiningOptimization
{
    namespace
    {
        // Attempts to predict what the worker's order process timer will be on the next frame
        int nextOrderProcessTimer(int simulationFrame, int currentOrderProcessTimer, const std::set<int> &resendFrames)
        {
            // If there was a resend LF ago, treat the worker's order process timer as 10
            // In reality it is 0 for two frames, then 8, but for our logic there is no difference
            if (resendFrames.contains(simulationFrame + 1 - BWAPI::Broodwar->getLatencyFrames()))
            {
                return 10;
            }

            // If we already don't know the worker's order process timer value, or the order process timers reset here, return -1
            if (currentOrderProcessTimer == -1 || OrderProcessTimer::isResetFrame(simulationFrame + 1))
            {
                return -1;
            }

            // Implement the cycle, decrementing and setting to 8 when past 0
            int result = currentOrderProcessTimer - 1;
            if (result < 0) result = 8;
            return result;
        }
    }

    template <typename ObservationType>
    std::optional<SolverPathBranch> Solver<ObservationType>::execute()
    {
        SolverResends resends;
        auto result = processNextNodes(startPosition, initialNextPathNodes, startFrame, resends, workerOrderProcessTimerAtStartFrame);
        if (result)
        {
            result->pathToNextBranch.push_front(startPosition);
        }
        return result;
    }

    template <typename ObservationType>
    std::optional<SolverPathBranch> Solver<ObservationType>::processNextNodes( // NOLINT(*-no-recursion)
            const PositionAndVelocity &pos,
            const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextNodes,
            int frame,
            SolverResends &resends,
            int workerOrderProcessTimer)
    {
        int nextWorkerOrderProcessTimer = nextOrderProcessTimer(frame, workerOrderProcessTimer, resends.resendFrames);

        // Processes a node and returns its result
        auto processNode =
                [&](const PathNode<ObservationType> &node) -> std::optional<SolverPathBranch> // NOLINT(*-no-recursion)
        {
            // Compute the position corresponding to this node
            auto here = node.pos.addTo(pos, positionDeltas);

            // TODO: Check for patch lock and patch switch

            // If we have reached the end of the path, create the new branch and populate it with the arrival data
            auto &next = node.applicableNextPositions(frame, resends.resendFrames);
            if (next.empty())
            {
                SolverPathBranch result;
                result.pathToNextBranch.emplace_front(std::move(here));

                // TODO: Get arrival data and populate the data maps
//                auto &arrivalData = node.applicableArrivalData(frame, resends.resendFrames);

                return result;
            }

            // Get the result from not resending here
            auto result = processNextNodes(here, next, frame + 1, resends, nextWorkerOrderProcessTimer);

            // If there can be a resend, try it
            auto resendViability = isResendViableHere(node, frame, resends);
            if (resendViability.first)
            {
                // Add the resend frame
                std::set<int> resendFrames = resends.resendFrames;
                resendFrames.insert(frame);
                SolverResends resendsHere{std::move(resendFrames), resendViability.second};

                // Get the result
                auto resendResult = processNextNodes(here, next, frame + 1, resendsHere, nextWorkerOrderProcessTimer);

                // If the result is better, replace the existing one
                // TODO
                result = resendResult;
            }

            if (!result) return std::nullopt;

            result->pathToNextBranch.emplace_front(std::move(here));
            return result;
        };

        // If the path doesn't branch here, we can just return the result for the single node
        if (nextNodes.size() == 1) return processNode(nextNodes.begin()->first);

        // The path branches, so we need to also branch the solve result
        SolverPathBranch result;
        for (const auto &[node, occurrences] : nextNodes)
        {
            auto nodeResult = processNode(node);
            if (!nodeResult) return std::nullopt;

            // Adjust the probabilities by this node's probability
            double nodeProbability = (double)occurrences / 255.0;

            auto addObservations = [&](const std::map<int, double> &source, std::map<int, double> &target)
            {
                for (const auto &[value, probability] : source)
                {
                    target[value] += (probability * nodeProbability);
                }
            };
            addObservations(nodeResult->arrivalFramesWithProbabilities, result.arrivalFramesWithProbabilities);
            addObservations(nodeResult->actionFramesWithProbabilities, result.actionFramesWithProbabilities);
            addObservations(nodeResult->delaysWithProbabilities, result.delaysWithProbabilities);
            addObservations(nodeResult->patchLockFramesWithProbabilities, result.patchLockFramesWithProbabilities);

            result.nextBranches.emplace_back(std::exchange(nodeResult, std::nullopt).value());
        }

        return result;
    }

    template <typename ObservationType>
    std::pair<bool, bool> Solver<ObservationType>::isResendViableHere(
            const PathNode<ObservationType> &node, int frame, SolverResends &previousResends)
    {
        if (previousResends.isFinal) return {false, false};

        // Need to update this to consider resends at stable nodes:
        // - A resend at a stable node to avoid an order process timer reset is fine to resend again after
        // - A resend at any other stable node only ever makes sense as the last resend, so further resends should be blocked from consideration

        // Reject immediately if a resend isn't possible on this frame
        if (!canResendOnFrame(frame, previousResends.resendFrames)) return {false, false};

        // We explore ahead LF frames and check if all observed nodes have resend data available
        int frameResendTakesEffect = frame + BWAPI::Broodwar->getLatencyFrames();

        bool anyNonFinalResends = false;
        std::function<bool(const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextNodes, int nextFrame)> checkNextNodes;
        checkNextNodes = [&](
                const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextNodes,
                int nextFrame)
        {
            if (nextNodes.empty()) return false;

            for (const auto &[nextNode, _] : nextNodes)
            {
                if (nextFrame == frameResendTakesEffect)
                {
                    if (nextNode.arrivalDataAfterResend.empty() && nextNode.nextPositionsAfterResend.empty())
                    {
                        if (!nextNode.isStableResendNode) return false;

                        // For stable resend nodes, it only makes sense to resend there if it is the final resend, except in the case where we
                        // are resending to avoid patch switching on the order process timer reset frame
                        if (takeoverFrame != -1 && OrderProcessTimer::isResetFrame(nextFrame)) anyNonFinalResends = true;
                    }
                    else
                    {
                        anyNonFinalResends = true;
                    }
                }
                else
                {
                    if (!checkNextNodes(nextNode.applicableNextPositions(nextFrame, previousResends.resendFrames), nextFrame + 1)) return false;
                }
            }

            return true;
        };

        return {checkNextNodes(node.applicableNextPositions(frame, previousResends.resendFrames), frame + 1), !anyNonFinalResends};
    }

    template class Solver<GatherArrivalData>;
    template class Solver<ReturnArrivalData>;
}
