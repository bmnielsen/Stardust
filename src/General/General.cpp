#include "Squad.h"

#include "Squads/AttackBaseSquad.h"
#include "Squads/DefendBaseSquad.h"
#include "Squads/EarlyGameDefendMainBaseSquad.h"
#include "Base.h"
#include "Map.h"
#include "Units.h"

#if INSTRUMENTATION_ENABLED
#include <nlohmann/json.hpp>
#endif

namespace General
{
    namespace
    {
        std::unordered_set<std::shared_ptr<Squad>> squads;

        std::unordered_map<Base *, std::shared_ptr<AttackBaseSquad>> baseToAttackSquad;
        std::unordered_map<Base *, std::shared_ptr<Squad>> baseToDefendSquad;
        std::unordered_map<MyUnit, std::shared_ptr<Squad>> cannonToSquad;
    }

    void initialize()
    {
        squads.clear();
        baseToAttackSquad.clear();
        baseToDefendSquad.clear();
        cannonToSquad.clear();
    }

    void updateClusters()
    {
        // Add completed and powered cannons to an appropriate squad
        // We currently only use cannons defensively, so we don't need to ever add them to attack squads
        for (auto &cannon : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon))
        {
            // Clean up dead or unpowered cannons
            if (!cannon->exists() || !cannon->bwapiUnit->isPowered())
            {
                auto it = cannonToSquad.find(cannon);
                if (it != cannonToSquad.end())
                {
                    it->second->removeUnit(cannon);
                    cannonToSquad.erase(it);
                }
                continue;
            }

            // Determine the base the cannon belongs to
            Base *base = nullptr;
            auto mainAreas = Map::getMyMainAreas();
            if (mainAreas.find(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(cannon->lastPosition))) != mainAreas.end())
            {
                base = Map::getMyMain();
            }
            else
            {
                int closest = INT_MAX;
                for (auto myBase : Map::getMyBases())
                {
                    int dist = cannon->lastPosition.getApproxDistance(myBase->getPosition());
                    if (dist < 320 && dist < closest)
                    {
                        closest = dist;
                        base = myBase;
                    }
                }
            }

            // Update the assignment
            auto baseSquad = baseToDefendSquad.find(base);
            auto currentSquad = cannonToSquad.find(cannon);
            if (currentSquad != cannonToSquad.end() && (baseSquad == baseToDefendSquad.end() || baseSquad->second != currentSquad->second))
            {
                currentSquad->second->removeUnit(cannon);
                cannonToSquad.erase(currentSquad);
            }
            if (baseSquad != baseToDefendSquad.end() && (currentSquad == cannonToSquad.end() || baseSquad->second != currentSquad->second))
            {
                baseSquad->second->addUnit(cannon);
                cannonToSquad[cannon] = baseSquad->second;
            }
        }

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

        if (auto match = std::dynamic_pointer_cast<AttackBaseSquad>(squad))
        {
            baseToAttackSquad[match->base] = match;
        }
        if (auto match = std::dynamic_pointer_cast<DefendBaseSquad>(squad))
        {
            baseToDefendSquad[match->base] = squad;
        }
        if (auto match = std::dynamic_pointer_cast<EarlyGameDefendMainBaseSquad>(squad))
        {
            baseToDefendSquad[Map::getMyMain()] = squad;
        }
    }

    void removeSquad(const std::shared_ptr<Squad> &squad)
    {
        squad->disband();
        squads.erase(squad);

        auto cleanupMap = [&squad](auto map)
        {
            for (auto it = map.begin(); it != map.end(); )
            {
                if (it->second == squad)
                {
                    it = map.erase(it);
                }
                else
                {
                    it++;
                }
            }
        };
        cleanupMap(baseToAttackSquad);
        cleanupMap(baseToDefendSquad);
        cleanupMap(cannonToSquad);
    }

    AttackBaseSquad *getAttackBaseSquad(Base *targetBase)
    {
        if (!targetBase) return nullptr;
        auto it = baseToAttackSquad.find(targetBase);
        if (it != baseToAttackSquad.end())
        {
            return it->second.get();
        }

        return nullptr;
    }

    void writeInstrumentation()
    {
#if INSTRUMENTATION_ENABLED
        nlohmann::json squadArray;

        std::set<std::string> squadLabels;
        for (auto &squad : squads)
        {
            // Ignore squads with no units
            if (squad->combatUnitCount() == 0) continue;

            // Check if we have multiple squads with the same label
            auto result = squadLabels.insert(squad->label);
            if (!result.second)
            {
                Log::Get() << "Instrumentation Error: Duplicate squad label " << squad->label;
            }

            squad->addInstrumentation(squadArray);
        }

        CherryVis::writeFrameData("squads", squadArray);
#endif
    }
}
