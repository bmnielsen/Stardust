#include "Alchemist.h"

#include "Map.h"
#include "PathFinding.h"

void Alchemist::enemyStartingMainDetermined()
{
    // On Alchemist there are two potential main chokes and naturals depending on which direction we want to go
    // So once we know where the enemy is, we pick them based on this logic:
    // - Our natural is the one further away from the enemy
    // - Our main choke is the one towards the enemy
    // - Enemy natural is the one closest to us
    // - Enemy main choke is the one closest to us

    // Start by finding the chokes
    auto chokePath = PathFinding::GetChokePointPath(Map::getMyMain()->getPosition(),
                                                    Map::getEnemyStartingMain()->getPosition(),
                                                    BWAPI::UnitTypes::Protoss_Dragoon,
                                                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
    auto firstRampOrChoke = [](auto it, auto end)
    {
        for (auto itCopy = it; itCopy != end; itCopy++)
        {
            auto c = Map::choke(*itCopy);
            if (c->isRamp) return c;
        }

        return Map::choke(*it);
    };
    Map::setMyMainChoke(firstRampOrChoke(chokePath.begin(), chokePath.end()));
    Map::setEnemyMainChoke(firstRampOrChoke(chokePath.rbegin(), chokePath.rend()));

    // Now find the naturals
    auto pathGoesThroughChoke = [](Base *natural, Base *main, Choke *choke)
    {
        auto chokePath = PathFinding::GetChokePointPath(main->getPosition(),
                                                        natural->getPosition(),
                                                        BWAPI::UnitTypes::Protoss_Dragoon,
                                                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
        for (auto pathChoke : chokePath)
        {
            if (Map::choke(pathChoke) == choke) return true;
        }

        return false;
    };
    int ownNaturalDist = INT_MAX;
    Base *ownNatural = nullptr;
    int enemyNaturalDist = INT_MAX;
    Base *enemyNatural = nullptr;
    for (auto base : Map::allBases())
    {
        if (base == Map::getMyMain()) continue;
        if (base == Map::getEnemyStartingMain()) continue;
        if (base->gas() == 0) continue;

        if (!pathGoesThroughChoke(base, Map::getMyMain(), Map::getMyMainChoke()))
        {
            int dist = PathFinding::GetGroundDistance(
                    Map::getMyMain()->getPosition(),
                    base->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (dist == -1 || dist > ownNaturalDist) continue;

            ownNaturalDist = dist;
            ownNatural = base;
        }

        if (pathGoesThroughChoke(base, Map::getEnemyStartingMain(), Map::getEnemyMainChoke()))
        {
            int dist = PathFinding::GetGroundDistance(
                    Map::getEnemyStartingMain()->getPosition(),
                    base->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (dist == -1 || dist > enemyNaturalDist) continue;

            enemyNaturalDist = dist;
            enemyNatural = base;
        }
    }

    if (ownNatural)
    {
        Map::setMyNatural(ownNatural);
    }
    else
    {
        Log::Get() << "WARNING: Could not compute our natural base";
    }

    if (enemyNatural)
    {
        Map::setEnemyStartingNatural(enemyNatural);
    }
    else
    {
        Log::Get() << "WARNING: Could not compute enemy natural base";
    }
}

void Alchemist::modifyMainBaseBuildingPlacementAreas(std::set<const BWEM::Area *> &areas)
{
    // The 3-oclock start position is split into two areas, so we need to handle it specially
    if (BWAPI::Broodwar->self()->getStartLocation() != BWAPI::TilePosition(117, 51)) return;

    // Find the area by looking at the first choke on a path
    auto path = PathFinding::GetChokePointPath(Map::getMyMain()->getPosition(), BWAPI::Position(15, 15));
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
