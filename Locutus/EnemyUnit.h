#pragma once

#include "Common.h"

class Unit
{
public:
    BWAPI::Unit     unit;                       // Reference to the unit
    int             lastSeen;                   // Frame the unit was last updated

    BWAPI::Player   player;                     // Player owning the unit
    
    BWAPI::UnitType type;                       // Type of the unit
    int             id;                         // Unit ID
    
    BWAPI::Position lastPosition;               // Position of the unit when last seen
    bool            lastPositionValid;          // Whether this position is still valid, i.e. we haven't seen the position empty later
    
    int             lastHealth;                 // Health when last seen
    int             lastShields;                // Shields when last seen
    
    bool            completed;                  // Whether the unit was completed
    int             estimatedCompletionFrame;   // If not completed, the frame when we expect the unit to complete

    bool            isFlying;                   // Whether the unit is flying

    int             groundCooldownUntil;        // The frame when the unit can use its ground weapon again
    int             airCooldownUntil;           // The frame when the unit can use its air weapon again
    
    bool            undetected;                 // Whether the unit is currently cloaked and undetected

    Unit(BWAPI::Unit unit);

    void update(BWAPI::Unit unit);
    void updateLastPositionValidity();

private:
    void computeCompletionFrame(BWAPI::Unit unit);
};
