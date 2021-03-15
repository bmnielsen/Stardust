#include "Squad.h"

#include "Squads/AttackBaseSquad.h"
#include "Base.h"

namespace General
{
    namespace
    {
        std::unordered_set<std::shared_ptr<Squad>> squads;
    }

    void initialize()
    {
        squads.clear();
    }

    void updateClusters()
    {
        for (auto &squad : squads)
        {
            squad->updateClusters();
        }
    }

    void issueOrders()
    {
        for (auto &squad : squads)
        {
            squad->execute();
        }
    }

    void addSquad(const std::shared_ptr<Squad> &squad)
    {
        squads.insert(squad);
    }

    void removeSquad(const std::shared_ptr<Squad> &squad)
    {
        squad->disband();
        squads.erase(squad);
    }

    AttackBaseSquad *getAttackBaseSquad(Base *targetBase)
    {
        if (!targetBase) return nullptr;

        for (auto &squad : squads)
        {
            if (auto match = std::dynamic_pointer_cast<AttackBaseSquad>(squad))
            {
                if (match->getTargetPosition() == targetBase->getPosition())
                {
                    return match.get();
                }
            }
        }

        return nullptr;
    }
}
