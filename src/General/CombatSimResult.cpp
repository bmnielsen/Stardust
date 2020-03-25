#include "CombatSimResult.h"

void CombatSimResult::setAttacking(bool attacking)
{
    // Currently this only affects the numbers if the combat sim spans a narrow choke
    if (!narrowChoke) return;

    auto scaleDamage = [](int initial, int final, double factor)
    {
        if (final >= initial) return final;

        return initial - (int)((double)(initial - final) * factor);
    };

    // When attacking through a narrow choke, we assume that we will do less damage than simulated
    if (attacking)
    {
        finalEnemy = scaleDamage(initialEnemy, finalEnemy, narrowChokeGain);
    }

    // TODO: Does it make sense to penalize the enemy when we are defending? Need to be careful here.

    // Make adjustments for elevation
    if (elevationGain > 0.001)
    {
        // Assume the enemy will do less damage than simulated
        finalMine = scaleDamage(initialMine, finalMine, 1.0 - elevationGain);
    }
    else if (elevationGain < -0.001)
    {
        // Assume we will do less damage than simulated
        finalEnemy = scaleDamage(initialEnemy, finalEnemy, 1.0 + elevationGain);
    }
}

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
