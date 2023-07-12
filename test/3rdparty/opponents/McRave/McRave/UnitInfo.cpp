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

        map<int, int> commandsPerFrame;
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

    void UnitInfo::updateHistory()
    {
        if (unit()->exists()) {
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
    }

    void UnitInfo::verifyPaths()
    {
        if (lastTile != unit()->getTilePosition()) {
            BWEB::Path emptyPath;
            destinationPath = emptyPath;
        }
    }

    void UnitInfo::update()
    {
        updateStatistics();
        verifyPaths();
        updateHistory();
    }

    void UnitInfo::updateStatistics()
    {
        auto t = unit()->getType();
        auto p = unit()->getPlayer();

        if (unit()->exists()) {

            lastPos                     = position;
            lastTile                    = tilePosition;
            lastWalk                    = walkPosition;

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
            invincible                  = unit()->isInvincible() || unit()->isStasised();

            // Points        
            position                    = unit()->getPosition();
            tilePosition                = t.isBuilding() ? unit()->getTilePosition() : TilePosition(unit()->getPosition());
            walkPosition                = calcWalkPosition(this);
            destination                 = Positions::Invalid;
            retreat                     = Positions::Invalid;
            surroundPosition            = Positions::Invalid;
            interceptPosition           = Positions::Invalid;

            // Flags
            flying                      = unit()->isFlying() || getType().isFlyer() || unit()->getOrder() == Orders::LiftingOff || unit()->getOrder() == Orders::BuildingLiftOff;
            movedFlag                   = false;

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
            engageRadius                = Math::simRadius(*this) + 96.0;
            retreatRadius               = Math::simRadius(*this);

            // States
            lState                      = LocalState::None;
            gState                      = GlobalState::None;
            tState                      = TransportState::None;

            // Attack Frame
            if ((getType() == Protoss_Reaver && unit()->getGroundWeaponCooldown() >= 59)
                || (getType() != Protoss_Reaver && canAttackGround() && unit()->getGroundWeaponCooldown() >= type.groundWeapon().damageCooldown() - 1)
                || (getType() != Protoss_Reaver && canAttackAir() && unit()->getAirWeaponCooldown() >= type.airWeapon().damageCooldown() - 1))
                lastAttackFrame         = Broodwar->getFrameCount();

            // Frames
            remainingTrainFrame         = max(0, remainingTrainFrame - 1);
            lastRepairFrame             = (unit()->isRepairing() || unit()->isBeingHealed()) ? Broodwar->getFrameCount() : lastRepairFrame;
            minStopFrame                = Math::stopAnimationFrames(t);
            lastStimFrame               = unit()->isStimmed() ? Broodwar->getFrameCount() : lastStimFrame;
            lastVisibleFrame            = Broodwar->getFrameCount();
            arriveFrame                 = isFlying() ? Broodwar->getFrameCount() + int(position.getDistance(BWEB::Map::getMainPosition()) / speed) :
                Broodwar->getFrameCount() + int(BWEB::Map::getGroundDistance(position, BWEB::Map::getMainPosition()) / speed);

            checkHidden();
            checkStuck();
            checkProxy();
            checkCompletion();
            checkThreatening();
        }

        // Create a list of units that are in range of this unit
        unitsInRangeOfThis.clear();
        if (getPlayer() == Broodwar->self()) {
            for (auto &u : Units::getUnits(PlayerState::Enemy)) {
                auto &unit = *u;
                if (((this->isFlying() && unit.canAttackAir()) || (!this->isFlying() && unit.canAttackGround())) && unit.isWithinReach(*this)) {
                    unitsInRangeOfThis.push_back(unit.weak_from_this());
                }
            }
        }

        // Check if this unit is close to a splash unit
        if (getPlayer() == Broodwar->self()) {
            nearSplash = false;
            auto closestSplasher = Util::getClosestUnit(position, PlayerState::Enemy, [&](auto &u) {
                return u->isSplasher() && ((!this->isFlying() && u->canAttackGround()) || (this->isFlying() && u->canAttackAir()));
            });

            if (closestSplasher && closestSplasher->isWithinReach(*this))
                nearSplash = true;

            targetedBySplash = any_of(unitsTargetingThis.begin(), unitsTargetingThis.end(), [&](auto &t) {
                return !t.expired() && t.lock()->isSplasher();
            });
        }

        // Check if this unit is close to / targeted by a suicidal unit
        if (getPlayer() == Broodwar->self()) {
            nearSuicide = false;
            auto closestSuicide = Util::getClosestUnit(position, PlayerState::Enemy, [&](auto &u) {
                return u->isSuicidal() && ((!this->isFlying() && u->canAttackGround()) || (this->isFlying() && u->canAttackAir()));
            });

            if (closestSuicide && closestSuicide->unit()->getOrderTargetPosition()) {
                auto distToSelf = Util::boxDistance(getType(), getPosition(), closestSuicide->getType(), closestSuicide->getPosition());
                if (distToSelf < 80.0)
                    nearSuicide = true;
            }

            targetedBySuicide = any_of(unitsTargetingThis.begin(), unitsTargetingThis.end(), [&](auto &t) {
                return !t.expired() && t.lock()->isSuicidal();
            });
        }

        // Check if this unit is close to a hidden unit
        if (getPlayer() == Broodwar->self()) {
            nearHidden = false;
            auto closestHidden = Util::getClosestUnit(position, PlayerState::Enemy, [&](auto &u) {
                return u->isHidden() && u->unit()->exists() && ((!this->isFlying() && u->canAttackGround()) || (this->isFlying() && u->canAttackAir()));
            });

            if (closestHidden && closestHidden->isWithinReach(*this))
                nearHidden = true;

            targetedByHidden = any_of(unitsTargetingThis.begin(), unitsTargetingThis.end(), [&](auto &t) {
                return !t.expired() && t.lock()->isHidden();
            });
        }

        target.reset();
        unitsTargetingThis.clear();
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
                lastMoveFrame = Broodwar->getFrameCount();
        }

        // Check if a unit hasn't moved in a while but is trying to
        if (!bwUnit->isAttackFrame() && (getPlayer() != Broodwar->self() || lastPos != getPosition() || !unit()->isMoving() || unit()->getLastCommand().getType() == UnitCommandTypes::Stop))
            lastMoveFrame = Broodwar->getFrameCount();
        else if (isStuck())
            lastStuckFrame = Broodwar->getFrameCount();
    }

    void UnitInfo::checkHidden()
    {
        // Burrowed check for non spider mine type units or units we can see using the order for burrowing
        burrowed = (getType() != Terran_Vulture_Spider_Mine && unit()->isBurrowed());

        // If this is a spider mine and doesn't have a target, then it is an inactive mine and unable to attack
        if (getType() == Terran_Vulture_Spider_Mine && (!unit()->exists() || (!hasTarget() && unit()->getSecondaryOrder() == Orders::Cloak))) {
            burrowed = true;
            groundReach = getGroundRange();
        }
        if (getType() == UnitTypes::Spell_Scanner_Sweep) {
            hidden = true;
            return;
        }

        // A unit is considered hidden if it is burrowed or cloaked and not under detection
        hidden = (burrowed || bwUnit->isCloaked())
            && (player->isEnemy(BWAPI::Broodwar->self()) ? !Actions::overlapsDetection(bwUnit, position, PlayerState::Self) : !Actions::overlapsDetection(bwUnit, position, PlayerState::Enemy));
    }

    void UnitInfo::checkThreatening()
    {
        if (!getPlayer()->isEnemy(Broodwar->self())
            || getType() == Zerg_Overlord)
            return;

        // Determine how close it is to strategic locations
        const auto choke = Terrain::isDefendNatural() ? BWEB::Map::getNaturalChoke() : BWEB::Map::getMainChoke();
        const auto area = Terrain::isDefendNatural() ? BWEB::Map::getNaturalArea() : BWEB::Map::getMainArea();
        const auto closestGeo = BWEB::Map::getClosestChokeTile(choke, getPosition());
        const auto closestStation = Stations::getClosestStationAir(getPosition(), PlayerState::Self);
        const auto rangeCheck = max({ getAirRange() + 32.0, getGroundRange() + 32.0, 64.0 });
        const auto proximityCheck = max(rangeCheck, 200.0);
        auto threateningThisFrame = false;

        // If the unit is close to stations, defenses or resources owned by us
        const auto atHome = Terrain::inTerritory(PlayerState::Self, getPosition()) && closestStation && closestStation->getBase()->Center().getDistance(getPosition()) < 640.0;
        const auto atChoke = getPosition().getDistance(closestGeo) <= rangeCheck;

        // If the unit attacked defenders, workers or buildings
        const auto attackedDefender = hasAttackedRecently() && hasTarget() && Terrain::inTerritory(PlayerState::Self, getTarget().lock()->getPosition()) && getTarget().lock()->getRole() == Role::Defender;
        const auto attackedWorkers = hasAttackedRecently() && hasTarget() && Terrain::inTerritory(PlayerState::Self, getTarget().lock()->getPosition()) && getTarget().lock()->getRole() == Role::Worker;
        const auto attackedBuildings = hasAttackedRecently() && hasTarget() && getTarget().lock()->getType().isBuilding();

        auto closestDefender = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
            return u->getRole() == Role::Defender && u->canAttackGround() && (u->isCompleted() || isWithinRange(*u) || Util::getTime() < Time(4, 00));
        });
        auto closestBuilder = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
            return u->getRole() == Role::Worker && u->getBuildPosition().isValid() && u->getBuildType().isValid();
        });
        auto fragileBuilding = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
            return !u->isHealthy() && u->getType().isBuilding() && (u->isCompleted() || isWithinRange(*u)) && Terrain::inTerritory(PlayerState::Self, u->getPosition());
        });

        // Check if our resources are in danger
        auto nearResources = [&]() {
            return closestStation && closestStation == Terrain::getMyMain() && closestStation->getResourceCentroid().getDistance(getPosition()) < proximityCheck && atHome;
        };

        // Check if our defenses can hit or be hit
        auto nearDefenders = [&]() {
            return (closestDefender && closestDefender->isWithinRange(*this)) || Zones::getZone(getPosition()) == ZoneType::Defend;
        };

        // Checks if it can damage an already damaged building
        auto nearFragileBuilding = [&]() {
            return fragileBuilding && canAttackGround() && Util::boxDistance(fragileBuilding->getType(), fragileBuilding->getPosition(), getType(), getPosition()) < proximityCheck;
        };

        // Check if any builders can be hit or blocked
        auto nearBuildPosition = [&]() {
            if (atHome && !isFlying() && Util::getTime() < Time(5, 00)) {
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
            if (BWEB::Map::getMainChoke() && getPosition().getDistance(Position(BWEB::Map::getMainChoke()->Center())) < 64.0 && int(Stations::getStations(PlayerState::Self).size()) >= 2)
                return true;

            if (Zones::getZone(getPosition()) == ZoneType::Engage) {
                if (closestDefender && closestDefender->isCompleted()) {
                    if (mapBWEM.GetArea(closestDefender->getTilePosition()) == mapBWEM.GetArea(getTilePosition()))
                        return false;
                }
                if (hasTarget() && Terrain::inTerritory(PlayerState::Self, getTarget().lock()->getPosition()) && (isWithinRange(*getTarget().lock()) || Units::getImmThreat() > 0.0))
                    return true;
            }

            // Fix for Andromeda like maps
            if (Terrain::inTerritory(PlayerState::Self, getPosition()) && mapBWEM.GetArea(getTilePosition()) != BWEB::Map::getNaturalArea() && int(Stations::getStations(PlayerState::Self).size()) >= 2)
                return true;

            return getTilePosition().isValid() && mapBWEM.GetArea(getTilePosition()) == BWEB::Map::getMainArea() && (int(Stations::getStations(PlayerState::Self).size()) >= 2 || Combat::defendChoke());
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
            threateningThisFrame = /*attackedDefender
            || */attackedWorkers
            || nearResources()
            || nearFragileBuilding()
            || nearBuildPosition()
            || nearDefenders()
            || nearArmy();

        // Specific case: Marine near a proxy bunker
        if (getType() == Terran_Marine && Util::getTime() < Time(5, 00)) {
            auto closestThreateningBunker = Util::getClosestUnit(getPosition(), PlayerState::Enemy, [&](auto &u) {
                return u->isThreatening() && u->getType() == Terran_Bunker;
            });
            if (closestThreateningBunker && closestThreateningBunker->getPosition().getDistance(getPosition()) < 160.0)
                threateningThisFrame = true;
        }

        // Determine if this unit is threatening
        if (threateningThisFrame)
            threateningFrames++;
        else
            threateningFrames = 0;

        if (threateningFrames > 24)
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

        // Check if an enemy building is a proxy
        if (player->isEnemy(Broodwar->self())) {
            if (getType() == Terran_Barracks || getType() == Terran_Bunker || getType() == Protoss_Gateway || getType() == Protoss_Photon_Cannon || getType() == Protoss_Pylon || getType() == Protoss_Forge) {
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

    void UnitInfo::checkCompletion()
    {
        int extra = 0;
        if (type.getRace() == BWAPI::Races::Terran)
            extra = 2;
        else if (type.getRace() == BWAPI::Races::Protoss)
            extra = 72;
        else
            extra = 9;

        // Calculate completion based on build time
        if (!bwUnit->isCompleted()) {
            auto ratio = (double(health) - (0.1 * double(type.maxHitPoints()))) / (0.9 * double(type.maxHitPoints()));
            completeFrame = Broodwar->getFrameCount() + int(std::round((1.0 - ratio) * double(type.buildTime()))) + extra;
            startedFrame = Broodwar->getFrameCount() - int(std::round((ratio) * double(type.buildTime())));
        }

        // Set completion based on seeing it already completed and this is the first time visible
        else if (startedFrame == -999 && completeFrame == -999) {
            completeFrame = Broodwar->getFrameCount();
            startedFrame = Broodwar->getFrameCount();
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

        // Check if we should overshoot for halting distance
        if (cmd == UnitCommandTypes::Move && !getBuildPosition().isValid() && (getType().isFlyer() || isHovering() || getType() == Protoss_High_Templar || attemptingSurround())) {
            auto distance = int(getPosition().getDistance(here));
            auto haltDistance = max({ distance, 32, getType().haltDistance() / 256 });
            auto overShootHere = here;

            if (distance > 0) {
                overShootHere = getPosition() - ((getPosition() - here) * int(round(haltDistance / distance)));
                overShootHere = Util::clipLine(getPosition(), overShootHere);
            }
            if (getType().isFlyer() || (isHovering() && Util::findWalkable(*this, overShootHere)))
                here = overShootHere;
        }

        const auto newCommand = [&]() {
            auto newCommandPosition = unit()->getLastCommand().getTargetPosition().getDistance(here) > 24;
            auto newCommandType = unit()->getLastCommand().getType() != cmd;
            auto newCommandFrame = Broodwar->getFrameCount() - unit()->getLastCommandFrame() - Broodwar->getLatencyFrames() > 24;
            return newCommandPosition || newCommandType || newCommandFrame;
        };

        // Add action and grid movement
        if ((cmd == UnitCommandTypes::Move || cmd == UnitCommandTypes::Right_Click_Position) && getPosition().getDistance(here) < 160.0) {
            Actions::addAction(unit(), here, getType(), PlayerState::Self);
        }

        // If this is a new order or new command than what we're requesting, we can issue it
        if (newCommand() && commandsPerFrame[Broodwar->getFrameCount()] < 128) {
            if (cmd == UnitCommandTypes::Move)
                unit()->move(here);
            if (cmd == UnitCommandTypes::Right_Click_Position)
                unit()->rightClick(here);
            if (cmd == UnitCommandTypes::Stop)
                unit()->stop();
            commandsPerFrame[Broodwar->getFrameCount()]++;
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
        if (newCommand() && commandsPerFrame[Broodwar->getFrameCount()] < 128) {
            if (cmd == UnitCommandTypes::Attack_Unit)
                unit()->attack(targetUnit.unit());
            else if (cmd == UnitCommandTypes::Right_Click_Unit)
                unit()->rightClick(targetUnit.unit());
            commandsPerFrame[Broodwar->getFrameCount()]++;
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
        auto unitTarget = getTarget().lock();

        if (isSuicidal())
            return true;

        // Special Case: Carriers
        if (getType() == UnitTypes::Protoss_Carrier) {
            auto leashRange = 320;
            if (getPosition().getDistance(unitTarget->getPosition()) >= leashRange)
                return true;
            for (auto &interceptor : unit()->getInterceptors()) {
                if (interceptor->getOrder() != Orders::InterceptorAttack && interceptor->getShields() == interceptor->getType().maxShields() && interceptor->getHitPoints() == interceptor->getType().maxHitPoints() && interceptor->isCompleted())
                    return true;
            }
            return false;
        }

        // Special Case: Reavers - Shuttles reset the cooldown of their attacks to 30 frames not 60 frames
        if (getType() == Protoss_Reaver && hasTransport() && unit()->isLoaded()) {
            auto dist = Util::boxDistance(getType(), getPosition(), unitTarget->getType(), unitTarget->getPosition());
            return (dist < getGroundRange());
        }

        auto weaponCooldown = getType() == Protoss_Reaver ? 60 : (unitTarget->getType().isFlyer() ? getType().airWeapon().damageCooldown() : getType().groundWeapon().damageCooldown());
        auto cooldown = lastAttackFrame + (weaponCooldown / 2) - Broodwar->getFrameCount() + Broodwar->getLatencyFrames();
        auto speed = hasTransport() ? getTransport().lock()->getSpeed() : getSpeed();
        auto range = (unitTarget->getType().isFlyer() ? getAirRange() : getGroundRange());
        auto boxDistance = Util::boxDistance(getType(), getPosition(), unitTarget->getType(), unitTarget->getPosition()) + (currentSpeed * Broodwar->getLatencyFrames());
        auto cooldownReady = getSpeed() > 0.0 ? max(0, cooldown) <= max(0.0, boxDistance - range) / speed : cooldown <= 0.0;
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
        auto spellWillBeReady = framesToEnergize <= getEngDist() / (hasTransport() ? getTransport().lock()->getSpeed() : getSpeed());

        if (!spellReady && !spellWillBeReady)
            return false;

        if (isSpellcaster() && getEngDist() >= 64.0)
            return true;

        auto ground = Grids::getEGroundCluster(here);
        auto air = Grids::getEAirCluster(here);

        if (ground + air >= Util::getCastLimit(tech) || (getType() == Protoss_High_Templar && hasTarget() && getTarget().lock()->isHidden()))
            return true;
        return false;
    }

    bool UnitInfo::canAttackGround()
    {
        // Can attack ground if weapon is capable or has an ability that can target ground
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
        // Can attack air if weapon is capable or has an ability that can target air
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

        return (type.maxShields() > 0 && percentShield > 0.5)
            || (type.isMechanical() && percentHealth > 0.25)
            || (type.getRace() == BWAPI::Races::Zerg && percentHealth > 0.25)
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

        auto unitTarget = getTarget().lock();
        auto range = unitTarget->isFlying() ? getAirRange() : getGroundRange();
        auto cargoPickup = getType() == BWAPI::UnitTypes::Protoss_High_Templar ? (!canStartCast(BWAPI::TechTypes::Psionic_Storm, unitTarget->getPosition()) || Grids::getEGroundThreat(getWalkPosition()) <= 0.1f) : !canStartAttack();

        return getLocalState() == LocalState::Retreat || getEngDist() > range + 32.0 || cargoPickup || bulletCount >= 4 || isTargetedBySuicide();
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
        auto resource = getResource().lock();
        auto sameArea = mapBWEM.GetArea(resource->getTilePosition()) == mapBWEM.GetArea(getTilePosition());
        auto distResource = getPosition().getDistance(resource->getPosition());
        auto distStation = getPosition().getDistance(resource->getStation()->getBase()->Center());
        return (sameArea && distResource < 256.0) || distResource < 128.0 || distStation < 128.0;
    }

    bool UnitInfo::localEngage()
    {
        if (!hasTarget())
            return false;
        auto unitTarget = getTarget().lock();

        auto countDefensesInRange = 0.0;
        if (getType() == Zerg_Mutalisk && hasTarget() && canOneShot(*unitTarget)) {
            return true;
            //for (auto &e : Units::getUnits(PlayerState::Enemy)) {
            //    if (e->canAttackAir() && e != unitTarget && (e->getPosition().getDistance(unitTarget->getPosition()) < e->getAirRange() + 32.0 || e->getPosition().getDistance(getPosition()) < e->getAirRange() + 32.0)) {
            //        countDefensesInRange += (e->getType().isBuilding() ? 1.0 : 0.25);
            //    }
            //}

            //if (canOneShot(*unitTarget)) {
            //    if ((countDefensesInRange < (Players::ZvP() ? 2.0 : 3.0) && Util::getTime() < Time(8, 00))
            //        || (countDefensesInRange < (Players::ZvP() ? 3.0 : 4.0) && Util::getTime() < Time(10, 00)))
            //        return true;
            //}
            //else {
            //    if (countDefensesInRange <= 0.0)
            //        return true;
            //}
        }

        return ((!isFlying() && unitTarget->isSiegeTank() && getType() != Zerg_Lurker && isWithinRange(*getTarget().lock()) && getGroundRange() > 32.0)
            || (getType() == Protoss_Reaver && !unit()->isLoaded() && isWithinRange(*unitTarget))
            || (unitTarget->getType() == Terran_Vulture_Spider_Mine && !unitTarget->isBurrowed())
            || (hasTransport() && !unit()->isLoaded() && getType() == Protoss_High_Templar && canStartCast(TechTypes::Psionic_Storm, unitTarget->getPosition()) && isWithinRange(*getTarget().lock()))
            || (hasTransport() && !unit()->isLoaded() && getType() == Protoss_Reaver && canStartAttack()) && isWithinRange(*getTarget().lock()));
    }

    bool UnitInfo::localRetreat()
    {
        auto unitTarget = getTarget().lock();

        return (getType() == Protoss_Zealot && hasTarget() && Broodwar->self()->getUpgradeLevel(UpgradeTypes::Leg_Enhancements) == 0 && unitTarget->getType() == Terran_Vulture)                 // ...unit is a slow Zealot attacking a Vulture
            || (getType() == Protoss_Corsair && hasTarget() && unitTarget->isSuicidal() && com(Protoss_Corsair) < 6)                                                                             // ...unit is a Corsair attacking Scourge with less than 6 completed Corsairs
            || (getType() == Terran_Medic && getEnergy() <= TechTypes::Healing.energyCost())                                                                                                     // ...unit is a Medic with no energy        
            || (getType() == Terran_SCV && Broodwar->getFrameCount() > 12000)                                                                                                                    // ...unit is an SCV outside of early game
            || (isLightAir() && hasTarget() && getType().maxShields() > 0 && unitTarget->getType() == Zerg_Overlord && Grids::getEAirThreat(getEngagePosition()) * 5.0 > (double)getShields())   // ...unit is a low shield light air attacking a Overlord under threat greater than our shields
            || (getType() == Zerg_Zergling && Players::ZvP() && !getUnitsTargetingThis().empty() && getHealth() < 20 && Util::getTime() < Time(4, 00))
            || (getType() == Zerg_Zergling && hasTarget() && !isHealthy() && unitTarget->getType().isWorker())
            || unit()->isIrradiated();
    }

    bool UnitInfo::canOneShot(UnitInfo& target) {
        auto stimOffset = 0.0f;
        if (target.isStimmed() && (!target.isWithinRange(*this) || (!canStartAttack() && isWithinRange(target))))
            stimOffset = 2.0f;

        // Check if this unit could load into a bunker
        if (Util::getTime() < Time(10, 00) && (target.getType() == Terran_Marine || target.getType() == Terran_SCV || target.getType() == Terran_Firebat)) {
            if (Players::getVisibleCount(PlayerState::Enemy, Terran_Bunker) > 0) {
                auto closestBunker = Util::getClosestUnit(target.getPosition(), PlayerState::Enemy, [&](auto &b) {
                    return b->getType() == Terran_Bunker;
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
        if (!hasTarget())
            return false;
        auto unitTarget = getTarget().lock();

        const auto nearEnemyStation = [&]() {
            const auto closestEnemyStation = Stations::getClosestStationGround(getPosition(), PlayerState::Enemy);
            return (closestEnemyStation && getPosition().getDistance(closestEnemyStation->getBase()->Center()) < 400.0);
        };

        const auto nearProxyStructure = [&]() {
            const auto closestProxy = Util::getClosestUnit(getPosition(), PlayerState::Self, [&](auto &u) {
                return u->getType().isBuilding() && u->isProxy();
            });
            return closestProxy && closestProxy->getPosition().getDistance(getPosition()) < 160.0;
        };

        const auto nearEnemyDefenseStructure = [&]() {
            const auto closestDefense = Util::getClosestUnit(getPosition(), PlayerState::Enemy, [&](auto &u) {
                return u->getType().isBuilding() && ((u->canAttackGround() && isFlying()) || (u->canAttackAir() && !isFlying()));
            });
            return closestDefense && closestDefense->getPosition().getDistance(getTarget().lock()->getPosition()) < 256.0;
        };

        const auto engagingWithWorkers = [&]() {
            if (!hasTarget())
                return false;
            const auto closestCombatWorker = Util::getClosestUnit(unitTarget->getPosition(), PlayerState::Self, [&](auto &u) {
                return u->getType().isWorker() && u->getRole() == Role::Combat;
            });
            return closestCombatWorker && closestCombatWorker->getPosition().getDistance(unitTarget->getPosition()) < getPosition().getDistance(unitTarget->getPosition()) + 64.0;
        };

        return (unitTarget->isThreatening() && !unitTarget->isHidden() && (Util::getTime() < Time(10, 00) || getSimState() == SimState::Win || Players::ZvZ() || getType() == Zerg_Zergling))                                                                          // ...target is threatening                    
            || (!getType().isWorker() && !Spy::enemyRush() && (getGroundRange() > unitTarget->getGroundRange() || unitTarget->getType().isWorker()) && Terrain::inTerritory(PlayerState::Self, unitTarget->getPosition()) && !unitTarget->isHidden())                 // ...unit can get free hits in our territory
            || (isSuicidal() && hasTarget() && (Terrain::inTerritory(PlayerState::Self, unitTarget->getPosition()) || unitTarget->isThreatening() || unitTarget->getPosition().getDistance(getGoal()) < 160.0))
            || (isSuicidal() && hasTarget() && Players::getStrength(PlayerState::Enemy).groundToAir <= 0.0 && !nearEnemyDefenseStructure())
            || (isHidden() && !Actions::overlapsDetection(unit(), getEngagePosition(), PlayerState::Enemy))
            || (getType() == Zerg_Lurker && isBurrowed() && !Actions::overlapsDetection(unit(), getEngagePosition(), PlayerState::Enemy))
            || (!isFlying() && Actions::overlapsActions(unit(), getEngagePosition(), TechTypes::Dark_Swarm, PlayerState::Neutral, 96))
            || (!isFlying() && (getGroundRange() < 32.0 || getType() == Zerg_Lurker) && Terrain::inTerritory(PlayerState::Enemy, getPosition()) && (Util::getTime() > Time(8, 00) || BuildOrder::isProxy()) && nearEnemyStation() && !Players::ZvZ())
            || (getType() == Zerg_Lurker && BuildOrder::isProxy() && nearProxyStructure())
            || (!isFlying() && Actions::overlapsActions(unit(), getPosition(), TechTypes::Dark_Swarm, PlayerState::Neutral, 96))
            || (isTargetedBySuicide() && !isFlying())
            || engagingWithWorkers();
    }

    bool UnitInfo::globalRetreat()
    {
        auto thisTarget = !target.expired() ? target.lock() : nullptr;
        auto freeTarget = thisTarget && ((!getTarget().lock()->canAttackAir() && this->isFlying()) || (!thisTarget->canAttackGround() && !this->isFlying()))
            && Terrain::inTerritory(PlayerState::Self, thisTarget->getPosition()) && !Terrain::inTerritory(PlayerState::Enemy, thisTarget->getPosition())
            && getPosition().getDistance(thisTarget->getPosition()) < getEngageRadius();

        // Try to save Mutas that are low hp when the firepower isn't needed
        const auto mutaSavingRequired = getType() == Zerg_Mutalisk && thisTarget &&
            (Players::ZvZ() ? (Players::getVisibleCount(PlayerState::Enemy, Zerg_Mutalisk) == 0) : (Util::getTime() > Time(8, 00)))
            && !thisTarget->isThreatening() && !isWithinRange(*thisTarget) && !thisTarget->isWithinRange(*this) && getHealth() <= 60
            && !Terrain::inTerritory(PlayerState::Enemy, getPosition());

        // Try to save scouts as they have high shield counts
        const auto scoutSavingRequired = getType() == Protoss_Scout && hasTarget() && !thisTarget->isThreatening() && !isWithinRange(*thisTarget) && getHealth() + getShields() <= 80;

        if (mutaSavingRequired || scoutSavingRequired)
            saveUnit = true;
        if (saveUnit) {
            if (getType() == Zerg_Mutalisk && getHealth() >= 100)
                saveUnit = false;
            if (getType() == Protoss_Scout && getShields() >= 90)
                saveUnit = false;
        }

        return nearHidden
            || (getGlobalState() == GlobalState::Retreat && !Terrain::inTerritory(PlayerState::Self, getPosition()) && !attemptingRunby())
            || (saveUnit && !freeTarget);
    }

    bool UnitInfo::attemptingRunby()
    {
        if (Spy::enemyProxy() && Spy::getEnemyBuild() == "2Gate" && timeCompletesWhen() < Time(4, 30))
            return true;
        return false;
    }

    bool UnitInfo::attemptingSurround()
    {
        if (attemptingRunby() || (hasTarget() && (getTarget().lock()->getType().isWorker() || getTarget().lock()->getCurrentSpeed() <= 0.0)))
            return false;
        if (surroundPosition.isValid() && !Terrain::inTerritory(PlayerState::Enemy, surroundPosition) && position.getDistance(surroundPosition) > 16.0)
            return true;
        return false;
    }

    bool UnitInfo::attemptingHarass()
    {
        if (Players::ZvZ() && vis(Zerg_Zergling) + 4 * com(Zerg_Sunken_Colony) < Players::getVisibleCount(PlayerState::Enemy, Zerg_Zergling))
            return false;
        if (Players::ZvZ() && Players::getVisibleCount(PlayerState::Enemy, Zerg_Mutalisk) > 0 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Spore_Colony) > 0)
            return false;
        if (Players::ZvZ() && BuildOrder::getCurrentTransition() == Spy::getEnemyTransition())
            return false;
        if (Players::ZvZ() && vis(Zerg_Mutalisk) <= Players::getVisibleCount(PlayerState::Enemy, Zerg_Mutalisk))
            return false;

        //if (hasTarget()) {
        //    auto unitTarget = getTarget().lock();
        //    if (unitTarget->canAttackGround() && unitTarget->getPosition().getDistance(BWEB::Map::getMainPosition()) < unitTarget->getPosition().getDistance(Terrain::getEnemyStartingPosition()))
        //        return false;
        //}
        return isLightAir() && Terrain::getHarassPosition().isValid();
    }

    bool UnitInfo::attemptingRegroup()
    {
        if (!isLightAir())
            return false;
        return hasCommander() && getPosition().getDistance(getCommander().lock()->getPosition()) > 128.0;
    }
}