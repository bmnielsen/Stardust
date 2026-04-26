#include "InitialWorkerSplitTester.h"

#include <random>

#include <BWAPI/SimulateGatherPathOptions.h>
#include <BWAPI/SimulateGatherPathResult.h>

#include "OrderProcessTimer.h"

#define VERBOSE_LOGGING false

namespace MiningOptimizationTraining
{
    namespace
    {
        std::default_random_engine rng;

        BWAPI::Unit patchAt(TilePosition tile)
        {
            for (auto patch : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                if (patch->getTilePosition() == tile)
                {
                    return patch;
                }
            }
            return nullptr;
        }

        template <typename ObservationType>
        struct QueuedNode
        {
            const InitialWorkerPathNode<ObservationType> *node;
            std::set<int> resends;
            std::set<const InitialWorkerPathNode<ObservationType> *> resendNodes;
            int frame;
        };

        template <typename ObservationType>
        struct Result
        {
            ObservationType arrivalData;
            std::set<int> resends;
            std::set<const InitialWorkerPathNode<ObservationType> *> resendNodes;
        };
    }

    void InitialWorkerSplitTesterModule::onStart()
    {
        InstrumentedDoNothingModule::onStart();

        Log::Get() << "Starting initial worker split validation on " << mapData.mapHash << " with enemy race " << enemyRace;
        rng = std::default_random_engine(BWAPI::Broodwar->getRandomSeed());

        // Verify we have data for each worker and gather the vectors of workers and patches
        for (auto worker : BWAPI::Broodwar->self()->getUnits())
        {
            if (!worker->getType().isWorker()) continue;

            auto firstGatherPaths = mapData.startingWorkerPositionToPatchToFirstGatherPath.find(worker->getExactPosition());
            EXPECT_NE(mapData.startingWorkerPositionToPatchToFirstGatherPath.end(), firstGatherPaths)
                                << "No first gather paths found for worker starting position " << worker->getExactPosition();

            workers.emplace_back(worker);

            if (!patches.empty()) continue;

            for (const auto &[patchTile, _] : firstGatherPaths->second)
            {
                patches.emplace_back(patchAt(patchTile));
            }
        }

        // Sort the workers and patches by position to ensure stability between runs
        auto positionSorter = [](const BWAPI::Unit &first, const BWAPI::Unit &second)
        {
            return first->getPosition() < second->getPosition();
        };
        std::sort(workers.begin(), workers.end(), positionSorter);
        std::sort(patches.begin(), patches.end(), positionSorter);

        if (chooseRandomResends)
        {
            // Pick a random set of patches for each worker
            auto firstPatches = patches;
            auto secondPatches = patches;
            std::shuffle(std::begin(firstPatches), std::end(firstPatches), rng);
            std::shuffle(std::begin(secondPatches), std::end(secondPatches), rng);
            size_t i = 0;
            for (auto worker : workers)
            {
#if VERBOSE_LOGGING
                Log::Get() << "Planning worker " << worker->getID();
#endif

                workerStatuses[worker] = {
                        planPatchCombinationRandomly(worker, firstPatches[i], secondPatches[i]),
                        firstPatches[i],
                        secondPatches[i]
                };
                i++;

                CherryVis::log(worker->getID()) << "First gather plan: " << workerStatuses[worker].gatherPlan.firstGather;
            }
            return;
        }

        // TODO: Implement logic to choose the best combination

        // Find the best path for each worker on each combination of patches
//        std::map<BWAPI::Unit, std::map<std::pair<TilePosition, TilePosition>, WorkerGatherPlan>> allGatherPlans;
//        for (auto worker : BWAPI::Broodwar->self()->getUnits())
//        {
//            if (!worker->getType().isWorker()) continue;
//
//            // Plan all of the patch combinations for this worker
//            auto &workerPlans = allGatherPlans[worker];
//            for (auto firstPatch : patches)
//            {
//                for (auto secondPatch : patches)
//                {
//                    workerPlans.emplace(std::make_pair(firstPatch, secondPatch),
//                                        planPatchCombination(worker->getExactPosition(), firstPatch, secondPatch));
//                }
//            }
//        }

        // Select the best combination of patch pairs for all of the workers
        // We do this by evaluating all of the possible combinations and scoring them based on (in order):
        // - earliest 7th collection, capped at frame 300 (so we can build our second worker as early as possible)
        // - fastest average second collection (so the workers are assigned to fast patches after the initial split)
        // TODO: How to handle uncertainty in order process timer resets in the scoring

    }

    void InitialWorkerSplitTesterModule::onFrame()
    {
        InstrumentedDoNothingModule::onFrameStart();

        for (auto &[worker, status] : workerStatuses)
        {
#if VERBOSE_LOGGING
            CherryVis::log(worker->getID()) << worker->getExactPosition();
#endif

            switch (status.state)
            {
                case 0:
                {
                    if (worker->getID() == 0)
                    {
                        std::set<int> resends;
                        for (auto resend : status.gatherPlan.firstGather.resends)
                        {
                            resends.insert(resend + 1);
                        }
                        auto simulateResult =
                                worker->simulateGatherPath(BWAPI::SimulateGatherPathOptions(resends)
                                                                   .switchToPatch(status.firstPatch->getBWIndex())
                                                                   );
                        Log::Get() << worker->getID() << ": Simulated path:";
                        for (const auto &pos : simulateResult->positions)
                        {
                            Log::Get() << pos;
                        }
                        Log::Get() << worker->getID() << ": Arrival " << simulateResult->positions.size();
                    }

                    EXPECT_TRUE(worker->gather(status.firstPatch))
                                        << worker->getID() << ": Failed to issue gather command: " << BWAPI::Broodwar->getLastError();
                    status.state++;
                    break;
                }
                case 1:
                    if (worker->getID() == 0 && currentFrame == 3)
                    {
                        auto simulateResult =
                                worker->simulateGatherPath(BWAPI::SimulateGatherPathOptions(status.gatherPlan.firstGather.resends)
                                                                   .setIncludeAllPositions());
                        Log::Get() << worker->getID() << ": Simulated path:";
                        for (const auto &pos : simulateResult->positions)
                        {
                            Log::Get() << pos;
                        }
                    }
                    if (status.gatherPlan.firstGather.resends.contains(currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        EXPECT_TRUE(worker->gather(status.firstPatch))
                            << worker->getID() << ": Failed to issue gather command: " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->getID()) << "Resent gather";
                    }
                    if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
                    {
                        EXPECT_TRUE(status.gatherPlan.firstGather.containsActionFrame(currentFrame))
                            << worker->getID() << ": " << currentFrame << " is not an expected first gather action frame";
                        status.state++;
                    }
                    break;
                case 2:
                case 5:
                    if (worker->getOrder() == BWAPI::Orders::ReturnMinerals)
                    {
                        status.state++;
                    }
                    break;
                case 3:
                    if (status.gatherPlan.firstReturn.resends.contains(currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        EXPECT_TRUE(worker->returnCargo())
                                            << worker->getID() << ": Failed to issue return command: " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->getID()) << "Resent return";
                    }
                    if (!worker->isCarryingMinerals())
                    {
                        EXPECT_TRUE(status.gatherPlan.firstReturn.containsActionFrame(currentFrame))
                                            << worker->getID() << ": " << currentFrame << " is not an expected first return action frame";
                        if (status.firstPatch != status.secondPatch)
                        {
                            worker->gather(status.secondPatch);
                        }
                        status.state++;
                    }
                    break;
                case 4:
                    if (status.gatherPlan.secondGather.resends.contains(currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        EXPECT_TRUE(worker->gather(status.secondPatch))
                                            << worker->getID() << ": Failed to issue gather command: " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->getID()) << "Resent gather";
                    }
                    if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
                    {
                        status.state++;
                    }
                    break;
                case 6:
                    if (status.gatherPlan.secondReturn.resends.contains(currentFrame + BWAPI::Broodwar->getLatencyFrames()))
                    {
                        EXPECT_TRUE(worker->returnCargo())
                                            << worker->getID() << ": Failed to issue return command: " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->getID()) << "Resent return";
                    }
                    if (!worker->isCarryingMinerals())
                    {
                        status.state++;
                    }
                    break;
                default:
                    worker->stop();
                    break;
            }
        }

        InstrumentedDoNothingModule::onFrameEnd();
    }

    WorkerGatherPlan InitialWorkerSplitTesterModule::planPatchCombinationRandomly(BWAPI::Unit worker, BWAPI::Unit firstPatch, BWAPI::Unit secondPatch)
    {
        // We go through each path and find all usable resend combinations (combinations that have path data from their next path start position)
        // Then we choose a random one

        // Find the possible order process timer reset values for this start position and enemy race
        std::set<int> orderProcessTimerResetValues;
        {
            for (const auto &resetData : mapData.startingWorkerPositionToOrderProcessTimerReset.at(worker->getPosition()))
            {
                if (resetData.opponentIsZerg && (enemyRace != BWAPI::Races::Unknown && enemyRace != BWAPI::Races::Zerg)) continue;
                if (!resetData.opponentIsZerg && (enemyRace == BWAPI::Races::Zerg)) continue;

                orderProcessTimerResetValues.insert(resetData.value);
            }
        }

        WorkerGatherPlan workerGatherPlan;

        auto planPath = [&]<typename ObservationType, typename NextObservationType>(
                int startFrame,
                std::optional<int> requireResendAfterFrame,
                bool pathStartsWithGatherCommand,
                const InitialWorkerPathNode<ObservationType> &rootNode,
                const std::map<BWAPI::ExactPosition, NextObservationType> *nextRootNodes = nullptr)
        {
            std::vector<Result<ObservationType>> results;
            std::deque<QueuedNode<ObservationType>> nodeQueue;

            // Add the no-resend result
            if (!requireResendAfterFrame || startFrame > (*requireResendAfterFrame))
            {
                results.emplace_back(rootNode.arrivalData, std::set<int>{}, std::set<const InitialWorkerPathNode<ObservationType> *>{});
            }

            // Push the root node onto the queue and start iterating over the paths
            nodeQueue.emplace_back(
                    &rootNode,
                    std::set<int>{},
                    std::set<const InitialWorkerPathNode<ObservationType> *>{},
                    startFrame);

            while (!nodeQueue.empty())
            {
                auto &node = *nodeQueue.begin();

                // Follow the nodes, register resend results, and queue resend nodes
                auto current = node.node;
                int frame = node.frame;
                while (current)
                {
                    if (frame >= (startFrame + BWAPI::Broodwar->getLatencyFrames() + 1) &&
                        (!requireResendAfterFrame || frame > (*requireResendAfterFrame)) &&
                        current->type != NodeType::PoorResendNode &&
                        !node.resends.contains(frame - BWAPI::Broodwar->getLatencyFrames()) &&
                        !OrderProcessTimer::isResetFrame(frame + 3))
                    {
                        std::set<int> resends = node.resends;
                        resends.insert(frame);

                        std::set<const InitialWorkerPathNode<ObservationType> *> resendNodes = node.resendNodes;
                        resendNodes.insert(current);

                        if (current->type == NodeType::StableNode)
                        {
                            results.emplace_back(current->arrivalData, resends, resendNodes);
                        }
                        else if (current->arrivalDataAfterResend)
                        {
                            results.emplace_back(*current->arrivalDataAfterResend, resends, resendNodes);
                            if (current->nextPositionAfterResend)
                            {
                                nodeQueue.emplace_back(current->nextPositionAfterResend.get(), resends, resendNodes, frame + 1);
                            }
                        }
                    }

                    current = current->nextPosition.get();
                    frame++;
                }

                nodeQueue.pop_front();
            }

            // Choose a result, validating that its next positions are in the next path data
            PlannedPath result;
            while (true)
            {
                EXPECT_FALSE(results.empty()) << worker->getID() << ": No valid paths from start position " << rootNode.pos;

                std::uniform_int_distribution<size_t> dist(0, results.size() - 1);
                size_t resultIndex = dist(rng);
                auto chosenResult = results[resultIndex];

                auto pathResults = chosenResult.arrivalData.computePathResult(
                        startFrame,
                        pathStartsWithGatherCommand,
                        chosenResult.resends.empty() ? std::nullopt : (std::optional<int>)*chosenResult.resends.rbegin(),
                        orderProcessTimerResetValues);

                if (pathResults.size() > 1)
                {
                    pathResults = chosenResult.arrivalData.computePathResult(
                            startFrame,
                            pathStartsWithGatherCommand,
                            chosenResult.resends.empty() ? std::nullopt : (std::optional<int>)*chosenResult.resends.rbegin(),
                            orderProcessTimerResetValues);
                }

                // If we have been provided with next root nodes, validate that all of the paths have all of the positions covered
                if (nextRootNodes)
                {
                    bool allExist = true;
                    for (const auto &[_, _2, pos] : pathResults)
                    {
                        allExist = allExist && nextRootNodes->contains(pos);
                    }

                    // Positions were missing, so reject this result and pick a new one
                    if (!allExist)
                    {
                        results.erase(results.begin() + resultIndex);
                        continue;
                    }
                }

                // Use this result
                result.resends = chosenResult.resends;
                for (const auto &[actionFrame, _, pos] : pathResults)
                {
                    result.actionFrames.emplace_back(actionFrame);
                    result.nextPathStartPositions.emplace_back(pos);
                }
                break;
            }
            return result;
        };

        auto &firstGatherRootNode = mapData
                .startingWorkerPositionToPatchToFirstGatherPath
                .at(worker->getExactPosition())
                .at(TilePosition::fromBWAPI(firstPatch->getTilePosition()));

        auto &firstReturnNodes = mapData
                .startingWorkerPositionToPatchToFirstReturnPaths
                .at(worker->getExactPosition())
                .at(TilePosition::fromBWAPI(firstPatch->getTilePosition()));

        workerGatherPlan.firstGather = planPath(0, 8, true, firstGatherRootNode, &firstReturnNodes);

        // TODO: Need to figure out how to handle multiple action frames
        EXPECT_EQ(1, workerGatherPlan.firstGather.actionFrames.size());

//        auto &secondGatherNodes = mapData
//                .startingWorkerPositionToPatchesToSecondGatherPaths
//                .at(worker->getExactPosition())
//                .at(std::make_pair(TilePosition::fromBWAPI(firstPatch->getTilePosition()), TilePosition::fromBWAPI(secondPatch->getTilePosition())));

        workerGatherPlan.firstReturn = planPath(*workerGatherPlan.firstGather.actionFrames.begin() + 84,
                                                std::nullopt,
                                                false,
                                                firstReturnNodes.at(*workerGatherPlan.firstGather.nextPathStartPositions.begin()),
                                                (std::map<BWAPI::ExactPosition, int>*)nullptr);


//        std::vector<Result<InitialWorkerGatherArrivalData>> results;
//
//        int startFrame = 0;
//        int minimumResendFrame = 4;
//
//        std::deque<QueuedNode<InitialWorkerGatherArrivalData>> nodeQueue;
//        auto &rootNode = mapData
//                .startingWorkerPositionToPatchToFirstGatherPath
//                .at(worker->getExactPosition())
//                .at(TilePosition::fromBWAPI(firstPatch->getTilePosition()));
//
//        results.emplace_back(rootNode.arrivalData, std::set<int>{}, std::set<const InitialWorkerPathNode<InitialWorkerGatherArrivalData> *>{});
//
//        nodeQueue.emplace_back(
//                &rootNode,
//                std::set<int>{},
//                std::set<const InitialWorkerPathNode<InitialWorkerGatherArrivalData> *>{},
//                startFrame);
//
//        while (!nodeQueue.empty())
//        {
//            auto &node = *nodeQueue.begin();
//
//            // Follow the nodes, register resend results, and queue resend nodes
//            auto current = node.node;
//            int frame = node.frame;
//            while (current)
//            {
//                if (frame >= minimumResendFrame &&
//                    current->arrivalDataAfterResend &&
//                    current->type != NodeType::PoorResendNode &&
//                    current->type != NodeType::StableNode &&
//                    (frame - BWAPI::Broodwar->getLatencyFrames()) != startFrame &&
//                    !node.resends.contains(frame - BWAPI::Broodwar->getLatencyFrames()) &&
//                    !OrderProcessTimer::isResetFrame(frame + 3))
//                {
//                    std::set<int> resends = node.resends;
//                    resends.insert(frame);
//
//                    std::set<const InitialWorkerPathNode<InitialWorkerGatherArrivalData> *> resendNodes = node.resendNodes;
//                    resendNodes.insert(current);
//
//                    results.emplace_back(*current->arrivalDataAfterResend, resends, resendNodes);
//                    if (current->nextPositionAfterResend)
//                    {
//                        nodeQueue.emplace_back(current->nextPositionAfterResend.get(), resends, resendNodes, frame + 1);
//                    }
//                }
//
//                current = current->nextPosition.get();
//                frame++;
//            }
//
//            nodeQueue.pop_front();
//        }

        // TODO: Add on later path selections as we implement them

//        std::uniform_int_distribution<size_t> dist(0, results.size() - 1);
//        auto chosenResult = results[dist(rng)];
//
//        auto actionFramesAndDelay = chosenResult.arrivalData.computeActionFramesAndDelay(
//                0,
//                true,
//                chosenResult.resends.empty()
//                   ? std::nullopt
//                   : (std::optional<int>)*chosenResult.resends.rbegin(),
//                orderProcessTimerResetValues);
//        std::set<int> actionFrames;
//        for (const auto &[actionFrame, _] : actionFramesAndDelay)
//        {
//            actionFrames.insert(actionFrame);
//        }

//
//        // Compute the order process timer value at the arrival frame
//        // The order process timer is 0 at the start of the frame two frames after the last resend takes effect
//        int orderProcessTimerAtArrival = 0;
//        for (int i = 0; i < (arrivalFrame - *chosenResult.resends.rbegin() - 2); i++)
//        {
//            if (orderProcessTimerAtArrival == 0)
//            {
//                orderProcessTimerAtArrival = 8;
//            }
//            else
//            {
//                orderProcessTimerAtArrival--;
//            }
//        }

#if VERBOSE_LOGGING
//        Log::Get() << "Arrival frame " << chosenResult.arrivalFrame
//                   << "; last resend frame " << (*chosenResult.resends.rbegin())
//                   << "; action frame " << *actionFrames.begin();

        Log::Get() << "Expected path:";
        auto current = &rootNode;
        while (current)
        {
            Log::Get() << current->pos;
            if (chosenResult.resendNodes.contains(current))
            {
                Log::Get() << "resend takes effect";
                current = current->nextPositionAfterResend.get();
            }
            else
            {
                current = current->nextPosition.get();
            }
        }
#endif

//        WorkerGatherPlan result;
//        result.firstGather.resends = chosenResult.resends;
//        result.firstGather.actionFrames = actionFrames;
        return workerGatherPlan;
    }
}
