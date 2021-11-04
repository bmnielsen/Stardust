#pragma once
#include <BWAPI.h>
#include "BWEB.h"

namespace McRave {

    class UnitInfo : public std::enable_shared_from_this<UnitInfo>
    {
    #pragma region UnitData
        double visibleGroundStrength = 0.0;
        double visibleAirStrength = 0.0;
        double maxGroundStrength = 0.0;
        double maxAirStrength = 0.0;
        double priority = 0.0;
        double percentHealth = 0.0;
        double percentShield = 0.0;
        double percentTotal = 0.0;
        double groundRange = 0.0;
        double groundReach = 0.0;
        double groundDamage = 0.0;
        double airRange = 0.0;
        double airReach = 0.0;
        double airDamage = 0.0;
        double speed = 0.0;
        double engageDist = 0.0;
        double simValue = 0.0;
        double engageRadius = 0.0;
        double retreatRadius = 0.0;
        double currentSpeed = 0.0;

        int lastAttackFrame = -999;
        int lastRepairFrame = -999;
        int lastVisibleFrame = -999;
        int lastTileMoveFrame = -999;
        int lastStuckFrame = 0;
        int lastThreateningFrame = 0;
        int lastStimFrame = 0;
        int threateningFrames = 0;
        int resourceHeldFrames = -999;
        int remainingTrainFrame = -999;
        int startedFrame = -999;
        int completeFrame = -999;
        int arriveFrame = -999;
        int shields = 0;
        int health = 0;
        int armor = 0;
        int shieldArmor = 0;
        int minStopFrame = 0;
        int energy = 0;
        int walkWidth = 0;
        int walkHeight = 0;

        bool proxy = false;
        bool completed = false;
        bool burrowed = false;
        bool flying = false;
        bool threatening = false;
        bool hidden = false;
        bool nearSplash = false;
        bool nearSuicide = false;
        bool markedForDeath = false;
        bool commander = false;
        bool targetedBySplash = false;
    #pragma endregion

    #pragma region Targets
        std::weak_ptr<UnitInfo> transport;
        std::weak_ptr<UnitInfo> target;
        std::weak_ptr<UnitInfo> simTarget;
        std::weak_ptr<UnitInfo> backupTarget;
        std::weak_ptr<ResourceInfo> resource;

        std::vector<std::weak_ptr<UnitInfo>> assignedCargo;
        std::vector<std::weak_ptr<UnitInfo>> targetedBy;
    #pragma endregion

    #pragma region States
        TransportState tState = TransportState::None;
        LocalState lState = LocalState::None;
        GlobalState gState = GlobalState::None;
        SimState sState = SimState::None;
        Role role = Role::None;
        GoalType gType = GoalType::None;
    #pragma endregion

    #pragma region BWAPIData
        BWAPI::Player player = nullptr;
        BWAPI::Unit bwUnit = nullptr;
        BWAPI::UnitType type = BWAPI::UnitTypes::None;
        BWAPI::UnitType buildingType = BWAPI::UnitTypes::None;

        BWAPI::Position formation = BWAPI::Positions::Invalid;
        BWAPI::Position position = BWAPI::Positions::Invalid;
        BWAPI::Position engagePosition = BWAPI::Positions::Invalid;
        BWAPI::Position destination = BWAPI::Positions::Invalid;
        BWAPI::Position lastPos = BWAPI::Positions::Invalid;
        BWAPI::Position goal = BWAPI::Positions::Invalid;
        BWAPI::Position commandPosition = BWAPI::Positions::Invalid;
        BWAPI::Position surroundPosition = BWAPI::Positions::Invalid;
        BWAPI::Position interceptPosition = BWAPI::Positions::Invalid;
        BWAPI::WalkPosition walkPosition = BWAPI::WalkPositions::Invalid;
        BWAPI::WalkPosition lastWalk = BWAPI::WalkPositions::Invalid;
        BWAPI::TilePosition tilePosition = BWAPI::TilePositions::Invalid;
        BWAPI::TilePosition buildPosition = BWAPI::TilePositions::Invalid;
        BWAPI::TilePosition lastTile = BWAPI::TilePositions::Invalid;
        BWAPI::UnitCommandType commandType = BWAPI::UnitCommandTypes::None;
        std::map<int, BWAPI::TilePosition> tileHistory;
        std::map<int, BWAPI::WalkPosition> walkHistory;
        std::map<int, BWAPI::Position> positionHistory;
        std::map<int, BWAPI::UnitCommandType> commandHistory;
        std::map<int, std::pair<BWAPI::Order, BWAPI::Position>> orderHistory;
    #pragma endregion

    #pragma region Paths
        BWEB::Path destinationPath;
        BWEB::Path targetPath;
        BWEM::CPPath quickPath;
    #pragma endregion

    #pragma region Updaters
        void updateTarget();
        void checkStuck();
        void checkHidden();
        void checkThreatening();
        void checkProxy();
    #pragma endregion

    public:

        bool concaveFlag = false;
        bool movedFlag = false;
        int lastUnreachableFrame = -999;
        bool borrowedPath = false;

        bool canOneShot(UnitInfo&);
        bool canTwoShot(UnitInfo&);


    #pragma region Constructors
        UnitInfo();

        UnitInfo(BWAPI::Unit u) {
            bwUnit = u;
        }
    #pragma endregion

    #pragma region Utility
        bool hasResource() { return !resource.expired(); }
        bool hasTransport() { return !transport.expired(); }
        bool hasTarget() { return !target.expired(); }
        bool hasSimTarget() { return !simTarget.expired(); }
        bool hasAttackedRecently() { return (BWAPI::Broodwar->getFrameCount() - lastAttackFrame < 120); }
        bool hasRepairedRecently() { return (BWAPI::Broodwar->getFrameCount() - lastRepairFrame < 120); }
        bool targetsFriendly() { return type == BWAPI::UnitTypes::Terran_Medic || type == BWAPI::UnitTypes::Terran_Science_Vessel || (type == BWAPI::UnitTypes::Zerg_Defiler && energy < 100); }

        bool isSuicidal() { return type == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine || type == BWAPI::UnitTypes::Zerg_Scourge || type == BWAPI::UnitTypes::Zerg_Infested_Terran; }
        bool isLightAir() { return type == BWAPI::UnitTypes::Protoss_Corsair || type == BWAPI::UnitTypes::Zerg_Mutalisk || type == BWAPI::UnitTypes::Terran_Wraith; }
        bool isCapitalShip() { return type == BWAPI::UnitTypes::Protoss_Carrier || type == BWAPI::UnitTypes::Terran_Battlecruiser || type == BWAPI::UnitTypes::Zerg_Guardian; }
        bool isHovering() { return type.isWorker() || type == BWAPI::UnitTypes::Protoss_Archon || type == BWAPI::UnitTypes::Protoss_Dark_Archon || type == BWAPI::UnitTypes::Terran_Vulture; }
        bool isTransport() { return type == BWAPI::UnitTypes::Protoss_Shuttle || type == BWAPI::UnitTypes::Terran_Dropship || type == BWAPI::UnitTypes::Zerg_Overlord; }
        bool isSpellcaster() { return type == BWAPI::UnitTypes::Protoss_High_Templar || type == BWAPI::UnitTypes::Protoss_Dark_Archon || type == BWAPI::UnitTypes::Terran_Medic || type == BWAPI::UnitTypes::Terran_Science_Vessel || type == BWAPI::UnitTypes::Zerg_Defiler; }
        bool isSiegeTank() { return type == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode || type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode; }
        bool isNearMapEdge() { return tilePosition.x < 2 || tilePosition.x > BWAPI::Broodwar->mapWidth() - 2 || tilePosition.y < 2 || tilePosition.y > BWAPI::Broodwar->mapHeight() - 2; }
        bool isCompleted() { return completed; }
        bool isStimmed() { return BWAPI::Broodwar->getFrameCount() - lastStimFrame < 300; }
        bool isCommander() { return commander; }

        bool isHealthy();
        bool isRequestingPickup();
        bool isWithinReach(UnitInfo&);
        bool isWithinRange(UnitInfo&);
        bool isWithinAngle(UnitInfo&);
        bool isWithinBuildRange();
        bool isWithinGatherRange();
        bool canStartAttack();
        bool canStartCast(BWAPI::TechType, BWAPI::Position);
        bool canStartGather();
        bool canAttackGround();
        bool canAttackAir();

        bool isStuck() { return BWAPI::Broodwar->getFrameCount() - lastTileMoveFrame > 240; }
        bool wasStuckRecently() {
            return BWAPI::Broodwar->getFrameCount() - lastStuckFrame < 240;
        }

        // General commands that verify we aren't spamming the same command and sticking the unit
        bool command(BWAPI::UnitCommandType, BWAPI::Position);
        bool command(BWAPI::UnitCommandType, UnitInfo&);
        BWAPI::Position getCommandPosition() { return commandPosition; }
        BWAPI::UnitCommandType getCommandType() { return commandType; }

        // Information about frame timings
        int frameStartedWhen() {
            return startedFrame;
        }
        int frameCompletesWhen() {
            return completeFrame;
        }
        int frameArrivesWhen() {
            return arriveFrame;
        }
        Time timeStartedWhen() {
            int started = frameStartedWhen();
            return Time(started / 1440, (started / 24) % 60);

        }
        Time timeCompletesWhen() {
            int completes = frameCompletesWhen();
            return Time(completes / 1440, (completes / 24) % 60);
        }
        Time timeArrivesWhen() {
            int arrival = frameArrivesWhen();
            return Time(arrival / 1440, (arrival / 24) % 60);
        }

        // Logic that dictates overriding simulation results
        bool localRetreat();
        bool localEngage();
        bool globalRetreat();
        bool globalEngage();

        bool attemptingRunby();
        bool attemptingSurround();
        bool attemptingHarass();

        void update();
        void verifyPaths();
        void setLastPositions();
    #pragma endregion
       
    #pragma region Getters
        std::vector<std::weak_ptr<UnitInfo>>& getAssignedCargo() { return assignedCargo; }
        std::vector<std::weak_ptr<UnitInfo>>& getTargetedBy() { return targetedBy; }
        std::map<int, BWAPI::TilePosition>& getTileHistory() { return tileHistory; }
        std::map<int, BWAPI::WalkPosition>& getWalkHistory() { return walkHistory; }
        std::map<int, BWAPI::Position>& getPositionHistory() { return positionHistory; }
        std::map<int, BWAPI::UnitCommandType>& getCommandHistory() { return commandHistory; }
        std::map<int, std::pair<BWAPI::Order, BWAPI::Position>>& getOrderHistory() { return orderHistory; }

        TransportState getTransportState() { return tState; }
        SimState getSimState() { return sState; }
        GlobalState getGlobalState() { return gState; }
        LocalState getLocalState() { return lState; }

        ResourceInfo &getResource() { return *resource.lock(); }
        UnitInfo &getTransport() { return *transport.lock(); }
        UnitInfo &getTarget() { return *target.lock(); }
        UnitInfo &getSimTarget() { return *simTarget.lock(); }

        Role getRole() { return role; }
        GoalType getGoalType() { return gType; }
        BWAPI::Unit& unit() { return bwUnit; }
        BWAPI::UnitType getType() { return type; }
        BWAPI::UnitType getBuildType() { return buildingType; }
        BWAPI::Player getPlayer() { return player; }
        BWAPI::Position getFormation() { return formation; }
        BWAPI::Position getPosition() { return position; }
        BWAPI::Position getEngagePosition() { return engagePosition; }
        BWAPI::Position getDestination() { return destination; }
        BWAPI::Position getGoal() { return goal; }
        BWAPI::Position getInterceptPosition() { return interceptPosition; }
        BWAPI::Position getSurroundPosition() { return surroundPosition; }
        BWAPI::WalkPosition getWalkPosition() { return walkPosition; }
        BWAPI::TilePosition getTilePosition() { return tilePosition; }
        BWAPI::TilePosition getBuildPosition() { return buildPosition; }
        BWAPI::TilePosition getLastTile() { return lastTile; }
        BWEB::Path& getDestinationPath() { return destinationPath; }
        BWEB::Path& getTargetPath() { return targetPath; }
        double getPercentTotal() { return percentTotal; }
        double getVisibleGroundStrength() { return visibleGroundStrength; }
        double getMaxGroundStrength() { return maxGroundStrength; }
        double getVisibleAirStrength() { return visibleAirStrength; }
        double getMaxAirStrength() { return maxAirStrength; }
        double getGroundRange() { return groundRange; }
        double getGroundReach() { return groundReach; }
        double getGroundDamage() { return groundDamage; }
        double getAirRange() { return airRange; }
        double getAirReach() { return airReach; }
        double getAirDamage() { return airDamage; }
        double getSpeed() { return speed; }
        double getPriority() { return priority; }
        double getEngDist() { return engageDist; }
        double getSimValue() { return simValue; }
        double getEngageRadius() { return engageRadius; }
        double getRetreatRadius() { return retreatRadius; }
        int getShields() { return shields; }
        int getHealth() { return health; }
        int getLastAttackFrame() { return lastAttackFrame; }
        int getLastVisibleFrame() { return lastVisibleFrame; }
        int getEnergy() { return energy; }
        int getRemainingTrainFrames() { return remainingTrainFrame; }
        int framesHoldingResource() { return resourceHeldFrames; }
        int getWalkWidth() { return walkWidth; }
        int getWalkHeight() { return walkHeight; }
        bool isTargetedBySplash() { return targetedBySplash; }
        bool isMarkedForDeath() { return markedForDeath; }
        bool isProxy() { return proxy; }
        bool isBurrowed() { return burrowed; }
        bool isFlying() { return flying; }
        bool isThreatening() { return threatening; }
        bool isHidden() { return hidden; }
        bool isNearSplash() { return nearSplash; }
        bool isNearSuicide() { return nearSuicide; }
    #pragma endregion      

    #pragma region Setters
        void setTargetedBySplash(bool newValue) { targetedBySplash = newValue; }
        void setMarkForDeath(bool newValue) { markedForDeath = newValue; }
        void setEngDist(double newValue) { engageDist = newValue; }
        void setSimValue(double newValue) { simValue = newValue; }
        void setLastAttackFrame(int newValue) { lastAttackFrame = newValue; }
        void setRemainingTrainFrame(int newFrame) { remainingTrainFrame = newFrame; }
        void setTransportState(TransportState newState) { tState = newState; }
        void setSimState(SimState newState) { sState = newState; }
        void setGlobalState(GlobalState newState) { gState = newState; }
        void setLocalState(LocalState newState) { lState = newState; }
        void setResource(ResourceInfo* unit) { unit ? resource = unit->weak_from_this() : resource.reset(); }
        void setTransport(UnitInfo* unit) { unit ? transport = unit->weak_from_this() : transport.reset(); }
        void setTarget(UnitInfo* unit) { unit ? target = unit->weak_from_this() : target.reset(); }
        void setSimTarget(UnitInfo* unit) { unit ? simTarget = unit->weak_from_this() : simTarget.reset(); }
        void setBackupTarget(UnitInfo* unit) { unit ? backupTarget = unit->weak_from_this() : backupTarget.reset(); }
        void setRole(Role newRole) { role = newRole; }
        void setGoalType(GoalType newGoalType) { gType = newGoalType; }
        void setBuildingType(BWAPI::UnitType newType) { buildingType = newType; }
        void setFormation(BWAPI::Position newPosition) { formation = newPosition; }
        void setPosition(BWAPI::Position newPosition) { position = newPosition; }
        void setEngagePosition(BWAPI::Position newPosition) { engagePosition = newPosition; }
        void setDestination(BWAPI::Position newPosition) { destination = newPosition; }
        void setGoal(BWAPI::Position newPosition) { goal = newPosition; }
        void setWalkPosition(BWAPI::WalkPosition newPosition) { walkPosition = newPosition; }
        void setTilePosition(BWAPI::TilePosition newPosition) { tilePosition = newPosition; }
        void setBuildPosition(BWAPI::TilePosition newPosition) { buildPosition = newPosition; }
        void setDestinationPath(BWEB::Path& newPath) { destinationPath = newPath; }
        void setTargetPath(BWEB::Path& newPath) { targetPath = newPath; }
    #pragma endregion

    #pragma region Drawing
        void circle(BWAPI::Color color);
        void box(BWAPI::Color color);
    #pragma endregion

    #pragma region Operators
        bool operator== (UnitInfo& p) {
            return bwUnit == p.unit();
        }

        bool operator!= (UnitInfo& p) {
            return bwUnit != p.unit();
        }

        bool operator< (UnitInfo& p) {
            return bwUnit < p.unit();
        }

        bool operator== (std::weak_ptr<UnitInfo>(unit)) {
            return bwUnit == unit.lock()->unit();
        }

        bool operator!= (std::weak_ptr<UnitInfo>(unit)) {
            return bwUnit != unit.lock()->unit();
        }

        bool operator< (std::weak_ptr<UnitInfo>(unit)) {
            return bwUnit < unit.lock()->unit();
        }
    #pragma endregion
    };

    inline bool operator== (std::weak_ptr<UnitInfo>(lunit), std::weak_ptr<UnitInfo>(runit)) {
        return lunit.lock()->unit() == runit.lock()->unit();
    }

    inline bool operator< (std::weak_ptr<UnitInfo>(lunit), std::weak_ptr<UnitInfo>(runit)) {
        return lunit.lock()->unit() < runit.lock()->unit();
    }
}
