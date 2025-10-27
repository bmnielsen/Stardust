#include "PathExplorationModule.h"

namespace MiningOptimizationTraining
{
    namespace
    {
        GatherObservations *observationsForNextPosition(std::vector<GatherObservations> &nextPositions, const PositionOnPath &pos)
        {
            auto nextObservationIt = std::find_if(
                    nextPositions.begin(),
                    nextPositions.end(),
                    [&pos](const GatherObservations &x)
                    {
                        return x.pos == pos;
                    });
            if (nextObservationIt == nextPositions.end()) return nullptr;
            return &*nextObservationIt;
        }
    }

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
        else
        {
            // TODO: Use resend next positions if a resend was sent
            currentGatherNode = observationsForNextPosition(currentGatherNode->nextPositions, **gatherPositionHistory.rbegin());
        }

        // If the current gather node has no information about whether a resend changes the path, query that now
        if (currentGatherNode && currentGatherNode->withinExplorationWindow() && currentGatherNode->canResendChangePath == ResendChangesPath::Unknown)
        {
            auto result = worker->wouldAGatherResendHereChangeThePath();
            if (result.has_value())
            {
                currentGatherNode->canResendChangePath = result.value() ? ResendChangesPath::Yes : ResendChangesPath::No;
            }
        }

        // TODO: Plan and execute resends
    }

    void WorkerPathExploration::recordGatherPath()
    {
        auto parsedPositionHistory = parsePositionHistory(gatherPositionHistory, depot, patch);
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

        // Now step through the path and make the observations
        for (auto it = gatherPositionHistory.begin(); it != parsedPositionHistory.arrivalPositionIt; it++)
        {
            // At the start of the loop, current is guaranteed to be a pointer to the gather observations for this node

            // Increment the occurrences
            if (current->occurrences < UINT32_MAX) current->occurrences++;

            // Reference the correct arrival and next position data depending on whether there was a resend taking effect here
            GatherArrivalObservations *arrivalObservations;
            std::vector<GatherObservations> *nextPositions;
            if (parsedPositionHistory.resendPositionIts.contains(it))
            {
                arrivalObservations = &current->arrivalObservationsAfterResend;
                nextPositions = &current->nextPositionsAfterResend;
            }
            else
            {
                arrivalObservations = &current->arrivalObservations;
                nextPositions = &current->nextPositions;
            }

            // Make the arrival observation. Frames to arrival is equivalent to the distance from here to the arrival position iterator.
            arrivalObservations->addArrivalObservation(
                    ArrivalData::create(std::distance(it, parsedPositionHistory.arrivalPositionIt),
                                        parsedPositionHistory.facingTarget));

            // Find or create the next position node

            // Start by getting the next position
            auto nextPositionIt = it + 1;
            if (nextPositionIt == parsedPositionHistory.arrivalPositionIt) break; // reached end of path

            // Find the next position in the vector
            current = observationsForNextPosition(*nextPositions, **nextPositionIt);
            if (!current)
            {
                current = &nextPositions->emplace_back(GatherObservations{**nextPositionIt});
                CherryVis::log(worker->getID()) << "Discovered new next position " << *current;
            }
        }
    }

    void WorkerPathExploration::recordGatherCollisions()
    {
        // Parse the gather position history to ensure we had a valid path
        auto parsedPositionHistory = parsePositionHistory(gatherPositionHistory, depot, patch);
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

            // Reference the correct arrival and next position data depending on whether there was a resend taking effect here
            GatherArrivalObservations *arrivalObservations;
            std::vector<GatherObservations> *nextPositions;
            if (parsedPositionHistory.resendPositionIts.contains(it))
            {
                arrivalObservations = &current->arrivalObservationsAfterResend;
                nextPositions = &current->nextPositionsAfterResend;
            }
            else
            {
                arrivalObservations = &current->arrivalObservations;
                nextPositions = &current->nextPositions;
            }

            // Make the arrival observation. Frames to arrival is equivalent to the distance from here to the arrival position iterator.
            arrivalObservations->addCollisionObservation(collision);

            // Find the next position node, which must exist since we have already recorded arrival observations

            // Start by getting the next position
            auto nextPositionIt = it + 1;
            if (nextPositionIt == parsedPositionHistory.arrivalPositionIt) break; // reached end of path

            // Find the next position in the vector
            current = observationsForNextPosition(*nextPositions, **nextPositionIt);
            if (!current)
            {
                Log::Get() << "ERROR: Next node not found when registering collision status"
                           << "; worker " << worker->getID() << " @ " << worker->getTilePosition();
                return;
            }
        }
    }
}
