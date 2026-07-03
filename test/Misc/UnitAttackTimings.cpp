#include "BWTest.h"
#include "DoNothingModule.h"

namespace
{
    struct TestCase
    {
        // Input

        BWAPI::UnitType unitType;
        bool groundAttack;

        // Output

        int cooldownStartFrame = 0;
        BWAPI::BulletType bulletType = BWAPI::BulletTypes::None;
        int bulletFrame = 0;
        int bulletRemoveDelay = 0;
        int damageFrame = 0;

        [[nodiscard]] int delay() const
        {
            if (bulletType == BWAPI::BulletTypes::None)
            {
                return (damageFrame - cooldownStartFrame);
            }
            return (bulletFrame - cooldownStartFrame);
        }

        friend std::ostream& operator<< (std::ostream& os, const TestCase& testCase)
        {
            os << testCase.unitType;
            if (testCase.groundAttack)
            {
                os << " gnd: ";
            }
            else
            {
                os << " air: ";
            }

            os << testCase.delay();

            if (testCase.bulletType != BWAPI::BulletTypes::None)
            {
                os << " (Bullet " << testCase.bulletType
                   << ", remove timer " << testCase.bulletRemoveDelay
                   << ", took " << (testCase.damageFrame - testCase.bulletFrame) << " to hit)";
            }
            return os;
        }
    };

    BWAPI::Bullet getActiveBullet()
    {
        for (auto bullet : BWAPI::Broodwar->getBullets())
        {
            // Ignore invalid bullets
            if (!bullet->exists() || !bullet->isVisible() ||
                (!bullet->getSource() && !bullet->getTarget()))
            {
                continue;
            }

            return bullet;
        }

        return nullptr;
    }
}

TEST(UnitAttackTimings, UnitAttackTimings)
{
    BWTest test;
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 10000;
    test.expectWin = false;

    std::vector<TestCase> testCases = {
        {BWAPI::UnitTypes::Terran_Marine, true},
        {BWAPI::UnitTypes::Terran_Ghost, true},
        {BWAPI::UnitTypes::Terran_Vulture, true},
        {BWAPI::UnitTypes::Terran_Goliath, true},
        {BWAPI::UnitTypes::Terran_Goliath, false},
        {BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode, true},
        {BWAPI::UnitTypes::Terran_SCV, true},
        {BWAPI::UnitTypes::Terran_Wraith, true},
        {BWAPI::UnitTypes::Terran_Wraith, false},
        {BWAPI::UnitTypes::Terran_Battlecruiser, true},
        {BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, true},
        {BWAPI::UnitTypes::Terran_Firebat, true},
        {BWAPI::UnitTypes::Zerg_Zergling, true},
        {BWAPI::UnitTypes::Zerg_Hydralisk, true},
        {BWAPI::UnitTypes::Zerg_Ultralisk, true},
        {BWAPI::UnitTypes::Zerg_Broodling, true},
        {BWAPI::UnitTypes::Zerg_Drone, true},
        {BWAPI::UnitTypes::Zerg_Mutalisk, true},
        {BWAPI::UnitTypes::Zerg_Guardian, true},
        {BWAPI::UnitTypes::Terran_Valkyrie, false},
        {BWAPI::UnitTypes::Protoss_Corsair, false},
        {BWAPI::UnitTypes::Protoss_Dark_Templar, true},
        {BWAPI::UnitTypes::Zerg_Devourer, false},
        {BWAPI::UnitTypes::Protoss_Probe, true},
        {BWAPI::UnitTypes::Protoss_Zealot, true},
        {BWAPI::UnitTypes::Protoss_Dragoon, true},
        {BWAPI::UnitTypes::Protoss_Archon, true},
        {BWAPI::UnitTypes::Protoss_Scout, true},
        {BWAPI::UnitTypes::Protoss_Scout, false},
        {BWAPI::UnitTypes::Protoss_Arbiter, true},
        {BWAPI::UnitTypes::Zerg_Lurker, true},
        {BWAPI::UnitTypes::Terran_Missile_Turret, false},
        {BWAPI::UnitTypes::Zerg_Spore_Colony, false},
        {BWAPI::UnitTypes::Zerg_Sunken_Colony, true},
        {BWAPI::UnitTypes::Protoss_Photon_Cannon, true},
    };

    std::deque<TestCase*> remainingTestCases;
    for (auto &testCase : testCases) remainingTestCases.emplace_back(&testCase);

    BWAPI::Unit groundTarget = nullptr;
    BWAPI::Unit airTarget = nullptr;
    BWAPI::Unit testUnit = nullptr;

    // States:
    // 0 - initializing test
    // 1 - ready to start a new test case
    // 2 - waiting for test unit to be created

    // 100 - error state
    int state = 0;
    test.onFrameMine = [&]()
    {
        if (remainingTestCases.empty())
        {
            BWAPI::Broodwar->leaveGame();
            return;
        }

        auto &currentTestCase = *remainingTestCases.front();

        while (true)
        {
            switch (state)
            {
                // Kill our workers and create the target units
                case 0:
                    if (BWAPI::Broodwar->getFrameCount() == 1)
                    {
                        for (auto unit : BWAPI::Broodwar->self()->getUnits())
                        {
                            if (unit->getType().isWorker()) BWAPI::Broodwar->killUnit(unit);
                        }

                        BWAPI::Broodwar->createUnit(
                            BWAPI::Broodwar->self(),
                            BWAPI::UnitTypes::Zerg_Ultralisk,
                            BWAPI::Position(BWAPI::TilePosition(42, 31)));
                        BWAPI::Broodwar->createUnit(
                            BWAPI::Broodwar->self(),
                            BWAPI::UnitTypes::Zerg_Overlord,
                            BWAPI::Position(BWAPI::TilePosition(78, 31)));
                    }
                    if (BWAPI::Broodwar->getFrameCount() == 10)
                    {
                        for (auto unit : BWAPI::Broodwar->self()->getUnits())
                        {
                            if (unit->getType() == BWAPI::UnitTypes::Zerg_Ultralisk) groundTarget = unit;
                            if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) airTarget = unit;
                        }
                        if (!groundTarget || !airTarget)
                        {
                            std::cout << "ERROR: Could not find one of the target units" << std::endl;
                            state = 100;
                            return;
                        }

                        // Give power for a forge and test cannon
                        BWAPI::Broodwar->createUnit(
                            BWAPI::Broodwar->self(),
                            BWAPI::UnitTypes::Protoss_Pylon,
                            BWAPI::Position(BWAPI::TilePosition(47, 28)));

                        // Give creep for test spore and sunkens
                        BWAPI::Broodwar->createUnit(
                            BWAPI::Broodwar->self(),
                            BWAPI::UnitTypes::Zerg_Hatchery,
                            BWAPI::Position(BWAPI::TilePosition(47, 35)));
                        BWAPI::Broodwar->createUnit(
                            BWAPI::Broodwar->self(),
                            BWAPI::UnitTypes::Zerg_Hatchery,
                            BWAPI::Position(BWAPI::TilePosition(73, 28)));
                    }
                    if (BWAPI::Broodwar->getFrameCount() == 20)
                    {
                        BWAPI::Broodwar->createUnit(
                            BWAPI::Broodwar->self(),
                            BWAPI::UnitTypes::Protoss_Forge,
                            BWAPI::Position(BWAPI::TilePosition(50, 28)));
                    }
                    if (BWAPI::Broodwar->getFrameCount() == 5000)
                    {
                        // Wait for creep to spread
                        state = 1;
                        continue;
                    }
                    return;

                // Create the test unit
                case 1:
                {
                    BWAPI::TilePosition testUnitPosition;
                    if (currentTestCase.groundAttack)
                    {
                        if (currentTestCase.unitType == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode)
                        {
                            testUnitPosition = BWAPI::TilePosition(42, 39);
                        }
                        else if (currentTestCase.unitType == BWAPI::UnitTypes::Protoss_Photon_Cannon)
                        {
                            testUnitPosition = BWAPI::TilePosition(42, 28);
                        }
                        else if (currentTestCase.unitType == BWAPI::UnitTypes::Protoss_Archon)
                        {
                            testUnitPosition = BWAPI::TilePosition(42, 33);
                        }
                        else if (currentTestCase.unitType == BWAPI::UnitTypes::Terran_Wraith)
                        {
                            testUnitPosition = BWAPI::TilePosition(42, 36);
                        }
                        else
                        {
                            testUnitPosition = BWAPI::TilePosition(42, 34);
                        }
                    }
                    else
                    {
                        if (currentTestCase.unitType == BWAPI::UnitTypes::Zerg_Spore_Colony)
                        {
                            testUnitPosition = BWAPI::TilePosition(78, 29);
                        }
                        else
                        {
                            testUnitPosition = BWAPI::TilePosition(78, 36);
                        }
                    }
                    BWAPI::Broodwar->createUnit(
                        BWAPI::Broodwar->self(),
                        currentTestCase.unitType,
                        BWAPI::Position(testUnitPosition));
                    state = 2;
                    return;
                }

                // Wait for the test unit to be created
                case 2:
                    for (auto unit : BWAPI::Broodwar->self()->getUnits())
                    {
                        if (unit == groundTarget || unit == airTarget) continue;
                        if (unit->getType() == currentTestCase.unitType) testUnit = unit;
                    }
                    if (testUnit && testUnit->exists() && testUnit->isVisible())
                    {
                        state = 3;
                        continue;
                    }
                    return;

                // Order the test unit to attack the target
                case 3:
                    // Lurkers must burrow first
                    if (testUnit->getType() == BWAPI::UnitTypes::Zerg_Lurker && !testUnit->isBurrowed())
                    {
                        if (testUnit->getLastCommand().getType() != BWAPI::UnitCommandTypes::Burrow)
                        {
                            testUnit->burrow();
                        }
                        return;
                    }

                    if (!testUnit->attack(currentTestCase.groundAttack ? groundTarget : airTarget))
                    {
                        // Lurker can return Unit_Busy until it is fully burrowed
                        return;
                    }

                    state = 4;
                    return;

                // Record when the test unit goes on cooldown
                case 4:
                    if (testUnit->getGroundWeaponCooldown() > 0 || testUnit->getAirWeaponCooldown() > 0)
                    {
                        currentTestCase.cooldownStartFrame = BWAPI::Broodwar->getFrameCount();
                        state = 5;
                        continue;
                    }
                    return;

                // Record when a bullet is created
                case 5:
                {
                    auto bullet = getActiveBullet();
                    if (bullet)
                    {
                        currentTestCase.bulletFrame = BWAPI::Broodwar->getFrameCount();
                        currentTestCase.bulletType = bullet->getType();
                        currentTestCase.bulletRemoveDelay = bullet->getRemoveTimer();
                        state = 6;
                        continue;
                    }

                    // Intentional fall-through
                }

                // Record when the target has damage dealt to it, unless it's a lurker
                case 6:
                {
                    auto target = currentTestCase.groundAttack ? groundTarget : airTarget;
                    if (testUnit->getType() == BWAPI::UnitTypes::Zerg_Lurker || target->getHitPoints() < target->getType().maxHitPoints())
                    {
                        currentTestCase.damageFrame = BWAPI::Broodwar->getFrameCount();
                        BWAPI::Broodwar->killUnit(testUnit);
                        state = 7;
                        return;
                    }

                    return;
                }

                // Wait for test unit to die
                case 7:
                    if (testUnit->exists()) return;

                    testUnit = nullptr;
                    state = 8;

                    // Intentional fall-through

                // Wait for any bullets to expire
                case 8:
                    if (getActiveBullet()) return;

                    remainingTestCases.pop_front();
                    groundTarget->setHitPoints(groundTarget->getType().maxHitPoints());
                    airTarget->setHitPoints(airTarget->getType().maxHitPoints());
                    state = 1;
                    return;

                case 100:
                    return;
                default:
                    std::cout << "ERROR: Unknown state " << state << std::endl;
                    return;
            }
        }
    };

    test.run();

    std::array<int, 300> delays = {0};
    for (auto &testCase : testCases)
    {
        std::cout << testCase << std::endl;
        delays[testCase.unitType.getID()] = testCase.delay();
    }

    std::string sep;
    for (size_t i = 0; i < 300; ++i)
    {
        std::cout << sep;
        if (i % 10 == 0) std::cout << "\n";
        std::cout << delays[i];
        sep = ", ";
    }
}
