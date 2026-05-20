#include "Solver.h"

#include "../MiningOptimizationConfiguration.h"
#include "Geo.h"
#include "OrderProcessTimer.h"

#include "../DataModel/MapData.h"

#define EPSILON 0.000001

/*
 * This file contains the main logic for the path solver.
 *
 * The solver has the following outputs:
 * - The best actions to be taken for each possible branch in the path
 * - The expected results (arrival frame, mining/delivery frame, penalty for collision or not facing patch) with their probabilities
 * - Data on what frames can be used for resending close to the patch when optimizing for takeover and patch locking
 *
 * This allows our mining optimization logic to call the solver, then follow the worker's path in the solver result, issuing the desired actions and
 * gradually becoming more confident of the result as any path branches are taken.
 *
 * Constraints considered by the solver:
 * - Gather resends cannot be issued LF from each other (Unit_Busy)
 * - Gather resends cannot be issued LF+1 frames before an order process timer reset, as this usually puts the worker in a weird state (it will
 *   stay in the ResetCollision order for more than the usual single frame, unless its order process timer resets to 0)
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
        // The order process timer values in this method are the values at the start of the given frame
        template <typename ObservationType>
        std::multiset<int> orderProcessTimerInFuture(int startFrame, // NOLINT(*-no-recursion)
                                                     const std::multiset<int> &possibleStartingValues,
                                                     const std::set<int> &resendFrames,
                                                     unsigned int framesInFuture)
        {
            if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
            {
                return OrderProcessTimer::atStartOfFrameAtDelta(startFrame, possibleStartingValues, resendFrames, {}, framesInFuture);
            }
            else
            {
                return OrderProcessTimer::atStartOfFrameAtDelta(startFrame, possibleStartingValues, {}, resendFrames, framesInFuture);
            }
        }
    }

    template <typename ObservationType>
    SolverResult<ObservationType> Solver<ObservationType>::execute()
    {
        // The possible order process timer frames we get as input to the solver are the values at the end of the start frame
        // During the solver logic, we want to know the possible values just before the worker's orders are processed, which may be different
        // from the values at the end of the previous frame if there has been a resend or an order process timer reset
        auto workerOrderProcessTimer = possibleWorkerOrderProcessTimerValuesAtStartFrame;
        if (OrderProcessTimer::isResetFrame(startFrame + 1))
        {
            workerOrderProcessTimer = {0, 1, 2, 3, 4, 5, 6, 7};
        }

        SolverResends resends;
        auto result =
                processNextNodes(startPosition, path.nextPositions, startFrame + 1, resends, workerOrderProcessTimer);
        result.pathToNextBranch.push_front(startPosition);
        result.pathNodesToNextBranch.push_front(nullptr); // This should otherwise be the root node, but we know we don't need to use it again
        return result;
    }

    template <typename ObservationType>
    SolverResult<ObservationType> Solver<ObservationType>::processNextNodes( // NOLINT(*-no-recursion)
            const PositionAndVelocity &pos,
            const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextNodes,
            int nextFrame,
            const SolverResends &resends,
            const std::multiset<int> &workerOrderProcessTimer) const
    {
        // Processes a node and returns its result
        auto processNode =
                [&](const PathNode<ObservationType> &node) -> SolverResult<ObservationType> // NOLINT(*-no-recursion)
        {
            // Compute the position corresponding to this node and define the helper that adds it to a result
            auto here = node.pos.addTo(pos, mapData.positionDeltas);
            auto addPositionTo = [&](SolverResult<ObservationType> &result) -> SolverResult<ObservationType>&
            {
                result.pathToNextBranch.emplace_front(std::move(here));
                result.pathNodesToNextBranch.push_front(&node);
                return result;
            };

            // If we have reached the end of the path, create the new branch and populate it with the arrival data
            auto &next = node.applicableNextPositions(nextFrame, resends.resendFrames);
            if (next.empty())
            {
                SolverResult<ObservationType> result;

                auto addArrivalData = [&](const ObservationType &arrivalData, double probability)
                {
                    // The arrival frame is given by the delay here
                    int arrivalFrame = nextFrame + arrivalData.arrivalDelay();
                    if constexpr (std::is_same_v<ObservationType, GatherArrivalData>)
                    {
                        result.arrivalDataWithProbabilities[{arrivalFrame, arrivalData.tenDistanceAndResendAlwaysArrivesIndex}] += probability;
                    }
                    else
                    {
                        result.arrivalDataWithProbabilities[{arrivalFrame, UINT8_MAX}] += probability;
                    }

                    // TODO: Add 10-distance and resend already arrives data here

                    // On gather there is an extra frame of delay between arrival and mining (the WaitForMinerals frame)
                    int transitionFrames = transitionFramesToAction();

                    // Compute the possible order process timer values at arrival, taking pending resends into account
                    auto possibleOrderProcessTimerValuesAtArrival =
                            orderProcessTimerInFuture<ObservationType>(nextFrame,
                                                                       workerOrderProcessTimer,
                                                                       resends.resendFrames,
                                                                       arrivalData.arrivalDelay());

                    // If there was a recent resend, override the order process timer value to simulate the fact that a 0 value won't actually take
                    // effect here
                    if (possibleOrderProcessTimerValuesAtArrival.size() == 1 && possibleOrderProcessTimerValuesAtArrival.contains(0))
                    {
                        for (int i = 0; i <= transitionFrames; i++)
                        {
                            if (resends.resendFrames.contains(arrivalFrame - BWAPI::Broodwar->getLatencyFrames() - i - 1))
                            {
                                possibleOrderProcessTimerValuesAtArrival = {8 + i + transitionFrames};
                            }
                        }
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

                    // Get the number of frames from the arrival frame to the next order process timer reset
                    // We do not consider resets at the arrival frame itself, since this is already handled
                    int orderProcessTimerResetAfterArrival = OrderProcessTimer::framesToNextReset(arrivalFrame + 1) + 1;

                    // Loop through the possible values
                    // As the set is sorted, we know we will handle the values before the reset first
                    int handledValuesBeforeReset = 0;
                    for (int orderProcessTimerValue : possibleOrderProcessTimerValuesAtArrival)
                    {
                        int actionDelay = orderProcessTimerValue + transitionFrames;
                        if (actionDelay < orderProcessTimerResetAfterArrival)
                        {
                            double probabilityHere = (probability * (1.0 / (double)possibleOrderProcessTimerValuesAtArrival.size()));
                            result.actionFramesWithProbabilities[arrivalFrame + actionDelay] += probabilityHere;
                            arrivalData.addDelayAfterAction(result.delaysWithProbabilities,
                                                            orderProcessTimerValue,
                                                            arrivalFrame + actionDelay,
                                                            probabilityHere);
                            handledValuesBeforeReset++;
                            continue;
                        }

                        // A reset has occurred before all values were considered
                        // We now have to consider the possible values 0 to 7 from the reset frame

                        // Their probability is the probability we made it to the reset frame times the probability of each value (1/8)
                        double resetValueProbability =
                                ((1.0 - ((double)handledValuesBeforeReset / (double)possibleOrderProcessTimerValuesAtArrival.size())) / 8.0)
                                * probability;

                        // The delay from the arrival frame will be the delta to the timer reset, plus the transition frame if it hasn't already
                        // happened, plus the value the order process timer resets to
                        actionDelay = orderProcessTimerResetAfterArrival
                                + ((actionDelay == orderProcessTimerResetAfterArrival) ? 0 : transitionFrames);

                        for (int resetOrderProcessTimerValue = 0; resetOrderProcessTimerValue <= 7; resetOrderProcessTimerValue++)
                        {
                            result.actionFramesWithProbabilities[arrivalFrame + actionDelay + resetOrderProcessTimerValue]
                                += resetValueProbability;
                            arrivalData.addDelayAfterAction(result.delaysWithProbabilities,
                                                            orderProcessTimerValue,
                                                            arrivalFrame + actionDelay + resetOrderProcessTimerValue,
                                                            resetValueProbability);
                        }
                    }

#if USE_NEXT_PATH_LENGTHS
                    result.nextPathLengthWithProbabilities[arrivalData.nextPathLength(mapData.minimumNextPathLength)] += probability;
#endif
                };

                // Process the arrival data, weighting by probability if there are unstable results
                auto &arrivalDataAndOccurrenceRates = node.applicableArrivalData(nextFrame, resends.resendFrames);

#if LOGGING_ENABLED
                if (arrivalDataAndOccurrenceRates.empty())
                {
                    Log::Get() << "ERROR: Empty arrival data at leaf node in path solver";
                }
#endif

                for (const auto &[arrivalData, occurrenceRate] : arrivalDataAndOccurrenceRates)
                {
                    addArrivalData(arrivalData, mapData.occurrenceRateToProbability(occurrenceRate));
                }

                return std::move(addPositionTo(result));
            }

            // Compute the order process timer values at the start of the next frame
            auto nextWorkerOrderProcessTimer =
                    orderProcessTimerInFuture<ObservationType>(nextFrame, workerOrderProcessTimer, resends.resendFrames, 1);

            // Get the result from not resending here
            auto result = processNextNodes(here, next, nextFrame + 1, resends, nextWorkerOrderProcessTimer);

            // If there can be a resend, try it
            auto resendViability = isResendViableHere(node, nextFrame, resends);
            if (!resendViability.first) return std::move(addPositionTo(result));

            // Add the resend frame
            std::set<int> resendFrames = resends.resendFrames;
            resendFrames.insert(nextFrame);
            SolverResends resendsHere{std::move(resendFrames), resendViability.second};

            // Get the result
            auto resendResult = processNextNodes(here, next, nextFrame + 1, resendsHere, nextWorkerOrderProcessTimer);
            resendResult.resendFramesOnThisBranch.insert(nextFrame);

            // Score the two results and return the best one
            auto scoreResult = [](const SolverResult<ObservationType> &result)
            {
                // Start with the action frame
                double score = SolverResult<ObservationType>::mapAverage(result.actionFramesWithProbabilities);

                // Add the delays, but weight them ever so slightly more than the base score
                // The rationale is that if we have otherwise equal results, we'd prefer not to have the delay
                // TODO: Test and consider making this more sophisticated - we actually don't really care about collision delays,
                //       but facing patch and mining start delays keep the patch busy for longer, which can be good or bad depending
                //       on whether another worker is waiting to mine or not
                score += (SolverResult<ObservationType>::mapAverage(result.delaysWithProbabilities) * 1.001);

#if USE_NEXT_PATH_LENGTHS
                score += NEXT_PATH_LENGTH_WEIGHT * SolverResult<ObservationType>::mapAverage(result.nextPathLengthWithProbabilities);
#endif

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
            double nodeProbability = mapData.occurrenceRateToProbability(occurrenceRate);

            auto addObservations = [&]<typename T>(const std::map<T, double> &source, std::map<T, double> &target)
            {
                for (const auto &[value, probability] : source)
                {
                    target[value] += (probability * nodeProbability);
                }
            };
            addObservations(nodeResult.arrivalDataWithProbabilities, result.arrivalDataWithProbabilities);
            addObservations(nodeResult.actionFramesWithProbabilities, result.actionFramesWithProbabilities);
            addObservations(nodeResult.delaysWithProbabilities, result.delaysWithProbabilities);
#if USE_NEXT_PATH_LENGTHS
            addObservations(nodeResult.nextPathLengthWithProbabilities, result.nextPathLengthWithProbabilities);
#endif

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
