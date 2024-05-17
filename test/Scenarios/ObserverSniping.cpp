#include "BWTest.h"

#include "DoNothingModule.h"
#include "Geo.h"

namespace
{
    // Module that moves an observer across the map
    class MoveObserverModule : public DoNothingModule
    {
    private:
        void onFrame() override
        {
            BWAPI::Unit obs = nullptr;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
                {
                    obs = unit;
                }
            }
            if (!obs) return;

            for (auto bullet : BWAPI::Broodwar->getBullets())
            {
                if (bullet->exists() && bullet->getType() == BWAPI::BulletTypes::Psionic_Storm)
                {
                    auto vector = Geo::ScaleVector(obs->getPosition() - bullet->getPosition(), 500);
                    if (vector.isValid())
                    {
                        obs->move(obs->getPosition() + vector);
                        return;
                    }
                }
            }

            if (obs->getLastCommand().type != BWAPI::UnitCommandTypes::Move)
            {
                obs->move(BWAPI::Position(BWAPI::TilePosition(1,1)));
            }
        }
    };
}

TEST(ObserverSniping, Storm)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new MoveObserverModule();
    };
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 65145;
    test.frameLimit = 1500;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_High_Templar, BWAPI::Position(BWAPI::TilePosition(41, 31))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(BWAPI::TilePosition(69, 43))),
    };

    test.onStartMine = []()
    {
        BWAPI::Broodwar->self()->setResearched(BWAPI::TechTypes::Psionic_Storm, true);
    };

    test.onStartOpponent = []()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Boosters, 1);
    };

    test.onFrameMine = []()
    {
        BWAPI::Unit ht = nullptr;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar)
            {
                ht = unit;
            }
        }

        if (!ht) return;

        if (ht->getLastCommand().type == BWAPI::UnitCommandTypes::Use_Tech ||
            ht->getLastCommand().type == BWAPI::UnitCommandTypes::Use_Tech_Position ||
            ht->getLastCommand().type == BWAPI::UnitCommandTypes::Use_Tech_Unit)
        {
            return;
        }

        if (ht->getEnergy() < 75)
        {
            ht->setEnergy(200);
            return;
        }

        BWAPI::Unit obs = nullptr;
        for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
            {
                obs = unit;
            }
        }

        if (!obs || ht->getDistance(obs->getPosition()) > 8 * 32)
        {
            ht->stop();
            return;
        }

        int frames = 12;
        auto pos = obs->getPosition() + BWAPI::Position((int) (frames * obs->getVelocityX()), (int) (frames * obs->getVelocityY()));

        ht->useTech(BWAPI::TechTypes::Psionic_Storm, pos);
    };

    test.run();
}

TEST(ObserverSniping, CorsairAndObs)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new MoveObserverModule();
    };
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 65145;
    test.frameLimit = 2000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Corsair, BWAPI::Position(BWAPI::TilePosition(41, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(BWAPI::TilePosition(41, 31))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(BWAPI::TilePosition(69, 43))),
    };

    test.onStartMine = []()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Sensor_Array, 1);
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Boosters, 1);
    };

    test.onStartOpponent = []()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Boosters, 1);
    };

    test.onFrameMine = []()
    {
        BWAPI::Unit obs = nullptr;
        for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer && unit->isDetected())
            {
                obs = unit;
            }
        }

        BWAPI::Unit corsair = nullptr;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Corsair)
            {
                corsair = unit;
            }
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
            {
                if (obs)
                {
                    unit->move(obs->getPosition());
                }
                else
                {
                    unit->stop();
                }
            }
        }

        if (!corsair) return;
        if (!obs) return;

        corsair->attack(obs);
    };

    test.run();
}

TEST(ObserverSniping, ScoutAndObs)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new MoveObserverModule();
    };
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 65145;
    test.frameLimit = 2000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Scout, BWAPI::Position(BWAPI::TilePosition(41, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(BWAPI::TilePosition(41, 31))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(BWAPI::TilePosition(69, 43))),
    };

    test.onStartMine = []()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Sensor_Array, 1);
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Boosters, 1);
    };

    test.onStartOpponent = []()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Gravitic_Boosters, 1);
    };

    test.onFrameMine = []()
    {
        BWAPI::Unit obs = nullptr;
        for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer && unit->isDetected())
            {
                obs = unit;
            }
        }

        BWAPI::Unit scout = nullptr;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Scout)
            {
                scout = unit;
            }
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
            {
                if (obs)
                {
                    unit->move(obs->getPosition());
                }
                else
                {
                    unit->stop();
                }
            }
        }

        if (!scout) return;
        if (!obs) return;

        scout->attack(obs);
    };

    test.run();
}
