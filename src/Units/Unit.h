#pragma once

#include <utility>

#include "Common.h"

class UnitImpl;

typedef std::shared_ptr<UnitImpl> Unit;

struct UpcomingAttack
{
    Unit attacker;
    BWAPI::Bullet bullet;
    int bulletId;           // Bullet objects are re-used, so keep track of the original ID
    int expiryFrame;        // The frame at which this upcoming attack "expires" (i.e. results in a bullet or deals damage)
    int damage;

    UpcomingAttack(Unit attacker, BWAPI::Bullet bullet, int damage)
            : attacker(std::move(attacker))
            , bullet(bullet)
            , bulletId(bullet ? bullet->getID() : -1)
            , expiryFrame(INT_MAX)
            , damage(damage) {}

    UpcomingAttack(Unit attacker, int frames, int damage)
    : attacker(std::move(attacker))
    , bullet(nullptr)
    , bulletId(-1)
    , expiryFrame(BWAPI::Broodwar->getFrameCount() + frames)
    , damage(damage) {}
};

class UnitImpl
{
public:

    BWAPI::Unit bwapiUnit;              // Reference to the unit
    BWAPI::Player player;               // Player owning the unit
    int tilePositionX;                  // X coordinate of the tile position
    int tilePositionY;                  // Y coordinate of the tile position
    BWAPI::TilePosition buildTile;      // For landed buildings, the tile position of the build tile (top-left tile)

    int lastSeen;                       // Frame the unit was last updated
    int lastSeenAttacking;              // Frame when the unit was last seen making an attack

    BWAPI::UnitType type;               // Type of the unit
    int id;                             // Unit ID

    BWAPI::Position lastPosition;       // Position of the unit when last seen
    bool lastPositionValid;             // Whether this position is still valid, i.e. we haven't seen the position empty later
    bool lastPositionVisible;           // Whether the last position was visible on the previous frame
    bool beingManufacturedOrCarried;    // Whether the unit is currently being manufactured or carried
    bool frameLastMoved;                // Last frame on which the unit changed position

    int lastHealth;                     // Health when last seen, adjusted for upcoming attacks
    int lastShields;                    // Shields when last seen, adjusted for upcoming attacks
    int health;                         // Estimated health of the unit, adjusted for upcoming attacks
    int shields;                        // Estimated shields of the unit, adjusted for upcoming attacks

    int lastHealFrame;                  // Last frame the unit was healed or repaired

    bool completed;                     // Whether the unit was completed
    int estimatedCompletionFrame;       // If not completed, the frame when we expect the unit to complete

    bool isFlying;                      // Whether the unit is flying

    int cooldownUntil;                  // The frame when the unit can use its ground weapon again
    int stimmedUntil;                   // If stimmed, when the stim will wear off

    bool undetected;                    // Whether the unit is currently cloaked and undetected
    bool burrowed;                      // Whether the unit is currently burrowed
    int lastBurrowing;                  // Frame we last observed the unit burrowing

    std::vector<UpcomingAttack>
            upcomingAttacks;            // List of attacks of this unit that are expected soon

    explicit UnitImpl(BWAPI::Unit unit);

    virtual ~UnitImpl() = default;

    void created();

    virtual void update(BWAPI::Unit unit);

    void updateUnitInFog();

    void addUpcomingAttack(const Unit &attacker, BWAPI::Bullet bullet);

    void addUpcomingAttack(const Unit &attacker);

    /* Information stuff, see Unit_Info.cpp */

    [[nodiscard]] BWAPI::TilePosition getTilePosition() const;

    [[nodiscard]] bool exists() const { return bwapiUnit != nullptr; };

    [[nodiscard]] virtual bool isBeingManufacturedOrCarried() const { return false; };

    [[nodiscard]] bool isBeingHealed() const { return BWAPI::Broodwar->getFrameCount() < (lastHealFrame + 24); };

    [[nodiscard]] bool isAttackable() const;

    [[nodiscard]] bool canAttack(const Unit &target) const;

    [[nodiscard]] bool canBeAttackedBy(const Unit &attacker) const;

    [[nodiscard]] bool canAttackGround() const;

    [[nodiscard]] bool canAttackAir() const;

    [[nodiscard]] bool isStaticGroundDefense() const;

    [[nodiscard]] bool isTransport() const;

    [[nodiscard]] int groundRange() const;

    [[nodiscard]] int airRange() const;

    [[nodiscard]] int range(const Unit &target) const;

    [[nodiscard]] BWAPI::WeaponType getWeapon(const Unit &target) const;

    [[nodiscard]] bool isInOurWeaponRange(const Unit &target,
                                          BWAPI::Position predictedTargetPosition = BWAPI::Positions::Invalid,
                                          int buffer = 0) const;

    [[nodiscard]] bool isInOurWeaponRange(const Unit &target, int buffer) const
    {
        return isInOurWeaponRange(target, BWAPI::Positions::Invalid, buffer);
    };

    [[nodiscard]] bool isInEnemyWeaponRange(const Unit &attacker,
                                            BWAPI::Position predictedAttackerPosition = BWAPI::Positions::Invalid,
                                            int buffer = 0) const;

    [[nodiscard]] bool isInEnemyWeaponRange(const Unit &attacker, int buffer) const
    {
        return isInEnemyWeaponRange(attacker, BWAPI::Positions::Invalid, buffer);
    };

    [[nodiscard]] int getDistance(BWAPI::Position position) const;

    [[nodiscard]] int getDistance(const Unit &other, BWAPI::Position predictedOtherPosition = BWAPI::Positions::Invalid) const;

    [[nodiscard]] BWAPI::Position predictPosition(int frames) const;

    [[nodiscard]] BWAPI::Position intercept(const Unit &target) const;

private:
    void updateGrid(BWAPI::Unit unit);
};

std::ostream &operator<<(std::ostream &os, const UnitImpl &unit);
