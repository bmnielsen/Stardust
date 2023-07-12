#include "BWTest.h"

#include "DoNothingModule.h"

namespace
{
    struct PositionAndVelocity
    {
        BWAPI::Position position;
        int velocityX;
        int velocityY;

        PositionAndVelocity(BWAPI::Unit unit)
                : position(unit->getPosition())
                , velocityX((int) (unit->getVelocityX() * 100.0))
                , velocityY((int) (unit->getVelocityY() * 100.0)) {}
    };

    inline bool operator<(const PositionAndVelocity &a, const PositionAndVelocity &b)
    {
        return (a.position < b.position) ||
               (a.position == b.position && a.velocityX < b.velocityX) ||
               (a.position == b.position && a.velocityX == b.velocityX && a.velocityY < b.velocityY);
    }

    class OptimizeTimingsModule : public DoNothingModule
    {
        BWAPI::Unit depot;
        BWAPI::Unit worker;
        BWAPI::Unit patch;

        std::map<int, PositionAndVelocity> positionHistory;
        std::set<PositionAndVelocity> optimalMineralOrderPositions;
        std::set<PositionAndVelocity> optimalDepotOrderPositions;

    public:
        OptimizeTimingsModule() : depot(nullptr), worker(nullptr), patch(nullptr) {}

        void onFrame() override
        {
            if (!worker)
            {
                int minDist = INT_MAX;
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isResourceDepot()) {
                        depot = unit;
                        continue;
                    }

                    if (!unit->getType().isWorker()) continue;

                    for (auto mineral : BWAPI::Broodwar->getMinerals())
                    {
                        int dist = mineral->getDistance(unit);
                        if (dist < minDist)
                        {
                            worker = unit;
                            patch = mineral;
                            minDist = dist;
                        }
                    }
                }

                std::cout << "Selected worker @ " << worker->getTilePosition() << " and mineral @ " << patch->getTilePosition() << std::endl;

                worker->move(BWAPI::Position(3876,323));
            }

            if (worker->getOrder() == BWAPI::Orders::Move && BWAPI::Broodwar->getFrameCount() < 100)
            {
                if (worker->getPosition() != BWAPI::Position(3876,323)) return;

                worker->gather(patch);
            }

            int distPatch = worker->getDistance(patch);
            int distDepot = worker->getDistance(depot);

            PositionAndVelocity currentPositionAndVelocity(worker);

            // If the worker is at the resource or depot, record the optimal position
            if (distPatch == 0)
            {
                // The worker is at the resource, so if we have enough position history recorded,
                // record the optimal position
                int frame = BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 11;
                auto positionIt = positionHistory.find(frame);
                if (positionIt != positionHistory.end())
                {
                    auto result = optimalMineralOrderPositions.insert(positionIt->second);
                    if (result.second) std::cout << "Added mineral " << positionIt->second.position << std::endl;
                }

                positionHistory.clear();
            }
            else if (distDepot == 0)
            {
                // The worker is at the depot, so if we have enough position history recorded,
                // record the optimal position
                int frame = BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames() - 11;
                auto positionIt = positionHistory.find(frame);
                if (positionIt != positionHistory.end())
                {
                    auto result = optimalDepotOrderPositions.insert(positionIt->second);
                    if (result.second) std::cout << "Added depot " << positionIt->second.position << std::endl;
                }

                positionHistory.clear();
            }
            else
            {
                // Check if this worker is at an optimal position to resend the gather order
                if (worker->getOrder() == BWAPI::Orders::MoveToMinerals &&
                    optimalMineralOrderPositions.find(currentPositionAndVelocity) != optimalMineralOrderPositions.end())
                {
                    worker->gather(patch);
                    std::cout << "Ordering gather" << std::endl;
                }

                // Check if this worker is at an optimal position to resend the return order
                if (worker->getOrder() == BWAPI::Orders::ReturnMinerals &&
                    optimalDepotOrderPositions.find(currentPositionAndVelocity) != optimalDepotOrderPositions.end())
                {
                    worker->rightClick(depot->getPosition());
                    std::cout << "Ordering return" << std::endl;
                }

                // Record the worker's position
                positionHistory.emplace(std::make_pair(BWAPI::Broodwar->getFrameCount(), currentPositionAndVelocity));
            }

            if (worker->getOrder() != BWAPI::Orders::MiningMinerals)
            {
                std::cout << BWAPI::Broodwar->getFrameCount()
                          << ";" << worker->getPosition() << ";distpatch=" << distPatch << ";distdepot=" << distDepot
                          << ";spd=" << sqrt(worker->getVelocityX() * worker->getVelocityX() + worker->getVelocityY() * worker->getVelocityY())
                          << ";cmd=" << worker->getLastCommand().getType() << ";f=" << (BWAPI::Broodwar->getFrameCount() - worker->getLastCommandFrame())
                          << ";ord=" << worker->getOrder() << ";t=" << worker->getOrderTimer()
                          << ";carry=" << worker->isCarryingMinerals()
                          << std::endl;
            }
        }
    };
}

TEST(MiningTimings, OptimizeBoth)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.myModule = []()
    {
        return new OptimizeTimingsModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 500;
    test.expectWin = false;

    test.run();
}

TEST(MiningTimings, TestReturnOptimization)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.onStartMine = []()
    {
        BWAPI::Broodwar->setLatCom(false);
    };

    int state = 0;
    int leftMinerals = 0;
    int arrivedDepot = 0;
    int leftDepot = 0;
    int arrivedMinerals = 0;
    bool orderResetPending = false;

    int reissueFrame = -1;

    test.onFrameMine = [&]()
    {
        BWAPI::Unit probe;
        BWAPI::Unit depot;
        int lowestID = INT_MAX;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus) depot = unit;
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Probe) continue;
            if (unit->getID() < lowestID)
            {
                probe = unit;
                lowestID = unit->getID();
            }
        }
        if (!probe) return;
        if (!depot) return;

        BWAPI::Unit mineralPatch;
        for (auto unit : BWAPI::Broodwar->getMinerals())
        {
            if (BWAPI::WalkPosition(unit->getPosition()) == BWAPI::WalkPosition(500, 38)) mineralPatch = unit;
        }
        if (!mineralPatch) return;

        int depotDist = probe->getDistance(depot);
        int mineralDist = probe->getDistance(mineralPatch);

        switch (state)
        {
            case 0:
                if (BWAPI::Broodwar->getFrameCount() == 35)
                {
                    probe->gather(mineralPatch);
                    state = 1;
                }
                break;
            case 1:
                if (mineralDist == 0)
                {
                    int newArrivedMinerals = BWAPI::Broodwar->getFrameCount();

                    if (arrivedMinerals > 0)
                    {
                        std::cout << BWAPI::Broodwar->getFrameCount()
                            << ": Round trip = " << (newArrivedMinerals - arrivedMinerals)
                            << "; excluding mining = " << (newArrivedMinerals - leftMinerals)
                            << "; deliver minerals = " << (arrivedDepot - leftMinerals)
                            << "; stopped at depot = " << (leftDepot - arrivedDepot)
                            << "; travel to minerals = " << (newArrivedMinerals - leftDepot)
                            << "; order reissued at " << ((!orderResetPending && reissueFrame > 0) ? reissueFrame : -1)
                            << (orderResetPending ? "; order reset" : "")
                            << std::endl;
                    }

                    state = 2;
                    if (!orderResetPending) reissueFrame++;
                    orderResetPending = false;
                    arrivedMinerals = newArrivedMinerals;
                }
                break;
            case 2:
                if (mineralDist > 0)
                {
                    leftMinerals = BWAPI::Broodwar->getFrameCount();
                    state = 3;

                    if ((BWAPI::Broodwar->getFrameCount() - 8) % 150 > (150 - 30))
                    {
                        orderResetPending = true;
                    }
                }
                break;
            case 3:
                if (depotDist == 0)
                {
                    arrivedDepot = BWAPI::Broodwar->getFrameCount();
                    state = 4;
                }
                else if (!orderResetPending && reissueFrame > 0 && BWAPI::Broodwar->getFrameCount() == (leftMinerals + reissueFrame))
                {
                    probe->returnCargo();
                }

                break;
            case 4:
                if (depotDist > 0)
                {
                    leftDepot = BWAPI::Broodwar->getFrameCount();
                    state = 1;
                }
                break;
        }

//        std::cout << std::fixed << std::showpoint << std::setprecision(4)
//            << BWAPI::Broodwar->getFrameCount()
//            << ": " << probe->getOrder()
//            << "; pos=" << probe->getPosition()
//            << "; distToMinerals=" << probe->getDistance(mineralPatch)
//            << "; distToDepot=" << probe->getDistance(depot)
//            << "; speed=" << std::sqrt(((probe->getVelocityX() * probe->getVelocityX()) + (probe->getVelocityY() * probe->getVelocityY())))
//            << std::endl;
    };

    test.run();
}

TEST(MiningTimings, TestReturnOptimization2)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.onStartMine = []()
    {
        BWAPI::Broodwar->setLatCom(false);
    };

    int state = 0;
    int leftMinerals = 0;
    int arrivedDepot = 0;
    int leftDepot = 0;
    int arrivedMinerals = 0;
    bool orderResetPending = false;

    int reissueFrame = 14;

    test.onFrameMine = [&]()
    {
        BWAPI::Unit probe;
        BWAPI::Unit depot;
        int lowestID = INT_MAX;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Nexus) depot = unit;
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Probe) continue;
            if (unit->getID() < lowestID)
            {
                probe = unit;
                lowestID = unit->getID();
            }
        }
        if (!probe) return;
        if (!depot) return;

        BWAPI::Unit mineralPatch;
        for (auto unit : BWAPI::Broodwar->getMinerals())
        {
            if (BWAPI::WalkPosition(unit->getPosition()) == BWAPI::WalkPosition(500, 38)) mineralPatch = unit;
        }
        if (!mineralPatch) return;

        int depotDist = probe->getDistance(depot);
        int mineralDist = probe->getDistance(mineralPatch);

        switch (state)
        {
            case 0:
                if (BWAPI::Broodwar->getFrameCount() == 35)
                {
                    probe->gather(mineralPatch);
                    state = 1;
                }
                break;
            case 1:
                if (mineralDist == 0)
                {
                    int newArrivedMinerals = BWAPI::Broodwar->getFrameCount();

                    if (arrivedMinerals > 0)
                    {
                        std::cout << BWAPI::Broodwar->getFrameCount()
                            << ": Round trip = " << (newArrivedMinerals - arrivedMinerals)
                            << "; excluding mining = " << (newArrivedMinerals - leftMinerals)
                            << "; deliver minerals = " << (arrivedDepot - leftMinerals)
                            << "; stopped at depot = " << (leftDepot - arrivedDepot)
                            << "; travel to minerals = " << (newArrivedMinerals - leftDepot)
                            << "; order reissued at " << ((!orderResetPending && reissueFrame > 0) ? reissueFrame : -1)
                            << (orderResetPending ? "; order reset" : "")
                            << std::endl;
                    }

                    state = 2;
                    orderResetPending = false;
                    arrivedMinerals = newArrivedMinerals;
                }
                break;
            case 2:
                if (mineralDist > 0)
                {
                    leftMinerals = BWAPI::Broodwar->getFrameCount();
                    state = 3;

                    if ((BWAPI::Broodwar->getFrameCount() - 8) % 150 > (150 - 30))
                    {
                        orderResetPending = true;
                    }
                }
                break;
            case 3:
                if (depotDist == 0)
                {
                    arrivedDepot = BWAPI::Broodwar->getFrameCount();
                    state = 4;
                }
                else if (!orderResetPending && reissueFrame > 0 && BWAPI::Broodwar->getFrameCount() == (leftMinerals + reissueFrame))
                {
                    probe->returnCargo();
                }

                break;
            case 4:
                if (depotDist > 0)
                {
                    leftDepot = BWAPI::Broodwar->getFrameCount();
                    state = 1;
                }
                break;
        }

        std::cout << std::fixed << std::showpoint << std::setprecision(4)
            << BWAPI::Broodwar->getFrameCount()
            << ": " << probe->getOrder()
            << "; pos=" << probe->getPosition()
            << "; distToMinerals=" << probe->getDistance(mineralPatch)
            << "; distToDepot=" << probe->getDistance(depot)
            << "; speed=" << std::sqrt(((probe->getVelocityX() * probe->getVelocityX()) + (probe->getVelocityY() * probe->getVelocityY())))
            << std::endl;
    };

    test.run();
}
