#include "Strategies.h"
#include "Strategist.h"

namespace Strategist
{
    bool isEnemyStrategy(std::variant<PvP::ProtossStrategy, PvT::TerranStrategy, PvZ::ZergStrategy> strategy)
    {
        if (auto *protossStrategy = std::get_if<PvP::ProtossStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvP*>(Strategist::getStrategyEngine()))
            {
                return strategyEngine->enemyStrategy == *protossStrategy;
            }

            return false;
        }

        if (auto *terranStrategy = std::get_if<PvT::TerranStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvT*>(Strategist::getStrategyEngine()))
            {
                return strategyEngine->enemyStrategy == *terranStrategy;
            }

            return false;
        }

        if (auto *zergStrategy = std::get_if<PvZ::ZergStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvZ*>(Strategist::getStrategyEngine()))
            {
                return strategyEngine->enemyStrategy == *zergStrategy;
            }

            return false;
        }

        return false;
    }
}