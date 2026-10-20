#include "Alchemist.h"

#include "Map.h"
#include "PathFinding.h"

void Alchemist::enemyStartingMainDetermined()
{
    /* On Alchemist there are two potential main chokes and naturals depending on which direction we want to go */
    /* So once we know where the enemy is, we pick them based on this logic: */
    /* - Our natural is the one further away from the enemy */
    /* - Our main choke is the one towards the enemy */
    /* - Enemy natural is the one closest to us */
    /* - Enemy main choke is the one closest to us */

    /* Start by finding the chokes */
    auto chokePath = Pathfinder::GetChokePointPath(Map::getMyMain()->getPosition(),
                                                    Map::getEnemyStartingMain()->getPosition(),
                                                    BWAPI::UnitTypes::Protoss_Dragoon,
                                                    Pathfinder::PathFindingOptions::UseNearestBWEMArea);
    auto firstRampOrChoke = [](auto iterRef, auto end)
    {
        for (auto itCopy = iterRef; itCopy != end; itCopy++)
        {
            auto c = Map::choke(*itCopy);
            if (c->isRamp) return c;
        }

        return Map::choke(*iterRef);
    };
    Map::setMyMainChoke(firstRampOrChoke(chokePath.begin(), chokePath.end()));
    Map::setEnemyMainChoke(firstRampOrChoke(chokePath.rbegin(), chokePath.rend()));

    /* Now find the naturals */
    auto pathGoesThroughChoke = [](Base *natBase, Base *main, Choke *choke)
    {
        auto chokePath = Pathfinder::GetChokePointPath(main->getPosition(),
                                                        natBase->getPosition(),
                                                        BWAPI::UnitTypes::Protoss_Dragoon,
                                                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);
        return std::any_of(chokePath.begin(), chokePath.end(), [&choke](const auto &bwemChoke)
        {
            return Map::choke(bwemChoke) == choke;
        });
    };
    int ownNaturalDist = INT_MAX;
    Base *ownNatural = nullptr;
    int enemyNaturalDist = INT_MAX;
    Base *enemyNatural = nullptr;
    for (auto baseLoc : Map::allBases())
    {
        if (baseLoc == Map::getMyMain()) continue;
        if (baseLoc == Map::getEnemyStartingMain()) continue;
        if (baseLoc->gas == 0) continue;

        if (!pathGoesThroughChoke(baseLoc, Map::getMyMain(), Map::getMyMainChoke()))
        {
            int span = Pathfinder::GetGroundDistance(
                    Map::getMyMain()->getPosition(),
                    baseLoc->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    Pathfinder::PathFindingOptions::UseNearestBWEMArea);
            if (span == -1 || span > ownNaturalDist) continue;

            ownNaturalDist = span;
            ownNatural = baseLoc;
        }

        if (pathGoesThroughChoke(baseLoc, Map::getEnemyStartingMain(), Map::getEnemyMainChoke()))
        {
            int span = Pathfinder::GetGroundDistance(
                    Map::getEnemyStartingMain()->getPosition(),
                    baseLoc->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    Pathfinder::PathFindingOptions::UseNearestBWEMArea);
            if (span == -1 || span > enemyNaturalDist) continue;

            enemyNaturalDist = span;
            enemyNatural = baseLoc;
        }
    }

    if (ownNatural)
    {
        Map::setMyNatural(ownNatural);
    }
    else
    {
        AppLog::Get() << "WARNING: Could not compute our natural base";
    }

    if (enemyNatural)
    {
        Map::setEnemyStartingNatural(enemyNatural);
    }
    else
    {
        AppLog::Get() << "WARNING: Could not compute enemy natural base";
    }
}

void Alchemist::modifyMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas)
{
    /* The 3-oclock start position is split into two areas, so we need to handle it specially */
    if (BWAPI::Broodwar->self()->getStartLocation() != BWAPI::TilePosition(117, 51)) return;

    /* Find the area by looking at the first choke on a path */
    auto path = Pathfinder::GetChokePointPath(Map::getMyMain()->getPosition(), BWAPI::Position(15, 15));
    if (path.empty()) return;

    auto baseElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::Broodwar->self()->getStartLocation());
    auto handleArea = [&](auto area)
    {
        auto areaElevation = BWAPI::Broodwar->getGroundHeight(BWAPI::TilePosition(area->Top()));
        if (areaElevation == baseElevation) areas.insert(area);
    };
    handleArea((*path.begin())->GetAreas().first);
    handleArea((*path.begin())->GetAreas().second);
}
