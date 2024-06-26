#include <fap.h>

#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "General.h"

namespace
{
    std::vector<unsigned char> collisionPlayer1;
    std::vector<unsigned char> collisionPlayer2;

    int id = 1;

    auto inline makeUnit(BWAPI::UnitType type, BWAPI::Position pos)
    {
        BWAPI::UnitType weaponType;
        switch (type)
        {
            case BWAPI::UnitTypes::Protoss_Carrier:
                weaponType = BWAPI::UnitTypes::Protoss_Interceptor;
                break;
            case BWAPI::UnitTypes::Terran_Bunker:
                weaponType = BWAPI::UnitTypes::Terran_Marine;
                break;
            case BWAPI::UnitTypes::Protoss_Reaver:
                weaponType = BWAPI::UnitTypes::Protoss_Scarab;
                break;
            default:
                weaponType = type;
                break;
        }

        // In open terrain, collision values scale based on the unit range
        // Rationale: melee units have a smaller area to maneuver in, so they interfere with each other more
        int collisionValue = type.isFlyer() ? 0 : std::max(0, 6 - weaponType.groundWeapon().maxRange() / 32);

        // In a choke, collision values depend on unit size
        // We allow no collision between full-size units and a stack of two smaller-size units
        int collisionValueChoke = type.isFlyer() ? 0 : 6;
        if (type.width() >= 32 || type.height() >= 32) collisionValue = 12;

        return FAP::makeUnit<>()
                .setUnitType(type)
                .setPosition(pos)
                .setTargetPosition(pos)
                .setHealth(type.maxHitPoints())
                .setShields(type.maxShields())
                .setFlying(type.isFlyer())

                .setSpeed(type.topSpeed())
                .setArmor(type.armor())
                .setGroundCooldown(weaponType.groundWeapon().damageCooldown()
                                   / std::max(weaponType.maxGroundHits() * weaponType.groundWeapon().damageFactor(), 1))
                .setGroundDamage(weaponType.groundWeapon().damageAmount())
                .setGroundMaxRange(weaponType.groundWeapon().maxRange())
                .setAirCooldown(weaponType.airWeapon().damageCooldown()
                                / std::max(weaponType.maxAirHits() * weaponType.airWeapon().damageFactor(), 1))
                .setAirDamage(weaponType.airWeapon().damageAmount())
                .setAirMaxRange(weaponType.airWeapon().maxRange())

                .setElevation(BWAPI::Broodwar->getGroundHeight(pos.x << 3, pos.y << 3))
                .setAttackerCount(type == BWAPI::UnitTypes::Terran_Bunker ? 4 : 8)
                .setAttackCooldownRemaining(0)

                .setSpeedUpgrade(false) // Squares the speed
                .setRangeUpgrade(false) // Squares the ranges
                .setShieldUpgrades(0)

                .setStimmed(false)
                .setUndetected(false)

                .setID(id++)
                .setTarget(0)

                .setCollisionValues(collisionValue, collisionValueChoke)

                .setData({});
    }

    int score(std::vector<FAP::FAPUnit<>> *units)
    {
        int result = 0;
        for (auto &unit : *units)
        {
            result += CombatSim::unitValue(unit);
        }

        return result;
    }

    void initializeBridgeSimUnits(FAP::FastAPproximation<> &sim)
    {
        sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(20, 57))));
        sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(21, 57))));
        sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(22, 57))));
        sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(20, 58))));
        sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(21, 58))));
        sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(22, 58))));

        sim.addPlayer2(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(10, 65))));
        sim.addPlayer2(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(11, 66))));
        sim.addPlayer2(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(12, 66))));
        sim.addPlayer2(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(10, 66))));
        sim.addPlayer2(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(11, 67))));
    }

    template<bool choke = false>
    void initializeRampSimUnits(FAP::FastAPproximation<> &sim)
    {
        sim.addPlayer1<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(7, 30))));
        sim.addPlayer1<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(8, 30))));
        sim.addPlayer1<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(9, 30))));
        sim.addPlayer1<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(7, 31))));
        sim.addPlayer1<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(8, 31))));
        sim.addPlayer1<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(9, 31))));

        sim.addPlayer2<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(15, 24))));
        sim.addPlayer2<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(15, 23))));
        sim.addPlayer2<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(16, 25))));
        sim.addPlayer2<choke>(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(17, 25))));
    }

    Choke *chokeNear(BWAPI::TilePosition tile)
    {
        for (auto &choke : Map::allChokes())
        {
            if (tile.getApproxDistance(BWAPI::TilePosition(choke->center)) < 5) return choke;
        }

        return nullptr;
    }

    /*
    void toCsv(const std::string &csv, FAP::FastAPproximation<> &sim, int frame, Choke *choke)
    {
        auto outputUnit = [&](const FAP::FAPUnit<> &unit)
        {
            auto pos = BWAPI::Position(unit.x, unit.y);
            auto wp = BWAPI::WalkPosition(pos);

            Log::Csv(csv) << frame
                          << unit.player
                          << unit.id
                          << unit.x
                          << unit.y
                          << wp.x
                          << wp.y
                          << choke->tileSide[unit.cell]
                          << pos.getApproxDistance(choke->end1Center)
                          << pos.getApproxDistance(choke->end2Center)
                          << unit.target
                          << (unit.health + unit.shields)
                          << unit.attackCooldownRemaining;
        };

        for (auto &unit : *(sim.getState().first))
        {
            outputUnit(unit);
        }
        for (auto &unit : *(sim.getState().second))
        {
            outputUnit(unit);
        }
    }
     */
}

TEST(CombatSim, Choke)
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
    test.map = Maps::GetOne("Mancha");
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        Log::initialize();
        CherryVis::initialize();
        Map::initialize();
        CombatSim::initialize();
        Log::SetDebug(true);

        collisionPlayer1.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);
        collisionPlayer2.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);

        int diffNormal;
        {
            std::fill(collisionPlayer1.begin(), collisionPlayer1.end(), 0);
            std::fill(collisionPlayer2.begin(), collisionPlayer2.end(), 0);
            FAP::FastAPproximation sim(collisionPlayer1, collisionPlayer2);
            initializeBridgeSimUnits(sim);

            int initialMine = score(sim.getState().first);
            int initialEnemy = score(sim.getState().second);

            auto start = std::chrono::high_resolution_clock::now();
            sim.simulate(144);
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();

            int finalMine = score(sim.getState().first);
            int finalEnemy = score(sim.getState().second);

            diffNormal = (initialEnemy - finalEnemy) - (initialMine - finalMine);

            std::cout << "Scores for normal: " << diffNormal << " ("
                      << initialMine << "-" << initialEnemy << " : "
                      << finalMine << "-" << finalEnemy << ")"
                      << " - took " << us << "us" << std::endl;
        }

        int diffChoke;
        {
            auto choke = chokeNear(BWAPI::TilePosition(15, 62));
            if (!choke->isNarrowChoke)
            {
                std::cerr << "Error: found non-narrow choke!" << std::endl;
                return;
            }

            std::fill(collisionPlayer1.begin(), collisionPlayer1.end(), 0);
            std::fill(collisionPlayer2.begin(), collisionPlayer2.end(), 0);
            FAP::FastAPproximation sim(collisionPlayer1, collisionPlayer2);
            sim.setChokeGeometry(choke->tileSide, choke->end1Center, choke->end2Center, choke->end1Exit, choke->end2Exit);
            initializeBridgeSimUnits(sim);

            int initialMine = score(sim.getState().first);
            int initialEnemy = score(sim.getState().second);

            auto start = std::chrono::high_resolution_clock::now();
            sim.simulate<false, true>(144);
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();

            /*
            Log::Csv("attackChoke") << "frame,player,id,x,y,wp_x,wp_y,side,end1dist,end2dist,tgt,hp,cdwn";
            for (int i = 1; i <= 144; i++)
            {
                sim.simulate<false, true>(1);
                toCsv("attackChoke", sim, i, choke);
            }
            */

            int finalMine = score(sim.getState().first);
            int finalEnemy = score(sim.getState().second);

            diffChoke = (initialEnemy - finalEnemy) - (initialMine - finalMine);

            std::cout << "Scores for choke: " << diffChoke << " ("
                      << initialMine << "-" << initialEnemy << " : "
                      << finalMine << "-" << finalEnemy << ")"
                      << " - took " << us << "us" << std::endl;
        }

        EXPECT_LT(diffChoke, 0);
    };

    test.run();
}

TEST(CombatSim, Ramp)
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
    test.map = Maps::GetOne("Mancha");
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        Log::initialize();
        CherryVis::initialize();
        Map::initialize();
        CombatSim::initialize();
        Log::SetDebug(true);

        collisionPlayer1.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);
        collisionPlayer2.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);

        int diffNormal;
        {
            std::fill(collisionPlayer1.begin(), collisionPlayer1.end(), 0);
            std::fill(collisionPlayer2.begin(), collisionPlayer2.end(), 0);
            FAP::FastAPproximation sim(collisionPlayer1, collisionPlayer2);
            initializeRampSimUnits(sim);

            int initialMine = score(sim.getState().first);
            int initialEnemy = score(sim.getState().second);

            auto start = std::chrono::high_resolution_clock::now();
            sim.simulate(144);
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();

            int finalMine = score(sim.getState().first);
            int finalEnemy = score(sim.getState().second);

            diffNormal = (initialEnemy - finalEnemy) - (initialMine - finalMine);

            std::cout << "Scores for normal: " << diffNormal << " ("
                      << initialMine << "-" << initialEnemy << " : "
                      << finalMine << "-" << finalEnemy << ")"
                      << " - took " << us << "us" << std::endl;
        }

        int diffChoke;
        {
            auto choke = chokeNear(BWAPI::TilePosition(12, 27));
            if (!choke->isNarrowChoke)
            {
                std::cerr << "Error: found non-narrow choke!" << std::endl;
                return;
            }

            std::fill(collisionPlayer1.begin(), collisionPlayer1.end(), 0);
            std::fill(collisionPlayer2.begin(), collisionPlayer2.end(), 0);
            FAP::FastAPproximation sim(collisionPlayer1, collisionPlayer2);
            sim.setChokeGeometry(choke->tileSide, choke->end1Center, choke->end2Center, choke->end1Exit, choke->end2Exit);
            initializeRampSimUnits<true>(sim);

            int initialMine = score(sim.getState().first);
            int initialEnemy = score(sim.getState().second);

            auto start = std::chrono::high_resolution_clock::now();
            sim.simulate<false, true>(144);
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();

            /*
            Log::Csv("attackChoke") << "frame,player,id,x,y,wp_x,wp_y,side,end1dist,end2dist,tgt,hp,cdwn";
            for (int i = 1; i <= 144; i++)
            {
                sim.simulate<false, true>(1);
                toCsv("attackChoke", sim, i, choke);
            }
            */

            int finalMine = score(sim.getState().first);
            int finalEnemy = score(sim.getState().second);

            diffChoke = (initialEnemy - finalEnemy) - (initialMine - finalMine);

            std::cout << "Scores for choke: " << diffChoke << " ("
                      << initialMine << "-" << initialEnemy << " : "
                      << finalMine << "-" << finalEnemy << ")"
                      << " - took " << us << "us" << std::endl;
        }

        EXPECT_LT(diffChoke, 0);
    };

    test.run();
}

TEST(CombatSim, TankMinRange)
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
    test.map = Maps::GetOne("Mancha");
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        Log::initialize();
        CherryVis::initialize();
        Map::initialize();
        CombatSim::initialize();
        Log::SetDebug(true);

        collisionPlayer1.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);
        collisionPlayer2.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);

        {
            std::fill(collisionPlayer1.begin(), collisionPlayer1.end(), 0);
            std::fill(collisionPlayer2.begin(), collisionPlayer2.end(), 0);
            FAP::FastAPproximation sim(collisionPlayer1, collisionPlayer2);

            sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(7, 30))));
            sim.addPlayer2(makeUnit(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, BWAPI::Position(BWAPI::TilePosition(7, 31))));

            int initialMine = score(sim.getState().first);
            int initialEnemy = score(sim.getState().second);

            sim.simulate(144);

            int finalMine = score(sim.getState().first);
            int finalEnemy = score(sim.getState().second);

            int diff = (initialEnemy - finalEnemy) - (initialMine - finalMine);

            std::cout << "Scores for sieged tank starting in min range: " << diff << " ("
                      << initialMine << "-" << initialEnemy << " : "
                      << finalMine << "-" << finalEnemy << ")" << std::endl;

            // Expect no loss
            EXPECT_EQ(initialMine, finalMine);
        }

        {
            std::fill(collisionPlayer1.begin(), collisionPlayer1.end(), 0);
            std::fill(collisionPlayer2.begin(), collisionPlayer2.end(), 0);
            FAP::FastAPproximation sim(collisionPlayer1, collisionPlayer2);

            sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(7, 30))));
            sim.addPlayer2(makeUnit(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, BWAPI::Position(BWAPI::TilePosition(14, 35))));

            int initialMine = score(sim.getState().first);
            int initialEnemy = score(sim.getState().second);

            sim.simulate(144);

            int finalMine = score(sim.getState().first);
            int finalEnemy = score(sim.getState().second);

            int diff = (initialEnemy - finalEnemy) - (initialMine - finalMine);

            std::cout << "Scores for sieged tank starting outside min range: " << diff << " ("
                      << initialMine << "-" << initialEnemy << " : "
                      << finalMine << "-" << finalEnemy << ")" << std::endl;
            
            EXPECT_GT(finalMine, initialMine - 40);
        }
    };

    test.run();
}

TEST(CombatSim, TankBug)
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
    test.map = Maps::GetOne("Glaive");
    test.randomSeed = 40072;
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        Log::initialize();
        CherryVis::initialize();
        Map::initialize();
        CombatSim::initialize();
        Log::SetDebug(true);

        collisionPlayer1.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);
        collisionPlayer2.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 4, 0);

        {
            std::fill(collisionPlayer1.begin(), collisionPlayer1.end(), 0);
            std::fill(collisionPlayer2.begin(), collisionPlayer2.end(), 0);
            FAP::FastAPproximation sim(collisionPlayer1, collisionPlayer2);

            sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(917,3355)));
            sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(892,3338)));
            sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(869,3315)));
            sim.addPlayer1(makeUnit(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(828,3268)));
            sim.addPlayer2(makeUnit(BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode, BWAPI::Position(977,3377)));

            int initialMine = score(sim.getState().first);
            int initialEnemy = score(sim.getState().second);

            sim.simulate(144);

            int finalMine = score(sim.getState().first);
            int finalEnemy = score(sim.getState().second);

            int diff = (initialEnemy - finalEnemy) - (initialMine - finalMine);

            std::cout << "Scores for scenario: " << diff << " ("
                      << initialMine << "-" << initialEnemy << " : "
                      << finalMine << "-" << finalEnemy << ")" << std::endl;
        }
    };

    test.run();
}

