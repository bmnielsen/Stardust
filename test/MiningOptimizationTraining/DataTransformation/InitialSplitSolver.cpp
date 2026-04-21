//#include "InitialSplitSolver.h"
//
//namespace MiningOptimizationTraining
//{
//    MiningOptimization::InitialSplitData InitialSplitSolver::execute()
//    {
//        // This method creates the best plan for the given combination of patches for this worker
//        // The best plan is the one that gets the earliest second delivery
//        // If there is variance in the possible order process timer resets, we use the worst case
//        // TODO: Test if it is better to use the average case
//
//        EXPECT_TRUE(mapData.startingWorkerPositionToOrderProcessTimerReset.contains(startPosition.pos()))
//                            << "No order process timer reset value data for start position " << startPosition;
//        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToFirstGatherPath.contains(startPosition))
//                            << "No first gather path data for start position " << startPosition;
//        EXPECT_TRUE(mapData.startingWorkerPositionToPatchToFirstGatherPath.at(startPosition).contains(firstPatch))
//                            << "No first gather path data for start position " << startPosition << " and patch " << firstPatch;
//
//        std::set<int> orderProcessTimerResetValues;
//        {
//            for (const auto &resetData : mapData.startingWorkerPositionToOrderProcessTimerReset.at(startPosition.pos()))
//            {
//                if (resetData.opponentIsZerg && (knownEnemyRace != BWAPI::Races::Random && knownEnemyRace != BWAPI::Races::Zerg)) continue;
//                if (!resetData.opponentIsZerg && (knownEnemyRace == BWAPI::Races::Zerg)) continue;
//
//                orderProcessTimerResetValues.insert(resetData.value);
//            }
//        }
//
//        std::vector<SolveResult> results;
//
//        auto &rootNode = mapData.startingWorkerPositionToPatchToFirstGatherPath.at(startPosition).at(firstPatch);
//        results.emplace_back(rootNode.arrivalData, {});
//
//        std::deque<std::pair<const InitialWorkerGatherPathNode*, std::set<int>>> nodeQueue;
//        nodeQueue.emplace_back(
//                &mapData.startingWorkerPositionToPatchToFirstGatherPath.at(startPosition).at(firstPatch),
//                std::set<int>{});
//
//
//        while (!nodeQueue.empty())
//        {
//            auto &[node, resends] = *nodeQueue.begin();
//            nodeQueue.pop_front();
//
//            // Add the result if we don't resend again
//            results.emplace_back(node->arrivalData.arrivalDelay);
//        }
//
//
//        // Implement a stripped-down version of the solver logic
//
//        return {};
//    }
//}
