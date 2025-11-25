#include "Solver.h"

#include "OrderProcessTimer.h"

#include "../DataModel/MapData.h"

/*
 * This file contains the main logic for the path solver.
 *
 * The algorithm is roughly this:
 * - Explore depth-first without resends until we reach the arrival node, taking all branches and weighting the results accordingly
 * - Backtrack, returning the weighted arrival delays at every step
 * - Wherever a resend is possible, repeat the above, exploring the resend path depth-first and backtracking with the weighted arrival delays
 * - At every backtrack step, we add the possible results to the result set
 * - Finally, once all possibilities have been collected, we choose the best one
 *
 * Considerations made along the way:
 * - Resends cannot be made LF after each other (because of Unit_Busy)
 * - Resends cannot be made LF+1 frames before an order process timer reset (because the unit potentially stays in ResetCollision for multiple frames)
 * - Stable nodes may have an extra resend added to avoid patch switching that otherwise won't affect the pathing
 *
 * The scoring of the best combination prioritizes making the resends as late as possible. This allows more time for us to observe the actual path
 * the worker is taking and replan if something changes.
 *
 * If the path is long and unstable, we may return a result indicating that we don't want to solve at this time, instead giving a frame where the
 * caller should call the solver again. This is to avoid having to compute an excessive number of branches when we could just wait a bit and be much
 * more clear on what path the worker is actually taking.
 */
namespace MiningOptimization
{
    namespace
    {
        // Attempts to predict what the worker's order process timer will be on the next frame
        int nextOrderProcessTimer(int simulationFrame, int currentOrderProcessTimer, const std::set<int> &resendFrames)
        {
            if (resendFrames.contains(simulationFrame + 1 - BWAPI::Broodwar->getLatencyFrames()))
            {
                // Really it sets to 0 for two frames while the worker recomputes its path, but for our logic we don't care
                return 10;
            }
            if (currentOrderProcessTimer == -1 || OrderProcessTimer::isResetFrame(simulationFrame + 1))
            {
                return -1;
            }
            int result = currentOrderProcessTimer - 1;
            if (result < 0) result = 8;
            return result;
        }
    }

    template <typename ObservationType>
    void Solver<ObservationType>::execute()
    {
        std::set<int> previousResendFrames = initialPreviousResendFrames;
        processNextNodes(initialNextPathNodes, startFrame, previousResendFrames, workerOrderProcessTimerAtStartFrame);
    }

    template <typename ObservationType>
    Solver<ObservationType>::NoResendArrivalDetails Solver<ObservationType>::processNextNodes(
            const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextNodes,
            int frame,
            std::set<int> &resendFrames,
            int workerOrderProcessTimer)
    {
        int nextWorkerOrderProcessTimer = nextOrderProcessTimer(frame, workerOrderProcessTimer, resendFrames);

        // Processes a node and returns its no resend arrival details
        auto processNode = [&](const PathNode<ObservationType> &node)
        {
            // Start by exploring the next nodes
            auto noResendArrival = [&]()
            {
                // If this is not the final node, propagate forward and increment the arrival delays
                if (!node.nextPositions.empty())
                {
                    auto resultRequiringArrivalAdjustment =
                            processNextNodes(node.nextPositions, frame + 1, resendFrames, nextWorkerOrderProcessTimer);
                    for (auto &[arrivalDetails, _] : resultRequiringArrivalAdjustment)
                    {
                        arrivalDetails.arrivalData.arrivalDelay++;
                    }
                    return resultRequiringArrivalAdjustment;
                }

                // This is the final node, so take the arrival data on the node and convert
                Solver<ObservationType>::NoResendArrivalDetails arrivalDetailsAndProbabilities;
                for (const auto &[arrivalData, arrivalOccurrences] : node.arrivalDataWhenFinalNode)
                {
                    arrivalDetailsAndProbabilities.emplace_back(ArrivalDetails{arrivalData, nextWorkerOrderProcessTimer},
                                                                (double)arrivalOccurrences / 255.0);
                }
                return arrivalDetailsAndProbabilities;
            }();

            // TODO: Consider resends

            return noResendArrival;
        };

        if (nextNodes.size() == 1) return processNode(nextNodes.begin()->first);

        Solver<ObservationType>::NoResendArrivalDetails arrivalDetailsAndProbabilities;
        for (const auto &[node, occurrences] : nextNodes)
        {
            auto nodeResult = processNode(node);

            // Adjust the probabilities by this node's probability
            double nodeProbability = (double)occurrences / 255.0;
            for (auto &[_, probability] : nodeResult)
            {
                probability *= nodeProbability;
            }

            arrivalDetailsAndProbabilities.insert(arrivalDetailsAndProbabilities.end(),
                                                  std::make_move_iterator(nodeResult.begin()),
                                                  std::make_move_iterator(nodeResult.end()));

        }

        return arrivalDetailsAndProbabilities;
    }

    template class Solver<GatherArrivalData>;
    template class Solver<ReturnArrivalData>;
}
