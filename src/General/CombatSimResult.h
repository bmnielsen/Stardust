#pragma once

#include <BWAPI/Game.h>

class CombatSimResult
{
public:
    int frame;

    CombatSimResult(int initialMine, int initialEnemy, int finalMine, int finalEnemy)
            : frame(BWAPI::Broodwar->getFrameCount()), initialMine(initialMine), initialEnemy(initialEnemy), finalMine(finalMine), finalEnemy(
            finalEnemy) {}

    CombatSimResult() : CombatSimResult(0, 0, 0, 0) {};

    // What percentage of my army value was lost during the sim
    double myPercentLost();

    // What percentage of the enemy's army value was lost during the sim
    double enemyPercentLost();

    // How much relative value was gained by my army during the sim (i.e. if positive, the value the enemy lost more than mine)
    int valueGain();

    // Similar to valueGain, but dealing in percentages (i.e. if positive, the enemy lost a higher percentage of their army than we lost of ours)
    double percentGain();

private:
    int initialMine;
    int initialEnemy;
    int finalMine;
    int finalEnemy;
};
