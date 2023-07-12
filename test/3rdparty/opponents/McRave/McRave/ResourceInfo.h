#pragma once
#include <BWAPI.h>
#include "BWEB.h"

namespace McRave
{
    class ResourceInfo : public std::enable_shared_from_this<ResourceInfo>
    {
    private:
        int remainingResources, initialResources, workerCap;
        bool threatened, pocket;
        BWAPI::Unit thisUnit;
        BWAPI::UnitType type;
        BWAPI::Position position;
        BWAPI::TilePosition tilePosition;
        BWEB::Station * station;
        ResourceState rState;
        std::map<std::weak_ptr<UnitInfo>, int> framesPerTrips;
        std::vector<std::weak_ptr<UnitInfo>> targetedBy;
        std::set<BWAPI::Position> returnOrderPositions;
        std::set<BWAPI::Position> gatherOrderPositions;
    public:
        ResourceInfo(BWAPI::Unit newResource) {
            thisUnit = newResource;

            station = nullptr;
            remainingResources = 0;
            initialResources = newResource->getInitialResources();
            rState = ResourceState::None;
            type = newResource->getType();
            position = newResource->getPosition();
            tilePosition = newResource->getTilePosition();
        }

        void updateThreatened();
        void updateWorkerCap();
        void updatePocketStatus();
        void updateResource();
        void addTargetedBy(std::weak_ptr<UnitInfo>);
        void removeTargetedBy(std::weak_ptr<UnitInfo>);

        // 
        bool isThreatened() { return threatened; }
        bool isBoulder() { return type.isMineralField() && initialResources <= 50; }
        bool isPocket() { return pocket; }
        bool hasStation() { return station != nullptr; }
        BWEB::Station * getStation() { return station; }
        void setStation(BWEB::Station* newStation) { station = newStation; }
        int getGathererCount() { return int(targetedBy.size()); }
        int getRemainingResources() { return remainingResources; }
        int getWorkerCap() { return workerCap; }
        ResourceState getResourceState() { return rState; }
        BWAPI::Unit unit() { return thisUnit; }
        BWAPI::UnitType getType() { return type; }
        BWAPI::Position getPosition() { return position; }
        BWAPI::TilePosition getTilePosition() { return tilePosition; }
        std::vector<std::weak_ptr<UnitInfo>>& targetedByWhat() { return targetedBy; }
        std::set<BWAPI::Position>& getReturnOrderPositions() { return returnOrderPositions; }
        std::set<BWAPI::Position>& getGatherOrderPositions() { return gatherOrderPositions; }

        // 
        void setRemainingResources(int newInt) { remainingResources = newInt; }
        void setResourceState(ResourceState newState) { rState = newState; }
        void setUnit(BWAPI::Unit newUnit) { thisUnit = newUnit; }
        void setType(BWAPI::UnitType newType) { type = newType; }
        void setPosition(BWAPI::Position newPosition) { position = newPosition; }
        void setTilePosition(BWAPI::TilePosition newTilePosition) { tilePosition = newTilePosition; }

        // 
        bool operator== (ResourceInfo& p) {
            return thisUnit == p.unit();
        }

        bool operator< (ResourceInfo& p) {
            return thisUnit < p.unit();
        }
    };
}
