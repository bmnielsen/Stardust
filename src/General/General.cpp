#include "Squad.h"

#include "Squads/AttackBaseSquad.h"
#include "Squads/DefendBaseSquad.h"
#include "Squads/EarlyGameDefendMainBaseSquad.h"
#include "Squads/DefendWallSquad.h"
#include "Base.h"
#include "Map.h"
#include "Units.h"

#if INSTRUMENTATION_ENABLED
#include <nlohmann/json.hpp>
#endif

namespace Marshal
{
    namespace
    {
        std::unordered_set<std::shared_ptr<UnitSquad>> squads;

        std::unordered_map<Base *, std::shared_ptr<AttackBaseSquad>> baseToAttackSquad;
        std::unordered_map<Base *, std::shared_ptr<UnitSquad>> baseToDefendSquad;
        std::unordered_map<OwnedUnit, std::shared_ptr<UnitSquad>> cannonToSquad;

        std::shared_ptr<UnitSquad> defendWallSquad;
    }

    void initialize()
    {
        squads.clear();
        baseToAttackSquad.clear();
        baseToDefendSquad.clear();
        cannonToSquad.clear();
        defendWallSquad = nullptr;
    }

    void updateClusters()
    {
        /* Add completed and powered cannons to an appropriate squad */
        /* We currently only use cannons defensively, so we don't need to ever add them to attack squads */
        for (auto &cannon : UnitMgr::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon))
        {
            /* Clean up dead or unpowered cannons */
            if (!cannon->exists() || !cannon->rawUnit->isPowered())
            {
                auto iterRef = cannonToSquad.find(cannon);
                if (iterRef != cannonToSquad.end())
                {
                    iterRef->second->removeUnit(cannon);
                    cannonToSquad.erase(iterRef);
                }
                continue;
            }

            /* Check if the cannon should belong to a defend wall squad */
            if (defendWallSquad)
            {
                bool foundCannon = false;

                for (auto cannonPlacement : BuildingPlacement::getForgeGatewayWall().cannons)
                {
                    if (cannon->getTilePosition() != cannonPlacement) continue;

                    auto currentSquad = cannonToSquad.find(cannon);
                    if (currentSquad == cannonToSquad.end() || currentSquad->second != defendWallSquad)
                    {
                        if (currentSquad != cannonToSquad.end())
                        {
                            currentSquad->second->removeUnit(cannon);
                        }

                        defendWallSquad->addUnit(cannon);
                        cannonToSquad[cannon] = defendWallSquad;
                    }

                    foundCannon = true;
                    break;
                }

                if (foundCannon) continue;
            }

            /* Determine the base the cannon belongs to */
            Base *baseLoc = nullptr;
            if (Map::getMyMainAreas().contains(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(cannon->prevPos))))
            {
                baseLoc = Map::getMyMain();
            }
            else
            {
                int closest = INT_MAX;
                for (auto myBase : Map::getMyBases())
                {
                    int span = cannon->prevPos.getApproxDistance(myBase->getPosition());
                    if (span < 320 && span < closest)
                    {
                        closest = span;
                        baseLoc = myBase;
                    }
                }
            }

            /* Update the assignment */
            auto baseSquad = baseToDefendSquad.find(baseLoc);
            auto currentSquad = cannonToSquad.find(cannon);
            if (currentSquad != cannonToSquad.end() && (baseSquad == baseToDefendSquad.end() || baseSquad->second != currentSquad->second))
            {
                currentSquad->second->removeUnit(cannon);
                cannonToSquad.erase(currentSquad);
                currentSquad = cannonToSquad.end();
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

    void addSquad(const std::shared_ptr<UnitSquad> &squad)
    {
        squads.insert(squad);

        if (auto match = std::dynamic_pointer_cast<AttackBaseSquad>(squad))
        {
            baseToAttackSquad[match->baseLoc] = match;
        }
        if (auto match = std::dynamic_pointer_cast<DefendBaseSquad>(squad))
        {
            baseToDefendSquad[match->baseLoc] = squad;
        }
        if (auto match = std::dynamic_pointer_cast<EarlyGameDefendMainBaseSquad>(squad))
        {
            baseToDefendSquad[Map::getMyMain()] = squad;
        }
        if (auto match = std::dynamic_pointer_cast<DefendWallSquad>(squad))
        {
            defendWallSquad = squad;
        }
    }

    void removeSquad(const std::shared_ptr<UnitSquad> &squad)
    {
        squad->disband();
        squads.erase(squad);

        auto cleanupMap = [&squad](auto map)
        {
            for (auto iterRef = map.begin(); iterRef != map.end(); )
            {
                if (iterRef->second == squad)
                {
                    iterRef = map.erase(iterRef);
                }
                else
                {
                    iterRef++;
                }
            }
        };
        cleanupMap(baseToAttackSquad);
        cleanupMap(baseToDefendSquad);
        cleanupMap(cannonToSquad);

        if (defendWallSquad == squad) defendWallSquad = nullptr;
    }

    AttackBaseSquad *getAttackBaseSquad(Base *targetBase)
    {
        if (!targetBase) return nullptr;
        auto iterRef = baseToAttackSquad.find(targetBase);
        if (iterRef != baseToAttackSquad.end())
        {
            return iterRef->second.get();
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
            /* Ignore squads with no units */
            if (squad->combatUnitCount() == 0) continue;

            /* Check if we have multiple squads with the same label */
            auto outcome = squadLabels.insert(squad->label);
            if (!outcome.second)
            {
                AppLog::Get() << "Instrumentation Error: Duplicate squad label " << squad->label;
            }

            squad->addInstrumentation(squadArray);
        }

        Telemetry::writeFrameData("squads", squadArray, 250);
#endif
    }
}
