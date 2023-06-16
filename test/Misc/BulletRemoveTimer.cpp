#include "BWTest.h"

#include "DoNothingModule.h"

TEST(BulletRemoveTimer, GatherInformation)
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
    test.opponentRace = BWAPI::Races::Protoss;
    test.myRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 2000;
    test.expectWin = false;

    std::vector<std::tuple<BWAPI::UnitType, BWAPI::WeaponType, BWAPI::BulletType, int>> testCases = {
            {BWAPI::UnitTypes::Terran_Wraith,         BWAPI::UnitTypes::Terran_Wraith.groundWeapon(),      BWAPI::BulletTypes::Burst_Lasers,           -1},
            {BWAPI::UnitTypes::Terran_Wraith,         BWAPI::UnitTypes::Terran_Wraith.airWeapon(),         BWAPI::BulletTypes::Gemini_Missiles,        -1},
            {BWAPI::UnitTypes::Terran_Vulture,        BWAPI::UnitTypes::Terran_Vulture.groundWeapon(),     BWAPI::BulletTypes::Fragmentation_Grenade,  -1},
            {BWAPI::UnitTypes::Terran_Missile_Turret, BWAPI::UnitTypes::Terran_Missile_Turret.airWeapon(), BWAPI::BulletTypes::Longbolt_Missile,       -1},
            {BWAPI::UnitTypes::Terran_Goliath,        BWAPI::UnitTypes::Terran_Goliath.airWeapon(),        BWAPI::BulletTypes::Longbolt_Missile,       -1},
            {BWAPI::UnitTypes::Terran_Battlecruiser,  BWAPI::UnitTypes::Terran_Battlecruiser.airWeapon(),  BWAPI::BulletTypes::ATS_ATA_Laser_Battery,  -1},
            {BWAPI::UnitTypes::Protoss_Scout,         BWAPI::UnitTypes::Protoss_Scout.airWeapon(),         BWAPI::BulletTypes::Anti_Matter_Missile,    -1},
            {BWAPI::UnitTypes::Protoss_Dragoon,       BWAPI::UnitTypes::Protoss_Dragoon.airWeapon(),       BWAPI::BulletTypes::Phase_Disruptor,        -1},
            {BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon.airWeapon(), BWAPI::BulletTypes::STA_STS_Cannon_Overlay, -1},
            {BWAPI::UnitTypes::Zerg_Mutalisk,         BWAPI::UnitTypes::Zerg_Mutalisk.airWeapon(),         BWAPI::BulletTypes::Glave_Wurm,             -1},
            {BWAPI::UnitTypes::Zerg_Spore_Colony,     BWAPI::UnitTypes::Zerg_Spore_Colony.airWeapon(),     BWAPI::BulletTypes::Seeker_Spores,          -1},
            {BWAPI::UnitTypes::Terran_Valkyrie,       BWAPI::UnitTypes::Terran_Valkyrie.airWeapon(),       BWAPI::BulletTypes::Halo_Rockets,           -1},
            {BWAPI::UnitTypes::Zerg_Lurker,           BWAPI::WeaponTypes::Unknown,                         BWAPI::BulletTypes::Subterranean_Spines,    -1},
            {BWAPI::UnitTypes::Protoss_Carrier,       BWAPI::WeaponTypes::Unknown,                         BWAPI::BulletTypes::Pulse_Cannon,           -1},
            {BWAPI::UnitTypes::Terran_Battlecruiser,  BWAPI::WeaponTypes::Unknown,                         BWAPI::BulletTypes::Yamato_Gun,             -1},
    };

    const int STATE_INITIALIZE_1 = 0;
    const int STATE_INITIALIZE_2 = 10;
    const int STATE_START = 20;
    const int STATE_CREATEUNIT = 30;
    const int STATE_CREATEUNIT_CARRIER = 31;
    const int STATE_CREATEUNIT_BATTLECRUISER_YAMATO = 32;
    const int STATE_CREATEUNIT_LURKER = 33;
    const int STATE_ATTACK = 40;
    const int STATE_DESTROYUNIT = 60;

    int testCaseIdx = 0;
    int state = STATE_INITIALIZE_1;

    test.onFrameOpponent = [&]()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            unit->stop();
        }
    };

    test.onFrameMine = [&]()
    {
        if (testCaseIdx == testCases.size())
        {
            return;
        }

        auto &[unitType, weaponType, bulletType, removeTimer] = testCases[testCaseIdx];

        BWAPI::Unit groundTarget = nullptr;
        BWAPI::Unit airTarget = nullptr;
        for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
        {
            if (!unit->exists()) continue;
            if (unit->getType() == BWAPI::UnitTypes::Terran_SCV)
            {
                groundTarget = unit;
                groundTarget->setHitPoints(BWAPI::UnitTypes::Terran_SCV.maxHitPoints());
            }
            if (unit->getType() == BWAPI::UnitTypes::Terran_Dropship)
            {
                airTarget = unit;
                if (testCaseIdx < (testCases.size() - 1))
                {
                    airTarget->setHitPoints(BWAPI::UnitTypes::Terran_Dropship.maxHitPoints());
                }
            }
        }

        BWAPI::Unit unitUnderTest = nullptr;
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == unitType)
            {
                unitUnderTest = unit;
            }
        }

        int lastState = state;
        switch (state)
        {
            case STATE_INITIALIZE_1:
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->enemy(), BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(117, 17)));
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->enemy(), BWAPI::UnitTypes::Terran_Dropship, BWAPI::Position(BWAPI::TilePosition(117, 17)));
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(BWAPI::TilePosition(114, 17)));
                state = STATE_INITIALIZE_2;
                break;
            }
            case STATE_INITIALIZE_2:
            {
                if (groundTarget && airTarget)
                {
                    state = STATE_START;
                }
                break;
            }
            case STATE_START:
            {
                auto tile = BWAPI::TilePosition(119, 17);
                if (unitType == BWAPI::UnitTypes::Zerg_Spore_Colony)
                {
                    tile = BWAPI::TilePosition(117, 14);
                }
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), unitType, BWAPI::Position(tile));
                state = STATE_CREATEUNIT;
                break;
            }
            case STATE_CREATEUNIT:
            {
                if (unitUnderTest)
                {
                    // Special cases
                    if (weaponType == BWAPI::WeaponTypes::Unknown)
                    {
                        if (unitType == BWAPI::UnitTypes::Protoss_Carrier)
                        {
                            unitUnderTest->train(BWAPI::UnitTypes::Protoss_Interceptor);
                            state = STATE_CREATEUNIT_CARRIER;
                        }
                        else if (unitType == BWAPI::UnitTypes::Terran_Battlecruiser)
                        {
                            unitUnderTest->setEnergy(200);
                            unitUnderTest->getPlayer()->setResearched(BWAPI::TechTypes::Yamato_Gun, true);
                            state = STATE_CREATEUNIT_BATTLECRUISER_YAMATO;
                        }
                        else if (unitType == BWAPI::UnitTypes::Zerg_Lurker)
                        {
                            unitUnderTest->burrow();
                            state = STATE_CREATEUNIT_LURKER;
                        }
                        break;
                    }

                    if (weaponType.targetsAir())
                    {
                        unitUnderTest->attack(airTarget);
                    }
                    else
                    {
                        unitUnderTest->attack(groundTarget);
                    }
                    state = STATE_ATTACK;
                }
                break;
            }
            case STATE_CREATEUNIT_CARRIER:
            {
                if (unitUnderTest->getInterceptorCount() > 0)
                {
                    unitUnderTest->attack(airTarget);
                    state = STATE_ATTACK;
                }
                break;
            }
            case STATE_CREATEUNIT_BATTLECRUISER_YAMATO:
            {
                if (unitUnderTest->getPlayer()->hasResearched(BWAPI::TechTypes::Yamato_Gun) &&
                    unitUnderTest->getEnergy() >= 150)
                {
                    unitUnderTest->useTech(BWAPI::TechTypes::Yamato_Gun, airTarget);
                    state = STATE_ATTACK;
                }
                break;
            }
            case STATE_CREATEUNIT_LURKER:
            {
                if (unitUnderTest->isBurrowed())
                {
                    unitUnderTest->attack(groundTarget);
                    state = STATE_ATTACK;
                }
                break;
            }
            case STATE_ATTACK:
            {
                for (auto bullet : BWAPI::Broodwar->getBullets())
                {
                    if (bullet->getType() == bulletType)
                    {
                        removeTimer = bullet->getRemoveTimer();
                        BWAPI::Broodwar->killUnit(unitUnderTest);
                        state = STATE_DESTROYUNIT;
                    }
                }
                break;
            }
            case STATE_DESTROYUNIT:
            {
                if (!unitUnderTest)
                {
                    testCaseIdx++;
                    state = STATE_START;
                }
                break;
            }
        }

        if (state != lastState)
        {
            std::cout << "State transition from " << lastState << " to " << state << std::endl;
            std::cout << "Unit type: " << unitType << std::endl;
            std::cout << "Weapon type: " << weaponType << std::endl;
            std::cout << "Bullet type: " << bulletType << std::endl;
        }
    };

    test.onEndMine = [&](bool won)
    {
        for (auto &[unitType, weaponType, bulletType, removeTimer] : testCases)
        {
            std::cout << unitType << ", " << weaponType << ", " << bulletType << ": " << removeTimer << std::endl;
        }
    };

    test.run();
}
