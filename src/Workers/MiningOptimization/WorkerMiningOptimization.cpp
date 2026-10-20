/* Worker mining optimization is split into multiple files */
/* This file contains the data maps and logic to read and write the data files */

#include "WorkerMiningOptimization.h"

#include "ObservationDataFiles.h"

#include "TilePosition.h"
#include "PositionAndVelocity.h"

#if INSTRUMENTATION_ENABLED
#define OUTPUT_METADATA_ANALYSIS false
#endif

namespace WorkerMiningOptimization
{
    namespace
    {
        /* Whether we are exploring new positions */
        bool exploring = false;

        /* Whether we are updating resource observations */
        bool updatingResourceObservations = false;

        /* The map hash of the currently-loaded data */
        std::string mapHashOfCurrentData;

        /* Metadata for positions used for optimizing approach to the patch */
        std::map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPositionObservations>> resourceToFullGatherPositionObservations;
        std::map<TilePosition, std::unordered_map<PositionAndVelocity, std::vector<uint8_t>>> resourceToGatherPositionObservations;

        /* Metadata for positions LF+1 frames before reaching 10 or less distance from the patch */
        /* Workers can try to switch to another patch if their chosen patch is being mined once they reach this distance, so we use these */
        /* positions to detect when we need to start resending gather commands to ensure mineral locking */
        std::map<TilePosition, std::unordered_set<PositionAndVelocity>> resourceTo10DistancePositions;

        /* Worker state for those on their way to the patch */
        std::map<OwnedWorker, WorkerGatherStatus> workerGatherStatuses;

        /* Metadata for positions used for optimizing return of resources */
        std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> resourceToFullReturnPositionObservations;
        std::map<TilePosition, std::unordered_map<PositionAndVelocity, std::vector<uint8_t>>> resourceToReturnPositionObservations;

        /* Worker state for those returning resources */
        std::map<OwnedWorker, WorkerReturnStatus> workerReturnStatuses;

        /* Resource observations */
        std::map<TilePosition, ResourceObservations> resourceToResourceObservations;
    }

    void initialize()
    {
        workerGatherStatuses.clear();
        workerReturnStatuses.clear();

        if (BWAPI::Broodwar->mapHash() != mapHashOfCurrentData)
        {
            ObservationDataFiles::readGatherPositionObservations(resourceToFullGatherPositionObservations);
            if (resourceToFullGatherPositionObservations.empty())
            {
                ObservationDataFiles::readGatherPositionObservations(resourceToGatherPositionObservations);
            }
            ObservationDataFiles::read10DistanceObservations(resourceTo10DistancePositions);
            ObservationDataFiles::readReturnPositionObservations(resourceToFullReturnPositionObservations);
            if (resourceToFullReturnPositionObservations.empty())
            {
                ObservationDataFiles::readReturnPositionObservations(resourceToReturnPositionObservations);
            }
            ObservationDataFiles::readResourceObservations(exploring || updatingResourceObservations, resourceToResourceObservations);

            mapHashOfCurrentData = BWAPI::Broodwar->mapHash();
        }
        else
        {
            AppLog::Get() << "Using already-loaded mining optimization data";
        }
    }

    void flushObservations()
    {
        flushGatherObservations(workerGatherStatuses);
        flushReturnObservations(workerReturnStatuses);
    }

    void write()
    {
#if WRITE_DATA_FILES
        if (exploring)
        {
            ObservationDataFiles::writeFullGatherPositionObservations(resourceToFullGatherPositionObservations);
            ObservationDataFiles::write10DistanceObservations(resourceTo10DistancePositions);
            ObservationDataFiles::writeFullReturnPositionObservations(resourceToFullReturnPositionObservations);
        }

        if (updatingResourceObservations)
        {
            ObservationDataFiles::writeResourceObservations(false, resourceToResourceObservations);
        }
#endif

#if OUTPUT_METADATA_ANALYSIS
        {
            int total = 0;
            int unstablePath = 0;
            int unstablePathAtExploreHorizon = 0;
            int neverUsed = 0;
            auto hasBeenUsed = [](const GatherResendArrivalObservations &observations)
            {
                if (observations.empty()) return false;
                if (observations.packedArrivalDelayAndFacingPatchToOccurrences.size() > 1) return true;
                return observations.packedArrivalDelayAndFacingPatchToOccurrences.begin()->second > 1;
            };
            for (const auto &[resource, optimalPositions] : resourceToFullGatherPositionObservations)
            {
                for (const auto &[_, optimalPosition] : optimalPositions)
                {
                    total++;
                    bool unstable = false;

                    std::map<int, const SecondResendGatherPositionObservations *> secondResendPositionsByDelta;
                    bool used = hasBeenUsed(optimalPosition.noSecondResendArrivalObservations);
                    for (const auto &[_, secondResendPosition] : optimalPosition.secondResendObservations)
                    {
                        used = used || hasBeenUsed(secondResendPosition.arrivalObservations);
                        unstable = unstable || (secondResendPosition.nextPositionAndOccurrences.size() > 1);
                        if (!unstable)
                        {
                            if (secondResendPositionsByDelta.contains(secondResendPosition.deltaToFirstResend))
                            {
                                unstable = true;
                            }
                            else
                            {
                                secondResendPositionsByDelta[secondResendPosition.deltaToFirstResend] = &secondResendPosition;
                            }
                        }
                    }

                    if (!used) neverUsed++;

                    std::vector<const GatherPositionObservations *> noResendNextPositions = optimalPosition.followingPositionsIfStable(
                            optimalPositions);
                    if (noResendNextPositions.empty() && !optimalPosition.nextPositionAndOccurrences.empty())
                    {
                        unstable = true;
                    }

                    if (unstable)
                    {
                        unstablePath++;
                    }
                    if (unstable && optimalPosition.probableDeltaToBenchmark() == -GATHER_EXPLORE_BEFORE)
                    {
                        unstablePathAtExploreHorizon++;
                    }
                }
            }

            if (total > 0)
            {
                AppLog::Get() << std::fixed << std::setprecision(1)
                           << "\nStatistics for " << total << " gather resend positions:"
                           << "\nNever used:         " << neverUsed << " (" << (100.0 * (double)neverUsed / (double)total) << "%)"
                           << "\nUnstable path:      " << unstablePath << " (" << (100.0 * (double)unstablePath / (double)total) << "%)"
                           << "\nUnstable path @-" << GATHER_EXPLORE_BEFORE << ": "
                           << unstablePathAtExploreHorizon << " (" << (100.0 * (double)unstablePathAtExploreHorizon / (double)total) << "%)";
            }
        }
#endif
    }

    WorkerGatherStatus &gatherStatusFor(const OwnedWorker &wrk, const OwnedUnit &depot, const Resource &resource)
    {
        auto workerStatusIt = workerGatherStatuses.find(wrk);
        if (workerStatusIt == workerGatherStatuses.end())
        {
            workerStatusIt = workerGatherStatuses.emplace(wrk, WorkerGatherStatus{wrk, depot, resource}).first;
        }
        else if (workerStatusIt->second.lastProcessedFrame != (frameNow - 1)
                 || workerStatusIt->second.depot != depot
                 || workerStatusIt->second.resource != resource)
        {
#if INSTRUMENTATION_ENABLED_VERBOSE
            Telemetry::log(wrk->ident) << "Resetting gather status"
                                       << "; lastProcessedFrame=" << workerStatusIt->second.lastProcessedFrame << " vs. currentFrame=" << frameNow
                                       << "; depot@" << workerStatusIt->second.depot->getTilePosition() << " vs. new@" << depot->getTilePosition()
                                       << "; resource@" << workerStatusIt->second.resource->tilePos << " vs. new@" << resource->tilePos;
#endif
            workerStatusIt->second.reset();
            workerStatusIt->second.depot = depot;
            workerStatusIt->second.resource = resource;
        }
        return workerStatusIt->second;
    }

    WorkerReturnStatus &returnStatusFor(const OwnedWorker &wrk, const OwnedUnit &depot, const Resource &resource)
    {
        auto workerStatusIt = workerReturnStatuses.find(wrk);
        if (workerStatusIt == workerReturnStatuses.end())
        {
            workerStatusIt = workerReturnStatuses.emplace(wrk, WorkerReturnStatus{wrk, depot, resource}).first;
        }
        else if (workerStatusIt->second.lastProcessedFrame != (frameNow - 1)
                 || workerStatusIt->second.depot != depot
                 || workerStatusIt->second.resource != resource)
        {
            workerStatusIt->second.reset();
            workerStatusIt->second.depot = depot;
            workerStatusIt->second.resource = resource;
        }
        return workerStatusIt->second;
    }

    WorkerGatherStatus *gatherStatusFor(const OwnedWorker &wrk)
    {
        if (!wrk->exists()) return nullptr;

        auto workerStatusIt = workerGatherStatuses.find(wrk);
        if (workerStatusIt == workerGatherStatuses.end()) return nullptr;

        if (workerStatusIt->second.lastProcessedFrame < (frameNow - 1)) return nullptr;

        if (!workerStatusIt->second.depot->exists()) return nullptr;
        if (workerStatusIt->second.resource->destroyed) return nullptr;

        return &workerStatusIt->second;
    }

    GatherPositionObservations *findGatherPositionObservations(const Resource &resource, 
                                                               const PositionAndVelocity &pt,
                                                               bool createIfNotFound,
                                                               std::unique_ptr<GatherPositionObservations> *storage)
    {
        /* If we have full observations, return a pointer without requiring any additional storage */
        if (!resourceToFullGatherPositionObservations.empty() || isExploring())
        {
            auto &positions = resourceToFullGatherPositionObservations[TilePosition::fromBWAPI(resource->tilePos)];
            auto iterRef = positions.find(pt);
            if (iterRef != positions.end()) return &(iterRef->second);
            if (!createIfNotFound) return nullptr;
            auto outcome = positions.emplace(pt, pt);
            return &(outcome.first->second);
        }

        /* Check if we have a data buffer for this position */
        auto &positions = resourceToGatherPositionObservations[TilePosition::fromBWAPI(resource->tilePos)];
        auto iterRef = positions.find(pt);
        if (iterRef == positions.end()) return nullptr;

        /* We have a data buffer, so deserialize and store in the unique storage pointer */
        auto outcome = ObservationDataFiles::deserializeGatherPositionObservations(iterRef->second);
        *storage = std::move(outcome);
        return storage->get();
    }


    ReturnPositionObservations *findReturnPositionObservations(const Resource &resource,
                                                               const PositionAndVelocity &pt,
                                                               bool createIfNotFound,
                                                               std::unique_ptr<ReturnPositionObservations> *storage)
    {
        /* If we have full observations, return a pointer without requiring any additional storage */
        if (!resourceToFullReturnPositionObservations.empty() || isExploring())
        {
            auto &positions = resourceToFullReturnPositionObservations[TilePosition::fromBWAPI(resource->tilePos)];
            auto iterRef = positions.find(pt);
            if (iterRef != positions.end()) return &(iterRef->second);
            if (!createIfNotFound) return nullptr;
            auto outcome = positions.emplace(pt, pt);
            return &(outcome.first->second);
        }

        /* Check if we have a data buffer for this position */
        auto &positions = resourceToReturnPositionObservations[TilePosition::fromBWAPI(resource->tilePos)];
        auto iterRef = positions.find(pt);
        if (iterRef == positions.end()) return nullptr;

        /* We have a data buffer, so deserialize and store in the unique storage pointer */
        auto outcome = ObservationDataFiles::deserializeReturnPositionObservations(iterRef->second);
        *storage = std::move(outcome);
        return storage->get();
    }

    std::unordered_set<PositionAndVelocity> &tenDistancePositionsFor(const Resource &resource)
    {
        return resourceTo10DistancePositions[TilePosition::fromBWAPI(resource->tilePos)];
    }

    ResourceObservations &resourceObservationsFor(const Resource &resource)
    {
        auto tilePos = TilePosition::fromBWAPI(resource->tilePos);
        auto iterRef = resourceToResourceObservations.find(tilePos);
        if (iterRef == resourceToResourceObservations.end())
        {
            iterRef = resourceToResourceObservations.emplace(tilePos, tilePos).first;
        }
        return iterRef->second;
    }

    bool isExploring()
    {
        return exploring;
    }

    void setExploring(bool newExploring)
    {
        exploring = newExploring;
    }

    bool isUpdatingResourceObservations()
    {
        return updatingResourceObservations;
    }

    void setUpdateResourceObservations(bool newUpdateResourceObservations)
    {
        updatingResourceObservations = newUpdateResourceObservations;
    }
}