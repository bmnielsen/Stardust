#pragma once
#include "McRave.h"

// Information from: https://docs.google.com/document/d/1p7Rw4v56blhf5bzhSnFVfgrKviyrapDFHh9J4FNUXM0

namespace McRave::Events
{
    inline void onUnitDiscover(BWAPI::Unit unit)
    {
        BWEB::Map::onUnitDiscover(unit);
        Players::storeUnit(unit);
        Planning::onUnitDiscover(unit);

        if (unit->getType().isResourceDepot())
            Stations::storeStation(unit);
    }

    inline void onUnitCreate(BWAPI::Unit unit)
    {
        Players::storeUnit(unit);
        if (unit->getType().isResourceContainer())
            Resources::storeResource(unit);
        if (unit->getType().isResourceDepot())
            Stations::storeStation(unit);
    }

    inline void onUnitDestroy(BWAPI::Unit unit)
    {
        BWEB::Map::onUnitDestroy(unit);
        Players::removeUnit(unit);
        Planning::onUnitDestroy(unit);

        if (unit->getType().isResourceDepot())
            Stations::removeStation(unit);
        if (unit->getType().isResourceContainer() || unit->getType() == BWAPI::UnitTypes::Zerg_Drone)
            Resources::removeResource(unit);
    }

    inline void onUnitMorph(BWAPI::Unit unit)
    {
        BWEB::Map::onUnitMorph(unit);
        auto info = Units::getUnitInfo(unit);

        // Cleanup canceled morphed buildings
        if (info && info->getType().isBuilding() && unit->getType() == BWAPI::UnitTypes::Zerg_Drone) {
            BWEB::Map::removeUsed(info->getTilePosition(), info->getType().tileWidth(), info->getType().tileHeight());

            if (info->getType().isResourceDepot())
                Stations::removeStation(info->unit());
            if (info->getPlayer() == BWAPI::Broodwar->self())
                info->setRole(Role::Worker);
        }

        // Refinery that morphed as an enemy
        if (unit->getType().isResourceContainer())
            Resources::storeResource(unit);


        Players::morphUnit(unit);
    }

    inline void onUnitComplete(BWAPI::Unit unit)
    {
        if (unit->getPlayer() == BWAPI::Broodwar->self())
            Players::storeUnit(unit);
        if (unit->getType().isResourceDepot())
            Stations::storeStation(unit);
        if (unit->getType().isResourceContainer())
            Resources::storeResource(unit);
    }

    inline void onUnitRenegade(BWAPI::Unit unit)
    {
        // HACK: Changing players is kind of annoying, so we just remove and re-store
        if (!unit->getType().isRefinery()) {
            Players::removeUnit(unit);
            Players::storeUnit(unit);
        }
    }

    inline void onUnitLift(UnitInfo& unit)
    {
        BWEB::Map::removeUsed(unit.getLastTile(), unit.getType().tileWidth(), unit.getType().tileHeight());

        if (unit.getType().isResourceDepot())
            Stations::removeStation(unit.unit());
    }

    inline void onUnitLand(UnitInfo& unit)
    {
        BWEB::Map::addUsed(unit.getTilePosition(), unit.getType());

        if (unit.getType().isResourceDepot())
            Stations::storeStation(unit.unit());
    }

    inline void onUnitDisappear(UnitInfo& unit)
    {
        bool notVisibleFully = false;
        for (int x = unit.getTilePosition().x - 2; x <= unit.getTilePosition().x + 2; x++) {
            for (int y = unit.getTilePosition().y - 2; y <= unit.getTilePosition().y + 2; y++) {
                BWAPI::TilePosition t(x, y);
                if (t.isValid() && BWEB::Map::isWalkable(t, BWAPI::UnitTypes::Protoss_Dragoon) && BWAPI::Broodwar->getGroundHeight(t) == BWAPI::Broodwar->getGroundHeight(unit.getTilePosition()) && !BWAPI::Broodwar->isVisible(t)) {
                    notVisibleFully = true;
                    break;
                }
            }
        }

        if (!notVisibleFully) {
            unit.movedFlag = true;
            auto closestEnemy = Util::getClosestUnit(unit.getPosition(), PlayerState::Enemy, [&](auto &u) {
                return *u != unit && !u->getType().isWorker() && !u->getType().isBuilding() && !u->isFlying() && !BWAPI::Broodwar->isVisible(u->getTilePosition());
            });
            closestEnemy ? unit.setAssumedLocation(closestEnemy->getPosition(), closestEnemy->getWalkPosition(), closestEnemy->getTilePosition()) :
                           unit.setAssumedLocation(BWAPI::Positions::Invalid, BWAPI::WalkPositions::Invalid, BWAPI::TilePositions::Invalid);
        }
    }

    /// https://github.com/bwapi/bwapi/issues/853
    inline void onUnitCancelBecauseBWAPISucks(UnitInfo& unit)
    {
        // Cleanup canceled morphed buildings
        BWEB::Map::removeUsed(unit.getTilePosition(), unit.getType().tileWidth(), unit.getType().tileHeight());

        if (unit.getType().isResourceDepot())
            Stations::removeStation(unit.unit());
        if (unit.getPlayer() == BWAPI::Broodwar->self())
            unit.setRole(Role::Worker);
    }
}