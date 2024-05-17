#pragma once
#include <BWAPI.h>

namespace McRave::Math {

    double maxGroundStrength(UnitInfo&);
    double visibleGroundStrength(UnitInfo&);
    double maxAirStrength(UnitInfo&);
    double visibleAirStrength(UnitInfo&);
    double priority(UnitInfo&);
    double relativeCost(UnitInfo&);
    double groundDPS(UnitInfo&);
    double airDPS(UnitInfo&);
    double groundCooldown(UnitInfo&);
    double airCooldown(UnitInfo&);
    double splashModifier(UnitInfo&);
    double grdEffectiveness(UnitInfo&);
    double airEffectiveness(UnitInfo&);
    double survivability(UnitInfo&);
    double groundRange(UnitInfo&);
    double airRange(UnitInfo&);
    double groundReach(UnitInfo&);
    double airReach(UnitInfo&);
    double groundDamage(UnitInfo&);
    double airDamage(UnitInfo&);
    double moveSpeed(UnitInfo&);
    double simRadius(UnitInfo&);
    int realisticMineralCost(BWAPI::UnitType);
    int realisticGasCost(BWAPI::UnitType);
    int stopAnimationFrames(BWAPI::UnitType);
    int firstAttackAnimationFrames(BWAPI::UnitType);
    int contAttackAnimationFrames(BWAPI::UnitType);
}