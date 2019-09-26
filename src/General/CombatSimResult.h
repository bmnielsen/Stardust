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

    // How much my army lost in value during the sim
    int myValueLost();

    // What percentage of my army value was lost during the sim
    double myPercentLost();

    // The difference in value between my army and the enemy army at the end of the sim (negative if the enemy army is more valuable)
    int valueDifference();

    // The percentage of the total army size that is mine after the sim
    double myArmyProportion();

    // How much relative value was gained by my army during the sim (i.e. if positive, the value the enemy lost more than mine)
    int valueGain();

    // How much the proportion of army size to total changed during the sim
    double proportionalGain();

private:
    int initialMine;
    int initialEnemy;
    int finalMine;
    int finalEnemy;
};
