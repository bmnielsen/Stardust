#include "InitialWorkerSplitTester.h"

#include <random>

#include <BWAPI/SimulateGatherPathOptions.h>
#include <BWAPI/SimulateGatherPathResult.h>

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
            std::set<int> resends;
            std::set<const InitialWorkerPathNode<ObservationType> *> resendNodes;
            int arrivalFrame;
        };
    }

    void InitialWorkerSplitTesterModule::onStart()
    {
        InstrumentedDoNothingModule::onStart();

        Log::Get() << "Starting initial worker split validation on " << mapData.mapHash << " with enemy race " << enemyRace;
        rng = std::default_random_engine(BWAPI::Broodwar->getRandomSeed());

        // Verify we have data for each worker and get the set of mineral patch positions
        std::vector<TilePosition> patches;
        for (auto worker : BWAPI::Broodwar->self()->getUnits())
        {
            if (!worker->getType().isWorker()) continue;

            auto firstGatherPaths = mapData.startingWorkerPositionToPatchToFirstGatherPath.find(worker->getExactPosition());
            EXPECT_NE(mapData.startingWorkerPositionToPatchToFirstGatherPath.end(), firstGatherPaths)
                                << "No first gather paths found for worker starting position " << worker->getExactPosition();

            if (!patches.empty()) continue;

            for (const auto &[patchTile, _] : firstGatherPaths->second)
            {
                patches.emplace_back(patchTile);
            }
        }

        if (chooseRandomResends)
        {
            // Pick a random set of patches for each worker
            auto firstPatches = patches;
            auto secondPatches = patches;
            std::shuffle(std::begin(firstPatches), std::end(firstPatches), rng);
            std::shuffle(std::begin(secondPatches), std::end(secondPatches), rng);
            size_t i = 0;
            for (auto worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (!worker->getType().isWorker()) continue;

#if VERBOSE_LOGGING
                Log::Get() << "Planning worker " << worker->getID();
#endif

                workerStatuses[worker] = {
                        planPatchCombinationRandomly(worker->getExactPosition(), firstPatches[i], secondPatches[i]),
                        patchAt(firstPatches[i]),
                        patchAt(secondPatches[i])
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
                                                                   .setIncludeAllPositions());
                        Log::Get() << worker->getID() << ": Simulated path:";
                        for (const auto &pos : simulateResult->positions)
                        {
                            Log::Get() << pos;
                        }
                    }

                    worker->gather(status.firstPatch);
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
                        worker->gather(status.firstPatch);
                        CherryVis::log(worker->getID()) << "Resent gather";
                    }
                    if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
                    {
                        EXPECT_TRUE(status.gatherPlan.firstGather.actionFrames.contains(currentFrame))
                            << worker->getID() << ": " << currentFrame << " is not an expected action frame";
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
                        worker->returnCargo();
                        CherryVis::log(worker->getID()) << "Resent return";
                    }
                    if (!worker->isCarryingMinerals())
                    {
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
                        worker->gather(status.secondPatch);
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
                        worker->returnCargo();
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

    WorkerGatherPlan InitialWorkerSplitTesterModule::planPatchCombinationRandomly(BWAPI::ExactPosition startPosition,
                                                                                  TilePosition firstPatch,
                                                                                  TilePosition secondPatch)
    {
        // We go through each path and find all usable resend combinations (combinations that have path data from their next path start position)
        // Then we choose a random one

        // Find the possible order process timer reset values for this start position and enemy race
        std::set<int> orderProcessTimerResetValues;
        {
            for (const auto &resetData : mapData.startingWorkerPositionToOrderProcessTimerReset.at(startPosition.pos()))
            {
                if (resetData.opponentIsZerg && (enemyRace != BWAPI::Races::Random && enemyRace != BWAPI::Races::Zerg)) continue;
                if (!resetData.opponentIsZerg && (enemyRace == BWAPI::Races::Zerg)) continue;

                orderProcessTimerResetValues.insert(resetData.value);
            }
        }

        std::vector<Result<InitialWorkerGatherArrivalData>> results;

        int startFrame = 0;
        int minimumResendFrame = 8;

        std::deque<QueuedNode<InitialWorkerGatherArrivalData>> nodeQueue;
        auto &rootNode = mapData.startingWorkerPositionToPatchToFirstGatherPath.at(startPosition).at(firstPatch);
        nodeQueue.emplace_back(
                &rootNode,
                std::set<int>{},
                std::set<const InitialWorkerPathNode<InitialWorkerGatherArrivalData> *>{},
                startFrame);

        while (!nodeQueue.empty())
        {
            auto &node = *nodeQueue.begin();

            // Follow the nodes, register resend results, and queue resend nodes
            auto current = node.node;
            int frame = node.frame;
            while (current)
            {
                if (frame >= minimumResendFrame &&
                    !node.resends.contains(frame - BWAPI::Broodwar->getLatencyFrames()) &&
                    current->type != NodeType::PoorResendNode)
                {
                    std::set<int> resends = node.resends;
                    resends.insert(frame);

                    std::set<const InitialWorkerPathNode<InitialWorkerGatherArrivalData> *> resendNodes = node.resendNodes;
                    resendNodes.insert(current);

                    if (current->type == NodeType::StableNode)
                    {
                        results.emplace_back(resends, resendNodes, frame + current->arrivalData.arrivalDelay);
                    }
                    else if (current->arrivalDataAfterResend)
                    {
                        results.emplace_back(resends, resendNodes, frame + current->arrivalDataAfterResend->arrivalDelay);
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

        // TODO: Add on later path selections as we implement them

        std::uniform_int_distribution<size_t> dist(0, results.size() - 1);
        auto chosenResult = results[dist(rng)];

        // Compute the order process timer value at the arrival frame
        // The order process timer is 0 at the start of the frame two frames after the last resend takes effect
        int orderProcessTimerAtArrival = 0;
        for (int i = 0; i < (chosenResult.arrivalFrame - *chosenResult.resends.rbegin() - 2); i++)
        {
            if (orderProcessTimerAtArrival == 0)
            {
                orderProcessTimerAtArrival = 8;
            }
            else
            {
                orderProcessTimerAtArrival--;
            }
        }

#if VERBOSE_LOGGING
        Log::Get() << "Arrival frame " << chosenResult.arrivalFrame
                   << "; last resend frame " << (*chosenResult.resends.rbegin())
                   << "; order process timer at arrival " << orderProcessTimerAtArrival;

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

        WorkerGatherPlan result;
        result.firstGather.resends = chosenResult.resends;
        result.firstGather.actionFrames = {chosenResult.arrivalFrame + orderProcessTimerAtArrival};
        return result;
    }
}
