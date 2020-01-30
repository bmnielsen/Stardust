#include "CombatSimResult.h"

double CombatSimResult::myPercentLost()
{
    if (initialMine == 0) return 0.0;

    return 1.0 - ((double) finalMine / (double) initialMine);
}

double CombatSimResult::enemyPercentLost()
{
    if (initialEnemy == 0) return 0.0;

    return 1.0 - ((double) finalEnemy / (double) initialEnemy);
}

int CombatSimResult::valueGain()
{
    return finalMine - initialMine - (finalEnemy - initialEnemy);
}

double CombatSimResult::percentGain()
{
    auto mine = myPercentLost();
    auto enemy = enemyPercentLost();

    return enemy - mine;
}