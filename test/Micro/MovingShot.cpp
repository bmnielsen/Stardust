#include "BWTest.h"

#include "DoNothingModule.h"

#include "Units.h"
#include "Workers.h"
#include "MyWorker.h"

#include "Header.h" // McRave

TEST(MovingShot, ProbeVsSCV)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(75, 20)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::TilePosition(72, 20)),
    };

    MyUnit probe = nullptr;
    Unit target = nullptr;

    test.onFrameMine = [&]()
    {
        if (!probe)
        {
            for (auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (unit->getDistance(BWAPI::Position(BWAPI::TilePosition(69, 20))) < 600)
                {
                    probe = unit;
                    Workers::reserveWorker(probe);
                    break;
                }
            }
            if (!probe) return;
        }

        if (!probe->exists())
        {
            CherryVis::log(probe->id) << "DIED";
            BWAPI::Broodwar->leaveGame();
            ASSERT_FALSE(true) << "Probe died";
            return;
        }

        if (!target)
        {
            for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_SCV))
            {
                target = unit;
            }
            if (!target) return;
        }
        if (!target->exists())
        {
            CherryVis::log(target->id) << "DIED";
            BWAPI::Broodwar->leaveGame();
            return;
        }

        probe->attackUnit(target);
    };

    test.onFrameOpponent = []()
    {
        BWAPI::Unit target = nullptr;
        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Terran_SCV) continue;
            if (unit->getDistance(BWAPI::Position(BWAPI::TilePosition(69, 20))) > 1000) continue;
            target = unit;
            break;
        }
        if (!target) return;

        if (target->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit) return;

        for (auto &unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe)
            {
                target->attack(unit);
                return;
            }
        }
    };

    test.onEndMine = [&](bool won)
    {
        if (target)
        {
            EXPECT_FALSE(target->exists()) << "Failed to kill target";
        }

        if (probe)
        {
            std::cout << "Final probe stats: shield=" << probe->lastShields << "; health=" << probe->lastHealth << std::endl;
        }
    };

    test.run();
}

TEST(MovingShot, ProbeVsProbe)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(75, 20)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(72, 20)),
    };

    MyUnit probe = nullptr;
    Unit target = nullptr;

    test.onFrameMine = [&]()
    {
        if (!probe)
        {
            for (auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (unit->getDistance(BWAPI::Position(BWAPI::TilePosition(69, 20))) < 600)
                {
                    probe = unit;
                    Workers::reserveWorker(probe);
                    break;
                }
            }
            if (!probe) return;
        }

        if (!probe->exists())
        {
            CherryVis::log(probe->id) << "DIED";
            BWAPI::Broodwar->leaveGame();
            ASSERT_FALSE(true) << "Probe died";
            return;
        }

        if (!target)
        {
            for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                target = unit;
            }
            if (!target) return;
        }
        if (!target->exists())
        {
            CherryVis::log(target->id) << "DIED";
            BWAPI::Broodwar->leaveGame();
            return;
        }

        probe->attackUnit(target);
    };

    test.onFrameOpponent = []()
    {
        BWAPI::Unit target = nullptr;
        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Probe) continue;
            if (unit->getDistance(BWAPI::Position(BWAPI::TilePosition(69, 20))) > 1000) continue;
            target = unit;
            break;
        }
        if (!target) return;

        if (target->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit) return;

        for (auto &unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe)
            {
                target->attack(unit);
                return;
            }
        }
    };

    test.onEndMine = [&](bool won)
    {
        if (target)
        {
            EXPECT_FALSE(target->exists()) << "Failed to kill target";
        }

        if (probe)
        {
            std::cout << "Final probe stats: shield=" << probe->lastShields << "; health=" << probe->lastHealth << std::endl;
        }
    };

    test.run();
}

TEST(MovingShot, ProbeVsZealot)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(75, 20)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(72, 20)),
    };

    MyUnit probe = nullptr;
    Unit target = nullptr;

    test.onFrameMine = [&]()
    {
        if (!probe)
        {
            for (auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (unit->getDistance(BWAPI::Position(BWAPI::TilePosition(69, 20))) < 600)
                {
                    probe = unit;
                    Workers::reserveWorker(probe);
                    break;
                }
            }
            if (!probe) return;
        }

        if (!probe->exists())
        {
            CherryVis::log(probe->id) << "DIED";
            BWAPI::Broodwar->leaveGame();
            ASSERT_FALSE(true) << "Probe died";
            return;
        }

        if (!target)
        {
            for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Zealot))
            {
                target = unit;
            }
            if (!target) return;
        }
        if (!target->exists())
        {
            CherryVis::log(target->id) << "DIED";
            BWAPI::Broodwar->leaveGame();
            return;
        }

        probe->attackUnit(target);
    };

    test.onFrameOpponent = []()
    {
        BWAPI::Unit target = nullptr;
        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Zealot) continue;
            if (unit->getDistance(BWAPI::Position(BWAPI::TilePosition(69, 20))) > 1000) continue;
            target = unit;
            break;
        }
        if (!target) return;

        if (target->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit) return;

        for (auto &unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe)
            {
                target->attack(unit);
                return;
            }
        }
    };

    test.onEndMine = [&](bool won)
    {
        if (target)
        {
            EXPECT_FALSE(target->exists()) << "Failed to kill target";
        }

        if (probe)
        {
            std::cout << "Final probe stats: shield=" << probe->lastShields << "; health=" << probe->lastHealth << std::endl;
        }
    };

    test.run();
}

TEST(MovingShot, CorsairVsScourge)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new McRaveModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Corsair, BWAPI::TilePosition(78, 20)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Scourge, BWAPI::TilePosition(72, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Scourge, BWAPI::TilePosition(72, 20)),
    };

    test.run();
}

TEST(MovingShot, CorsairVsMutas)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new McRaveModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Corsair, BWAPI::TilePosition(73, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Corsair, BWAPI::TilePosition(73, 20)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::TilePosition(72, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::TilePosition(72, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::TilePosition(72, 20)),
    };

    test.run();
}
