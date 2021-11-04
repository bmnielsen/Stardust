#include "McRave.h"

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;

namespace McRave
{
    UnitInfo::UnitInfo() {}

    namespace {

        WalkPosition calcWalkPosition(UnitInfo* unit)
        {
            auto walkWidth = unit->getType().isBuilding() ? unit->getType().tileWidth() * 4 : unit->getWalkWidth();
            auto walkHeight = unit->getType().isBuilding() ? unit->getType().tileHeight() * 4 : unit->getWalkHeight();

            if (!unit->getType().isBuilding())
                return WalkPosition(unit->getPosition()) - WalkPosition(walkWidth / 2, walkHeight / 2);
            else
                return WalkPosition(unit->getTilePosition());
            return WalkPositions::None;
        }

        double calcSimRadius(UnitInfo* unit)
        {
            if (unit->isFlying()) {
                if (Players::getTotalCount(PlayerState::Enemy, Terran_Goliath) > 0)
                    return 352.0;
                if (Players::getTotalCount(PlayerState::Enemy, Protoss_Photon_Cannon) > 0
                    || Players::getTotalCount(PlayerState::Enemy, Protoss_Carrier) > 0
                    || Players::getTotalCount(PlayerState::Enemy, Protoss_Arbiter) > 0
                    || Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0
                    || Players::getTotalCount(PlayerState::Enemy, Zerg_Spore_Colony) > 0)
                    return 320.0;
                if (Players::getTotalCount(PlayerState::Enemy, Terran_Ghost) > 0
                    || Players::getTotalCount(PlayerState::Enemy, Terran_Valkyrie) > 0
                    || Players::getTotalCount(PlayerState::Enemy, Terran_Missile_Turret) > 0)
                    return 288.0;
                return 256.0;
            }

            if (Players::getTotalCount(PlayerState::Enemy, Terran_Siege_Tank_Siege_Mode) > 0
                || Players::getTotalCount(PlayerState::Enemy, Terran_Siege_Tank_Tank_Mode) > 0
                || Players::getTotalCount(PlayerState::Enemy, Terran_Battlecruiser) > 0
                || Players::getTotalCount(PlayerState::Enemy, Protoss_Carrier) > 0
                || Players::getTotalCount(PlayerState::Enemy, Protoss_Reaver) > 0
                || Players::getTotalCount(PlayerState::Enemy, Protoss_Arbiter) > 0
                || Players::getTotalCount(PlayerState::Enemy, Zerg_Guardian) > 0
                || Players::getTotalCount(PlayerState::Enemy, Zerg_Defiler) > 0)
                return 540.0 + Players::getSupply(PlayerState::Self, Races::None);
            if (Players::getTotalCount(PlayerState::Enemy, Terran_Vulture) > 0
                || Players::getTotalCount(PlayerState::Enemy, Terran_Goliath) > 0
                || Players::getTotalCount(PlayerState::Enemy, Terran_Wraith) > 0
                || Players::getTotalCount(PlayerState::Enemy, Terran_Valkyrie) > 0
                || Players::getTotalCount(PlayerState::Enemy, Terran_Bunker) > 0
                || Players::getTotalCount(PlayerState::Enemy, Terran_Marine) > 0
                || Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0
                || Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) > 0
                || Players::getTotalCount(PlayerState::Enemy, Protoss_Scout) > 0
                || Players::getTotalCount(PlayerState::Enemy, Protoss_Photon_Cannon) > 0
                || Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) > 0
                || Players::getTotalCount(PlayerState::Enemy, Zerg_Hydralisk) > 0
                || Players::getTotalCount(PlayerState::Enemy, Zerg_Lurker) > 0
                || Players::getTotalCount(PlayerState::Enemy, Zerg_Mutalisk) > 0
                || Players::getTotalCount(PlayerState::Enemy, Zerg_Sunken_Colony) > 0)
                return 400.0 + Players::getSupply(PlayerState::Self, Races::None);
            return 320.0 + Players::getSupply(PlayerState::Self, Races::None);
        }

        Position calcInterceptPosition(UnitInfo& unit) {
            return Positions::Invalid;
        }

        Position calcSurroundPosition(UnitInfo& unit) {
            // If we can't see the units speed, return its current position
            if (!unit.hasTarget()
                || !unit.getTarget().unit()->exists()
                || unit.getSpeed() == 0.0
                || unit.getTarget().getSpeed() == 0.0
                || !unit.getTarget().getPosition().isValid()
                || !Terrain::getEnemyStartingPosition().isValid())
                return Positions::Invalid;

            auto trapTowards = Positions::Invalid;
            if (unit.getTarget().isFlying())
                trapTowards = Terrain::getEnemyStartingPosition();
            else if (unit.getTarget().isThreatening())
                trapTowards = Position(BWEB::Map::getMainChoke()->Center());
            else {
                auto path = mapBWEM.GetPath(unit.getTarget().getPosition(), Terrain::getEnemyStartingPosition());
                trapTowards = (path.empty() || !path.front()) ? Terrain::getEnemyStartingPosition() : Position(path.front()->Center());
            }
            auto timeToEngage = clamp((unit.getEngDist() / unit.getSpeed()) * unit.getTarget().getSpeed() / unit.getSpeed(), 12.0, 96.0);
            auto targetDestination = Util::clipPosition(((unit.getTarget().getPosition() * 3) + trapTowards) / 4);
            return targetDestination;
        }
    }

    void UnitInfo::circle(Color color) {
        Visuals::drawCircle(position, type.width(), color);
    }

    void UnitInfo::box(Color color) {
        Visuals::drawBox(
            Position(position.x - type.dimensionLeft(), position.y - type.dimensionUp()),
            Position(position.x + type.dimensionRight() + 1, position.y + type.dimensionDown() + 1),
            color);
    }

    void UnitInfo::setLastPositions()
    {
        lastPos = getPosition();
        lastTile = getTilePosition();
        lastWalk =  walkPosition;
    }

    void UnitInfo::verifyPaths()
    {
        if (lastTile != unit()->getTilePosition()) {
            BWEB::Path emptyPath;
            destinationPath = emptyPath;
            targetPath = emptyPath;
        }

        if (lastTile.isValid() && unit()->getTilePosition().isValid() && mapBWEM.GetArea(lastTile) != mapBWEM.GetArea(unit()->getTilePosition()))
            quickPath.clear();
    }

    void UnitInfo::update()
    {
        auto t = unit()->getType();
        auto p = unit()->getPlayer();

        if (unit()->exists()) {

            setLastPositions();
            verifyPaths();
            movedFlag = false;
            targetedBySplash = false;

            // Unit Stats
            type                        = t;
            player                      = p;
            health                      = unit()->getHitPoints() > 0 ? unit()->getHitPoints() : (health == 0 ? t.maxHitPoints() : health);
            armor                       = player->getUpgradeLevel(t.armorUpgrade());
            shields                     = unit()->getShields() > 0 ? unit()->getShields() : (shields == 0 ? t.maxShields() : shields);
            shieldArmor                 = player->getUpgradeLevel(UpgradeTypes::Protoss_Plasma_Shields);
            energy                      = unit()->getEnergy();
            percentHealth               = t.maxHitPoints() > 0 ? double(health) / double(t.maxHitPoints()) : 0.0;
            percentShield               = t.maxShields() > 0 ? double(shields) / double(t.maxShields()) : 0.0;
            percentTotal                = t.maxHitPoints() + t.maxShields() > 0 ? double(health + shields) / double(t.maxHitPoints() + t.maxShields()) : 0.0;
            walkWidth                   = int(ceil(double(t.width()) / 8.0));
            walkHeight                  = int(ceil(double(t.height()) / 8.0));
            completed                   = unit()->isCompleted() && !unit()->isMorphing();
            currentSpeed                = sqrt(pow(unit()->getVelocityX(), 2) + pow(unit()->getVelocityY(), 2));

            // Points        
            position                    = unit()->getPosition();
            tilePosition                = t.isBuilding() ? unit()->getTilePosition() : TilePosition(unit()->getPosition());
            walkPosition                = calcWalkPosition(this);
            destination                 = Positions::Invalid;
            formation                   = Positions::Invalid;
            concaveFlag                 = false;

            // McRave Stats
            groundRange                 = Math::groundRange(*this);
            groundDamage                = Math::groundDamage(*this);
            groundReach                 = Math::groundReach(*this);
            airRange                    = Math::airRange(*this);
            airReach                    = Math::airReach(*this);
            airDamage                   = Math::airDamage(*this);
            speed                       = Math::moveSpeed(*this);
            visibleGroundStrength       = Math::visibleGroundStrength(*this);
            maxGroundStrength           = Math::maxGroundStrength(*this);
            visibleAirStrength          = Math::visibleAirStrength(*this);
            maxAirStrength              = Math::maxAirStrength(*this);
            priority                    = Math::priority(*this);
            engageRadius                = calcSimRadius(this) + 96.0;
            retreatRadius               = calcSimRadius(this);

            // States
            lState                      = LocalState::None;
            gState                      = GlobalState::None;
            tState                      = TransportState::None;
            flying                      = unit()->isFlying() || getType().isFlyer() || unit()->getOrder() == Orders::LiftingOff || unit()->getOrder() == Orders::BuildingLiftOff;

            // Frames
            remainingTrainFrame         = max(0, remainingTrainFrame - 1);
            lastAttackFrame             = (t != Protoss_Reaver && (unit()->isStartingAttack())) ? Broodwar->getFrameCount() : lastAttackFrame;
            lastRepairFrame             = (unit()->isRepairing() || unit()->isBeingHealed()) ? Broodwar->getFrameCount() : lastRepairFrame;
            minStopFrame                = Math::stopAnimationFrames(t);
            lastStimFrame               = unit()->isStimmed() ? Broodwar->getFrameCount() : lastStimFrame;

            // BWAPI won't reveal isStartingAttack when hold position is executed if the unit can't use hold position
            if (getPlayer() != Broodwar->self() && getType().isWorker()) {
                if (unit()->getGroundWeaponCooldown() == 1 || unit()->getAirWeaponCooldown() == 1)
                    lastAttackFrame = Broodwar->getFrameCount();
            }

            if (unit()->exists())
                lastVisibleFrame = Broodwar->getFrameCount();

            checkHidden();
            checkStuck();
            checkProxy();

            if (!bwUnit->isCompleted()) {
                auto ratio = (double(health) - (0.1 * double(type.maxHitPoints()))) / (0.9 * double(type.maxHitPoints()));
                completeFrame = Broodwar->getFrameCount() + int(std::round((1.0 - ratio) * double(type.buildTime())));
                startedFrame = Broodwar->getFrameCount() - int(std::round((ratio) * double(type.buildTime())));
            }
            else if (startedFrame == -999 && completeFrame == -999) {
                startedFrame = Broodwar->getFrameCount();
                completeFrame = Broodwar->getFrameCount();
            }

            arriveFrame = isFlying() ? int(position.getDistance(BWEB::Map::getMainPosition()) / speed) :
                Broodwar->getFrameCount() + int(BWEB::Map::getGroundDistance(position, BWEB::Map::getMainPosition()) / speed);


            tileHistory[Broodwar->getFrameCount()] = getTilePosition();
            walkHistory[Broodwar->getFrameCount()] = getWalkPosition();
            positionHistory[Broodwar->getFrameCount()] = getPosition();

            orderHistory[Broodwar->getFrameCount()] = make_pair(unit()->getOrder(), unit()->getOrderTargetPosition());
            commandHistory[Broodwar->getFrameCount()] = unit()->getLastCommand().getType();

            if (tileHistory.size() > 30)
                tileHistory.erase(tileHistory.begin());
            if (walkHistory.size() > 30)
                walkHistory.erase(walkHistory.begin());
            if (positionHistory.size() > 30)
                positionHistory.erase(positionHistory.begin());

            if (orderHistory.size() > 10)
                orderHistory.erase(orderHistory.begin());
            if (commandHistory.size() > 10)
                commandHistory.erase(commandHistory.begin());
        }

        updateTarget();

        if (getPlayer()->isEnemy(Broodwar->self()))
            checkThreatening();

        // Check if this unit is close to a splash unit
        if (getPlayer() == Broodwar->self() && flying) {
            nearSplash = false;
            auto closestSplasher = Util::getClosestUnit(position, PlayerState::Enemy, [&](auto &u) {
                return (u.getType() == Protoss_Corsair && Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) >= 6) || u.getType() == Protoss_Archon || u.getType() == Terran_Valkyrie || u.getType() == Zerg_Devourer;
            });

            if (closestSplasher && Util::boxDistance(getType(), getPosition(), closestSplasher->getType(), closestSplasher->getPosition()) < closestSplasher->getAirRange())
                nearSplash = true;
        }

        // Check if this unit is close to a suicidal unit
        if (getPlayer() == Broodwar->self() && flying) {
            nearSuicide = false;
            auto closestSuicide = Util::getClosestUnit(position, PlayerState::Enemy, [&](auto &u) {
                return u.getType() == Zerg_Scourge || u.getType() == Zerg_Infested_Terran;
            });

            if (closestSuicide && Util::boxDistance(getType(), getPosition(), closestSuicide->getType(), closestSuicide->unit()->getOrderTargetPosition()) < closestSuicide->getAirReach() && Util::boxDistance(getType(), getPosition(), closestSuicide->getType(), closestSuicide->unit()->getPosition()) < 48.0)
                nearSuicide = true;
        }
    }

    void UnitInfo::updateTarget()
    {
        // Update my target
        if (getPlayer() == Broodwar->self()) {
            target = weak_ptr<UnitInfo>();
            if (getType() == Terran_Vulture_Spider_Mine) {
                if (unit()->getOrderTarget())
                    target = Units::getUnitInfo(unit()->getOrderTarget());
            }
            else
                Targets::getTarget(*this);

            surroundPosition            = calcSurroundPosition(*this);
            interceptPosition           = calcInterceptPosition(*this);
        }

        // Update enemy target based on orders or closest self target
        else if (getPlayer()->isEnemy(Broodwar->self())) {

            if (unit()->getOrderTarget()) {
                auto &targetInfo = Units::getUnitInfo(unit()->getOrderTarget());
                if (targetInfo) {
                    target = targetInfo;
                    targetInfo->getTargetedBy().push_back(this->weak_from_this());
                }
            }
            else if (getType() != Terran_Vulture_Spider_Mine) {
                auto closest = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
                    return (u.isFlying() && getAirDamage() > 0.0) || (!u.isFlying() && getGroundDamage() > 0.0);
                });
                if (closest)
                    target = closest;
            }

            if (hasTarget() && (getType() == Protoss_Reaver || getType() == Protoss_Archon || getType() == Protoss_Corsair || getType() == Terran_Valkyrie || getType() == Zerg_Devourer) && isWithinRange(getTarget()))
                getTarget().setTargetedBySplash(true);

            if (hasTarget()) {
                auto range = max(64.0, getTarget().getType().isFlyer() ? getAirRange() : getGroundRange());
                auto distance = Util::boxDistance(getType(), getPosition(), getTarget().getType(), getTarget().getPosition());
                auto direction = ((distance - range) / distance);
                auto engageX = int((getPosition().x - getTarget().getPosition().x) * direction);
                auto engageY = int((getPosition().y - getTarget().getPosition().y) * direction);
                auto engagePosition = getPosition() - Position(engageX, engageY);

                // If unit is loaded or further than their range, we want to calculate the expected engage position
                if (distance > range || unit()->isLoaded())
                    setEngagePosition(engagePosition);
                else
                    setEngagePosition(getPosition());

                setEngDist(getPosition().getDistance(getEngagePosition()));

                // HACK: Replicate the target to other light air around it
                if (getTarget().isLightAir()) {
                    for (auto &p : Players::getPlayers()) {
                        if (p.second.isSelf()) {
                            for (auto &u : p.second.getUnits())
                                if (u->isLightAir() && find(u->getTargetedBy().begin(), u->getTargetedBy().end(), this->weak_from_this()) == u->getTargetedBy().end() && u->getPosition().getDistance(getTarget().getPosition()) < 120.0)
                                    u->getTargetedBy().push_back(this->weak_from_this());
                        }
                    }
                }
            }
        }
    }

    void UnitInfo::checkStuck() {

        // Check if a worker is being blocked from mining or returning cargo
        if (unit()->isCarryingGas() || unit()->isCarryingMinerals())
            resourceHeldFrames = max(resourceHeldFrames, 0) + 1;
        else if (unit()->isGatheringGas() || unit()->isGatheringMinerals())
            resourceHeldFrames = min(resourceHeldFrames, 0) - 1;
        else
            resourceHeldFrames = 0;

        // Check if clipped between terrain or buildings
        if (getType().isWorker()) {

            vector<TilePosition> directions{ {1,0}, {-1,0}, {0, 1}, {0,-1} };
            bool trapped = true;

            // Check if the unit is stuck by terrain and buildings
            for (auto &tile : directions) {
                auto current = getTilePosition() + tile;
                if (BWEB::Map::isUsed(current) == None || BWEB::Map::isWalkable(current, getType()))
                    trapped = false;
            }

            // Check if the unit intends to be here to build
            if (getBuildPosition().isValid()) {
                const auto center = Position(getBuildPosition()) + Position(getBuildType().tileWidth() * 16, getBuildType().tileHeight() * 16);
                if (Util::boxDistance(getType(), getPosition(), getBuildType(), center) <= 0.0)
                    trapped = false;

                // Check if the unit is trapped between 3 mineral patches
                auto cnt = 0;
                for (auto &tile : directions) {
                    auto current = getTilePosition() + tile;
                    if (BWEB::Map::isUsed(current).isMineralField())
                        cnt++;
                }
                if (cnt < 3)
                    trapped = false;
            }

            if (!trapped)
                lastTileMoveFrame = Broodwar->getFrameCount();
        }

        // Check if a unit hasn't moved in a while but is trying to
        if (getPlayer() != Broodwar->self() || lastPos != getPosition() || !unit()->isMoving() || unit()->getLastCommand().getType() == UnitCommandTypes::Stop || getLastAttackFrame() == Broodwar->getFrameCount())
            lastTileMoveFrame = Broodwar->getFrameCount();
        else if (isStuck())
            lastStuckFrame = Broodwar->getFrameCount();
    }

    void UnitInfo::checkHidden()
    {
        // Burrowed check for non spider mine type units or units we can see using the order for burrowing
        burrowed = (getType() != Terran_Vulture_Spider_Mine && unit()->isBurrowed()) || unit()->getOrder() == Orders::Burrowing;

        // If this is a spider mine and doesn't have a target, then it is an inactive mine and unable to attack
        if (getType() == Terran_Vulture_Spider_Mine && (!unit()->exists() || (!hasTarget() && unit()->getSecondaryOrder() == Orders::Cloak))) {
            burrowed = true;
            groundReach = getGroundRange();
        }

        // A unit is considered hidden if it is burrowed or cloaked and not under detection
        hidden = (burrowed || bwUnit->isCloaked())
            && (player->isEnemy(BWAPI::Broodwar->self()) ? !Actions::overlapsDetection(bwUnit, position, PlayerState::Self) : !Actions::overlapsDetection(bwUnit, position, PlayerState::Enemy));
    }

    void UnitInfo::checkThreatening()
    {
        // Determine how close it is to strategic locations
        const auto choke = Terrain::isDefendNatural() ? BWEB::Map::getNaturalChoke() : BWEB::Map::getMainChoke();
        const auto area = Terrain::isDefendNatural() ? BWEB::Map::getNaturalArea() : BWEB::Map::getMainArea();
        const auto closestGeo = BWEB::Map::getClosestChokeTile(choke, getPosition());
        const auto closestStation = Stations::getClosestStationAir(PlayerState::Self, getPosition());
        const auto rangeCheck = max({ getAirRange() + 32.0, getGroundRange() + 32.0, 64.0 });
        const auto proximityCheck = max(rangeCheck, 200.0);
        auto threateningThisFrame = false;

        // If the unit is close to stations, defenses or resources owned by us
        const auto atHome = Terrain::isInAllyTerritory(getTilePosition()) && closestStation && closestStation->getBase()->Center().getDistance(getPosition()) < 640.0;
        const auto atChoke = getPosition().getDistance(closestGeo) <= rangeCheck;

        // If the unit attacked defenders, workers or buildings
        const auto attackedDefender = hasAttackedRecently() && hasTarget() && Terrain::isInAllyTerritory(getTarget().getTilePosition()) && getTarget().getRole() == Role::Defender;
        const auto attackedWorkers = hasAttackedRecently() && hasTarget() && Terrain::isInAllyTerritory(getTarget().getTilePosition()) && getTarget().getRole() == Role::Worker;
        const auto attackedBuildings = hasAttackedRecently() && hasTarget() && getTarget().getType().isBuilding();

        // Check if our resources are in danger
        auto nearResources = [&]() {
            return closestStation && closestStation == Terrain::getMyMain() && closestStation->getResourceCentroid().getDistance(getPosition()) < proximityCheck && atHome;
        };

        // Check if our defenses can hit or be hit
        auto nearDefenders = [&]() {
            auto closestDefender = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
                return u.getRole() == Role::Defender && ((!u.unit()->isMorphing() && u.unit()->isCompleted() && u.canAttackGround()) || (Util::getTime() < Time(4, 15) && hasAttackedRecently()));
            });
            return closestDefender && (closestDefender->isWithinRange(*this) || isWithinRange(*closestDefender));
        };

        // Checks if it can damage an already damaged building
        auto nearFragileBuilding = [&]() {
            auto fragileBuilding = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
                return !u.isHealthy() && u.getType().isBuilding() && (u.unit()->isCompleted() || isWithinRange(u)) && Terrain::isInAllyTerritory(u.getTilePosition());
            });
            return fragileBuilding && canAttackGround() && Util::boxDistance(fragileBuilding->getType(), fragileBuilding->getPosition(), getType(), getPosition()) < proximityCheck;
        };

        // Check if any builders can be hit or blocked
        auto nearBuildPosition = [&]() {
            if (atHome && !isFlying() && Util::getTime() < Time(5, 00)) {
                auto closestBuilder = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
                    return u.getRole() == Role::Worker && u.getBuildPosition().isValid() && u.getBuildType().isValid();
                });

                if (closestBuilder) {
                    auto center = Position(closestBuilder->getBuildPosition()) + Position(closestBuilder->getBuildType().tileWidth() * 16, closestBuilder->getBuildType().tileHeight() * 16);
                    if (Util::boxDistance(getType(), getPosition(), closestBuilder->getBuildType(), center) < proximityCheck
                        || (attackedWorkers && Util::boxDistance(getType(), getPosition(), closestBuilder->getType(), closestBuilder->getPosition()) < rangeCheck))
                        return true;
                }
            }
            return false;
        };

        // Checks if this unit is range of our army
        auto nearArmy = [&]() {
            if (Combat::defendChoke()) {
                for (auto &pos : Combat::getDefendPositions()) {
                    Visuals::drawCircle(pos, 2, Colors::Red);
                    if (getPosition().getDistance(pos) < rangeCheck)
                        return true;
                    if (getPosition().getDistance(BWEB::Map::getMainPosition()) < pos.getDistance(BWEB::Map::getMainPosition()))
                        return true;
                }
            }
            if (getPosition().getDistance(Position(BWEB::Map::getMainChoke()->Center())) < 64.0 && int(Stations::getMyStations().size()) >= 2)
                return true;

            // Fix for Andromeda like maps
            if (Terrain::isInAllyTerritory(getTilePosition()) && mapBWEM.GetArea(getTilePosition()) != BWEB::Map::getNaturalArea() && int(Stations::getMyStations().size()) >= 2)
                return true;

            return getTilePosition().isValid() && mapBWEM.GetArea(getTilePosition()) == BWEB::Map::getMainArea() && (int(Stations::getMyStations().size()) >= 2 || Combat::defendChoke());
        };

        const auto constructing = unit()->exists() && (unit()->isConstructing() || unit()->getOrder() == Orders::ConstructingBuilding || unit()->getOrder() == Orders::PlaceBuilding);

        // Building
        if (getType().isBuilding()) {
            threateningThisFrame = Planning::overlapsPlan(*this, getPosition())
                || ((atChoke || atHome) && (airDamage > 0.0 || groundDamage > 0.0 || getType() == Protoss_Shield_Battery || getType().isRefinery()))
                || nearResources();
        }

        // Worker
        else if (getType().isWorker())
            threateningThisFrame = (atHome || atChoke) && (constructing || attackedWorkers || attackedDefender || attackedBuildings);

        // Unit
        else
            threateningThisFrame = attackedDefender
            || attackedWorkers
            || nearResources()
            || nearFragileBuilding()
            || nearBuildPosition()
            || nearDefenders()
            || nearArmy();

        // Specific case: Marine near a proxy bunker
        if (getType() == Terran_Marine && Util::getTime() < Time(5, 00)) {
            auto closestThreateningBunker = Util::getClosestUnit(getPosition(), PlayerState::Enemy, [&](auto &u) {
                return u.isThreatening() && u.getType() == Terran_Bunker;
            });
            if (closestThreateningBunker && closestThreateningBunker->getPosition().getDistance(getPosition()) < 160.0)
                threateningThisFrame = true;
        }

        // Determine if this unit is threatening
        if (threateningThisFrame) threateningFrames++;
        else threateningFrames = 0;

        if (threateningFrames > 8)
            lastThreateningFrame = Broodwar->getFrameCount();
        threatening = Broodwar->getFrameCount() - lastThreateningFrame <= min(64, Util::getTime().minutes * 2);
    }

    void UnitInfo::checkProxy()
    {
        // If we're safe, no longer detect proxy
        if (Players::getSupply(PlayerState::Self, Races::None) >= 60 || Util::getTime() > Time(6, 00)) {
            proxy = false;
            return;
        }

        if (player->isEnemy(Broodwar->self())) {
            if (getType() == Terran_Barracks || getType() == Terran_Bunker || getType() == Protoss_Gateway || getType() == Protoss_Photon_Cannon || getType() == Protoss_Pylon) {
                auto closestMain = BWEB::Stations::getClosestMainStation(getTilePosition());
                auto closestNat = BWEB::Stations::getClosestNaturalStation(getTilePosition());
                auto isNotInMain = closestNat && closestNat->getBase()->GetArea() != mapBWEM.GetArea(getTilePosition()) && getPosition().getDistance(closestNat->getBase()->Center()) > 640.0;
                auto isNotInNat = closestMain && closestMain->getBase()->GetArea() != mapBWEM.GetArea(getTilePosition()) && getPosition().getDistance(closestMain->getBase()->Center()) > 640.0;

                auto closerToMyMain = closestMain && closestMain == Terrain::getMyMain() && getPosition().getDistance(closestMain->getBase()->Center()) < 480.0;
                auto closerToMyNat = closestNat && closestNat == Terrain::getMyNatural() && getPosition().getDistance(closestNat->getBase()->Center()) < 320.0;
                auto proxyBuilding = (isNotInMain && isNotInNat) || closerToMyMain || closerToMyNat;

                if (proxyBuilding)
                    proxy = true;
            }
        }
    }

    bool UnitInfo::command(UnitCommandType cmd, Position here)
    {
        // Check if we need to wait a few frames before issuing a command due to stop frames
        auto frameSinceAttack = Broodwar->getFrameCount() - lastAttackFrame;
        auto cancelAttackRisk = frameSinceAttack <= minStopFrame - Broodwar->getLatencyFrames();
        commandPosition = here;
        commandType = cmd;

        if (cancelAttackRisk && !isLightAir())
            return false;

        // Add some wiggle room for movement
        here += Position(rand() % 2 - 1, rand() % 2 - 1);

        // Check if we should overshoot for halting distance
        if (cmd == UnitCommandTypes::Move && !getBuildPosition().isValid() && (getType().isFlyer() || isHovering() || getType() == Protoss_High_Templar)) {
            auto distance = int(getPosition().getDistance(here));
            auto haltDistance = max({ distance, 32, getType().haltDistance() / 256 }) + 128.0;
            auto overShootHere = here;

            if (distance > 0) {
                overShootHere = getPosition() - ((getPosition() - here) * int(round(haltDistance / distance)));
                overShootHere = Util::clipLine(getPosition(), overShootHere);
            }
            if (getType().isFlyer() || (isHovering() && Util::findWalkable(*this, overShootHere)))
                here = overShootHere;
        }

        // Check if this is a new command
        const auto newCommand = [&]() {
            auto newCommandPosition = unit()->getLastCommand().getTargetPosition().getDistance(here) > 32;
            auto newCommandType = unit()->getLastCommand().getType() != cmd;
            auto newCommandFrame = Broodwar->getFrameCount() - unit()->getLastCommandFrame() - Broodwar->getLatencyFrames() > 12;
            return newCommandPosition || newCommandType || newCommandFrame;
        };

        // Add action and grid movement
        if ((cmd == UnitCommandTypes::Move || cmd == UnitCommandTypes::Right_Click_Position) && getPosition().getDistance(here) < 160.0) {
            Actions::addAction(unit(), here, getType(), PlayerState::Self);
            Grids::addMovement(here, *this);
        }

        // If this is a new order or new command than what we're requesting, we can issue it
        if (newCommand()) {
            if (cmd == UnitCommandTypes::Move)
                unit()->move(here);
            if (cmd == UnitCommandTypes::Right_Click_Position)
                unit()->rightClick(here);
            return true;
        }
        return false;
    }

    bool UnitInfo::command(UnitCommandType cmd, UnitInfo& targetUnit)
    {
        // Check if we need to wait a few frames before issuing a command due to stop frames
        auto frameSinceAttack = Broodwar->getFrameCount() - lastAttackFrame;
        auto cancelAttackRisk = frameSinceAttack <= minStopFrame - Broodwar->getLatencyFrames();
        commandPosition = targetUnit.getPosition();
        commandType = cmd;

        if (cancelAttackRisk && !getType().isBuilding() && !isLightAir())
            return false;

        // Check if this is a new command
        const auto newCommand = [&]() {
            auto newCommandTarget = (unit()->getLastCommand().getType() != cmd || unit()->getLastCommand().getTarget() != targetUnit.unit());
            return newCommandTarget;
        };

        // Add action
        Actions::addAction(unit(), targetUnit.getPosition(), getType(), PlayerState::Self);

        // If this is a new order or new command than what we're requesting, we can issue it
        if (newCommand() || cmd == UnitCommandTypes::Right_Click_Unit) {
            if (cmd == UnitCommandTypes::Attack_Unit)
                unit()->attack(targetUnit.unit());
            else if (cmd == UnitCommandTypes::Right_Click_Unit)
                unit()->rightClick(targetUnit.unit());
            return true;
        }
        return false;
    }

    bool UnitInfo::canStartAttack()
    {
        if (!hasTarget()
            || (getGroundDamage() == 0 && getAirDamage() == 0)
            || isSpellcaster()
            || (getType() == UnitTypes::Zerg_Lurker && !isBurrowed()))
            return false;

        // Special Case: Carriers
        if (getType() == UnitTypes::Protoss_Carrier) {
            auto leashRange = 320;
            if (getPosition().getDistance(getTarget().getPosition()) >= leashRange)
                return true;
            for (auto &interceptor : unit()->getInterceptors()) {
                if (interceptor->getOrder() != Orders::InterceptorAttack && interceptor->getShields() == interceptor->getType().maxShields() && interceptor->getHitPoints() == interceptor->getType().maxHitPoints() && interceptor->isCompleted())
                    return true;
            }
            return false;
        }

        auto weaponCooldown = getType() == Protoss_Reaver ? 60 : (getTarget().getType().isFlyer() ? getType().airWeapon().damageCooldown() : getType().groundWeapon().damageCooldown());
        auto cooldown = lastAttackFrame + (weaponCooldown / 2) - Broodwar->getFrameCount() + Broodwar->getLatencyFrames();
        auto range = (getTarget().getType().isFlyer() ? getAirRange() : getGroundRange());
        auto boxDistance = Util::boxDistance(getType(), getPosition(), getTarget().getType(), getTarget().getPosition()) + (currentSpeed * Broodwar->getLatencyFrames());
        auto cooldownReady = getSpeed() > 0.0 ? max(0, cooldown) <= max(0.0, boxDistance - range) / (hasTransport() ? getTransport().getSpeed() : getSpeed()) : cooldown <= 0.0;
        return cooldownReady;
    }

    bool UnitInfo::canStartGather()
    {
        return false;
    }

    bool UnitInfo::canStartCast(TechType tech, Position here)
    {
        if (!getPlayer()->hasResearched(tech)
            || Actions::overlapsActions(unit(), here, tech, PlayerState::Self, int(Util::getCastRadius(tech))))
            return false;

        auto energyNeeded = tech.energyCost() - energy;
        auto framesToEnergize = 17.856 * energyNeeded;
        auto spellReady = energy >= tech.energyCost();
        auto spellWillBeReady = framesToEnergize <= getEngDist() / (hasTransport() ? getTransport().getSpeed() : getSpeed());

        if (!spellReady && !spellWillBeReady)
            return false;

        if (isSpellcaster() && getEngDist() >= 64.0)
            return true;

        auto ground = Grids::getEGroundCluster(here);
        auto air = Grids::getEAirCluster(here);

        if (ground + air >= Util::getCastLimit(tech) || (getType() == Protoss_High_Templar && hasTarget() && getTarget().isHidden()))
            return true;
        return false;
    }

    bool UnitInfo::canAttackGround()
    {
        return getGroundDamage() > 0.0
            || getType() == Protoss_High_Templar
            || getType() == Protoss_Dark_Archon
            || getType() == Protoss_Carrier
            || getType() == Terran_Medic
            || getType() == Terran_Science_Vessel
            || getType() == Zerg_Defiler
            || getType() == Zerg_Queen;
    }

    bool UnitInfo::canAttackAir()
    {
        return getAirDamage() > 0.0
            || getType() == Protoss_High_Templar
            || getType() == Protoss_Dark_Archon
            || getType() == Protoss_Carrier
            || getType() == Terran_Science_Vessel
            || getType() == Zerg_Defiler
            || getType() == Zerg_Queen;
    }

    bool UnitInfo::isHealthy()
    {
        if (type.isBuilding())
            return percentHealth > 0.5;

        return (type.maxShields() > 0 && percentShield > LOW_SHIELD_PERCENT_LIMIT)
            || (type.isMechanical() && percentHealth > LOW_MECH_PERCENT_LIMIT)
            || (type.getRace() == BWAPI::Races::Zerg && percentHealth > LOW_BIO_PERCENT_LIMIT)
            || (type == BWAPI::UnitTypes::Zerg_Zergling && Players::ZvP() && health > 16)
            || (type == BWAPI::UnitTypes::Zerg_Zergling && Players::ZvT() && health > 16);
    }

    bool UnitInfo::isRequestingPickup()
    {
        if (!hasTarget()
            || !hasTransport()
            || unit()->isLoaded())
            return false;

        // Check if we Unit being attacked by multiple bullets
        auto bulletCount = 0;
        for (auto &bullet : BWAPI::Broodwar->getBullets()) {
            if (bullet && bullet->exists() && bullet->getPlayer() != BWAPI::Broodwar->self() && bullet->getTarget() == bwUnit)
                bulletCount++;
        }

        auto range = getTarget().getType().isFlyer() ? getAirRange() : getGroundRange();
        auto cargoReady = getType() == BWAPI::UnitTypes::Protoss_High_Templar ? canStartCast(BWAPI::TechTypes::Psionic_Storm, getTarget().getPosition()) : canStartAttack();
        auto threat = Grids::getEGroundThreat(getWalkPosition()) > 0.0;

        return getLocalState() == LocalState::Retreat || getEngDist() > range + 32.0 || (!cargoReady && threat) || bulletCount >= 4 || isTargetedBySplash();
    }

    bool UnitInfo::isWithinReach(UnitInfo& otherUnit)
    {
        auto boxDistance = Util::boxDistance(getType(), getPosition(), otherUnit.getType(), otherUnit.getPosition());
        return otherUnit.getType().isFlyer() ? airReach >= boxDistance : groundReach >= boxDistance;
    }

    bool UnitInfo::isWithinRange(UnitInfo& otherUnit)
    {
        auto boxDistance = Util::boxDistance(getType(), getPosition(), otherUnit.getType(), otherUnit.getPosition());
        auto range = otherUnit.getType().isFlyer() ? getAirRange() : getGroundRange();
        auto ff = (!isHovering() && !isFlying()) ? 0.0 : -8.0;
        if (isSuicidal())
            return getPosition().getDistance(otherUnit.getPosition()) <= 16.0;
        return (range + ff) >= boxDistance;
    }

    bool UnitInfo::isWithinAngle(UnitInfo& otherUnit)
    {
        if (!isFlying())
            return true;

        auto desiredAngle = BWEB::Map::getAngle(make_pair(getPosition(), otherUnit.getPosition()));
        auto currentAngle = unit()->getAngle();
        return abs(desiredAngle - currentAngle) < 0.9;
    }

    bool UnitInfo::isWithinBuildRange()
    {
        if (!getBuildPosition().isValid())
            return false;

        const auto center = Position(getBuildPosition()) + Position(getBuildType().tileWidth() * 16, getBuildType().tileHeight() * 16);
        const auto close = Util::boxDistance(getType(), getPosition(), getBuildType(), center) <= 32;

        return close;
    }

    bool UnitInfo::isWithinGatherRange()
    {
        if (!hasResource())
            return false;
        auto sameArea = mapBWEM.GetArea(getResource().getTilePosition()) == mapBWEM.GetArea(getTilePosition());
        auto distResource = getPosition().getDistance(getResource().getPosition());
        auto distStation = getPosition().getDistance(getResource().getStation()->getBase()->Center());
        return (sameArea && distResource < 256.0) || distResource < 128.0 || distStation < 128.0;
    }

    bool UnitInfo::localEngage()
    {
        auto oneShotTimer = Time(12, 00);
        return ((!isFlying() && getTarget().isSiegeTank() && getType() != Zerg_Lurker && ((isWithinRange(getTarget()) && getGroundRange() > 32.0) || (isWithinReach(getTarget()) && getGroundRange() <= 32.0)))
            || (getType() == Protoss_Reaver && !unit()->isLoaded() && isWithinRange(getTarget()))
            || (getTarget().getType() == Terran_Vulture_Spider_Mine && !getTarget().isBurrowed())
            || (getType() == Zerg_Mutalisk && Util::getTime() < oneShotTimer && hasTarget() && canOneShot(getTarget()))
            || (hasTransport() && !unit()->isLoaded() && getType() == Protoss_High_Templar && canStartCast(TechTypes::Psionic_Storm, getTarget().getPosition()) && isWithinRange(getTarget()))
            || (hasTransport() && !unit()->isLoaded() && getType() == Protoss_Reaver && canStartAttack()) && isWithinRange(getTarget()));
    }

    bool UnitInfo::localRetreat()
    {
        auto targetedByHidden = false;
        for (auto &t : getTargetedBy()) {
            if (auto targeter = t.lock()) {
                if (targeter->isHidden())
                    targetedByHidden = true;
            }
        }
        if (targetedByHidden)
            return true;

        return (getType() == Protoss_Zealot && hasTarget() && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Leg_Enhancements) == 0 && getTarget().getType() == Terran_Vulture)                 // ...unit is a slow Zealot attacking a Vulture
            || (getType() == Protoss_Corsair && hasTarget() && getTarget().isSuicidal() && com(Protoss_Corsair) < 6)                                                                             // ...unit is a Corsair attacking Scourge with less than 6 completed Corsairs
            || (getType() == Terran_Medic && getEnergy() <= TechTypes::Healing.energyCost())                                                                                                     // ...unit is a Medic with no energy        
            || (getType() == Terran_SCV && Broodwar->getFrameCount() > 12000)                                                                                                                    // ...unit is an SCV outside of early game
            || (isLightAir() && hasTarget() && getType().maxShields() > 0 && getTarget().getType() == Zerg_Overlord && Grids::getEAirThreat(getEngagePosition()) * 5.0 > (double)getShields())   // ...unit is a low shield light air attacking a Overlord under threat greater than our shields
            || (getType() == Zerg_Zergling && Players::ZvZ() && getTargetedBy().size() >= 3 && !Terrain::isInAllyTerritory(getTilePosition()) && Util::getTime() < Time(3, 15))
            || (getType() == Zerg_Zergling && Players::ZvP() && !getTargetedBy().empty() && getHealth() < 20 && Util::getTime() < Time(4, 00) && !isWithinRange(getTarget()))
            || (getType() == Zerg_Zergling && hasTarget() && !isHealthy() && getTarget().getType().isWorker())
            || (hasTarget() && getTarget().isSuicidal() && getTarget().unit()->getOrderTargetPosition().getDistance(getPosition()) < 64.0)
            || unit()->isIrradiated()
            ;
    }

    bool UnitInfo::canOneShot(UnitInfo& target) {
        auto stimOffset = 0.0f;
        if (target.isStimmed() && (!target.isWithinRange(*this) || (!canStartAttack() && isWithinRange(target))))
            stimOffset = 2.0f;

        // Check if this unit could load into a bunker
        if (Util::getTime() < Time(10, 00) && (target.getType() == Terran_Marine || target.getType() == Terran_SCV || target.getType() == Terran_Firebat)) {
            if (Players::getVisibleCount(PlayerState::Enemy, Terran_Bunker) > 0) {
                auto closestBunker = Util::getClosestUnit(target.getPosition(), PlayerState::Enemy, [&](auto &b) {
                    return b.getType() == Terran_Bunker;
                });
                if (closestBunker && closestBunker->getPosition().getDistance(target.getPosition()) < 200.0) {
                    return false;
                }
            }
        }

        // One shotting workers
        if (target.getType().isWorker()) {
            if (target.getType() == Protoss_Probe || target.getType() == Zerg_Drone)
                return Grids::getAAirCluster(getPosition()) >= 5.0f;
            if (target.getType() == Terran_SCV)
                return Grids::getAAirCluster(getPosition()) >= 7.0f;
            return false;
        }

        // One shot threshold for individual units
        if (target.getType() == Terran_Marine)
            return Grids::getAAirCluster(getPosition()) >= (5.0f + stimOffset);
        if (target.getType() == Terran_Firebat)
            return Grids::getAAirCluster(getPosition()) >= 7.0f;
        if (target.getType() == Terran_Medic)
            return Grids::getAAirCluster(getPosition()) >= 8.0f;
        if (target.getType() == Terran_Vulture)
            return Grids::getAAirCluster(getPosition()) >= 9.0f;
        if (target.getType() == Protoss_Dragoon)
            return Grids::getAAirCluster(getPosition()) >= 24.0f;
        if (target.getType() == Terran_Goliath) {
            if (target.isWithinRange(*this))
                return Grids::getAAirCluster(getPosition()) >= 16.0f;
            else
                return Grids::getAAirCluster(getPosition()) >= 18.0f;
        }

        // Check if it's a low life unit - doesn't account for armor 
        if (target.unit()->exists() && (target.getHealth() + target.getShields()) < Grids::getAAirCluster(getPosition()) * 7.0f)
            return true;
        return false;
    }

    bool UnitInfo::canTwoShot(UnitInfo& target) {
        if (target.getType() == Terran_Goliath || target.getType() == Protoss_Scout)
            return Grids::getAAirCluster(getPosition()) >= 8.0f;
        if (target.getType() == Protoss_Zealot || target.isSiegeTank())
            return Grids::getAAirCluster(getPosition()) >= 10.0f;
        if (target.getType() == Protoss_Dragoon || target.getType() == Protoss_Corsair || target.getType() == Terran_Missile_Turret)
            return Grids::getAAirCluster(getPosition()) >= 12.0f;
        return false;
    }

    bool UnitInfo::globalEngage()
    {
        const auto nearEnemyStation = [&]() {
            const auto closestEnemyStation = Stations::getClosestStationGround(PlayerState::Enemy, getPosition());
            return (closestEnemyStation && getPosition().getDistance(closestEnemyStation->getBase()->Center()) < 400.0);
        };

        const auto nearProxyStructure = [&]() {
            const auto closestProxy = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
                return u.getType().isBuilding() && u.isProxy();
            });
            return closestProxy && closestProxy->getPosition().getDistance(getPosition()) < 160.0;
        };

        const auto nearEnemyDefenseStructure = [&]() {
            const auto closestDefense = Util::getClosestUnit(getPosition(), PlayerState::Enemy, [&](auto &u) {
                return u.getType().isBuilding() && ((u.canAttackGround() && isFlying()) || (u.canAttackAir() && !isFlying()));
            });
            return closestDefense && closestDefense->getPosition().getDistance(getPosition()) < 256.0;
        };

        auto lingVsVulture = hasTarget() && getTarget().getType() == Terran_Vulture && getType() == Zerg_Zergling && getTarget().getPosition().getDistance(getPosition()) > 160.0;

        return (getTarget().isThreatening() && !isLightAir() && !lingVsVulture && !getTarget().isHidden() && (Util::getTime() < Time(10, 00) || getSimState() == SimState::Win || Players::ZvZ()))                                                                          // ...target is threatening                    
            || (!getType().isWorker() && (getGroundRange() > getTarget().getGroundRange() || getTarget().getType().isWorker()) && Terrain::isInAllyTerritory(getTarget().getTilePosition()) && !getTarget().isHidden())                 // ...unit can get free hits in our territory
            || (isSuicidal() && hasTarget() && (getTarget().isWithinRange(*this) || Terrain::isInAllyTerritory(getTarget().getTilePosition()) || getTarget().isThreatening() || getTarget().getPosition().getDistance(getGoal()) < 160.0) && !nearEnemyDefenseStructure())
            || ((isHidden() || getType() == Zerg_Lurker) && !Actions::overlapsDetection(unit(), getEngagePosition(), PlayerState::Enemy))
            || (!isFlying() && Actions::overlapsActions(unit(), getEngagePosition(), TechTypes::Dark_Swarm, PlayerState::Neutral, 96))
            || (!isFlying() && (getGroundRange() < 32.0 || getType() == Zerg_Lurker) && Terrain::isInEnemyTerritory(getTilePosition()) && (Util::getTime() > Time(8, 00) || BuildOrder::isProxy()) && nearEnemyStation() && !Players::ZvZ())
            || (getType() == Zerg_Lurker && BuildOrder::isProxy() && nearProxyStructure())
            || (!isFlying() && Actions::overlapsActions(unit(), getPosition(), TechTypes::Dark_Swarm, PlayerState::Neutral, 96))
            || isTargetedBySplash();
    }

    bool UnitInfo::globalRetreat()
    {
        return (Grids::getESplash(getWalkPosition()) > 0 && !isTargetedBySplash())                                                                                                                  // ...unit is within splash radius of a Spider Mine or Scarab
            || (hasTarget() && getTarget().isHidden() && getPosition().getDistance(getTarget().getPosition()) <= (getType().isFlyer() ? getTarget().getAirReach() : getTarget().getGroundReach()))  // ...target is hidden and Unit is within target reach
            || (getGlobalState() == GlobalState::Retreat && !Terrain::isInAllyTerritory(getTilePosition()))                                                                                         // ...global state is retreating
            || (getType() == Zerg_Mutalisk && hasTarget() && !getTarget().isThreatening() && !isWithinRange(getTarget()) && !getTarget().isWithinRange(*this) && getHealth() <= 50 && Util::getTime() > Time(8, 00))                // ...unit is a low HP Mutalisk attacking a target under air threat    
            || (getType() == Zerg_Hydralisk && BuildOrder::getCompositionPercentage(Zerg_Lurker) >= 1.00)
            || (getType() == Zerg_Hydralisk && !getGoal().isValid() && (!Players::getPlayerInfo(Broodwar->self())->hasUpgrade(UpgradeTypes::Grooved_Spines) || !Players::getPlayerInfo(Broodwar->self())->hasUpgrade(UpgradeTypes::Muscular_Augments)))
            || (isNearSuicide())
            ;
    }

    bool UnitInfo::attemptingRunby()
    {
        auto time = Time(3, 15);
        if (Broodwar->getStartLocations().size() >= 3)
            time = Time(3, 30);
        if (Broodwar->getStartLocations().size() >= 4)
            time = Time(3, 45);

        if (Strategy::enemyProxy() && Strategy::getEnemyBuild() == "2Gate" && timeCompletesWhen() < time)
            return true;
        return false;
    }

    bool UnitInfo::attemptingSurround()
    {
        if (!hasTarget()
            || !getSurroundPosition().isValid()
            || getGroundRange() > 32.0
            || getAirRange() > 32.0
            || isSuicidal()
            || Util::getTime() < Time(3, 30)
            || getTarget().getType() == getType()
            || getTarget().getType().isBuilding()
            || getTarget().getType().isWorker())
            return false;

        auto closestChoke = Util::getClosestChokepoint(getTarget().getPosition());
        if (getTarget().getPosition().getDistance(Position(closestChoke->Center())) < 64.0 && vis(getType()) >= 10 && closestChoke->Width() < 64.0)
            return false;

        double howClose = 24.0;
        for (auto &t : getTarget().getTargetedBy()) {
            if (auto targeter = t.lock()) {
                if (targeter->getPosition().getDistance(getSurroundPosition()) < getPosition().getDistance(getSurroundPosition()))
                    howClose -= 16.0;
            }
        }
        if (getPosition().getDistance(getSurroundPosition()) + howClose > getTarget().getPosition().getDistance(getSurroundPosition()))
            return true;
        return false;
    }

    bool UnitInfo::attemptingHarass()
    {
        if (Players::ZvZ() && vis(Zerg_Zergling) + 4 * com(Zerg_Sunken_Colony) < Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling))
            return false;
        if (Players::ZvZ() && Terrain::getEnemyMain() && Terrain::getEnemyNatural() && Stations::getAirDefenseCount(Terrain::getEnemyMain()) > 0 && Stations::getAirDefenseCount(Terrain::getEnemyNatural()) > 0 && int(Stations::getMyStations().size()) < 2)
            return false;
        if (Players::ZvZ() && vis(Zerg_Mutalisk) <= Players::getVisibleCount(PlayerState::Enemy, Zerg_Mutalisk))
            return false;
        return isLightAir() && Terrain::getHarassPosition().isValid();
    }
}