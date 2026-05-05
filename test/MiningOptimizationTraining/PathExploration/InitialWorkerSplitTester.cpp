#include "InitialWorkerSplitTester.h"

#include <random>

#include <BWAPI/SimulateGatherPathOptions.h>
#include <BWAPI/SimulateGatherPathResult.h>
#include <BWAPI/PrepareGatherPathOptions.h>
#include <BWAPI/PrepareGatherPathResult.h>

#include "OrderProcessTimer.h"

#include "../DataTransformation/InitialSplitSolver.h"

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
                        std::nullopt,
                        firstPatches[i],
                        secondPatches[i]
                };
                i++;

                CherryVis::log(worker->getID()) << "First gather plan: " << workerStatuses[worker].gatherPlan->firstGather;
            }
            return;
        }

        // Temporary logic: still pick the patches randomly, but use the solver to plan the resends
//         {
//             auto firstPatches = patches;
//             auto secondPatches = patches;
//             std::shuffle(std::begin(firstPatches), std::end(firstPatches), rng);
//             std::shuffle(std::begin(secondPatches), std::end(secondPatches), rng);
//             size_t i = 0;
//             for (auto worker : workers)
//             {
//                 auto firstPatch = TilePosition::fromBWAPI(firstPatches[i]->getTilePosition());
//                 auto secondPatch = TilePosition::fromBWAPI(secondPatches[i]->getTilePosition());
//
// #if VERBOSE_LOGGING
//                 Log::Get() << "Planning worker " << worker->getID() << "; assigned to patch " << firstPatch << " and " << secondPatch;
// #endif
//
//                 auto solver = InitialSplitSolver(mapData, PositionAndVelocity(worker), firstPatch, secondPatch, enemyRace);
//
//                 auto result = solver.execute();
//
//                 if (result)
//                 {
//                     workerStatuses[worker] = {
//                         std::nullopt,
//                         *result,
//                         firstPatches[i],
//                         secondPatches[i]
//                     };
//
// #if VERBOSE_LOGGING
//                     Log::Get() << worker->getID() << " first rotation: " << result->firstRotation;
// #endif
//                     CherryVis::log(worker->getID()) << "first rotation: " << result->firstRotation;
//                 }
//                 else
//                 {
//                     Log::Get() << "WARNING: Worker " << worker->getID() << " could not execute solver";
//                     workerStatuses[worker] = {
//                         std::nullopt,
//                         std::nullopt,
//                         firstPatches[i],
//                         secondPatches[i]
//                     };
//                 }
//
//                 i++;
//             }
//         }

        // For each worker, run the solver for each combination of patches and condense it to have the top 16 permutations for each
        std::map<BWAPI::Unit, std::map<std::pair<TilePosition, TilePosition>, MiningOptimization::InitialSplitData>>
            workerToPatchPermutationToInitialSplitData;
        for (auto worker : workers)
        {
            auto positionAndVelocity = PositionAndVelocity(worker);
            std::vector<std::tuple<TilePosition, std::vector<std::pair<TilePosition, MiningOptimization::InitialSplitData>>, uint16_t>>
                firstPatchResults;
            for (auto firstPatchUnit : patches)
            {
                auto firstPatch = TilePosition::fromBWAPI(firstPatchUnit->getTilePosition());
                std::vector<std::pair<TilePosition, MiningOptimization::InitialSplitData>> secondPatchResults;
                for (auto secondPatchUnit : patches)
                {
                    auto secondPatch = TilePosition::fromBWAPI(secondPatchUnit->getTilePosition());
                    auto result = InitialSplitSolver(mapData,
                        positionAndVelocity,
                        firstPatch,
                        secondPatch,
                        enemyRace).execute();
                    if (!result) continue;

                    if (secondPatchResults.size() < 4)
                    {
                        secondPatchResults.emplace_back(secondPatch, std::move(*result));
                        continue;
                    }

                    for (auto &existingSecondPatchResult : secondPatchResults)
                    {
                        if (result->worstSecondRotationActionFrame() < existingSecondPatchResult.second.worstSecondRotationActionFrame())
                        {
                            existingSecondPatchResult.first = secondPatch;
                            existingSecondPatchResult.second = std::move(*result);
                            break;
                        }
                    }
                }

                uint16_t bestScore = UINT16_MAX;
                for (auto &secondPatchResult : secondPatchResults)
                {
                    bestScore = std::min(bestScore, secondPatchResult.second.worstSecondRotationActionFrame());
                }

                if (firstPatchResults.size() < 4)
                {
                    firstPatchResults.emplace_back(firstPatch, std::move(secondPatchResults), bestScore);
                    continue;
                }

                for (auto &[existingPatch, existingSecondPatchResults, existingScore]
                        : firstPatchResults)
                {
                    if (bestScore < existingScore)
                    {
                        existingPatch = firstPatch;
                        existingSecondPatchResults = std::move(secondPatchResults);
                        existingScore = bestScore;
                        break;
                    }
                }
            }

            auto &workerData = workerToPatchPermutationToInitialSplitData[worker];
            for (auto &[firstPatch, secondPatchResults, _] : firstPatchResults)
            {
                for (auto &[secondPatch, initialSplitData] : secondPatchResults)
                {
                    workerData.emplace(std::make_pair(firstPatch, secondPatch), initialSplitData);
                }
            }
        }

        // Generate combinations for all four workers to find the best solution
        uint16_t bestSeventhDelivery = UINT16_MAX;
        uint16_t bestEighthDelivery = UINT16_MAX;
        std::array<std::pair<TilePosition, TilePosition>, 4> bestSolution;
        for (auto &[firstWorkerAssignment, firstWorkerResult]
                : workerToPatchPermutationToInitialSplitData[workers[0]])
        {
            for (auto &[secondWorkerAssignment, secondWorkerResult]
                    : workerToPatchPermutationToInitialSplitData[workers[1]])
            {
                if (secondWorkerAssignment.first == firstWorkerAssignment.first) continue;
                if (secondWorkerAssignment.second == firstWorkerAssignment.second) continue;

                for (auto &[thirdWorkerAssignment, thirdWorkerResult]
                        : workerToPatchPermutationToInitialSplitData[workers[2]])
                {
                    if (thirdWorkerAssignment.first == firstWorkerAssignment.first) continue;
                    if (thirdWorkerAssignment.second == firstWorkerAssignment.second) continue;
                    if (thirdWorkerAssignment.first == secondWorkerAssignment.first) continue;
                    if (thirdWorkerAssignment.second == secondWorkerAssignment.second) continue;

                    for (auto &[fourthWorkerAssignment, fourthWorkerResult]
                            : workerToPatchPermutationToInitialSplitData[workers[3]])
                    {
                        if (fourthWorkerAssignment.first == firstWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == firstWorkerAssignment.second) continue;
                        if (fourthWorkerAssignment.first == secondWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == secondWorkerAssignment.second) continue;
                        if (fourthWorkerAssignment.first == thirdWorkerAssignment.first) continue;
                        if (fourthWorkerAssignment.second == thirdWorkerAssignment.second) continue;

                        std::multiset<uint16_t> result = {
                            firstWorkerResult.worstSecondRotationActionFrame(),
                            secondWorkerResult.worstSecondRotationActionFrame(),
                            thirdWorkerResult.worstSecondRotationActionFrame(),
                            fourthWorkerResult.worstSecondRotationActionFrame()
                        };

                        uint16_t seventhDelivery = *(std::prev(result.end(), 2));
                        uint16_t eighthDelivery = *result.rbegin();
                        if (seventhDelivery < bestSeventhDelivery || (seventhDelivery == bestSeventhDelivery && eighthDelivery < bestEighthDelivery))
                        {
                            bestSeventhDelivery = seventhDelivery;
                            bestEighthDelivery = eighthDelivery;
                            bestSolution = {
                                firstWorkerAssignment,
                                secondWorkerAssignment,
                                thirdWorkerAssignment,
                                fourthWorkerAssignment,
                            };
                        }
                    }
                }
            }
        }

        if (bestSeventhDelivery == UINT16_MAX)
        {
            Log::Get() << "ERROR: No worker combination found!";
            return;
        }

        for (size_t workerIndex = 0; workerIndex < 4; workerIndex++)
        {
            auto &worker = workers[workerIndex];
            auto &assignment = bestSolution[workerIndex];
            auto &solveResult = workerToPatchPermutationToInitialSplitData[worker][assignment];

#if VERBOSE_LOGGING
            Log::Get() << worker->getID() << " assigned to " << assignment.first << " and " << assignment.second;
            Log::Get() << worker->getID() << " first rotation: " << solveResult.firstRotation;
#endif
            CherryVis::log(worker->getID()) << " assigned to " << assignment.first << " and " << assignment.second;
            CherryVis::log(worker->getID()) << "first rotation: " << solveResult.firstRotation;

            workerStatuses[worker] = {
                std::nullopt,
                solveResult,
                patchAt(assignment.first),
                patchAt(assignment.second)
            };
        }
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
                    EXPECT_TRUE(worker->gather(status.firstPatch))
                                        << worker->getID() << ": Failed to issue gather command: " << BWAPI::Broodwar->getLastError();
                    status.state++;
                    break;
                }
                case 1:
                    if (status.isResendFrame())
                    {
                        EXPECT_TRUE(worker->gather(status.firstPatch))
                            << worker->getID() << ": Failed to issue gather command: " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->getID()) << "Resent gather";
                    }
                    if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
                    {
                        EXPECT_TRUE(status.isFirstGatherActionFrame())
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
                    if (status.isResendFrame())
                    {
                        EXPECT_TRUE(worker->returnCargo())
                                            << worker->getID() << ": Failed to issue return command: " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->getID()) << "Resent return";
                    }
                    if (!worker->isCarryingMinerals())
                    {
                        EXPECT_TRUE(status.isFirstReturnActionFrame())
                                            << worker->getID() << ": " << currentFrame << " is not an expected first return action frame";
                        if (status.firstPatch != status.secondPatch)
                        {
                            EXPECT_TRUE(worker->gather(status.secondPatch))
                                            << worker->getID() << ": Failed to issue gather command: " << BWAPI::Broodwar->getLastError();
                            CherryVis::log(worker->getID()) << "Issued gather to second patch";
                        }
                        else
                        {
                            CherryVis::log(worker->getID()) << "Continuing with same patch";
                        }
                        status.state++;

                        if (status.initialSplitData)
                        {
                            for (auto &[frame, rotation] : status.initialSplitData->firstRotationDeliveryToSecondRotation)
                            {
                                if (frame == currentFrame)
                                {
                                    status.chosenSecondRotation = rotation;
#if VERBOSE_LOGGING
                                    Log::Get() << worker->getID() << " second rotation: " << rotation;
#endif
                                    CherryVis::log(worker->getID()) << "second rotation: " << rotation;
                                    break;
                                }
                            }
                        }
                    }
                    break;
                case 4:
                    if (status.isResendFrame())
                    {
                        EXPECT_TRUE(worker->gather(status.secondPatch))
                                            << worker->getID() << ": Failed to issue gather command: " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->getID()) << "Resent gather";
                    }
                    if (worker->getOrder() == BWAPI::Orders::WaitForMinerals)
                    {
                        EXPECT_TRUE(status.isSecondGatherActionFrame())
                            << worker->getID() << ": " << currentFrame << " is not an expected second gather action frame";
                        status.state++;
                    }
                    break;
                case 6:
                    if (status.isResendFrame())
                    {
                        EXPECT_TRUE(worker->returnCargo())
                                            << worker->getID() << ": Failed to issue return command: " << BWAPI::Broodwar->getLastError();
                        CherryVis::log(worker->getID()) << "Resent return";
                    }
                    if (!worker->isCarryingMinerals())
                    {
                        EXPECT_TRUE(status.isSecondReturnActionFrame())
                                            << worker->getID() << ": " << currentFrame << " is not an expected second return action frame";
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
                std::optional<int> requireMiningEndBeforeFrame,
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
                    if (frame >= (startFrame + BWAPI::Broodwar->getLatencyFrames() + 2) &&
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
                if (results.empty())
                {
                    Log::Get() << "WARNING: No valid paths for worker " << worker->getID();
                    return result;
                }

                std::uniform_int_distribution<size_t> dist(0, results.size() - 1);
                size_t resultIndex = dist(rng);
                auto chosenResult = results[resultIndex];

                auto pathResults = chosenResult.arrivalData.computePathResult(
                        startFrame,
                        pathStartsWithGatherCommand,
                        chosenResult.resends.empty() ? std::nullopt : (std::optional<int>)*chosenResult.resends.rbegin(),
                        orderProcessTimerResetValues);

                bool validResult = true;

                // If we want to require mining end before a certain frame, validate that all paths satisfy this
                if (requireMiningEndBeforeFrame)
                {
                    for (const auto &pathResult : pathResults)
                    {
                        if ((pathResult.actionFrame + 84) >= *requireMiningEndBeforeFrame)
                        {
                            validResult = false;
                            break;
                        }
                    }
                }

                // If we have been provided with next root nodes, validate that all of the paths have all of the positions covered
                if (nextRootNodes)
                {
                    for (const auto &pathResult : pathResults)
                    {
                        if (!nextRootNodes->contains(pathResult.nextPathStartPosition))
                        {
                            validResult = false;
                            break;
                        }
                    }
                }

                if (!validResult)
                {
                    results.erase(results.begin() + resultIndex);
                    continue;
                }

                // Use this result
                result.resends = chosenResult.resends;
                for (const auto &pathResult : pathResults)
                {
                    result.actionFrames.emplace_back(pathResult.actionFrame);
                    result.nextPathStartPositions.emplace_back(pathResult.nextPathStartPosition);
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
                .startingWorkerPositionToPatchToReturnPaths
                .at(worker->getExactPosition())
                .at(TilePosition::fromBWAPI(firstPatch->getTilePosition()));

        workerGatherPlan.firstGather = planPath(0,
                                                8,
                                                158,
                                                true,
                                                firstGatherRootNode,
                                                &firstReturnNodes);
        if (workerGatherPlan.firstGather.actionFrames.empty()) return workerGatherPlan;

//        auto &secondGatherNodes = mapData
//                .startingWorkerPositionToPatchesToSecondGatherPaths
//                .at(worker->getExactPosition())
//                .at(std::make_pair(TilePosition::fromBWAPI(firstPatch->getTilePosition()), TilePosition::fromBWAPI(secondPatch->getTilePosition())));
        int startFrame = *workerGatherPlan.firstGather.actionFrames.begin() + 84;
        workerGatherPlan.firstReturn = planPath(startFrame + 1,
                                                std::nullopt,
                                                std::nullopt,
                                                false,
                                                firstReturnNodes.at(*workerGatherPlan.firstGather.nextPathStartPositions.begin()),
                                                (std::map<BWAPI::ExactPosition, int>*)nullptr);

#if VERBOSE_LOGGING
        if (!workerGatherPlan.firstReturn.resends.empty())
        {
            auto resendDelta = *workerGatherPlan.firstReturn.resends.begin() - startFrame;
            Log::Get() << worker->getID() << ": Resend delta " << resendDelta;

            auto prepareResult = worker->prepareGatherPath(
                    BWAPI::PrepareGatherPathOptions(*workerGatherPlan.firstGather.nextPathStartPositions.begin())
                    .prepareReturnFrom(firstPatch->getBWIndex()));
            auto normalSimResult = worker->simulateGatherPath(
                BWAPI::SimulateGatherPathOptions(prepareResult->state).setIncludeAllPositions());
            for (int i = -1; i <= 1; i++)
            {
                auto resendSimResult = worker->simulateGatherPath(
                    BWAPI::SimulateGatherPathOptions({prepareResult->startFrame + resendDelta + i}, prepareResult->state)
                    .setIncludeAllPositions());
                Log::Get() << i << ": " << resendSimResult->positions.size();
                for (auto pos : resendSimResult->positions)
                {
                    Log::Get() << "   " << pos;
                }
            }
        }
#endif

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

//        Log::Get() << "Expected path:";
//        auto current = &rootNode;
//        while (current)
//        {
//            Log::Get() << current->pos;
//            if (chosenResult.resendNodes.contains(current))
//            {
//                Log::Get() << "resend takes effect";
//                current = current->nextPositionAfterResend.get();
//            }
//            else
//            {
//                current = current->nextPosition.get();
//            }
//        }
#endif

//        WorkerGatherPlan result;
//        result.firstGather.resends = chosenResult.resends;
//        result.firstGather.actionFrames = actionFrames;
        return workerGatherPlan;
    }
}
