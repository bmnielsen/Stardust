#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"
#include "PositionObservationMetadata.h"
#include "WorkerGatherStatus.h"

// In our data files we track the full paths between depot and patch, but we only explore positions within the bounds defined here
#define EXPLORE_BEFORE 12
#define EXPLORE_AFTER 5

namespace WorkerMiningOptimization
{
    void initialize();

    void flushObservations();

    void write();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    void flushStartOfMiningObservations(std::map<MyWorker, WorkerGatherStatus> &workerGatherStatuses);
    void handleStartOfMiningPatchSwitch(WorkerGatherStatus &workerStatus);

    WorkerGatherStatus &gatherStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource);
    WorkerGatherStatus *gatherStatusFor(const MyWorker &worker);
    std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &optimalGatherPositionsFor(const Resource &resource);
    std::unordered_set<PositionAndVelocity> &tenDistancePositionsFor(const Resource &resource);

    bool isExploring();
    void setExploring(bool exploring);
}
