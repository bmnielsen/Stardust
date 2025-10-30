#include "PathExplorationModule.h"
#include "MiningOptimizationTraining/DataModel/Configuration.h"

namespace MiningOptimizationTraining
{
    void WorkerPathExploration::gathering()
    {
        appendCurrentPosition(gatherPositionHistory);

        // If the worker has reached the WaitForMinerals frame, we can record the path information
        if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            recordGatherPath();
            return;
        }

        // Update the current gather node
        if (!currentGatherNode)
        {
            auto &rootNodes = mapData.resourceToGatherRootNodes[TilePosition::fromBWAPI(patch->getTilePosition())];
            auto rootNodeIt = rootNodes.find(**gatherPositionHistory.rbegin());
            if (rootNodeIt != rootNodes.end())
            {
                currentGatherNode = &rootNodeIt->second;
            }
        }
        else if (resendsPlanned && plannedGatherResendFrames.size() > executedGatherResendFrames.size())
        {
            bool resendTakesEffectHere = executedGatherResendFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames() - 1);
            auto nextNode = currentGatherNode->observationsForSpecificNextPosition(
                    resendTakesEffectHere,
                    **gatherPositionHistory.rbegin());
            auto expectedNextNode = currentGatherNode->observationsForMostLikelyNextPosition(resendTakesEffectHere);

            // Validate that we are following the expected path. Otherwise clear the state so we can replan.
            if (expectedNextNode != nextNode)
            {
                resendsPlanned = false;

                // Remove any planned resends that have not already been executed
                for (auto it = plannedGatherResendFrames.begin(); it != plannedGatherResendFrames.end(); )
                {
                    if (*it >= currentFrame)
                    {
                        it = plannedGatherResendFrames.erase(it);
                    }
                    else
                    {
                        it++;
                    }
                }
            }
            currentGatherNode = nextNode;
        }
        else
        {
            currentGatherNode = currentGatherNode->observationsForSpecificNextPosition(
                    executedGatherResendFrames.contains(currentFrame - BWAPI::Broodwar->getLatencyFrames() - 1),
                    **gatherPositionHistory.rbegin());
        }

        // If we don't have a gather node, we are observing a new path (or have already arrived at the patch) and nothing more is needed
        if (!currentGatherNode) return;

        // If the current node has no information about whether a resend changes the path, query that now
        if (currentGatherNode && currentGatherNode->withinExplorationWindow() && currentGatherNode->canResendChangePath == ResendChangesPath::Unknown)
        {
            auto result = worker->wouldAGatherResendHereChangeThePath();
            if (result.has_value())
            {
                currentGatherNode->canResendChangePath = result.value() ? ResendChangesPath::Yes : ResendChangesPath::No;
            }
        }

        planResends();

        // Execute a planned resend for this frame
        if (plannedGatherResendFrames.contains(currentFrame))
        {
            auto result = worker->gather(patch);
            if (result)
            {
                executedGatherResendFrames.insert(currentFrame);
            }
            else
            {
                Log::Get() << "ERROR: Failed to send gather command: " << BWAPI::Broodwar->getLastError()
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
            }
        }
    }

    void WorkerPathExploration::planResends()
    {
        // Hop out if we have already planned
        if (resendsPlanned) return;

        // Hop out if we have already executed the maximum number of resends
        if (executedGatherResendFrames.size() >= GATHER_RESEND_LIMIT) return;

        // Start by getting the node where resends take effect
        int frame = currentFrame;
        GatherObservations *observationsAtResend = currentGatherNode;
        while (frame < (currentFrame + BWAPI::Broodwar->getLatencyFrames()) && observationsAtResend)
        {
            observationsAtResend =
                    observationsAtResend->observationsForMostLikelyNextPosition(
                            executedGatherResendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames()));
            frame++;
        }
        if (!observationsAtResend) return;

        // Ensure it is within the exploration window
        if (!observationsAtResend->withinExplorationWindow()) return;

        // Find the least explored branch of the path and add its resends
        auto result = observationsAtResend->leastObservedInPath(executedGatherResendFrames, frame);
        plannedGatherResendFrames.insert(result.resendFrames.begin(), result.resendFrames.end());
        resendsPlanned = true;

        CherryVis::log(worker->getID()) << "Planned resends: " << result;
    }

    void WorkerPathExploration::recordGatherPath()
    {
        auto parsedPositionHistory =
                parsePositionHistory(gatherPositionHistory, gatherPositionHistoryStartFrame, executedGatherResendFrames, depot, patch);
        if (!parsedPositionHistory.valid) return;

        // Find or create the root node
        auto &rootNodes = mapData.resourceToGatherRootNodes[TilePosition::fromBWAPI(patch->getTilePosition())];
        auto rootNodeIt = rootNodes.find(**gatherPositionHistory.begin());
        if (rootNodeIt == rootNodes.end())
        {
            auto result = rootNodes.emplace(**gatherPositionHistory.begin(), GatherObservations{**gatherPositionHistory.begin()});
            rootNodeIt = result.first;
            CherryVis::log(worker->getID()) << "Discovered new root node " << rootNodeIt->second;
        }
        auto current = &rootNodeIt->second;
        if (current->occurrences < UINT32_MAX) current->occurrences++;

        // Now step through the path and make the observations
        for (auto it = gatherPositionHistory.begin(); it != parsedPositionHistory.arrivalPositionIt; it++)
        {
            // At the start of the loop, current is guaranteed to be a pointer to the gather observations for this node
            // We have also already incremented the occurrences, since it depends on the other next positions

            bool resendTookEffectHere = parsedPositionHistory.resendPositionIts.contains(it);

            // Reference the correct arrival and next position data depending on whether there was a resend taking effect here
            GatherArrivalObservations &arrivalObservations =
                    (resendTookEffectHere ? current->arrivalObservationsAfterResend : current->arrivalObservations);
            std::vector<GatherObservations> &nextPositions =
                    (resendTookEffectHere ? current->nextPositionsAfterResend : current->nextPositions);

            // Make the arrival observation. Frames to arrival is equivalent to the distance from here to the arrival position iterator.
            arrivalObservations.addArrivalObservation(
                    ArrivalData::create(std::distance(it, parsedPositionHistory.arrivalPositionIt),
                                        parsedPositionHistory.facingTarget));

            // Find or create the next position node

            // Start by getting the next position
            auto nextPositionIt = it + 1;
            if (nextPositionIt == parsedPositionHistory.arrivalPositionIt) break; // reached end of path

            // Sum up the occurrences
            // We use this to determine if there is room to make another observation
            // TODO: We actually should include both resend and no resend next positions since we use their ratios in path exploration
            uint32_t totalOccurrences = 0;
            for (auto &next : nextPositions)
            {
                totalOccurrences += next.occurrences;
            }

            // Find the next position in the vector
            auto next = current->observationsForSpecificNextPosition(resendTookEffectHere, **nextPositionIt);
            if (!next)
            {
                current = &nextPositions.emplace_back(GatherObservations{**nextPositionIt});
                if (totalOccurrences < UINT32_MAX) current->occurrences++;
                CherryVis::log(worker->getID()) << "Discovered new next position " << *current;
                continue;
            }

            // Increment the occurrences if there is room
            if (totalOccurrences < UINT32_MAX)
            {
                next->occurrences++;

                // If there are more than one positions here, sort the vector to ensure the most likely is first
                if (nextPositions.size() > 1)
                {
                    std::sort(nextPositions.begin(), nextPositions.end(), [](const GatherObservations &a, const GatherObservations &b)
                    {
                        return a.occurrences > b.occurrences;
                    });

                    // Re-reference the next position since its pointer might have changed during the sort
                    next = current->observationsForSpecificNextPosition(resendTookEffectHere, **nextPositionIt);
                }
            }
            current = next;
        }
    }

    void WorkerPathExploration::recordGatherCollisions()
    {
        // Parse the gather position history to ensure we had a valid path
        auto parsedPositionHistory =
                parsePositionHistory(gatherPositionHistory, gatherPositionHistoryStartFrame, executedGatherResendFrames, depot, patch);
        if (!parsedPositionHistory.valid) return;

        // This method gets called 8 frames after the worker starts returning minerals
        // Since a collision adds a full order process timer cycle of delay, we can therefore detect it by checking if the worker
        // is still at the patch and hasn't moved the past two frames
        bool collision = (patch->getDistance(worker) == 0
                && **returnPositionHistory.rbegin() == **(returnPositionHistory.rbegin() + 1)
                && **returnPositionHistory.rbegin() == **(returnPositionHistory.rbegin() + 2));
        if (collision)
        {
            CherryVis::log(worker->getID()) << "Detected collision with patch";
        }

        // Find the root node, which must exist since we have already recorded arrival observations
        auto &rootNodes = mapData.resourceToGatherRootNodes[TilePosition::fromBWAPI(patch->getTilePosition())];
        auto rootNodeIt = rootNodes.find(**gatherPositionHistory.begin());
        if (rootNodeIt == rootNodes.end())
        {
            Log::Get() << "ERROR: Root node not found when registering collision status"
                       << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
            return;
        }
        auto current = &rootNodeIt->second;

        // Now step through the path and record the observations
        for (auto it = gatherPositionHistory.begin(); it != parsedPositionHistory.arrivalPositionIt; it++)
        {
            // At the start of the loop, current is guaranteed to be a pointer to the gather observations for this node

            bool resendTookEffectHere = parsedPositionHistory.resendPositionIts.contains(it);

            // Make the collision observation, using the correct arrival data depending on whether there was a resend taking effect here
            GatherArrivalObservations &arrivalObservations =
                    (resendTookEffectHere ? current->arrivalObservationsAfterResend : current->arrivalObservations);
            arrivalObservations.addCollisionObservation(collision);

            // Find the next position node, which must exist since we have already recorded arrival observations

            // Start by getting the next position
            auto nextPositionIt = it + 1;
            if (nextPositionIt == parsedPositionHistory.arrivalPositionIt) break; // reached end of path

            // Find the next position
            current = current->observationsForSpecificNextPosition(resendTookEffectHere, **nextPositionIt);
            if (!current)
            {
                Log::Get() << "ERROR: Next node not found when registering collision status"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                return;
            }
        }
    }
}
