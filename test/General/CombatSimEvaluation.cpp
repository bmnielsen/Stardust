#include "BWTest.h"
#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"

#define ITERATIONS 288

namespace
{
    struct SimResults
    {
        std::map<int, std::pair<int, int>> frameToActuals;
        std::map<int, std::vector<std::pair<int, std::pair<int, int>>>> frameToSimulated;
    };

    BWAPI::Position computeCentroid(const std::vector<UnitTypeAndPosition> &units)
    {
        if (units.empty()) return BWAPI::Positions::Invalid;

        int centroidX = 0;
        int centroidY = 0;
        for (const auto &unit : units)
        {
            centroidX += unit.getCenterPosition().x;
            centroidY += unit.getCenterPosition().y;
        }
        return {centroidX / (int)units.size(), centroidY / (int)units.size()};
    }

    void attackMoveOpponent(BWTest &test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };

        test.onStartOpponent = []()
        {
            BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge, 1);
        };

        auto myCentroid = computeCentroid(test.myInitialUnits);
        test.onFrameOpponent = [&myCentroid]()
        {
            int attackUnits = 0;

            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker()) continue;
                if (unit->getType().supplyProvided() > 0) continue;

                attackUnits++;

                if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Move) continue;

                unit->attack(myCentroid);
            }

            if (BWAPI::Broodwar->getFrameCount() > 10 && attackUnits == 0)
            {
                BWAPI::Broodwar->leaveGame();
            }
        };
    }

    void holdPositionOpponent(BWTest &test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };

        test.onStartOpponent = []()
        {
            BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge, 1);
        };

        test.onFrameOpponent = []()
        {
            int attackUnits = 0;

            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker()) continue;
                if (unit->getType().supplyProvided() > 0) continue;

                attackUnits++;

                if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Hold_Position) continue;

                unit->holdPosition();
            }

            if (BWAPI::Broodwar->getFrameCount() > 10 && attackUnits == 0)
            {
                BWAPI::Broodwar->leaveGame();
            }
        };
    }

    void attackBaseMine(BWTest &test, SimResults &results, BWAPI::TilePosition enemyMain)
    {
        test.frameLimit = 2000;
        test.expectWin = false;

        test.myInitialUnits.emplace_back(BWAPI::UnitTypes::Protoss_Observer, computeCentroid(test.opponentInitialUnits), true);

        Base *baseToAttack;
        test.onStartMine = [&baseToAttack]()
        {
            CombatSim::setMaxIterations(ITERATIONS);

            BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge, 1);

            baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 9)));

            Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

            std::vector<std::shared_ptr<Play>> openingPlays;
            openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
            Strategist::setOpening(openingPlays);
        };

        unsigned long lastCount = 0;
        test.onFrameMine = [&]()
        {
            auto squad = General::getAttackBaseSquad(baseToAttack);
            if (!squad) return;

            auto cluster = squad->vanguardCluster();
            if (!cluster) return;

            // Only process new results
            if (cluster->recentSimResults.size() == lastCount) return;
            lastCount = cluster->recentSimResults.size();
            if (cluster->recentSimResults.empty()) return;

            auto &simResult = cluster->recentSimResults.rbegin()->first;

            // Set the actuals
            results.frameToActuals[BWAPI::Broodwar->getFrameCount() - 1] = std::make_pair(simResult.initialMine, simResult.initialEnemy);

            // Set the simulated values for the future frame
#if DEBUG_COMBATSIM_EACHFRAME
            for (int i = 0; i < simResult.eachFrameMine.size(); i++)
            {
                // Register result on the frame where the sim is finished - either one army is dead or the iteration count is reached
                if (i == (simResult.eachFrameMine.size() - 1) || simResult.eachFrameMine[i] == 0 || simResult.eachFrameEnemy[i] == 0)
                {
                    results
                        .frameToSimulated[BWAPI::Broodwar->getFrameCount() + i - 1]
                        .emplace_back(BWAPI::Broodwar->getFrameCount() - 1, std::make_pair(simResult.eachFrameMine[i], simResult.eachFrameEnemy[i]));
                    break;
                }
            }
#endif
        };
    }

    void outputResults(BWTest &test, SimResults &results)
    {
#if !(DEBUG_COMBATSIM_EACHFRAME)
        std::cout << "ERROR: Cannot do sim analysis without DEBUG_COMBATSIM_EACHFRAME being enabled" << std::endl;
#endif

        test.onEndMine = [&](bool)
        {
            if (results.frameToActuals.empty())
            {
                std::cout << "No results" << std::endl;
                return;
            }

            // Compute the final state
            // Right now we are just assuming the lowest-value army dies completely
            auto &[finalMine, finalEnemy] = results.frameToActuals.rbegin()->second;
            if (finalMine < finalEnemy)
            {
                finalMine = 0;
            }
            else
            {
                finalEnemy = 0;
            }

            unsigned int simCount = 0;
            unsigned long squaredErrors = 0;
            int highestErrFrame = 0;
            int highestErr = 0;
            for (const auto &[frame, simulated] : results.frameToSimulated)
            {
                int actualMine;
                int actualEnemy;

                auto actualIt = results.frameToActuals.find(frame);
                if (actualIt == results.frameToActuals.end())
                {
                    actualMine = finalMine;
                    actualEnemy = finalEnemy;
                }
                else
                {
                    actualMine = actualIt->second.first;
                    actualEnemy = actualIt->second.second;
                }

                for (const auto &[simFrame, result] : simulated)
                {
                    auto &[simulatedMine, simulatedEnemy] = result;

                    int err = std::abs(actualMine - simulatedMine) + std::abs(actualEnemy - simulatedEnemy);
                    squaredErrors += (err * err);
                    simCount++;

                    if (err > highestErr)
                    {
                        highestErr = err;
                        highestErrFrame = simFrame;
                    }
//
//                    Log::Debug()
//                            << simFrame
//                            << ": Err=" << err
//                            << "; Mine: " << simulatedMine << "->" << actualMine
//                            << "; Enemy: " << simulatedEnemy << "->" << actualEnemy;
                }
            }

            if (simCount > 0)
            {
                double rmse = std::sqrt((double)squaredErrors / (double)simCount);
                std::cout << "RMSE: " << rmse << std::endl;
                std::cout << "Worst frame: " << highestErrFrame << " (" << highestErr << ")" << std::endl;
            }
            else
            {
                std::cout << "No results" << std::endl;
            }
        };
    }
}

TEST(CombatSimEvaluation, OpenGround_Dragoons_AttackMove_SmallNumber_Outnumbering)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1659, 1412), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1697, 1468), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1635, 1479), true),
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1178, 1376), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1331, 1241), true),
    };

    SimResults results;
    attackMoveOpponent(test);
    attackBaseMine(test, results, BWAPI::TilePosition(7, 9));
    outputResults(test, results);

    test.run();
}

TEST(CombatSimEvaluation, OpenGround_Dragoons_AttackMove_LargeNumber_Outnumbering)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1659, 1412), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1697, 1468), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1635, 1479), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1609, 1434), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1635, 1515), true),
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1178, 1376), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1331, 1241), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1318, 1319), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1270, 1378), true),
    };

    SimResults results;
    attackMoveOpponent(test);
    attackBaseMine(test, results, BWAPI::TilePosition(7, 9));
    outputResults(test, results);

    test.run();
}

TEST(CombatSimEvaluation, OpenGround_Dragoons_AttackMove_SmallNumber_Outnumbered)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1659, 1412), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1697, 1468), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1635, 1479), true),
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1178, 1376), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1331, 1241), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1318, 1319), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1270, 1378), true),
    };

    SimResults results;
    attackMoveOpponent(test);
    attackBaseMine(test, results, BWAPI::TilePosition(7, 9));
    outputResults(test, results);

    test.run();
}

TEST(CombatSimEvaluation, OpenGround_Dragoons_AttackMove_LargeNumber_Outnumbered)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1659, 1412), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1697, 1468), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1635, 1479), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1609, 1434), true),
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1178, 1376), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1212, 1376), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1331, 1241), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1318, 1319), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1270, 1378), true),
    };

    SimResults results;
    attackMoveOpponent(test);
    attackBaseMine(test, results, BWAPI::TilePosition(7, 9));
    outputResults(test, results);

    test.run();
}

TEST(CombatSimEvaluation, Bridge_Dragoons_AttackMove_Outnumbering)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1120, 1383), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1068, 1378), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1120, 1423), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1068, 1418), true),
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(735, 1228), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(705, 1148), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(767, 1151), true),
    };

    SimResults results;
    attackMoveOpponent(test);
    attackBaseMine(test, results, BWAPI::TilePosition(7, 9));
    outputResults(test, results);

    test.run();
}

TEST(CombatSimEvaluation, Bridge_Dragoons_HoldPosition_Outnumbering)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1120, 1383), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1068, 1378), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1120, 1423), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1068, 1418), true),
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(735, 1228), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(767, 1214), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(799, 1200), true),
    };

    SimResults results;
    holdPositionOpponent(test);
    attackBaseMine(test, results, BWAPI::TilePosition(7, 9));
    outputResults(test, results);

    test.run();
}

TEST(CombatSimEvaluation, Bridge_Dragoons_AttackMove_Outnumbered)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1120, 1383), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1068, 1378), true),
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(735, 1228), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(705, 1148), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(767, 1151), true),
    };

    SimResults results;
    attackMoveOpponent(test);
    attackBaseMine(test, results, BWAPI::TilePosition(7, 9));
    outputResults(test, results);

    test.run();
}

TEST(CombatSimEvaluation, Bridge_Dragoons_HoldPosition_Outnumbered)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1120, 1383), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1068, 1378), true),
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(735, 1228), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(767, 1214), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(799, 1200), true),
    };

    SimResults results;
    holdPositionOpponent(test);
    attackBaseMine(test, results, BWAPI::TilePosition(7, 9));
    outputResults(test, results);

    test.run();
}
