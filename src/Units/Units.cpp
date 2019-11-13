#include <CQL.h>

#include "Units.h"
#include "Geo.h"
#include "Players.h"
#include "MyDragoon.h"
#include "MyWorker.h"

namespace
{
    auto &bwemMap = BWEM::Map::Instance();
}

namespace Units
{
#ifndef _DEBUG
    namespace
    {
#endif
        CQL::Table<Unit> units;

        // There is a VS bug that prevents CQL from working currently
        // So until it works, we'll use a map to access everything
        std::map<BWAPI::Unit, std::shared_ptr<Unit>> bwapiUnitToUnit;
        std::map<BWAPI::Player, std::set<std::shared_ptr<Unit>>> playerToUnits;

        std::map<BWAPI::Unit, std::unique_ptr<MyUnit>> bwapiUnitToMyUnit;

        std::map<BWAPI::UnitType, std::set<BWAPI::Unit>> myCompletedUnitsByType;
        std::map<BWAPI::UnitType, std::set<BWAPI::Unit>> myIncompleteUnitsByType;
#ifndef _DEBUG
    }
#endif

    void update()
    {
        // Update all units
        for (auto player : BWAPI::Broodwar->getPlayers())
        {
            // Ignore neutral units
            if (player->isNeutral()) continue;

            // First handle units that are gone from the last recorded position
            // We don't need to do this for allied players, as we can see their units
            if (!player->isAlly(BWAPI::Broodwar->self()))
            {
                //units.range<1>(player->getID(), player->getID()) >>= [](Unit const &unit) {
                //    unit.updateLastPositionValidity();
                //};

                for (const auto &unit : getForPlayer(player))
                {
                    unit->updateLastPositionValidity();
                }
            }

            // Now update all the units we can see
            for (auto unit : player->getUnits())
            {
                // Update our own units
                if (player == BWAPI::Broodwar->self())
                {
                    getMine(unit).update();

                    if (unit->isCompleted())
                    {
                        myCompletedUnitsByType[unit->getType()].insert(unit);
                        myIncompleteUnitsByType[unit->getType()].erase(unit);
                    }
                    else
                        myIncompleteUnitsByType[unit->getType()].insert(unit);
                }

                //auto entry = units.lookup<0>(unit);
                //if (entry)
                //{
                //    entry->update(unit);

                //    int tilePositionX = unit->getPosition().x << 3;
                //    if (tilePositionX != entry->tilePositionX) units.update<2>(entry, tilePositionX);

                //    int tilePositionY = unit->getPosition().y << 3;
                //    if (tilePositionY != entry->tilePositionY) units.update<3>(entry, tilePositionY);
                //}
                //else
                //{
                //    *units.emplace(unit);
                //}

                auto &entry = get(unit);
                entry.update(unit);
                entry.tilePositionX = unit->getPosition().x >> 5U;
                entry.tilePositionY = unit->getPosition().y >> 5U;
                if (entry.player != player)
                {
                    auto ptr = bwapiUnitToUnit[unit];
                    playerToUnits[entry.player].erase(bwapiUnitToUnit[unit]);
                    playerToUnits[player].insert(ptr);
                    entry.player = player;
                }
            }
        }

        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            bool output = false;
#if DEBUG_PROBE_STATUS
            output = output || unit->getType() == BWAPI::UnitTypes::Protoss_Probe;
#endif
#if DEBUG_ZEALOT_STATUS
            output = output || unit->getType() == BWAPI::UnitTypes::Protoss_Zealot;
#endif
#if DEBUG_DRAGOON_STATUS
            output = output || unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon;
#endif
#if DEBUG_SHUTTLE_STATUS
            output = output || unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle;
#endif

            if (!output) continue;

            auto &myUnit = getMine(unit);
            std::ostringstream debug;

            // First line is command
            debug << "cmd=" << unit->getLastCommand().getType() << ";f=" << (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame());
            if (unit->getLastCommand().getTarget())
            {
                debug << ";tgt=" << unit->getLastCommand().getTarget()->getType()
                      << "#" << unit->getLastCommand().getTarget()->getID()
                      << "@" << BWAPI::WalkPosition(unit->getLastCommand().getTarget()->getPosition())
                      << ";d=" << unit->getLastCommand().getTarget()->getDistance(unit);
            }
            else if (unit->getLastCommand().getTargetPosition())
            {
                debug << ";tgt=" << BWAPI::WalkPosition(unit->getLastCommand().getTargetPosition());
            }

            // Next line is order
            debug << "\nord=" << unit->getOrder() << ";t=" << unit->getOrderTimer();
            if (unit->getOrderTarget())
            {
                debug << ";tgt" << unit->getOrderTarget()->getType()
                      << "#" << unit->getOrderTarget()->getID()
                      << "@" << BWAPI::WalkPosition(unit->getOrderTarget()->getPosition())
                      << ";d=" << unit->getOrderTarget()->getDistance(unit);
            }
            else if (unit->getOrderTargetPosition())
            {
                debug << ";tgt=" << BWAPI::WalkPosition(unit->getOrderTargetPosition());
            }

            // Last line is movement data and other unit-specific stuff
            debug << "\n";
            if (unit->getType().topSpeed() > 0.001)
            {
                auto speed = sqrt(unit->getVelocityX() * unit->getVelocityX() + unit->getVelocityY() * unit->getVelocityY());
                debug << "spd=" << ((int) (100.0 * speed / unit->getType().topSpeed()));
            }

            debug << ";mvng=" << unit->isMoving() << ";stck=" << myUnit.isStuck() << ";rdy=" << myUnit.isReady();

            if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
            {
                auto &myDragoon = dynamic_cast<MyDragoon &>(myUnit);
                debug << ";atckf=" << (BWAPI::Broodwar->getFrameCount() - myDragoon.getLastAttackStartedAt());
            }

            CherryVis::log(unit) << debug.str();
        }
/*
        return;

        std::ostringstream debug;
        bool anyDebugUnits = false;

        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType().isWorker() && true)
            {
                anyDebugUnits = true;

                debug << "\n" << unit->getType() << " " << unit->getID() << " @ " << unit->getPosition() << "^"
                      << BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(unit->getPosition()));
                debug << ",speed=" << (unit->getVelocityX() * unit->getVelocityX() + unit->getVelocityY() * unit->getVelocityY()) << ": ";

                debug << "command: " << unit->getLastCommand().getType() << ",frame="
                      << (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame());
                if (unit->getLastCommand().getTarget())
                    debug << ",target=" << unit->getLastCommand().getTarget()->getType() << " " << unit->getLastCommand().getTarget()->getID()
                          << " @ " << unit->getLastCommand().getTarget()->getPosition() << ",dist="
                          << unit->getLastCommand().getTarget()->getDistance(unit) << ",sqdist=" << Geo::EdgeToEdgeSquaredDistance(unit->getType(),
                                                                                                                                   unit->getPosition(),
                                                                                                                                   unit->getLastCommand().getTarget()->getType(),
                                                                                                                                   unit->getLastCommand().getTarget()->getPosition());
                else if (unit->getLastCommand().getTargetPosition())
                    debug << ",targetpos " << unit->getLastCommand().getTargetPosition();

                debug << ". order: " << unit->getOrder() << ",timer=" << unit->getOrderTimer();
                if (unit->getOrderTarget())
                    debug << ",target=" << unit->getOrderTarget()->getType() << " " << unit->getOrderTarget()->getID() << " @ "
                          << unit->getOrderTarget()->getPosition() << ",dist=" << unit->getOrderTarget()->getDistance(unit) << ",sqdist="
                          << Geo::EdgeToEdgeSquaredDistance(unit->getType(),
                                                            unit->getPosition(),
                                                            unit->getOrderTarget()->getType(),
                                                            unit->getOrderTarget()->getPosition());
                else if (unit->getOrderTargetPosition())
                    debug << ",targetpos " << unit->getOrderTargetPosition();

                debug << ". isMoving=" << unit->isMoving();
                if (unit->isCarryingMinerals())
                    debug << ",carrying minerals";
                if (unit->isCarryingGas())
                    debug << ",carrying gas";
            }

            if (unit->getType().isBuilding() && false)
            {
                anyDebugUnits = true;

                debug << "\n" << unit->getType() << " " << unit->getID() << " @ " << unit->getPosition() << "^"
                      << BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(unit->getPosition())) << ": ";

                debug << "completed=" << unit->isCompleted() << ",remainingBuildTime=" << unit->getRemainingBuildTime();
            }

            if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon && false)
            {
                auto &myUnit = getMine(unit);
                auto &myDragoon = dynamic_cast<MyDragoon &>(myUnit);

                anyDebugUnits = true;

                debug << "\n" << unit->getType() << " " << unit->getID() << " @ " << unit->getPosition() << ": ";

                debug << "command: " << unit->getLastCommand().getType() << ",frame="
                      << (BWAPI::Broodwar->getFrameCount() - unit->getLastCommandFrame());
                if (unit->getLastCommand().getTarget())
                    debug << ",target=" << unit->getLastCommand().getTarget()->getType() << " " << unit->getLastCommand().getTarget()->getID()
                          << " @ " << unit->getLastCommand().getTarget()->getPosition() << ",dist="
                          << unit->getLastCommand().getTarget()->getDistance(unit);
                else if (unit->getLastCommand().getTargetPosition())
                    debug << ",targetpos " << unit->getLastCommand().getTargetPosition();

                debug << ". order: " << unit->getOrder();
                if (unit->getOrderTarget())
                    debug << ",target=" << unit->getOrderTarget()->getType() << " " << unit->getOrderTarget()->getID() << " @ "
                          << unit->getOrderTarget()->getPosition() << ",dist=" << unit->getOrderTarget()->getDistance(unit);
                else if (unit->getOrderTargetPosition())
                    debug << ",targetpos " << unit->getOrderTargetPosition();

                debug << ". isMoving=" << unit->isMoving() << ";isattackframe=" << unit->isAttackFrame() << ";isstartingattack="
                      << unit->isStartingAttack() << ";cooldown=" << unit->getGroundWeaponCooldown();

                if (myUnit.isStuck()) debug << ";is stuck";
                if (myDragoon.getLastAttackStartedAt() > 0)
                    debug << ";lastAttackStartedAt=" << (BWAPI::Broodwar->getFrameCount() - myDragoon.getLastAttackStartedAt());
                debug << ";isReady=" << myUnit.isReady();
            }
        }

        if (anyDebugUnits) Log::Debug() << debug.str();
        */
    }

    void issueOrders()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            getMine(unit).issueMoveOrders();
        }
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        auto it = bwapiUnitToUnit.find(unit);
        if (it != bwapiUnitToUnit.end())
        {
            if (it->second->lastPositionValid)
            {
                Players::grid(it->second->player).unitDestroyed(it->second->type, it->second->lastPosition, it->second->completed);
#if DEBUG_GRID_UPDATES
                CherryVis::log(unit) << "Grid::unitDestroyed " << it->second->lastPosition;
#endif
            }

            playerToUnits[unit->getPlayer()].erase(it->second);
            bwapiUnitToUnit.erase(it);
        }

        bwapiUnitToMyUnit.erase(unit);
        myCompletedUnitsByType[unit->getType()].erase(unit);
        myIncompleteUnitsByType[unit->getType()].erase(unit);
    }

    void onBulletCreate(BWAPI::Bullet bullet)
    {
        if (!bullet->getSource() || !bullet->getTarget()) return;

        // If this bullet is a ranged bullet that deals damage after a delay, track it on the unit it is moving towards
        if (bullet->getType() == BWAPI::BulletTypes::Gemini_Missiles ||         // Wraith
            bullet->getType() == BWAPI::BulletTypes::Fragmentation_Grenade ||   // Vulture
            bullet->getType() == BWAPI::BulletTypes::Longbolt_Missile ||        // Missile turret
            bullet->getType() == BWAPI::BulletTypes::ATS_ATA_Laser_Battery ||   // Battlecruiser
            bullet->getType() == BWAPI::BulletTypes::Burst_Lasers ||            // Wraith
            bullet->getType() == BWAPI::BulletTypes::Anti_Matter_Missile ||     // Scout
            bullet->getType() == BWAPI::BulletTypes::Pulse_Cannon ||            // Interceptor
            bullet->getType() == BWAPI::BulletTypes::Yamato_Gun ||              // Battlecruiser
            bullet->getType() == BWAPI::BulletTypes::Phase_Disruptor ||         // Dragoon
            bullet->getType() == BWAPI::BulletTypes::STA_STS_Cannon_Overlay ||  // Photon cannon?
            bullet->getType() == BWAPI::BulletTypes::Glave_Wurm ||              // Mutalisk
            bullet->getType() == BWAPI::BulletTypes::Seeker_Spores ||           // Spore colony
            bullet->getType() == BWAPI::BulletTypes::Halo_Rockets ||            // Valkyrie
            bullet->getType() == BWAPI::BulletTypes::Subterranean_Spines)       // Lurker
        {
            auto &target = get(bullet->getTarget());
            target.addUpcomingAttack(bullet->getSource(), bullet);
        }
    }

    MyUnit &getMine(BWAPI::Unit unit)
    {
        auto it = bwapiUnitToMyUnit.find(unit);
        if (it != bwapiUnitToMyUnit.end())
            return *it->second;

        if (unit->getType().isWorker())
        {
            return *bwapiUnitToMyUnit.emplace(unit, std::make_unique<MyWorker>(unit)).first->second;
        }

        if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
        {
            return *bwapiUnitToMyUnit.emplace(unit, std::make_unique<MyDragoon>(unit)).first->second;
        }

        return *bwapiUnitToMyUnit.emplace(unit, std::make_unique<MyUnit>(unit)).first->second;
    }

    Unit const &get(BWAPI::Unit unit)
    {
        //auto entry = units.lookup<0>(unit);
        //if (entry)
        //{
        //    return *entry;
        //}
        //return *units.emplace(unit);

        auto it = bwapiUnitToUnit.find(unit);
        if (it != bwapiUnitToUnit.end())
            return *it->second;

        auto newUnit = std::make_shared<Unit>(unit);
        bwapiUnitToUnit.emplace(unit, newUnit);
        playerToUnits[unit->getPlayer()].emplace(newUnit);

        if (unit->getPlayer() == BWAPI::Broodwar->enemy())
        {
            Log::Get() << "Enemy discovered: " << unit->getType() << " @ " << unit->getTilePosition();
        }

        return *newUnit;
    }

    std::set<std::shared_ptr<Unit>> &getForPlayer(BWAPI::Player player)
    {
        return playerToUnits[player];
    }

    void getInRadius(std::set<std::shared_ptr<Unit>> &units, BWAPI::Player player, BWAPI::Position position, int radius)
    {
        for (auto &unit : playerToUnits[player])
        {
            if (unit->lastPositionValid && unit->lastPosition.getApproxDistance(position) <= radius)
                units.insert(unit);
        }

        // For debugging
        size_t count = 0;
        for (auto unit : player->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon && unit->getPosition().getApproxDistance(position) <= radius)
                count++;
        }
        if (count > units.size())
        {
            for (auto unit : player->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon && unit->getPosition().getApproxDistance(position) <= radius)
                {
                    auto &unt = get(unit);
                    Log::Debug() << "Missed " << unit->getType() << " @ " << unit->getPosition() << ": lastPosition=" << unt.lastPosition
                                 << "; lastPositionValid=" << unt.lastPositionValid << "; dist=" << unt.lastPosition.getApproxDistance(position);
                }
            }
        }
    }

    void getInArea(std::set<std::shared_ptr<Unit>> &units,
                   BWAPI::Player player,
                   const BWEM::Area *area,
                   const std::function<bool(const std::shared_ptr<Unit> &)> &predicate)
    {
        for (auto &unit : playerToUnits[player])
        {
            if (predicate && !predicate(unit)) continue;
            if (unit->lastPositionValid && bwemMap.GetArea(BWAPI::WalkPosition(unit->lastPosition)) == area)
                units.insert(unit);
        }
    }

    int countAll(BWAPI::UnitType type)
    {
        return countCompleted(type) + countIncomplete(type);
    }

    int countCompleted(BWAPI::UnitType type)
    {
        return myCompletedUnitsByType[type].size();
    }

    int countIncomplete(BWAPI::UnitType type)
    {
        return myIncompleteUnitsByType[type].size();
    }

    std::map<BWAPI::UnitType, int> countIncompleteByType()
    {
        std::map<BWAPI::UnitType, int> result;
        for (auto &typeAndUnits : myIncompleteUnitsByType)
        {
            result[typeAndUnits.first] = typeAndUnits.second.size();
        }
        return result;
    }
}
