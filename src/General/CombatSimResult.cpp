#include "CombatSimResult.h"

namespace
{
    const double pi = 3.14159265358979323846;
}

CombatSimResult CombatSimResult::adjustForChoke(bool attacking) const
{
    auto result = CombatSimResult(myUnitCount, enemyUnitCount, initialMine, initialEnemy, finalMine, finalEnemy, nullptr, 0, 0, 0.0);

    // Currently this only affects the numbers if the combat sim spans a narrow choke
    if (!narrowChoke) return result;

    auto scaleDamage = [](int initial, int final, double factor)
    {
        if (final >= initial) return final;

        return initial - (int) ((double) (initial - final) * factor);
    };

    // When attacking through a narrow choke, we assume that we will do less damage than simulated
    if (attacking)
    {
        // The gain is scaled from 1.0 (approximately no change) to 0.25 (expect to do much less damage)

        // We assume our cluster is not packaged optimally, so increase the area by 50%
        auto clusterDiameter = sqrt((double) myArea * 4.0 / pi) * 1.5;

        // There is also a certain amount of loss based on the choke sides, so consider the width to be one tile lower
        auto narrowChokeGain = std::max(0.25, std::min(1.0, (double) (narrowChoke->width - 32) / clusterDiameter));

        result.finalEnemy = scaleDamage(initialEnemy, finalEnemy, narrowChokeGain);

#if DEBUG_COMBATSIM
        CherryVis::log() << "Adjusted for attacking; choke gain " << narrowChokeGain
                         << ": " << result.initialMine << "," << result.initialEnemy
                         << "-" << result.finalMine << "," << result.finalEnemy;
#endif
    }
    else
    {
        // Similar to ours, but somewhat more conservative
        auto clusterDiameter = sqrt((double) enemyArea * 4.0 / pi) * 1.3;
        auto narrowChokeGain = std::max(0.25, std::min(1.0, (double) narrowChoke->width / clusterDiameter));

        result.finalMine = scaleDamage(initialMine, finalMine, narrowChokeGain);

#if DEBUG_COMBATSIM
        CherryVis::log() << "Adjusted for defending; choke gain " << narrowChokeGain
                         << ": " << result.initialMine << "," << result.initialEnemy
                         << "-" << result.finalMine << "," << result.finalEnemy;
#endif
    }

    // Make adjustments for elevation
    if (elevationGain > 0.001)
    {
        // Assume the enemy will do less damage than simulated
        result.finalMine = scaleDamage(initialMine, finalMine, 1.0 - elevationGain);

#if DEBUG_COMBATSIM
        CherryVis::log() << "Adjusted for higher elevation; choke gain " << (1.0 - elevationGain)
                         << ": " << result.initialMine << "," << result.initialEnemy
                         << "-" << result.finalMine << "," << result.finalEnemy;
#endif
    }
    else if (elevationGain < -0.001)
    {
        // Assume we will do less damage than simulated
        result.finalEnemy = scaleDamage(initialEnemy, finalEnemy, 1.0 + elevationGain);

#if DEBUG_COMBATSIM
        CherryVis::log() << "Adjusted for lower elevation; choke gain " << (1.0 + elevationGain)
                         << ": " << result.initialMine << "," << result.initialEnemy
                         << "-" << result.finalMine << "," << result.finalEnemy;
#endif
    }

    return result;
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
