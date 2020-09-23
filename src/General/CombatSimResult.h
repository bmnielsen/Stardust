#pragma once

#include "Choke.h"

class CombatSimResult
{
public:
    int frame;
    int myUnitCount;
    int enemyUnitCount;
    int initialMine;
    int initialEnemy;
    int finalMine;
    int finalEnemy;

    Choke *narrowChoke;

    CombatSimResult(int myUnitCount,
                    int enemyUnitCount,
                    int initialMine,
                    int initialEnemy,
                    int finalMine,
                    int finalEnemy,
                    Choke *narrowChoke)
            : frame(BWAPI::Broodwar->getFrameCount())
            , myUnitCount(myUnitCount)
            , enemyUnitCount(enemyUnitCount)
            , initialMine(initialMine)
            , initialEnemy(initialEnemy)
            , finalMine(finalMine)
            , finalEnemy(finalEnemy)
            , narrowChoke(narrowChoke) {}

    CombatSimResult() : CombatSimResult(0, 0, 0, 0, 0, 0, nullptr) {};

    // What percentage of my army value was lost during the sim
    [[nodiscard]] double myPercentLost() const;

    // What percentage of the enemy's army value was lost during the sim
    [[nodiscard]] double enemyPercentLost() const;

    // How much relative value was gained by my army during the sim (i.e. if positive, the value the enemy lost more than mine)
    [[nodiscard]] int valueGain() const;

    // Similar to valueGain, but dealing in percentages (i.e. if positive, the enemy lost a higher percentage of their army than we lost of ours)
    [[nodiscard]] double percentGain() const;

    // What percentage of the total army value is ours
    [[nodiscard]] double myPercentageOfTotal() const;

    CombatSimResult &operator+=(const CombatSimResult &other)
    {
        initialMine += other.initialMine;
        initialEnemy += other.initialEnemy;
        finalMine += other.finalMine;
        finalEnemy += other.finalEnemy;

        return *this;
    }

    CombatSimResult &operator/=(int divisor)
    {
        initialMine /= divisor;
        initialEnemy /= divisor;
        finalMine /= divisor;
        finalEnemy /= divisor;

        return *this;
    }
};
