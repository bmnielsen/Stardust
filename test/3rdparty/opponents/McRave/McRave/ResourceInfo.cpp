#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;

namespace McRave {

    void ResourceInfo::updatePocketStatus()
    {
        if (!getType().isMineralField())
            return;

        // Determine if this resource is a pocket resource, useful to deter choosing a building worker (they get stuck)
        vector<TilePosition> directions ={ {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 1}, {2, -1}, {2, 0}, {2, 1} };
        vector<TilePosition> pocketTiles;
        for (auto &dir : directions) {
            auto tile = tilePosition + dir;
            auto center = Position(tile) + Position(16, 16);
            if (BWEB::Map::isUsed(tile) == UnitTypes::None) {
                if (center.getDistance(getStation()->getBase()->Center()) < position.getDistance(getStation()->getBase()->Center()) - 16.0)
                    pocketTiles.push_back(tile);                
            }
        }
        pocket = int(pocketTiles.size()) <= 1;
    }

    void ResourceInfo::updateThreatened()
    {
        // Determine if this resource is threatened based on a substantial threat nearby
        const auto time = (Spy::enemyRush() || Broodwar->self()->getRace() == Races::Zerg) ? Time(3, 00) : Time(4, 00);
        threatened = Units::getImmThreat() > 0.0 && Util::getTime() > time && Grids::getEGroundThreat(position) > min(6.0f, 0.10f * float(Util::getTime().minutes));

        // Determine if this resource is threatened based on an assigned worker being attacked
        for (auto &w : targetedBy) {
            if (auto worker = w.lock()) {
                if (!worker->hasTarget())
                    continue;
                auto workerTarget = worker->getTarget().lock();

                if (worker->isWithinGatherRange() && !worker->isBurrowed() && !worker->getUnitsTargetingThis().empty() && workerTarget->isThreatening() && !workerTarget->getType().isWorker()) {
                    for (auto &e : worker->getUnitsTargetingThis()) {
                        if (auto enemy = e.lock()) {
                            if (enemy->isWithinRange(*worker)) {
                                threatened = true;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    void ResourceInfo::updateWorkerCap()
    {
        // Calculate the worker cap for the resource
        workerCap = 2 + !type.isMineralField();
        for (auto &t : targetedBy) {
            if (t.expired())
                continue;

            if (auto targeter = t.lock()) {
                if (targeter->unit()->isCarryingGas() || targeter->unit()->isCarryingMinerals())
                    framesPerTrips[t]++;
                else if (Util::boxDistance(type, position, targeter->getType(), targeter->getPosition()) < 16.0)
                    framesPerTrips[t]=0;

                if (!type.isMineralField() && framesPerTrips[t] > 52)
                    workerCap = 4;
                if (targeter->getBuildPosition().isValid())
                    workerCap++;
            }
        }
    }

    void ResourceInfo::updateResource()
    {
        type                = thisUnit->getType();
        remainingResources  = thisUnit->getResources();
        position            = thisUnit->getPosition();
        tilePosition        = thisUnit->getTilePosition();

        updatePocketStatus();
        updateThreatened();
        updateWorkerCap();
    }

    void ResourceInfo::addTargetedBy(std::weak_ptr<UnitInfo> unit) {
        targetedBy.push_back(unit);
        framesPerTrips.emplace(unit, 0);
    }

    void ResourceInfo::removeTargetedBy(std::weak_ptr<UnitInfo> unit) {
        for (auto itr = targetedBy.begin(); itr != targetedBy.end(); itr++) {
            if (*itr == unit) {
                targetedBy.erase(itr);
                break;
            }
        }
        for (auto itr = framesPerTrips.begin(); itr != framesPerTrips.end(); itr++) {
            if (itr->first == unit) {
                framesPerTrips.erase(itr);
                break;
            }
        }
    }
}