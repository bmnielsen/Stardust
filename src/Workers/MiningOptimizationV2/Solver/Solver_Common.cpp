#include "Solver.h"

#include "Geo.h"
#include "OrderProcessTimer.h"

#include "../DataModel/MapData.h"

#define EPSILON 0.000001

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
        // Computes what the possible order process timer values are for a worker in a given number of frames
        template <typename ObservationType>
        std::set<int> orderProcessTimerInFuture(int startFrame, // NOLINT(*-no-recursion)
                                                const std::set<int> &possibleStartingValues,
                                                const std::set<int> &resendFrames,
                                                unsigned int framesInFuture)
        {
            if (framesInFuture == 0) return possibleStartingValues;

            // If there is a resend frame within the window, reset the order process timer value accordingly
            // For gather, the worker's timer goes to 0 for two frames, then 8. But as the worker can't actually start mining at either of the
            // intermediate frames, we can just treat the value as 10 on the first frame and everything works out.
            // For return, it is similar, but without the extra frame, so we use 9 instead.
            for (auto resendFrame : resendFrames)
            {
                int frameResendTakesEffect = resendFrame + BWAPI::Broodwar->getLatencyFrames();
                if (frameResendTakesEffect > startFrame && frameResendTakesEffect <= (startFrame + framesInFuture))
                {
                    constexpr int valueAfterResend = []()
                    {
                        if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
                        {
                            return 10;
                        }
                        else
                        {
                            return 9;
                        }
                    }();

                    // Call recursively to allow a later resend frame to override
                    return orderProcessTimerInFuture<ObservationType>(frameResendTakesEffect,
                                                                      {valueAfterResend},
                                                                      resendFrames,
                                                                      framesInFuture - (frameResendTakesEffect - startFrame));
                }
            }

            // If there is a reset frame within the window, the values can be from 0 to 7 inclusive from that frame
            int framesToNextReset = OrderProcessTimer::framesToNextReset(startFrame + 1);
            if (framesToNextReset < framesInFuture)
            {
                return orderProcessTimerInFuture<ObservationType>(startFrame + framesToNextReset + 1,
                                                                  {0, 1, 2, 3, 4, 5, 6, 7},
                                                                  {},
                                                                  framesInFuture - framesToNextReset - 1);
            }

            // Nothing has happened that would interfere with the normal cycle, so run it on all the values
            std::set<int> result;
            for (auto startingValue : possibleStartingValues)
            {
                startingValue -= (int)framesInFuture;
                while (startingValue < 0) startingValue += 9;
                result.insert(startingValue);
            }
            return result;
        }
    }

    template <typename ObservationType>
    SolverResult<ObservationType> Solver<ObservationType>::execute()
    {
        // If we don't know the worker's order process timer value, we assume it can be any of the valid values
        std::set<int> workerOrderProcessTimer;
        if (workerOrderProcessTimerAtStartFrame == -1)
        {
            workerOrderProcessTimer = {0, 1, 2, 3, 4, 5, 6, 7, 8};
        }
        else
        {
            workerOrderProcessTimer = {workerOrderProcessTimerAtStartFrame};
        }

        SolverResends resends;
        auto result = processNextNodes(startPosition, path.nextPositions, startFrame + 1, resends, workerOrderProcessTimer);
        result.pathToNextBranch.push_front(startPosition);
        result.pathNodesToNextBranch.push_front(nullptr); // This should otherwise be the root node, but we know we don't need to use it again
        return result;
    }

    template <typename ObservationType>
    SolverResult<ObservationType> Solver<ObservationType>::processNextNodes( // NOLINT(*-no-recursion)
            const PositionAndVelocity &pos,
            const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextNodes,
            int frame,
            const SolverResends &resends,
            const std::set<int> &workerOrderProcessTimer) const
    {
        // The order process timer provided to this method is the value at the start of the frame
        // This computes the value at the start of the next frame
        auto nextWorkerOrderProcessTimer =
                orderProcessTimerInFuture<ObservationType>(frame, workerOrderProcessTimer, resends.resendFrames, 1);

        // Processes a node and returns its result
        auto processNode =
                [&](const PathNode<ObservationType> &node) -> SolverResult<ObservationType> // NOLINT(*-no-recursion)
        {
            // Compute the position corresponding to this node and define the helper that adds it to a result
            auto here = node.pos.addTo(pos, positionDeltas);
            auto addPositionTo = [&](SolverResult<ObservationType> &result) -> SolverResult<ObservationType>&
            {
                result.pathToNextBranch.emplace_front(std::move(here));
                result.pathNodesToNextBranch.push_front(&node);
                return result;
            };

            // Adds patch lock and switch probabilities to the branch
            // If evaluating a resend branch, we detect patch switches, but patch locks are no longer possible since they will be cleared by the
            // resend
            auto addPatchLockAndSwitchProbabilities = [&](SolverResult<ObservationType> &branch, bool canPatchLock)
            {
                // Not relevant if it is not gather takeover, the takeover frame has passed, or the worker can't have order process timer 0 here
                if (takeoverFrame == -1 || takeoverFrame <= frame || !workerOrderProcessTimer.contains(0)) return;

                // Patch locking and switching kicks in at 10 distance from the patch
                auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                    here,
                                                    BWAPI::UnitTypes::Resource_Mineral_Field,
                                                    resource->center);
                if (dist > 10) return;

                // There is a possibility of patch lock or switch, so compute the probability based on our forecast
                auto patchLockProbability = otherPatchesForecast.atFrame(frame);
                auto patchSwitchProbability = 1.0 - patchLockProbability;
                if (!canPatchLock) patchLockProbability = 0.0;

                // The probability of each order process timer value occurring is equal
                auto probabilityWorkerOrderProcessTimerIsZero = (1.0 / (double)workerOrderProcessTimer.size());

                auto add = [&](std::map<int, double> &map, double probability)
                {
                    if (probability < EPSILON) return;
                    map[frame] = probability * probabilityWorkerOrderProcessTimerIsZero;
                };
                add(branch.patchLockFramesWithProbabilities, patchLockProbability);
                add(branch.patchSwitchFramesWithProbabilities, patchSwitchProbability);
            };

            // If we have reached the end of the path, create the new branch and populate it with the arrival data
            auto &next = node.applicableNextPositions(frame, resends.resendFrames);
            if (next.empty())
            {
                SolverResult<ObservationType> result;

                auto addArrivalData = [&](const ObservationType &arrivalData, double probability)
                {
                    // The arrival frame is given by the delay here
                    int arrivalFrame = frame + arrivalData.arrivalDelay;
                    result.arrivalFramesWithProbabilities[arrivalFrame] += probability;

                    // Compute the possible order process timer values at arrival, taking pending resends into account
                    std::set<int> possibleOrderProcessTimerValuesAtArrival;
                    if (arrivalData.arrivalDelay == 1)
                    {
                        possibleOrderProcessTimerValuesAtArrival = workerOrderProcessTimer;
                    }
                    else if (arrivalData.arrivalDelay == 2)
                    {
                        possibleOrderProcessTimerValuesAtArrival = nextWorkerOrderProcessTimer;
                    }
                    else
                    {
                        possibleOrderProcessTimerValuesAtArrival = orderProcessTimerInFuture<ObservationType>(frame,
                                                                                                              workerOrderProcessTimer,
                                                                                                              resends.resendFrames,
                                                                                                              arrivalData.arrivalDelay - 1);
                    }

                    // Now use this data to compute when the action (mining start or resource delivery) will occur
                    // As the action will occur once the order process timer reaches 0, in the simple case we can just add the order process timer
                    // value to the arrival frame and get the action frame.
                    // However, the order process timer might reset after arrival. In this case, there will be 8 additional possible order process
                    // timer values to consider.
                    // Note that in normal situations, we will not have a set of possible values coming into this block and find a reset while
                    // waiting, since usually the reason for having a set of possible values is that there has already been a reset. However we need
                    // to handle it since we technically can run the solver without knowing the unit's initial order process timer value (like for
                    // a spawned unit).

                    // TODO: Consider the takeover frame? Or will that be handled elsewhere?

                    // Get the number of frames from the arrival frame to the next order process timer reset
                    // We do not include resets at the arrival frame itself, as these have already been considered when computing the possible values
                    int orderProcessTimerResetAfterArrival = OrderProcessTimer::framesToNextReset(arrivalFrame + 1) + 1;

                    // Loop through the possible values
                    // As the set is sorted, we know we will handle the values before the reset first
                    int handledValuesBeforeReset = 0;
                    for (int orderProcessTimerValue : possibleOrderProcessTimerValuesAtArrival)
                    {
                        if (orderProcessTimerValue < orderProcessTimerResetAfterArrival)
                        {
                            result.actionFramesWithProbabilities[arrivalFrame + orderProcessTimerValue] +=
                                    (probability * (1.0 / (double)possibleOrderProcessTimerValuesAtArrival.size()));
                            handledValuesBeforeReset++;
                            continue;
                        }

                        // A reset has occurred before all values were considered
                        // We now have to consider the possible values 0 to 7 from the reset frame
                        // Their probability is the probability we made it to the reset frame times the probability of each value (1/8)
                        double resetValueProbability =
                                ((1.0 - ((double)handledValuesBeforeReset / (double)possibleOrderProcessTimerValuesAtArrival.size())) / 8.0)
                                * probability;
                        for (int resetOrderProcessTimerValue = 0; resetOrderProcessTimerValue <= 7; resetOrderProcessTimerValue++)
                        {
                            result.actionFramesWithProbabilities[arrivalFrame + orderProcessTimerResetAfterArrival + resetOrderProcessTimerValue]
                                += resetValueProbability;
                        }
                    }

                    result.delaysWithProbabilities[arrivalData.delayAfterAction()] += probability;
                    result.nextPathLengthWithProbabilities[arrivalData.nextPathLength(minimumNextPathLength)] += probability;
                };

                // Process the arrival data, weighting by probability if there are unstable results
                auto &arrivalDataAndOccurrenceRates = node.applicableArrivalData(frame, resends.resendFrames);

#if LOGGING_ENABLED
                if (arrivalDataAndOccurrenceRates.empty())
                {
                    Log::Get() << "ERROR: Empty arrival data at leaf node in path solver";
                }
#endif

                for (const auto &[arrivalData, occurrenceRate] : arrivalDataAndOccurrenceRates)
                {
                    addArrivalData(arrivalData, (double)occurrenceRate / 255.0);
                }

                addPatchLockAndSwitchProbabilities(result, true);

                return std::move(addPositionTo(result));
            }

            // Get the result from not resending here
            auto result = processNextNodes(here, next, frame + 1, resends, nextWorkerOrderProcessTimer);
            addPatchLockAndSwitchProbabilities(result, true);

            // If there can be a resend, try it
            auto resendViability = isResendViableHere(node, frame, resends);
            if (!resendViability.first) return std::move(addPositionTo(result));

            // Add the resend frame
            std::set<int> resendFrames = resends.resendFrames;
            resendFrames.insert(frame);
            SolverResends resendsHere{std::move(resendFrames), resendViability.second};

            // Get the result
            auto resendResult = processNextNodes(here, next, frame + 1, resendsHere, nextWorkerOrderProcessTimer);
            resendResult.resendFramesOnThisBranch.insert(frame);
            addPatchLockAndSwitchProbabilities(resendResult, false);

            // Score the two results and return the best one
            auto scoreResult = [](const SolverResult<ObservationType> &result)
            {
                // Start with the action frame
                double score = SolverResult<ObservationType>::mapAverage(result.actionFramesWithProbabilities);

                // Add the delays
                score += SolverResult<ObservationType>::mapAverage(result.delaysWithProbabilities);

                // TODO: Consider patch locking and switching

                // Add a tenth of the next path length
                score += 0.1 * SolverResult<ObservationType>::mapAverage(result.nextPathLengthWithProbabilities);

                return score;
            };
            if (scoreResult(result) <= scoreResult(resendResult))
            {
                return std::move(addPositionTo(result));
            }
            return std::move(addPositionTo(resendResult));
        };

        // If the path doesn't branch here, we can just return the result for the single node
        if (nextNodes.size() == 1) return processNode(nextNodes.begin()->first);

        // The path branches, so we need to also branch the solve result
        SolverResult<ObservationType> result;
        for (const auto &[node, occurrenceRate] : nextNodes)
        {
            auto nodeResult = processNode(node);

            // Adjust the probabilities by this node's probability
            double nodeProbability = (double)occurrenceRate / 255.0;

            auto addObservations = [&](const std::map<int, double> &source, std::map<int, double> &target)
            {
                for (const auto &[value, probability] : source)
                {
                    target[value] += (probability * nodeProbability);
                }
            };
            addObservations(nodeResult.arrivalFramesWithProbabilities, result.arrivalFramesWithProbabilities);
            addObservations(nodeResult.actionFramesWithProbabilities, result.actionFramesWithProbabilities);
            addObservations(nodeResult.delaysWithProbabilities, result.delaysWithProbabilities);
            addObservations(nodeResult.patchLockFramesWithProbabilities, result.patchLockFramesWithProbabilities);
            addObservations(nodeResult.patchSwitchFramesWithProbabilities, result.patchSwitchFramesWithProbabilities);
            addObservations(nodeResult.nextPathLengthWithProbabilities, result.nextPathLengthWithProbabilities);

            result.nextBranches.emplace_back(std::move(nodeResult));
        }

        return result;
    }

    template <typename ObservationType>
    std::pair<bool, bool> Solver<ObservationType>::isResendViableHere(
            const PathNode<ObservationType> &node, int frame, const SolverResends &previousResends) const
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
