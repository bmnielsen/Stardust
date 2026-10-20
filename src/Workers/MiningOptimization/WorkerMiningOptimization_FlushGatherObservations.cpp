/* Worker mining optimization is split into multiple files */
/* This file contains the logic needed to update the data maps with new observations related to optimizing the start of mining */

#include "WorkerMiningOptimization.h"
#include "DebugFlag_WorkerMiningOptimization.h"

#include "PositionAndVelocity.h"
#include "OccurrencesAndCollisions.h"
#include "GatherPositionObservations.h"
#include "WorkerGatherStatus.h"
#include "WorkerMiningInstrumentation.h"
#include "Units.h"
#include "Map.h"
#include "Workers.h"

#include "Geo.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define PATCHLOCKING_DEBUG true
#define PATCHLOCKING_DEBUG_VERBOSE false
#define VALIDATE_ARRIVAL_FRAMES false
#endif

namespace WorkerMiningOptimization
{
    namespace
    {
#if PATCHLOCKING_DEBUG
        std::deque<std::set<Resource>> actualOtherPatchesGatheredHistory;
        std::deque<std::set<Resource>> predictedOtherPatchesGatheredHistory;
        int patchLockingTotalCollections;
        int patchLockingPotentialPatchLocks;
        int patchLockingPredictedPatchLocksAtCommandFrame;
        int patchLockingSuccessfulPatchLocks;
#endif

        struct PositionsInHistory
        {
            std::vector<std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator> resendItsBeforeArrival;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator arrivalPositionIt;
            std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator tenDistancePositionIt;

            std::vector<GatherPositionObservationPtr> positionHistory;
            std::vector<GatherPositionObservationPtr> resendsWithObservationData;

            /* Whether the worker is facing the patch closely enough to be able to begin mining without incurring an extra order process timer */
            /* cycle to make a turn. */
            bool facingPatch = true;
        };

        bool extractPositionsInHistory(PositionsInHistory &positionsInHistory,
                                       WorkerGatherStatus &wrkStatus,
                                       bool createObservations)
        {
            if (!wrkStatus.hasLeftDepot && wrkStatus.pathStartsAtDepot && !wrkStatus.switchedPatches)
            {
                AppLog::Get() << "ERROR: Worker was not tracked as leaving depot"
                           << "; worker id " << wrkStatus.wrk->ident << " @ " << wrkStatus.wrk->getTilePosition();
                return false;
            }

            /* If the path is too short to possibly optimize, return here */
            /* This might happen if we have a case where the worker gets reassigned or otherwise doesn't follow a normal mining path */
            if (wrkStatus.positionHistory.size() < (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
#if OPTIMALPOSITIONS_DEBUG
                Telemetry::log(wrkStatus.wrk->ident) << "Position history too short to use for path optimization";
#endif
                return false;
            }

            std::vector<std::vector<std::shared_ptr<const PositionAndVelocity>>::iterator> resendPositionIts;
            auto extractPositions = [&]()
            {
                positionsInHistory.resendItsBeforeArrival.clear();
                positionsInHistory.arrivalPositionIt = wrkStatus.positionHistory.end();
                positionsInHistory.tenDistancePositionIt = wrkStatus.positionHistory.end();

                positionsInHistory.positionHistory.clear();
                positionsInHistory.resendsWithObservationData.clear();

                resendPositionIts.clear();
                auto nextResendPositionIt = wrkStatus.resentPositions.begin();

                for (auto iterRef = wrkStatus.positionHistory.begin(); iterRef != wrkStatus.positionHistory.end(); iterRef++)
                {
                    if (nextResendPositionIt != wrkStatus.resentPositions.end() && **nextResendPositionIt == **iterRef)
                    {
                        /* We filter out resend positions after arrival later */
                        resendPositionIts.push_back(iterRef);
                        nextResendPositionIt++;
                    }

                    auto span = Geom::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                        (*iterRef)->pt(),
                                                        BWAPI::UnitTypes::Resource_Mineral_Field,
                                                        wrkStatus.resource->midpoint);

                    /* Arrival position is defined as the position where: */
                    /* - distance to the patch is 0 */
                    /* - position is the same as the position at mining start */
                    /* - heading is stable */
                    if (positionsInHistory.arrivalPositionIt == wrkStatus.positionHistory.end()
                        && span == 0
                        && wrkStatus.wrk->prevPos == (*iterRef)->pt()
                        && PositionAndVelocity::isStableArrivalPosition(wrkStatus.positionHistory, iterRef))
                    {
                        positionsInHistory.arrivalPositionIt = iterRef;
                    }

                    if (positionsInHistory.tenDistancePositionIt == wrkStatus.positionHistory.end() && span <= 10)
                    {
                        positionsInHistory.tenDistancePositionIt = iterRef - BWAPI::Broodwar->getLatencyFrames() - 1;
                    }
                }
            };
            extractPositions();

            /* Shorten too long paths */
            auto startToEnd = std::distance(wrkStatus.positionHistory.begin(), positionsInHistory.arrivalPositionIt);
            int pathLengthLimit = wrkStatus.pathStartsAtSpawnPosition ? 125 : 100;
            if (startToEnd > pathLengthLimit)
            {
#if OPTIMALPOSITIONS_DEBUG
                Telemetry::log(wrkStatus.wrk->ident) << "Path length from start to arrival is " << startToEnd
                                                        << "; trimming to " << pathLengthLimit;
#endif

                wrkStatus.positionHistory.erase(wrkStatus.positionHistory.begin(),
                                                   wrkStatus.positionHistory.begin() + (startToEnd - pathLengthLimit));
                extractPositions();
                startToEnd = std::distance(wrkStatus.positionHistory.begin(), positionsInHistory.arrivalPositionIt);
            }

            /* Don't process too short paths */
            if (startToEnd < (BWAPI::Broodwar->getLatencyFrames() + 11))
            {
#if OPTIMALPOSITIONS_DEBUG
                Telemetry::log(wrkStatus.wrk->ident) << "Path length from start to arrival is " << startToEnd
                                                        << ", too short to use for path optimization";
#endif
                return false;
            }

            /* Clear the ten distance position iterator if it is invalid */
            if (positionsInHistory.tenDistancePositionIt != wrkStatus.positionHistory.end() &&
                std::distance(wrkStatus.positionHistory.begin(), positionsInHistory.tenDistancePositionIt) < 0)
            {
                positionsInHistory.tenDistancePositionIt = wrkStatus.positionHistory.end();
            }

            /* Return false if any of the resend positions couldn't be found */
            if (wrkStatus.resentPositions.size() != resendPositionIts.size())
            {
#if LOGGING_ENABLED
                AppLog::Get() << "ERROR: Not all resent positions found in position history"
                           << "; worker id " << wrkStatus.wrk->ident << " @ " << wrkStatus.wrk->getTilePosition();
#endif
                return false;
            }

            /* Filter out the resends that happened before arrival */
            for (auto resendPositionIt : resendPositionIts)
            {
                if (std::distance(resendPositionIt, positionsInHistory.arrivalPositionIt) <= BWAPI::Broodwar->getLatencyFrames())
                {
                    break;
                }

                positionsInHistory.resendItsBeforeArrival.push_back(resendPositionIt);
            }

            /* No further calculations are needed unless we are exploring */
            if (!isExploring()) return true;

            /* Determine if the worker's heading will require turning to start mining */
            /* The worker has two frames to turn before the order process timer will be nonzero and it will have to wait 9 frames */
            /* The worker always points directly at the center of the patch when mining */
            auto vectorToPatch = wrkStatus.resource->midpoint - wrkStatus.wrk->prevPos;
            auto angleDiff = Geom::BWAngleDiff(wrkStatus.wrk->BWHeading(), Geom::BWDirection(vectorToPatch));
            if (angleDiff > 2 * wrkStatus.wrk->type.turnRadius())
            {
                positionsInHistory.facingPatch = false;
#if OPTIMALPOSITIONS_DEBUG
                Telemetry::log(wrkStatus.wrk->ident) << "Not facing patch, angle diff is " << angleDiff;
#endif
            }

            /* Reference the observations and potentially create new nodes */

            /* Start by finding or creating the root node */
            auto rootNode = findGatherPositionObservations(wrkStatus.resource, **wrkStatus.positionHistory.begin(), createObservations);
            if (!rootNode) return false;
            if (rootNode->occurrences < UINT32_MAX) rootNode->occurrences++;

            auto current = GatherPositionObservationPtr(rootNode);
            positionsInHistory.positionHistory.push_back(current);

            /* Add main line positions up to the first resend (or arrival position if there was no resend) */
            auto limit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames();
            if (!positionsInHistory.resendItsBeforeArrival.empty())
            {
                limit = positionsInHistory.resendItsBeforeArrival[0] + 1;
            }
            for (auto positionIt = wrkStatus.positionHistory.begin() + 1; positionIt != limit; positionIt++)
            {
                /* Try to find the next position */
                auto [next, atLimit] = findNextPositionCheckingOccurrences(**positionIt, current.pt->nextPositions);

                /* If we have a new path branch that we can't create, bail out now */
                if (!next && (!createObservations || atLimit))
                {
#if OPTIMALPOSITIONS_DEBUG
                    if (createObservations && atLimit)
                    {
                        Telemetry::log(wrkStatus.wrk->ident) << "Found new branch in path, but observation limit has been reached";
                    }
#endif
                    return false;
                }

                /* Create a new item if needed, otherwise bump the occurrence count if possible */
                if (!next)
                {
                    next = &current.pt->nextPositions.emplace_back(**positionIt);
                }
                else if (!atLimit)
                {
                    next->occurrences++;
                }
                updateNextOccurenceRates(current.pt->nextPositions);

                current = GatherPositionObservationPtr(next);
                positionsInHistory.positionHistory.push_back(current);
            }
            if (positionsInHistory.resendItsBeforeArrival.empty()) return true;

            positionsInHistory.resendsWithObservationData.push_back(current);

            /* Add second resend positions up to the second resend (or arrival position if there was no second resend) */
            limit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames();
            if (positionsInHistory.resendItsBeforeArrival.size() > 1)
            {
                limit = positionsInHistory.resendItsBeforeArrival[1] + 1;
            }
            for (auto positionIt = positionsInHistory.resendItsBeforeArrival[0] + 1; positionIt != limit; positionIt++)
            {
                /* Try to find the next position */
                auto &nextPositions = current.nextSecondResendPositions();
                auto [next, atLimit] = findNextPositionCheckingOccurrences(**positionIt, nextPositions);

                /* If we have a new path branch that we can't create, bail out now */
                if (!next && (!createObservations || atLimit))
                {
#if OPTIMALPOSITIONS_DEBUG
                    if (createObservations && atLimit)
                    {
                        Telemetry::log(wrkStatus.wrk->ident) << "Found new branch in second resend path, but observation limit has been reached";
                    }
#endif
                    return false;
                }

                /* Create a new item if needed, otherwise bump the occurrence count if possible */
                if (!next)
                {
                    next = &nextPositions.emplace_back(**positionIt);
                }
                else if (!atLimit)
                {
                    next->occurrences++;
                }
                updateNextOccurenceRates(nextPositions);

                current = GatherPositionObservationPtr(next);
                positionsInHistory.positionHistory.push_back(current);
            }

            if (positionsInHistory.resendItsBeforeArrival.size() > 1)
            {
                positionsInHistory.resendsWithObservationData.push_back(current);
            }

            return true;
        }

        /* Used to track whether a mining worker collides with the patch after mining */
        struct MiningWorker
        {
            OwnedWorker wrk;
            Resource resource;
            std::vector<PositionAndVelocity> positionHistoryWithObservationData;
            std::vector<PositionAndVelocity> resendsWithObservationData;
            size_t resendsBeforeArrivalCount;
        };

        std::vector<MiningWorker> miningWorkers;

#if OPTIMALPOSITIONS_DEBUG
        std::set<BWAPI::TilePosition> exploredPatches;
        int collisions = 0;
        int noncollisions = 0;
#endif
#if LOGGING_ENABLED
        unsigned int hadPathData = 0;
        unsigned int didNotHavePathData = 0;
#endif

        void handlePossiblePatchCollision(MiningWorker &miningWorker)
        {
            /* There is a collision if the worker is at the patch and isn't moving */
            bool collision = (miningWorker.resource->getDistance(miningWorker.wrk) == 0
                    && (frameNow - miningWorker.wrk->frameLastMoved) > 2);
            WorkerMiningInstrumentation::trackCollisionObservation(miningWorker.resource, collision);

#if OPTIMALPOSITIONS_DEBUG
            if (collision)
            {
                Telemetry::log(miningWorker.wrk->ident) << "Collision with patch";
                collisions++;
            }
            else
            {
                noncollisions++;
            }
#endif
            if (!WorkerMiningOptimization::isExploring()) return;

            /* If there have been more than two resends, we can't record an observation since the path may have changed in an unexpected way */
            if (miningWorker.resendsBeforeArrivalCount > 2) return;

            /* Guard against invalid data */
            if (miningWorker.positionHistoryWithObservationData.empty()) return;
            if (miningWorker.resendsWithObservationData.size() != miningWorker.resendsBeforeArrivalCount) return;

            /* Find the root node */
            auto rootNode = findGatherPositionObservations(miningWorker.resource,
                                                           miningWorker.positionHistoryWithObservationData.front(),
                                                           false);
            if (!rootNode)
            {
#if LOGGING_ENABLED
                AppLog::Get() << "ERROR: No root node found when handling gather collisions"
                           << "; worker id " << miningWorker.wrk->ident << " @ " << miningWorker.wrk->getTilePosition();
#endif
                return;
            }

            std::shared_ptr<PositionAndVelocity> firstResendPosition;
            std::shared_ptr<PositionAndVelocity> lastResendPosition;
            if (!miningWorker.resendsWithObservationData.empty())
            {
                firstResendPosition = std::make_shared<PositionAndVelocity>(miningWorker.resendsWithObservationData.front());
                lastResendPosition = std::make_shared<PositionAndVelocity>(miningWorker.resendsWithObservationData.back());
            }

            auto recordObservationsOnNode = [&](GatherPositionObservationPtr &node)
            {
                if (lastResendPosition)
                {
                    if (node.position() == *lastResendPosition)
                    {
                        node.resendArrivalObservations().addCollision(collision);
                    }
                    return;
                }

                node.pt->addNoResendCollision(collision);
            };

            auto current = GatherPositionObservationPtr(rootNode);
            recordObservationsOnNode(current);

            for (auto positionIt = miningWorker.positionHistoryWithObservationData.begin() + 1;
                 positionIt != miningWorker.positionHistoryWithObservationData.end();
                 positionIt++)
            {
                auto next = current.nextPositionIfExists(*positionIt, firstResendPosition);
                if (!next) break;

                current = *next;
                recordObservationsOnNode(current);
            }
        }

        void updateApproachOptimization(WorkerGatherStatus &wrkStatus, PositionsInHistory &positionsInHistory)
        {
#if LOGGING_ENABLED
            auto &wrk = wrkStatus.wrk;
#endif

            /* Iterator to the apparent optimal position in the position history */
            auto optimalPositionIt = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames() - 11;

#if LOGGING_ENABLED
            if (!isExploring())
            {
                if (wrkStatus.plannedResendPosition && wrkStatus.resentPositions.empty())
                {
                    AppLog::Get() << "WARNING: Worker didn't resend at planned position " << *wrkStatus.plannedResendPosition
                               << "; worker id " << wrk->ident << " @ " << wrk->getTilePosition();
                }
                else if (wrkStatus.plannedSecondResendPosition && wrkStatus.resentPositions.size() < 2)
                {
                    AppLog::Get() << "WARNING: Worker didn't resend at second planned position " << *wrkStatus.plannedSecondResendPosition
                               << "; worker id " << wrk->ident << " @ " << wrk->getTilePosition();
                }
            }
#endif

            /* Check if we want to make any provisional observations about resend positions where we know the worker doesn't arrive on time, */
            /* but don't know the exact timing as we were forced to resend another command later */
            /* We only do this if there are no earlier observations of the position, and we replace this observation later if we get a clean */
            /* path */
            if (wrkStatus.switchedPatches || positionsInHistory.resendItsBeforeArrival.size() > std::min(2, wrkStatus.plannedResendCount()))
            {
                auto provisionalObservation = [&](int resendIndex)
                {
                    /* There must be a resend after this - otherwise we could just make a normal observation */
                    if (positionsInHistory.resendItsBeforeArrival.size() < (resendIndex + 2)) return;

                    /* Must have resend data for this position */
                    if (positionsInHistory.resendsWithObservationData.size() > resendIndex) return;

                    /* There must have been a full order process cycle after this resend before the next one */
                    if (std::distance(positionsInHistory.resendItsBeforeArrival[resendIndex],
                                      positionsInHistory.resendItsBeforeArrival[resendIndex + 1]) < 11)
                    {
                        return;
                    }

                    /* Reference the observations for this position */
                    auto &observations = positionsInHistory.resendsWithObservationData[resendIndex].resendArrivalObservations();

                    /* There must not have been any observations already */
                    if (!observations.empty()) return;

                    /* Make the observation */
                    observations.addArrival(
                            (int)std::distance(positionsInHistory.resendItsBeforeArrival[resendIndex], optimalPositionIt),
                            positionsInHistory.facingPatch);

#if OPTIMALPOSITIONS_DEBUG
                    if (resendIndex == 1)
                    {
                        Telemetry::log(wrk->ident) << "Added provisional observation of " << **positionsInHistory.resendItsBeforeArrival[0]
                                                   << " : " << **positionsInHistory.resendItsBeforeArrival[1];
                    }
                    else
                    {
                        Telemetry::log(wrk->ident) << "Added provisional observation of " << **positionsInHistory.resendItsBeforeArrival[0];
                    }
#endif
                };
                if (wrkStatus.plannedResendPosition) provisionalObservation(0);
                if (wrkStatus.plannedSecondResendPosition) provisionalObservation(1);
                if (wrkStatus.switchedPatches || positionsInHistory.resendItsBeforeArrival.size() > 2) return;
            }

            /* If we sent no command, record the path for exploration */
            if (positionsInHistory.resendItsBeforeArrival.empty())
            {
                /* Update the metadata for the positions in the path */
                auto optimalPositionDataIt =
                        positionsInHistory.positionHistory.begin() + std::distance(wrkStatus.positionHistory.begin(), optimalPositionIt);
                for (auto positionIt = positionsInHistory.positionHistory.begin();
                     positionIt != positionsInHistory.positionHistory.end();
                     positionIt++)
                {
                    auto delta = (int)std::distance(optimalPositionDataIt, positionIt);

#if OPTIMALPOSITIONS_DEBUG
                    if (positionIt->pt->deltaToBenchmarkAndOccurrenceRate.empty())
                    {
#if OPTIMALPOSITIONS_DEBUG_VERBOSE
                        Telemetry::log(wrk->ident) << "Added metadata for " << *positionIt << " at delta " << delta;
#endif
                    }
                    else if (!positionIt->pt->deltaToBenchmarkAndOccurrenceRate.contains((int8_t)delta))
                    {
                        Telemetry::log(wrk->ident) << "New delta of " << delta << " came up for " << positionIt->pt->pt;
                    }
#endif

                    positionIt->pt->addDeltaToBenchmark(delta);
                }
                return;
            }

            auto lastResendPositionIt = *positionsInHistory.resendItsBeforeArrival.rbegin();

#if OPTIMALPOSITIONS_DEBUG
            bool exploring = positionsInHistory.resendsWithObservationData.rbegin()->resendArrivalObservations().addArrival(
                    (int)std::distance(lastResendPositionIt, optimalPositionIt), positionsInHistory.facingPatch);

#if OPTIMALPOSITIONS_DEBUG_VERBOSE
            if (positionsInHistory.resendItsBeforeArrival.size() > 1)
            {
                Telemetry::log(wrk->ident) << "Added observation of " << positionsInHistory.resendsWithObservationData[0]
                                           << " : " << positionsInHistory.resendsWithObservationData[1];
            }
            else
            {
                Telemetry::log(wrk->ident) << "Added observation of " << positionsInHistory.resendsWithObservationData[0];
            }
#endif

            if (exploring)
            {
                exploredPatches.insert(wrkStatus.resource->tilePos);
            }
#else
            /* Track the observation */
            positionsInHistory.resendsWithObservationData.rbegin()->resendArrivalObservations().addArrival(
                    (int)std::distance(lastResendPositionIt, optimalPositionIt), positionsInHistory.facingPatch);
#endif

/* // Consider exploration of second resend positions */
/* // This is not needed if: */
/* // - There was a second resend */
/* // - The information for this path is incomplete */
/* // - This position is not inside our optimization horizon */
/* // - The path did not start at the depot */
/* if (secondResendData) return; */
/* if (resentPositionData.deltaToBenchmarkAndOccurrences.empty()) return; */
/* int probableDeltaToBenchmark = resentPositionData.probableDeltaToBenchmark(); */
/* if (probableDeltaToBenchmark < -GATHER_EXPLORE_BEFORE) return; */
/* if (probableDeltaToBenchmark > GATHER_EXPLORE_AFTER) return; */
/* if (!workerStatus.pathStartsAtDepot) return; */
/* // Check if the path after the resend is the same as the path without a resend */
/* // If so, we don't bother tracking second resends on this, as they will be the same as the normal path */
/* auto pathsMatch = [&]() */
/* { */
/* if (resentPositionData.resendChangesPath == ResendChangesPath::Yes) return false; */
/* auto noResendPath = resentPositionData.followingPositionsIfStable(optimalGatherPositions); */
/* if (noResendPath.empty()) return false; // No resend path is unstable */
/* size_t noResendPathIdx = 0; */
/* for (auto positionIt = positionsInHistory.resendPositionIts[0] + 1; positionIt != positionsInHistory.arrivalPositionIt; positionIt++) */
/* { */
/* if (noResendPath[noResendPathIdx]->pos != **positionIt) */
/* { */
/* return false; */
/* } */
/* noResendPathIdx++; */
/* if (noResendPathIdx >= noResendPath.size()) break; */
/* } */
/* return true; */
/* }; */
/* if (pathsMatch()) */
/* { */
/* resentPositionData.resendChangesPath = ResendChangesPath::No; */
/* return; */
/* } */
/* // If this is the first detection of a changed path, add the existing next positions as a second resend position */
/* if (resentPositionData.resendChangesPath != ResendChangesPath::Yes) */
/* { */
/* resentPositionData.resendChangesPath = ResendChangesPath::Yes; */
/* for (const auto &[nextPosition, _] : resentPositionData.nextPositionAndOccurrences) */
/* { */
/* auto secondResendObservationsIt = resentPositionData.secondResendObservations.find(nextPosition); */
/* if (secondResendObservationsIt == resentPositionData.secondResendObservations.end()) */
/* { */
/* resentPositionData.secondResendObservations.emplace( */
/* nextPosition, */
/* SecondResendGatherPositionObservations{1}); */
/* #if OPTIMALPOSITIONS_DEBUG_VERBOSE */
/* CherryVis::log(worker->id) << "Added metadata for " << resentPositionData */
/* << " : " << nextPosition */
/* << " after discovering unstable path after resends"; */
/* #endif */
/* } */
/* } */
/* } */
/* // Queue up second resend positions to explore */
/* auto limit = positionsInHistory.arrivalPositionIt - BWAPI::Broodwar->getLatencyFrames(); */
/* for (auto positionIt = positionsInHistory.resendPositionIts[0] + 1; std::distance(positionIt, limit) > 0; positionIt++) */
/* { */
/* auto secondResendObservationsIt = resentPositionData.secondResendObservations.find(**positionIt); */
/* if (secondResendObservationsIt != resentPositionData.secondResendObservations.end()) continue; */
/* resentPositionData.secondResendObservations.emplace( */
/* **positionIt, */
/* SecondResendGatherPositionObservations{ */
/* (uint8_t)std::distance(positionsInHistory.resendPositionIts[0], positionIt)}); */
/* #if OPTIMALPOSITIONS_DEBUG_VERBOSE */
/* CherryVis::log(worker->id) << "Added metadata for " << resentPositionData */
/* << " : " << **positionIt */
/* << ", delta " << std::distance(positionsInHistory.resendPositionIts[0], positionIt); */
/* #endif */
/* } */
        }

        void updateTenDistancePosition(WorkerGatherStatus &wrkStatus, PositionsInHistory &positionsInHistory, bool switchedPatch)
        {
            /* Update 10-distance position */
            /* For simplicity we track all of them we encounter even though there is some overlap with the "main" path data */
            if (positionsInHistory.tenDistancePositionIt != wrkStatus.positionHistory.end() &&
                (positionsInHistory.resendItsBeforeArrival.empty() ||
                 std::distance(positionsInHistory.tenDistancePositionIt, positionsInHistory.resendItsBeforeArrival[0]) > 0))
            {
#if TAKEOVER_DEBUG
                auto outcome = tenDistancePositionsFor(wrkStatus.resource).insert(**positionsInHistory.tenDistancePositionIt);
                if (outcome.second)
                {
                    Telemetry::log(wrkStatus.wrk->ident) << "Added new 10-distance position " << **positionsInHistory.tenDistancePositionIt;
                }
#else
                tenDistancePositionsFor(wrkStatus.resource).insert(**positionsInHistory.tenDistancePositionIt);
#endif
            }
        }
    }

    void flushGatherObservations(std::map<OwnedWorker, WorkerGatherStatus> &workerGatherStatuses)
    {
        if (frameNow == 0) miningWorkers.clear();

#if PATCHLOCKING_DEBUG
        if (frameNow == 0)
        {
            actualOtherPatchesGatheredHistory.clear();
            predictedOtherPatchesGatheredHistory.clear();
            patchLockingTotalCollections = 0;
            patchLockingPotentialPatchLocks = 0;
            patchLockingPredictedPatchLocksAtCommandFrame = 0;
            patchLockingSuccessfulPatchLocks = 0;
        }

        /* Update the actual resources being mined data */

        /* Start by making a set of all the resources that are being mined */
        std::set<Resource> resourcesBeingMined;
        for (const auto &wrk : UnitMgr::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
        {
            if (!wrk->exists()) continue;
            if (wrk->rawUnit->getOrder() != BWAPI::Orders::MiningMinerals) continue;
            if (!wrk->rawUnit->getOrderTarget()) continue;
            auto resource = UnitMgr::resourceAt(wrk->rawUnit->getOrderTarget()->getTilePosition());
            if (resource) resourcesBeingMined.insert(resource);
        }

        /* Now go through all of the mineral patches and find those where all other patches are being mined */
        std::set<Resource> resourcesWithAllOtherBeingMined;
        for (const auto &baseLoc : Map::allBases())
        {
            for (const auto &resource : baseLoc->mineralPatches())
            {
                bool allMined = true;
                for (const auto &otherResource : resource->resourcesInSwitchPatchRange)
                {
                    if (otherResource->destroyed) continue;
                    if (!resourcesBeingMined.contains(otherResource))
                    {
                        allMined = false;
                        break;
                    }
                }
                if (allMined)
                {
                    resourcesWithAllOtherBeingMined.insert(resource);
#if PATCHLOCKING_DEBUG_VERBOSE
                    Telemetry::log(resource->ident) << "Patch lock possible on this frame";
#endif
                }
            }
        }
        actualOtherPatchesGatheredHistory.push_front(std::move(resourcesWithAllOtherBeingMined));
        if (actualOtherPatchesGatheredHistory.size() > 20) actualOtherPatchesGatheredHistory.pop_back();

        /* Do the same with the predicted forecast values */
        std::set<Resource> resourcesWithAllOtherPredictedToBeMined;
        for (const auto &baseLoc : Map::allBases())
        {
            for (const auto &resource : baseLoc->mineralPatches())
            {
                auto &forecast = resource->getAllOtherPatchesGatheredProbabilityForecast();
                if (forecast[BWAPI::Broodwar->getLatencyFrames() + 10] >= PATCH_LOCK_THRESHOLD)
                {
                    resourcesWithAllOtherPredictedToBeMined.insert(resource);
#if PATCHLOCKING_DEBUG_VERBOSE
                    Telemetry::log(resource->ident) << "Patch lock predicted possible in 13 frames";
#endif
                }
            }
        }
        predictedOtherPatchesGatheredHistory.push_front(std::move(resourcesWithAllOtherPredictedToBeMined));
        if (predictedOtherPatchesGatheredHistory.size() > (20 + BWAPI::Broodwar->getLatencyFrames() + 10))
        {
            predictedOtherPatchesGatheredHistory.pop_back();
        }

        /* Output debug data every 1000 frames */
        if (frameNow > 0 && frameNow % 1000 == 0 && patchLockingPotentialPatchLocks > 0 && patchLockingTotalCollections > 0)
        {
            AppLog::Get() << std::fixed << std::setprecision(1)
                       << "Patch locking effectiveness: " << (100.0 * patchLockingSuccessfulPatchLocks) / (double)(patchLockingPotentialPatchLocks)
                       << "% successful patch locks, "
                       << (100.0 * patchLockingPredictedPatchLocksAtCommandFrame) / (double)(patchLockingTotalCollections)
                       << "% collections were predicted lockable, "
                       << (100.0 * patchLockingPotentialPatchLocks) / (double)(patchLockingTotalCollections)
                       << "% collections were actually lockable";
        }
#endif
#if OPTIMALPOSITIONS_DEBUG
        if (frameNow == 0)
        {
            exploredPatches.clear();
            collisions = 0;
            noncollisions = 0;
        }
        else if (frameNow % 1000 == 0 && WorkerMiningOptimization::isExploring())
        {
            AppLog::Get() << "Explored " << exploredPatches.size() << " patch(es)";
            if ((collisions + noncollisions) > 0)
            {
                AppLog::Get() << std::fixed << std::setprecision(1)
                           << "Gather collision rate: " << (100.0 * collisions) / (double)(collisions + noncollisions)
                           << "% over " << (collisions + noncollisions) << " collections";
            }
        }
#endif
#if LOGGING_ENABLED
        if (frameNow == 0)
        {
            hadPathData = 0;
            didNotHavePathData = 0;
        }
        else if (frameNow % 1000 == 0)
        {
            auto total = hadPathData + didNotHavePathData;
            if (total > 0)
            {
                AppLog::Get() << std::fixed << std::setprecision(1)
                           << "Gathers with path data: " << (100.0 * hadPathData) / (double)total
                           << "% over " << total << " collections";
            }
        }
#endif

        /* Update collision state for workers that are finished mining */
        for (auto iterRef = miningWorkers.begin(); iterRef != miningWorkers.end();)
        {
            auto &wrk = iterRef->wrk;
            if (!wrk->exists())
            {
                iterRef = miningWorkers.erase(iterRef);
                continue;
            }

            /* Wait until the worker started carrying a resource 8 frames ago */
            if (!wrk->carryingResource || wrk->lastCarryingResourceChange != (frameNow - 8))
            {
                iterRef++;
                continue;
            }

            /* Skip the worker if it has been ordered to do something else in the meantime */
            if (wrk->rawUnit->getOrder() != BWAPI::Orders::ReturnMinerals)
            {
#if OPTIMALPOSITIONS_DEBUG
                Telemetry::log(wrk->ident) << "Not tracking collision and speed observation, as the worker has apparently been re-ordered";
#endif
                iterRef = miningWorkers.erase(iterRef);
                continue;
            }

            handlePossiblePatchCollision(*iterRef);

            /* Don't need to track this any more */
            iterRef = miningWorkers.erase(iterRef);
        }

        /* Flush the worker statuses for workers that have started mining */
        for (auto iterRef = workerGatherStatuses.begin(); iterRef != workerGatherStatuses.end();)
        {
            auto &wrk = iterRef->first;

            /* Clean up if the worker is dead or has started mining */
            if (!wrk->exists() || wrk->rawUnit->getOrder() == BWAPI::Orders::MiningMinerals)
            {
                iterRef = workerGatherStatuses.erase(iterRef);
                continue;
            }

            /* Wait to process the worker until it is on its first WaitForMinerals frame */
            if (wrk->lastTransitionedToWaitForMineralsOrder != frameNow)
            {
                iterRef++;
                continue;
            }

            /* Skip workers whose paths did not start at the depot */
            if (!iterRef->second.pathStartsAtDepot)
            {
#if OPTIMALPOSITIONS_DEBUG
                Telemetry::log(wrk->ident) << "Path did not start at depot, so not using it for path optimization";
#endif
                iterRef++;
                continue;
            }

            /* Add the final position to the history */
            iterRef->second.appendCurrentPosition();

            /* We skip processing this worker if it hasn't tracked its positions history correctly */
            PositionsInHistory positionsInHistory;
            if (!extractPositionsInHistory(positionsInHistory, iterRef->second, WorkerMiningOptimization::isExploring())
                || positionsInHistory.arrivalPositionIt == iterRef->second.positionHistory.end())
            {
#if OPTIMALPOSITIONS_DEBUG
                if (positionsInHistory.arrivalPositionIt == iterRef->second.positionHistory.end())
                {
                    Telemetry::log(wrk->ident) << "Not tracking path optimization data as arrival position could not be determined";
                }
#endif
                iterRef++;
                continue;
            }

#if LOGGING_ENABLED
            if (iterRef->second.hasPathData)
            {
                hadPathData++;
            }
            else
            {
                didNotHavePathData++;
            }

#if VALIDATE_ARRIVAL_FRAMES
            if (!iterRef->second.switchedPatches && !iterRef->second.expectedArrivalFrameAndOccurrenceRate.empty() && !isExploring())
            {
                int arrivalFrame = frameNow - (int)std::distance(positionsInHistory.arrivalPositionIt, iterRef->second.positionHistory.end()) + 1;
                bool found = false;
                for (const auto &[tick, _] : iterRef->second.expectedArrivalFrameAndOccurrenceRate)
                {
                    if (tick == arrivalFrame)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    AppLog::Get() << "WARNING: Actual arrival frame " << arrivalFrame
                               << " not found in expected arrival frame(s) " << iterRef->second.expectedArrivalFramesDebug()
                               << "; worker id " << iterRef->second.wrk->ident << " @ " << iterRef->second.wrk->getTilePosition();
                }
            }
#endif
#endif

            if (WorkerMiningOptimization::isExploring())
            {
                updateApproachOptimization(iterRef->second, positionsInHistory);

                /* Tracking of 10-distance positions for paths that don't start at the depot */
                if (!iterRef->second.switchedPatches)
                {
                    updateTenDistancePosition(iterRef->second, positionsInHistory, false);
                }
            }

            /* We don't need to do any more if the worker switched patches */
            if (iterRef->second.switchedPatches)
            {
                iterRef++;
                continue;
            }

#if PATCHLOCKING_DEBUG
            /* Mark this collection's patch locking status */
            /* We don't count early frames since those give free locking in our test infrastructure */
            if (!isExploring() && frameNow > 300)
            {
                patchLockingTotalCollections++;

                /* We patch locked if: */
                /* - we can see the other worker is still mining */
                /* - we started transitioning to mining before the takeover frame (in this case, the other could have still been mining, so */
                /* though we technically didn't patch lock, we have still saved some frames) */
                auto otherWorkerMining = WorkerMgr::getOtherWorkerMining(iterRef->second.resource, wrk);
                if ((otherWorkerMining && otherWorkerMining->rawUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                    || frameNow < iterRef->second.takeoverFrame)
                {
                    patchLockingSuccessfulPatchLocks++;
                    patchLockingPredictedPatchLocksAtCommandFrame++;
                    patchLockingPotentialPatchLocks++;
                }
                else
                {
                    /* There wasn't a patch lock, so let's determine if one could have been done */
                    int arrivalFrame = frameNow - (int)std::distance(positionsInHistory.arrivalPositionIt, iterRef->second.positionHistory.end()) + 1;
                    bool patchLockPossible = false;
                    for (int tick = arrivalFrame;
                        tick < std::min(iterRef->second.takeoverFrame, frameNow);
                        tick++)
                    {
                        /* Check the actuals */
                        int idx = frameNow - tick;
                        if (idx < 19 && actualOtherPatchesGatheredHistory[idx].contains(iterRef->second.resource) &&
                            actualOtherPatchesGatheredHistory[idx + 1].contains(iterRef->second.resource))
                        {
                            if (!patchLockPossible)
                            {
                                patchLockingPotentialPatchLocks++;
                                patchLockPossible = true;
                            }

                            if (predictedOtherPatchesGatheredHistory[idx + BWAPI::Broodwar->getLatencyFrames() + 11].contains(iterRef->second.resource))
                            {
                                patchLockingPredictedPatchLocksAtCommandFrame++;
#if PATCHLOCKING_DEBUG_VERBOSE
                                AppLog::Get() << "WARNING: Worker could have patch locked on frame " << tick
                                           << "; worker id " << wrk->ident << " @ " << wrk->getTilePosition();
#endif
                                break;
                            }
                        }
                    }
                }
            }
#endif

            /* Move required fields into the MiningWorker struct that we use to track patch collisions */
            /* As the underlying vectors may change in the meantime, we convert pointers to positions */
            auto convertToPositions = [](const std::vector<GatherPositionObservationPtr> &source)
            {
                std::vector<PositionAndVelocity> outcome;
                outcome.reserve(source.size());
                for (const auto &sourcePos : source)
                {
                    outcome.emplace_back(sourcePos.position());
                }
                return outcome;
            };
            miningWorkers.emplace_back(MiningWorker{
                    iterRef->second.wrk,
                    iterRef->second.resource,
                    convertToPositions(positionsInHistory.positionHistory),
                    convertToPositions(positionsInHistory.resendsWithObservationData),
                    positionsInHistory.resendItsBeforeArrival.size()});

            iterRef++;
        }
    }

    void handleGatherPatchSwitch(WorkerGatherStatus &wrkStatus)
    {
        PositionsInHistory positionsInHistory;
        if (!extractPositionsInHistory(positionsInHistory, wrkStatus, WorkerMiningOptimization::isExploring())) return;

        updateTenDistancePosition(wrkStatus, positionsInHistory, true);
    }
}