#include "PathExplorationModule.h"

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

                // There was a resend that took effect here, but this hasn't necessarily changed the path.
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
            auto nextObservationIt = std::find_if(
                    nextPositions->begin(),
                    nextPositions->end(),
                    [&nextPositionIt](const GatherObservations &x)
                    {
                        return x.pos == **nextPositionIt;
                    });
            if (nextObservationIt == nextPositions->end())
            {
                current = &nextPositions->emplace_back(GatherObservations{**nextPositionIt});
                CherryVis::log(worker->getID()) << "Discovered new next position " << *current;
            }
            else
            {
                current = &*nextObservationIt;
            }
        }
    }
}
