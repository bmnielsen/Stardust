#include "Squad.h"

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
        squads.erase(squad);
    }
}
