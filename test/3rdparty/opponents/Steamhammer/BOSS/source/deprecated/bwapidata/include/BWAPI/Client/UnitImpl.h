#pragma once
#include <BWAPI.h>
#include "UnitData.h"
#include <set>
#include <string>

namespace BWAPI
{
  class Player;
  class UnitImpl : public Unit
  {
    private:
      int id;
      UnitType initialType;
      int initialResources;
      int initialHitPoints;
      Position initialPosition;
      int lastOrderFrame;
      void* clientInfo;
    public:
      UnitData* self;
      std::set<Unit*> connectedUnits;
      std::set<Unit*> loadedUnits;
      void clear();
      void saveInitialState();

      UnitImpl(int id);
      virtual int          getID() const;
      virtual Player*      getPlayer() const;
      virtual UnitType     getType() const;
      virtual Position     getPosition() const;
      virtual TilePosition getTilePosition() const;
      virtual double       getAngle() const;
      virtual double       getVelocityX() const;
      virtual double       getVelocityY() const;
      virtual int          getHitPoints() const;
      virtual int          getShields() const;
      virtual int          getEnergy() const;
      virtual int          getResources() const;
      virtual int          getResourceGroup() const;

      virtual double getDistance(Unit* target) const;
      virtual double getDistance(Position target) const;
      virtual bool   hasPath(Unit* target) const;
      virtual bool   hasPath(Position target) const;
      virtual int    getLastOrderFrame() const;
      virtual int    getUpgradeLevel(UpgradeType upgrade) const;

      virtual UnitType     getInitialType() const;
      virtual Position     getInitialPosition() const;
      virtual TilePosition getInitialTilePosition() const;
      virtual int          getInitialHitPoints() const;
      virtual int          getInitialResources() const;

      virtual int getKillCount() const;
      virtual int getInterceptorCount() const;
      virtual int getScarabCount() const;
      virtual int getSpiderMineCount() const;
      virtual int getGroundWeaponCooldown() const;
      virtual int getAirWeaponCooldown() const;
      virtual int getSpellCooldown() const;
      virtual int getDefenseMatrixPoints() const;

      virtual int getDefenseMatrixTimer() const;
      virtual int getEnsnareTimer() const;
      virtual int getIrradiateTimer() const;
      virtual int getLockdownTimer() const;
      virtual int getMaelstromTimer() const;
      virtual int getOrderTimer() const;
      virtual int getPlagueTimer() const;
      virtual int getRemoveTimer() const;
      virtual int getStasisTimer() const;
      virtual int getStimTimer() const;

      virtual UnitType            getBuildType() const;
      virtual std::list<UnitType> getTrainingQueue() const;
      virtual TechType            getTech() const;
      virtual UpgradeType         getUpgrade() const;
      virtual int                 getRemainingBuildTime() const;
      virtual int                 getRemainingTrainTime() const;
      virtual int                 getRemainingResearchTime() const;
      virtual int                 getRemainingUpgradeTime() const;
      virtual Unit*               getBuildUnit() const;

      virtual Unit*    getTarget() const;
      virtual Position getTargetPosition() const;
      virtual Order    getOrder() const;
      virtual Unit*    getOrderTarget() const;
      virtual Order    getSecondaryOrder() const;
      virtual Position getRallyPosition() const;
      virtual Unit*    getRallyUnit() const;
      virtual Unit*    getAddon() const;
      virtual Unit*    getNydusExit() const;
      virtual Unit*    getPowerUp() const;

      virtual Unit*           getTransport() const;
      virtual std::set<Unit*> getLoadedUnits() const;
      virtual Unit*           getCarrier() const;
      virtual std::set<Unit*> getInterceptors() const;
      virtual Unit*           getHatchery() const;
      virtual std::set<Unit*> getLarva() const;

      virtual bool exists() const;
      virtual bool hasNuke() const;
      virtual bool isAccelerating() const;
      virtual bool isAttacking() const;
      virtual bool isBeingConstructed() const;
      virtual bool isBeingGathered() const;
      virtual bool isBeingHealed() const;
      virtual bool isBlind() const;
      virtual bool isBraking() const;
      virtual bool isBurrowed() const;
      virtual bool isCarryingGas() const;
      virtual bool isCarryingMinerals() const;
      virtual bool isCloaked() const;
      virtual bool isCompleted() const;
      virtual bool isConstructing() const;
      virtual bool isDefenseMatrixed() const;
      virtual bool isDetected() const;
      virtual bool isEnsnared() const;
      virtual bool isFollowing() const;
      virtual bool isGatheringGas() const;
      virtual bool isGatheringMinerals() const;
      virtual bool isHallucination() const;
      virtual bool isHoldingPosition() const;
      virtual bool isIdle() const;
      virtual bool isInterruptible() const;
      virtual bool isIrradiated() const;
      virtual bool isLifted() const;
      virtual bool isLoaded() const;
      virtual bool isLockedDown() const;
      virtual bool isMaelstrommed() const;
      virtual bool isMorphing() const;
      virtual bool isMoving() const;
      virtual bool isParasited() const;
      virtual bool isPatrolling() const;
      virtual bool isPlagued() const;
      virtual bool isRepairing() const;
      virtual bool isResearching() const;
      virtual bool isSelected() const;
      virtual bool isSieged() const;
      virtual bool isStartingAttack() const;
      virtual bool isStasised() const;
      virtual bool isStimmed() const;
      virtual bool isStuck() const;
      virtual bool isTraining() const;
      virtual bool isUnderStorm() const;
      virtual bool isUnpowered() const;
      virtual bool isUpgrading() const;
      virtual bool isVisible() const;
      virtual bool isVisible(Player* player) const;

      virtual bool issueCommand(UnitCommand command);

      virtual bool attackMove(Position target);
      virtual bool attackUnit(Unit* target);
      virtual bool build(TilePosition target, UnitType type);
      virtual bool buildAddon(UnitType type);
      virtual bool train(UnitType type);
      virtual bool morph(UnitType type);
      virtual bool research(TechType tech);
      virtual bool upgrade(UpgradeType upgrade);
      virtual bool setRallyPoint(Position target);
      virtual bool setRallyPoint(Unit* target);
      virtual bool move(Position target);
      virtual bool patrol(Position target);
      virtual bool holdPosition();
      virtual bool stop();
      virtual bool follow(Unit* target);
      virtual bool gather(Unit* target);
      virtual bool returnCargo();
      virtual bool repair(Unit* target);
      virtual bool burrow();
      virtual bool unburrow();
      virtual bool cloak();
      virtual bool decloak();
      virtual bool siege();
      virtual bool unsiege();
      virtual bool lift();
      virtual bool land(TilePosition target);
      virtual bool load(Unit* target);
      virtual bool unload(Unit* target);
      virtual bool unloadAll();
      virtual bool unloadAll(Position target);
      virtual bool rightClick(Position target);
      virtual bool rightClick(Unit* target);
      virtual bool haltConstruction();
      virtual bool cancelConstruction();
      virtual bool cancelAddon();
      virtual bool cancelTrain(int slot = -2);
      virtual bool cancelMorph();
      virtual bool cancelResearch();
      virtual bool cancelUpgrade();
      virtual bool useTech(TechType tech);
      virtual bool useTech(TechType tech, Position target);
      virtual bool useTech(TechType tech, Unit* target);

      virtual void setClientInfo(void* clientinfo);
      virtual void* getClientInfo() const;
  };
}
