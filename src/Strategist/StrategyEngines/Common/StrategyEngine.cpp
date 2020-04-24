#include "StrategyEngine.h"

MainArmyPlay *StrategyEngine::getMainArmyPlay(std::vector<std::shared_ptr<Play>> &plays)
{
    for (auto &play : plays)
    {
        if (auto mainArmyPlay = std::dynamic_pointer_cast<MainArmyPlay>(play))
        {
            return mainArmyPlay.get();
        }
    }

    return nullptr;
}
