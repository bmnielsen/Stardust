#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"
#include "GatherPositionObservations.h"
#include "WorkerGatherStatus.h"
#include "ReturnPositionObservations.h"
#include "WorkerReturnStatus.h"
#include "ResourceObservations.h"

#include <optional>

#define ENABLE_GATHER_OPTIMIZATION true
#define ENABLE_RETURN_OPTIMIZATION true
#define ENABLE_PATH_BASED_TAKEOVER true
#define WRITE_DATA_FILES true

// In our data files we track the full paths between depot and patch, but we only explore positions within the bounds defined here
#define GATHER_EXPLORE_BEFORE 12
#define GATHER_EXPLORE_AFTER 5

// Return bounds are relative to the position LF+8 frames before arrival in the normal path, which is often the optimal position to resend from
#define RETURN_EXPLORE_BEFORE 15
#define RETURN_EXPLORE_AFTER 5

// Threshold for other patch gather probability used when optimizing patch locking
#define PATCH_LOCK_THRESHOLD 0.8

namespace WorkerMiningOptimization
{
    void initialize();

    void update();

    void gameEnd();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    void planGatherResendsSingle(WorkerGatherStatus &workerStatus);

    void validatePlannedGatherPathSingle(WorkerGatherStatus &workerStatus,
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition);

    void planGatherResendsDouble(WorkerGatherStatus &workerStatus, GatherPositionObservations &positionMetadata);

    bool validatePlannedGatherPathDouble(WorkerGatherStatus &workerStatus,
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition);

    // Checks for patch lock assuming the worker will have arrived at the patch when the resend takes effect
    std::optional<int> checkForPatchLock(const WorkerGatherStatus &workerStatus, int resendFrame);

    void flushGatherObservations(std::map<MyWorker, WorkerGatherStatus> &workerGatherStatuses);

    void handleGatherPatchSwitch(WorkerGatherStatus &workerStatus);

    WorkerGatherStatus &gatherStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    WorkerGatherStatus *gatherStatusFor(const MyWorker &worker);

    GatherPositionObservations *findGatherPositionObservations(const Resource &resource,
                                                               const PositionAndVelocity &pos,
                                                               bool createIfNotFound,
                                                               std::unique_ptr<GatherPositionObservations> *storage = nullptr);

    std::unordered_set<PositionAndVelocity> &tenDistancePositionsFor(const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    void flushReturnObservations(std::map<MyWorker, WorkerReturnStatus> &workerReturnStatuses);

    WorkerReturnStatus &returnStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    ReturnPositionObservations *findReturnPositionObservations(const Resource &resource,
                                                               const PositionAndVelocity &pos,
                                                               bool createIfNotFound,
                                                               std::unique_ptr<ReturnPositionObservations> *storage = nullptr);

    // Gets observations related to a specific resource
    ResourceObservations &resourceObservationsFor(const Resource &resource);

    bool isExploring();

    void setExploring(bool newExploring);

    bool isUpdatingResourceObservations();

    void setUpdateResourceObservations(bool newUpdateResourceObservations);
}
