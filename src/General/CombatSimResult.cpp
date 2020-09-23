#include "CombatSimResult.h"

double CombatSimResult::myPercentLost() const
{
    if (initialMine == 0) return 0.0;

    return 1.0 - ((double) finalMine / (double) initialMine);
}

double CombatSimResult::enemyPercentLost() const
{
    if (initialEnemy == 0) return 0.0;

    return 1.0 - ((double) finalEnemy / (double) initialEnemy);
}

int CombatSimResult::valueGain() const
{
    return finalMine - initialMine - (finalEnemy - initialEnemy);
}

double CombatSimResult::percentGain() const
{
    auto mine = myPercentLost();
    auto enemy = enemyPercentLost();

    return enemy - mine;
}

double CombatSimResult::myPercentageOfTotal() const
{
    if (finalMine == 0 && finalEnemy == 0) return 0.0;

    return (double) finalMine / (double) (finalMine + finalEnemy);
}
