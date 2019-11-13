#pragma once

#include "Common.h"

struct UpcomingAttack
{
    BWAPI::Unit attacker;
    BWAPI::Bullet bullet;
    int bulletId;
    int damage;

    UpcomingAttack(BWAPI::Unit attacker, BWAPI::Bullet bullet, int damage)
            : attacker(attacker)
            , bullet(bullet)
            , bulletId(bullet ? bullet->getID() : -1)
            , damage(damage) {}
};

class Unit
{
public:

    BWAPI::Unit unit;               // Reference to the unit
    BWAPI::Player player;             // Player owning the unit
    int tilePositionX;      // X coordinate of the tile position
    int tilePositionY;      // Y coordinate of the tile position

    int lastSeen;                   // Frame the unit was last updated

    BWAPI::UnitType type;                       // Type of the unit
    int id;                         // Unit ID

    BWAPI::Position lastPosition;               // Position of the unit when last seen
    bool lastPositionValid;          // Whether this position is still valid, i.e. we haven't seen the position empty later

    int lastHealth;                 // Health when last seen
    int lastShields;                // Shields when last seen

    bool completed;                  // Whether the unit was completed
    int estimatedCompletionFrame;   // If not completed, the frame when we expect the unit to complete

    bool isFlying;                   // Whether the unit is flying

    int cooldownUntil;              // The frame when the unit can use its ground weapon again
    int stimmedUntil;               // If stimmed, when the stim will wear off

    bool undetected;                 // Whether the unit is currently cloaked and undetected
    bool burrowed;                   // Whether the unit is currently burrowed
    int lastBurrowing;              // Frame we last observed the unit burrowing

    std::vector<UpcomingAttack> upcomingAttacks; // List of attacks of this unit that are expected soon
    bool doomed;                     // Whether this unit is likely to be dead after the upcoming attacks are finished

    explicit Unit(BWAPI::Unit unit);

    virtual ~Unit() = default;

    virtual void update(BWAPI::Unit unit);

    void updateLastPositionValidity();

    void addUpcomingAttack(BWAPI::Unit attacker, BWAPI::Bullet bullet);

private:
    void updateGrid(BWAPI::Unit unit);

    void computeCompletionFrame(BWAPI::Unit unit);
};
