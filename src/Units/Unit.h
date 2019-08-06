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
    // These four properties will be used for CQL queries, so they are not marked mutable
    // (except for now until CQL works)

    BWAPI::Unit             unit;               // Reference to the unit
    mutable BWAPI::Player   player;             // Player owning the unit
    mutable int             tilePositionX;      // X coordinate of the tile position
    mutable int             tilePositionY;      // Y coordinate of the tile position

    // Remaining properties are marked mutable, as we can update them without violating any CQL orderings

    mutable int              lastSeen;                   // Frame the unit was last updated
                             
    mutable BWAPI::UnitType  type;                       // Type of the unit
    mutable int              id;                         // Unit ID
                             
    mutable BWAPI::Position  lastPosition;               // Position of the unit when last seen
    mutable bool             lastPositionValid;          // Whether this position is still valid, i.e. we haven't seen the position empty later
                             
    mutable int              lastHealth;                 // Health when last seen
    mutable int              lastShields;                // Shields when last seen
                             
    mutable bool             completed;                  // Whether the unit was completed
    mutable int              estimatedCompletionFrame;   // If not completed, the frame when we expect the unit to complete
                             
    mutable bool             isFlying;                   // Whether the unit is flying
                             
    mutable int              groundCooldownUntil;        // The frame when the unit can use its ground weapon again
    mutable int              airCooldownUntil;           // The frame when the unit can use its air weapon again
                             
    mutable bool             undetected;                 // Whether the unit is currently cloaked and undetected

    mutable std::vector<UpcomingAttack> upcomingAttacks; // List of attacks of this unit that are expected soon
    mutable bool             doomed;                     // Whether this unit is likely to be dead after the upcoming attacks are finished

    Unit(BWAPI::Unit unit);

    void update(BWAPI::Unit unit) const;
    void updateLastPositionValidity() const;
    void addUpcomingAttack(BWAPI::Unit attacker, BWAPI::Bullet bullet) const;

private:
    void updateGrid(BWAPI::Unit unit) const;
    void computeCompletionFrame(BWAPI::Unit unit) const;
};

namespace std {
    // Getter
    template<size_t Ind>
    constexpr auto &get(Unit &u) noexcept {
        if constexpr (Ind == 0)
            return u.unit;
        else if constexpr (Ind == 1)
            return u.player;
        else if constexpr (Ind == 2)
            return u.tilePositionX;
        else if constexpr (Ind == 3)
            return u.tilePositionY;
    }

    // Const getter
    template<size_t Ind>
    constexpr auto &get(Unit const &u) noexcept {
        if constexpr (Ind == 0)
            return u.unit;
        else if constexpr (Ind == 1)
            return u.player;
        else if constexpr (Ind == 2)
            return u.tilePositionX;
        else if constexpr (Ind == 3)
            return u.tilePositionY;
    }
}

// How many parts this decomposes into
template <> struct std::tuple_size<Unit> : public std::integral_constant<size_t, 4> { };

// Type of each part, we peek at our get() for the answer.
template<size_t Ind> struct std::tuple_element<Ind, Unit> {
    using type = decltype(std::get<Unit, Ind>);
};