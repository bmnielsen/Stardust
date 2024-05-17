#include "BWTest.h"

#include "StardustAIModule.h"
#include "Map.h"
#include "Strategist.h"
#include "DoNothingModule.h"
#include "TestAttackBasePlay.h"
#include "UpgradeStrategyEngine.h"
#include "DoNothingStrategyEngine.h"

TEST(AoEAvoidance, Storm)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new StardustAIModule();
        CherryVis::disable();
        return module;
    };
    test.myModule = []()
    {
        auto module = new StardustAIModule();
        module->frameSkip = 5000;
        return module;
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 7000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 19)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 21)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 22)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(88, 23)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(89, 19)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 117)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(111, 117)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Templar_Archives, BWAPI::TilePosition(111, 120)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 116)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 117)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 118)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 119)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 120)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(121, 116)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(121, 117)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(121, 118)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(121, 119)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(121, 120)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_High_Templar, BWAPI::TilePosition(105, 97)),
    };

    Base *baseToAttack = nullptr;

    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 117)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.onStartOpponent = []()
    {
        Strategist::setStrategyEngine(std::make_unique<UpgradeStrategyEngine>(BWAPI::TechTypes::Psionic_Storm));
        Strategist::setOpening({});
    };

    test.onFrameOpponent = []()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_High_Templar) continue;

            for (auto target : BWAPI::Broodwar->enemy()->getUnits())
            {
                if (target->getType() != BWAPI::UnitTypes::Protoss_Dragoon) continue;

                int dist = unit->getDistance(target);
                if (dist <= BWAPI::WeaponTypes::Psionic_Storm.maxRange())
                {
                    unit->useTech(BWAPI::TechTypes::Psionic_Storm, target);
                }
            }
        }
    };

    test.onEndMine = [](bool)
    {

    };

    test.run();
}

TEST(AoEAvoidance, Scarabs)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new DoNothingModule();
        return module;
    };
    test.myModule = []()
    {
        auto module = new StardustAIModule();
        module->frameSkip = 1000;
        return module;
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 4000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(44, 90)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(45, 90)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(46, 90)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(44, 91)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(45, 91)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(46, 91)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(33, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(35, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(37, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Reaver, BWAPI::TilePosition(35, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Reaver, BWAPI::TilePosition(36, 112)),
    };

    Base *baseToAttack = nullptr;

    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 116)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    test.onStartOpponent = []()
    {
        // Order workers to mine
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (!unit->getType().isWorker()) continue;

            auto mineral = BWAPI::Broodwar->getClosestUnit(unit->getPosition(), BWAPI::Filter::IsMineralField);
            if (mineral) unit->gather(mineral);
        }
    };

    test.onFrameOpponent = []()
    {
        // Ensure dragoons hold position and reavers build scarabs
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
            {
                if (unit->getOrder() != BWAPI::Orders::HoldPosition)
                {
                    unit->holdPosition();
                }
            }

            if (unit->getType() == BWAPI::UnitTypes::Protoss_Reaver)
            {
                unit->train(BWAPI::UnitTypes::Protoss_Scarab);
            }
        }
    };

    test.onEndMine = [](bool)
    {

    };

    test.run();
}

TEST(AoEAvoidance, Spines)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new DoNothingModule();
        return module;
    };
    test.myModule = []()
    {
        auto module = new StardustAIModule();
        module->frameSkip = 200;
        return module;
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 2000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(44, 90)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(45, 90)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(46, 90)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(44, 91)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::TilePosition(33, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::TilePosition(35, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::TilePosition(37, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Lurker, BWAPI::TilePosition(35, 111)),
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom-right base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 116)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    test.onFrameOpponent = []()
    {
        // Ensure hydralisks hold position and lurkers burrow
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Hydralisk)
            {
                if (unit->getOrder() != BWAPI::Orders::HoldPosition)
                {
                    unit->holdPosition();
                }
            }

            if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
            {
                if (!unit->isBurrowed())
                {
                    unit->burrow();
                }
            }
        }
    };

    test.onEndMine = [](bool)
    {

    };

    test.run();
}
