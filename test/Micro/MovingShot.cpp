#include "BWTest.h"

#include "DoNothingModule.h"

#include "Units.h"
#include "Workers.h"
#include "MyWorker.h"

TEST(MovingShot, Probe)
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
    Unit scv = nullptr;

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

        if (!scv)
        {
            for (auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Terran_SCV))
            {
                scv = unit;
            }
            if (!scv) return;
        }
        if (!scv->exists())
        {
            CherryVis::log(scv->id) << "DIED";
            BWAPI::Broodwar->leaveGame();
            return;
        }

//        auto speed = [](BWAPI::Unit unit)
//        {
//            return sqrt(unit->getVelocityX() * unit->getVelocityX() + unit->getVelocityY() * unit->getVelocityY());
//        };
//
//        CherryVis::log(probe->id)
//            << "dist=" << probe->getDistance(scv)
//            << "; cooldown=" << (probe->cooldownUntil - currentFrame + 1)
//            << "; spd=" << speed(probe->bwapiUnit)
//            << "; spd%=" << (speed(probe->bwapiUnit) * 100.0 / probe->type.topSpeed());
//
//        CherryVis::log(scv->id)
//            << "dist=" << scv->getDistance(scv)
//            << "; cooldown=" << (scv->cooldownUntil - currentFrame + 1)
//            << "; spd=" << speed(scv->bwapiUnit)
//            << "; spd%=" << (speed(scv->bwapiUnit) * 100.0 / scv->type.topSpeed());

        probe->attackUnit(scv);
    };

    test.onFrameOpponent = []()
    {
        BWAPI::Unit scv = nullptr;
        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Terran_SCV) continue;
            if (unit->getDistance(BWAPI::Position(BWAPI::TilePosition(69, 20))) > 1000) continue;
            scv = unit;
            break;
        }
        if (!scv) return;

//        scv->stop();
//        return;

        if (scv->getLastCommand().getType() == BWAPI::UnitCommandTypes::Attack_Unit) return;

        for (auto &unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe)
            {
                scv->attack(unit);
                return;
            }
        }
    };

    test.onEndMine = [&](bool won)
    {
        if (scv)
        {
            EXPECT_FALSE(scv->exists()) << "Failed to kill SCV";
        }

        if (probe)
        {
            std::cout << "Final probe stats: shield=" << probe->lastShields << "; health=" << probe->lastHealth << std::endl;
        }
    };

    test.run();
}
