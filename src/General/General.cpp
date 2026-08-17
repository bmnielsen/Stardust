#include "Squad.h"

#include "Squads/AttackBaseSquad.h"
#include "Squads/DefendBaseSquad.h"
#include "Squads/EarlyGameDefendMainBaseSquad.h"
#include "Squads/DefendWallSquad.h"
#include "Base.h"
#include "Map.h"
#include "Units.h"
#include "EnemyArmy.h"

#if INSTRUMENTATION_ENABLED
#include <nlohmann/json.hpp>
#endif

namespace General
{
    namespace
    {
#if INSTRUMENTATION_ENABLED_VERBOSE
        const double pi = 3.14159265358979323846;
#endif

        std::unordered_set<std::shared_ptr<Squad>> squads;

        std::unordered_map<Base *, std::shared_ptr<AttackBaseSquad>> baseToAttackSquad;
        std::unordered_map<Base *, std::shared_ptr<Squad>> baseToDefendSquad;
        std::unordered_map<MyUnit, std::shared_ptr<Squad>> cannonToSquad;

        std::shared_ptr<Squad> defendWallSquad;

        std::vector<EnemyArmy> enemyArmies;
        std::map<Unit, const EnemyArmy*> enemyUnitToArmy;
    }

    void initialize()
    {
        squads.clear();
        baseToAttackSquad.clear();
        baseToDefendSquad.clear();
        cannonToSquad.clear();
        defendWallSquad = nullptr;
        enemyArmies.clear();
        enemyUnitToArmy.clear();
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

            // Check if the cannon should belong to a defend wall squad
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

            // Determine the base the cannon belongs to
            Base *base = nullptr;
            if (Map::getMyMainAreas().contains(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(cannon->lastPosition))))
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

        // Group enemy units not at any base into armies
        // This is using a fairly fast and loose clustering
        enemyArmies.clear();
        enemyUnitToArmy.clear();
        for (const auto &enemyUnit : Units::enemyCombatUnitsNotAtAnEnemyBase())
        {
            if (!enemyUnit->simPositionValid)
            {
                continue;
            }

            // Try to find an army this unit fits into
            bool added = false;
            for (auto army : enemyArmies)
            {
                if (army.tryAddUnit(enemyUnit))
                {
                    added = true;
                    break;
                }
            }
            if (added) continue;

            // Create a new army with this unit
            enemyArmies.emplace_back(enemyUnit);
        }
        for (const auto &enemyArmy : enemyArmies)
        {
            for (const auto &enemyUnit : enemyArmy.units)
            {
                enemyUnitToArmy.emplace(enemyUnit, &enemyArmy);
            }

#if INSTRUMENTATION_ENABLED_VERBOSE
            int ballRadius = (int) sqrt((double) (32 * 32 * enemyArmy.units.size()) / pi);
            CherryVis::drawCircle(enemyArmy.center.x, enemyArmy.center.y, ballRadius + 16, CherryVis::DrawColor::Blue);
#endif
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
        if (auto match = std::dynamic_pointer_cast<DefendWallSquad>(squad))
        {
            defendWallSquad = squad;
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

        if (defendWallSquad == squad) defendWallSquad = nullptr;
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

    const EnemyArmy* armyForEnemyUnit(const Unit &enemyUnit)
    {
        auto it = enemyUnitToArmy.find(enemyUnit);
        if (it == enemyUnitToArmy.end()) return nullptr;

        return it->second;
    }

    // Gets an enemy army matching the given predicate, or nullptr if none match
    const EnemyArmy* getEnemyArmy(const std::function<bool(const EnemyArmy &)> &predicate)
    {
        for (const auto &enemyArmy : enemyArmies)
        {
            if (predicate(enemyArmy)) return &enemyArmy;
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

        CherryVis::writeFrameData("squads", squadArray, 250);
#endif
    }
}
