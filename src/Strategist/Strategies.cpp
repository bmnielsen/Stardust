#include "Strategies.h"
#include "Strategist.h"

namespace Director
{
    bool isOurStrategy(std::variant<PvP::OurStrategy, PvT::OurStrategy, PvZ::OurStrategy, PvU::OurStrategy> strategy)
    {
        if (auto *s = std::get_if<PvP::OurStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvP*>(Director::getStrategyEngine()))
            {
                return strategyEngine->ourStrategy == *s;
            }

            return false;
        }

        if (auto *s = std::get_if<PvT::OurStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvT*>(Director::getStrategyEngine()))
            {
                return strategyEngine->ourStrategy == *s;
            }

            return false;
        }

        if (auto *s = std::get_if<PvZ::OurStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvZ*>(Director::getStrategyEngine()))
            {
                return strategyEngine->ourStrategy == *s;
            }

            return false;
        }

        if (auto *s = std::get_if<PvU::OurStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvU*>(Director::getStrategyEngine()))
            {
                return strategyEngine->ourStrategy == *s;
            }

            return false;
        }

        return false;
    }

    bool isEnemyStrategy(std::variant<PvP::ProtossStrategy, PvT::TerranStrategy, PvZ::ZergStrategy> strategy)
    {
        if (auto *protossStrategy = std::get_if<PvP::ProtossStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvP*>(Director::getStrategyEngine()))
            {
                return strategyEngine->enemyStrategy == *protossStrategy;
            }

            return false;
        }

        if (auto *terranStrategy = std::get_if<PvT::TerranStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvT*>(Director::getStrategyEngine()))
            {
                return strategyEngine->enemyStrategy == *terranStrategy;
            }

            return false;
        }

        if (auto *zergStrategy = std::get_if<PvZ::ZergStrategy>(&strategy))
        {
            if (auto strategyEngine = dynamic_cast<PvZ*>(Director::getStrategyEngine()))
            {
                return strategyEngine->enemyStrategy == *zergStrategy;
            }

            return false;
        }

        return false;
    }
}