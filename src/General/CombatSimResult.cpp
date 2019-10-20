#include "CombatSimResult.h"

int CombatSimResult::myValueLost()
{
    return initialMine - finalMine;
}

double CombatSimResult::myPercentLost()
{
    if (initialMine == 0) return 0.0;

    return 1.0 - ((double) finalMine / (double) initialMine);
}

int CombatSimResult::valueDifference()
{
    return finalMine - finalEnemy;
}

double CombatSimResult::myArmyProportion()
{
    if (finalMine == 0 && finalEnemy == 0) return 1.0;

    return (double) finalMine / (double) (finalMine + finalEnemy);
}

int CombatSimResult::valueGain()
{
    return finalMine - initialMine - (finalEnemy - initialEnemy);
}

double CombatSimResult::proportionalGain()
{
    if (initialMine == 0 && initialEnemy == 0) return 0.0;

    return myArmyProportion() - ((double) initialMine / (double) (initialMine + initialEnemy));
}