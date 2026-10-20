#include "Map.h"

#include <ranges>

#include "MapSpecificOverrides/Alchemist.h"
#include "MapSpecificOverrides/Arcadia.h"
#include "MapSpecificOverrides/Colosseum.h"
#include "MapSpecificOverrides/CrossingField.h"
#include "MapSpecificOverrides/Destination.h"
#include "MapSpecificOverrides/Fortress.h"
#include "MapSpecificOverrides/GodsGarden.h"
#include "MapSpecificOverrides/JudgmentDay.h"
#include "MapSpecificOverrides/Katrina.h"
#include "MapSpecificOverrides/MatchPoint.h"
#include "MapSpecificOverrides/NeoSylphid.h"
#include "MapSpecificOverrides/Outsider.h"
#include "MapSpecificOverrides/Plasma.h"
#include "MapSpecificOverrides/Roadkill.h"

#include "StartingLocation.h"
#include "Units.h"
#include "PathFinding.h"
#include "Geo.h"
#include "Opponent.h"

#include "NoGoAreas.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_HEATMAPS true
#endif

namespace Map
{
    namespace
    {
        int mapWidth;
        int mapHeight;
        int mapWidthPixels;
        int mapHeightPixels;

        MapSpecificOverride *_mapSpecificOverride;
        std::vector<Base *> bases;
        std::vector<std::unique_ptr<StartingLocation>> startingLocations;

        std::map<const BWEM::ChokePoint *, Choke *> chokes;
        int _minChokeWidth;

        std::map<Base *, std::set<const BWEM::Area *>> startingBaseAreas;

        std::map<const BWEM::Area *, std::set<BWAPI::TilePosition>> areasToEdgePositions;
        std::map<BWAPI::TilePosition, const BWEM::Area *> edgePositionsToArea;

        std::vector<bool> inOwnMineralLine;
        std::vector<bool> borderingMineralPatch;

        std::vector<bool> narrowChokeTiles;
        std::vector<bool> leafAreaTiles;
        std::vector<bool> islandTiles;

        std::vector<int> tileLastSeen;

#if CVIS_HEATMAPS
        std::vector<long> power;
#endif

        struct PlayerBases
        {
            Base *startingMain;
            Base *startingNatural;
            Choke *startingMainChoke;
            Choke *startingNaturalChoke;

            Base *main;
            std::set<Base *> allOwned;
            std::vector<Base *> probableExpansions;
            std::vector<Base *> islandExpansions;

            PlayerBases() : startingMain(nullptr), startingNatural(nullptr), startingMainChoke(nullptr), startingNaturalChoke(nullptr), main(nullptr)
            {}
        };

        std::map<BWAPI::Player, PlayerBases> playerToPlayerBases;

        Choke *computeMainChoke(Base *main, Base *natBase)
        {
            if (!main || !natBase) return nullptr;

            /* Main choke is defined as the last ramp traversed in the path from the main to the natural, or the first */
            /* choke if a ramp is not found */
            auto path = Pathfinder::GetChokePointPath(
                    main->getPosition(),
                    natBase->getPosition(),
                    BWAPI::UnitTypes::Protoss_Dragoon,
                    Pathfinder::PathFindingOptions::UseNearestBWEMArea);
            if (path.empty()) return nullptr;

            for (auto &bwemChoke : std::ranges::reverse_view(path))
            {
                auto c = choke(bwemChoke);
                if (c->isRamp) return c;
            }

            return choke(*path.begin());
        }

        Choke *computeNaturalChoke(Base *main, Base *natBase, Choke *mainChoke)
        {
            if (!main || !natBase || !mainChoke) return nullptr;

            /* Finds the choke out of the natural area that: */
            /* - is not the main choke */
            /* - is not blocked or very narrow */
            /* - goes to the area closest to the map center */
            /* Ties are broken by closest choke to our main */

            auto naturalArea = natBase->getArea();
            auto mapCenter = BWEM::Map::Instance().Center();

            /* Find the closest choke to the area closest to the center */
            auto areaDistBest = INT_MAX;
            auto chokeDistBest = INT_MAX;
            const BWEM::ChokePoint *best = nullptr;
            for (auto& choke : naturalArea->ChokePoints())
            {
                if (choke->Center() == mainChoke->choke->Center()) continue;
                if (choke->Blocked() || choke->Geometry().size() <= 3) continue;

                auto& area = choke->GetAreas().first == naturalArea ? choke->GetAreas().second : choke->GetAreas().first;
                if (!area->Top().isValid()) continue;

                const auto areaDist = BWAPI::Position(area->Top()).getApproxDistance(mapCenter);
                if (areaDist > areaDistBest) continue;

                const auto chokeDist = BWAPI::Position(choke->Center()).getApproxDistance(main->getPosition());
                if (areaDist < areaDistBest || chokeDist < chokeDistBest)
                {
                    best = choke;
                    areaDistBest = areaDist;
                    chokeDistBest = chokeDist;
                }
            }

            if (!best) return nullptr;
            return choke(best);
        }

        int closestBaseDistance(Base *baseLoc, const std::set<Base *> &otherBases)
        {
            int closestDistance = -1;
            for (auto otherBase : otherBases)
            {
                int span = Pathfinder::GetGroundDistance(
                        baseLoc->getPosition(),
                        otherBase->getPosition(),
                        BWAPI::UnitTypes::Protoss_Probe,
                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                if (span >= 0 && (span < closestDistance || closestDistance == -1))
                    closestDistance = span;
            }

            return closestDistance;
        }

        Base *getNaturalForStartLocation(BWAPI::TilePosition startLocation)
        {
            auto startPosition = BWAPI::Position(startLocation) + BWAPI::Position(64, 48);

            /* Natural is the closest base that has gas, unless it is further from a start position than our main */
            /* In this case the map either has a backdoor expansion or two exits from the main, both of which we handle */
            /* via map-specific overrides */

            /* Get the closest base */
            Base *bestNatural = nullptr;
            int bestDist = INT_MAX;
            for (auto &baseLoc : bases)
            {
                if (baseLoc->getTilePosition() == startLocation) continue;
                if (baseLoc->gas == 0) continue;

                int span = Pathfinder::GetGroundDistance(
                        startPosition,
                        baseLoc->getPosition(),
                        BWAPI::UnitTypes::Protoss_Probe,
                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                if (span == -1 || span > bestDist) continue;

                bestDist = span;
                bestNatural = baseLoc;
            }
            if (!bestNatural) return nullptr;

            /* Verify it is closer than the main to all other start locations */
            for (auto otherStartLocation : BWAPI::Broodwar->getStartLocations())
            {
                if (startLocation == otherStartLocation) continue;
                auto otherPosition = BWAPI::Position(otherStartLocation) + BWAPI::Position(64, 48);
                int distFromMain = Pathfinder::GetGroundDistance(
                        startPosition,
                        otherPosition,
                        BWAPI::UnitTypes::Protoss_Probe,
                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                int distFromBase = Pathfinder::GetGroundDistance(
                        bestNatural->getPosition(),
                        otherPosition,
                        BWAPI::UnitTypes::Protoss_Probe,
                        Pathfinder::PathFindingOptions::UseNearestBWEMArea);

                if (distFromMain != -1 && distFromBase != -1 && distFromMain < distFromBase)
                {
                    return nullptr;
                }
            }

            return bestNatural;
        }

        void computeProbableExpansions(BWAPI::Player player, PlayerBases &playerBases)
        {
            playerBases.probableExpansions.clear();
            playerBases.islandExpansions.clear();

            /* If we don't yet know the location of this player's starting base, we can't guess where they will expand */
            if (!playerBases.startingMain) return;

            auto myBases = getMyBases(player);
            auto enemyBases = getEnemyBases(player);

            std::vector<std::tuple<int, Base *, bool>> scoredBases;
            for (auto &baseLoc : bases)
            {
                /* Skip bases that are already taken */
                /* We consider any based owned by a different player than us to be taken */
                /* We consider bases owned by us to be taken if the resource depot exists */
                if (baseLoc->owner &&
                    (baseLoc->owner != BWAPI::Broodwar->self() || player != BWAPI::Broodwar->self()
                     || (baseLoc->resourceDepot)))
                {
                    continue;
                }

                /* Want to be close to our own base */
                int distanceFromUs = closestBaseDistance(baseLoc, myBases);

                /* If the player has no known bases, assume they expanded based on proximity to their starting main */
                if (distanceFromUs == -1)
                {
                    distanceFromUs = Pathfinder::GetGroundDistance(
                            baseLoc->getPosition(),
                            playerBases.startingMain->getPosition(),
                            BWAPI::UnitTypes::Protoss_Probe,
                            Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                }

                /* Might be an island base */
                bool island = (distanceFromUs == -1);
                if (island) distanceFromUs = playerBases.startingMain->getPosition().getApproxDistance(baseLoc->getPosition());

                /* Want to be far from the enemy base. */
                int distanceFromEnemy = closestBaseDistance(baseLoc, enemyBases);
                if (distanceFromEnemy == -1) distanceFromEnemy = 10000;

                /* Initialize score based on distances */
                int score = (distanceFromEnemy * 3) / 2 - distanceFromUs;

                /* Increase score based on available resources */
                score += baseLoc->minerals / 100;
                score += baseLoc->gas / 50;

                scoredBases.emplace_back(score, baseLoc, island);
            }

            if (scoredBases.empty()) return;

            std::sort(scoredBases.begin(), scoredBases.end());
            std::reverse(scoredBases.begin(), scoredBases.end());
            for (auto &scoredBase : scoredBases)
            {
                (std::get<2>(scoredBase) ? playerBases.islandExpansions : playerBases.probableExpansions).push_back(std::get<1>(scoredBase));
            }
        }

        void setBaseOwner(Base *baseLoc, BWAPI::Player owner, Unit resourceDepot = nullptr)
        {
            auto setResourceDepot = [&]()
            {
                if (!resourceDepot) return;

                baseLoc->resourceDepot = resourceDepot;
                if (resourceDepot->player == BWAPI::Broodwar->self())
                {
                    resourceDepot->rawUnit->setRallyPoint(baseLoc->mineralLineCenter);
                }
            };

            if (baseLoc->owner == owner)
            {
                setResourceDepot();
                return;
            }

            /* If the base previously had an owner, remove it from their list of owned bases */
            if (baseLoc->owner)
            {
                auto &playerBases = playerToPlayerBases[baseLoc->owner];
                playerBases.allOwned.erase(baseLoc);

                /* If this was the player's main base, pick the oldest of the player's known owned bases as their new main base */
                if (baseLoc == playerBases.main)
                {
                    playerBases.main = nullptr;
                    int oldestBase = INT_MAX;
                    for (auto &other : playerBases.allOwned)
                    {
                        if (other->island) continue;

                        if (other->ownedSince < oldestBase)
                        {
                            oldestBase = other->ownedSince;
                            playerBases.main = other;
                        }
                    }
                }

                /* If this was our base, remove the mineral line tiles */
                if (baseLoc->owner == BWAPI::Broodwar->self())
                {
                    for (auto tilePos : baseLoc->mineralLineTiles)
                    {
                        inOwnMineralLine[tilePos.x + tilePos.y * mapWidth] = false;
                    }
                    Pathfinder::removeBlockingTiles(baseLoc->mineralLineTiles);
                }
            }

            auto playerLabel = [](BWAPI::Player player)
            {
                return player ? player->getName() : "(none)";
            };
            Telemetry::log() << "Changing base " << baseLoc->getTilePosition() << " owner from " << playerLabel(baseLoc->owner) << " to "
                             << playerLabel(owner);
            AppLog::Get() << "Changing base " << baseLoc->getTilePosition() << " owner from " << playerLabel(baseLoc->owner) << " to "
                       << playerLabel(owner);

            baseLoc->owner = owner;
            baseLoc->ownedSince = frameNow;
            baseLoc->resourceDepot = nullptr;
            setResourceDepot();

            /* If the base has a new owner, add it to their list of owned bases */
            if (owner)
            {
                /* Add to new owner's bases */
                auto &ownerBases = playerToPlayerBases[owner];
                ownerBases.allOwned.insert(baseLoc);

                /* If this base is at a start position and we haven't registered a starting main base for the new owner, do so now */
                if (!ownerBases.startingMain && baseLoc->isStartingBase())
                {
                    ownerBases.startingMain = baseLoc;
                    ownerBases.main = baseLoc;

                    for (auto &startingLocation : startingLocations)
                    {
                        if (startingLocation->main != baseLoc) continue;

                        ownerBases.startingNatural = startingLocation->natBase;
                        ownerBases.startingMainChoke = startingLocation->mainChoke;
                        ownerBases.startingNaturalChoke = startingLocation->naturalChoke;
                        break;
                    }

                    if (owner == BWAPI::Broodwar->enemy())
                    {
                        _mapSpecificOverride->enemyStartingMainDetermined();
                    }
                }

                /* If this player had a starting main, but doesn't currently have a main, set this base as the main */
                if (ownerBases.startingMain && !ownerBases.main && !baseLoc->island)
                {
                    ownerBases.main = baseLoc;
                }

                /* If this is our base, add the mineral line tiles as blocking the navigation grids */
                if (owner == BWAPI::Broodwar->self())
                {
                    for (auto tilePos : baseLoc->mineralLineTiles)
                    {
                        inOwnMineralLine[tilePos.x + tilePos.y * mapWidth] = true;
                    }
                    Pathfinder::addBlockingTiles(baseLoc->mineralLineTiles);
                }
            }

            /* Location of bases affects where all players might expand, so recompute the probable expansions for all players */
            for (auto &playerAndBases : playerToPlayerBases)
            {
                computeProbableExpansions(playerAndBases.first, playerAndBases.second);
            }
        }

        /* Given a newly-discovered building, determine whether it infers something about base ownership */
        /* The general logic is: */
        /* - If we see a building within 10 tiles of the expected position of a resource depot, we consider the base to be */
        /* owned by that player */
        /* - We keep track of the resource depot itself when we see it, so if two players have buildings in the same base, */
        /* the player owning the resource depot is considered to be the owner of the base */
        /* - We infer unknown start positions if we see buildings nearby */
        void inferBaseOwnershipFromUnitCreated(const Unit &gameUnit)
        {
            /* Only consider non-neutral non-flying buildings */
            if (gameUnit->player == BWAPI::Broodwar->neutral()) return;
            if (!gameUnit->type.isBuilding()) return;
            if (gameUnit->isFlying) return;

            /* Only consider base ownership for self with depots */
            if (gameUnit->player == BWAPI::Broodwar->self() && !gameUnit->type.isResourceDepot()) return;

            /* Check if there is a base near the building */
            auto nearbyBase = baseNear(gameUnit->prevPos);
            if (nearbyBase)
            {
                /* If this is an enemy building and the base is our natural, only change the ownership if the building overlaps the depot position */
                /* or is a resource depot */
                if (nearbyBase == Map::getMyNatural() && !gameUnit->type.isResourceDepot() && gameUnit->player != BWAPI::Broodwar->self())
                {
                    if (!Geom::Overlaps(BWAPI::UnitTypes::Protoss_Nexus, nearbyBase->getPosition(), gameUnit->type, gameUnit->prevPos)) return;
                }

                /* Is this unit a resource depot that is closer than the existing resource depot registered for this base? */
                Unit depot = nullptr;
                if (gameUnit->type.isResourceDepot())
                {
                    if (nearbyBase->resourceDepot)
                    {
                        int existingDist = nearbyBase->resourceDepot->prevPos.getApproxDistance(nearbyBase->getPosition());
                        int newDist = gameUnit->prevPos.getApproxDistance(nearbyBase->getPosition());
                        if (newDist < existingDist) depot = gameUnit;
                    }
                    else
                        depot = gameUnit;
                }

                /* If the base was previously unowned, or this is a closer depot, change the ownership */
                if (!nearbyBase->owner || depot)
                {
                    setBaseOwner(nearbyBase, gameUnit->player, depot);
                }
            }

            /* If we haven't found the player's main base yet, determine if this building infers the start location */
            auto &playerBases = playerToPlayerBases[gameUnit->player];
            if (!playerBases.startingMain)
            {
                for (auto &startingLocation : startingLocations)
                {
                    auto &startingLocationBase = startingLocation->main;

                    if (startingLocationBase->owner) continue;

                    int span = Pathfinder::GetGroundDistance(
                            gameUnit->prevPos,
                            startingLocationBase->getPosition(),
                            BWAPI::UnitTypes::Protoss_Probe,
                            Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                    if (span != -1 && span < 1500)
                    {
                        setBaseOwner(startingLocationBase, gameUnit->player);
                        break;
                    }

                    if (startingLocation->natBase)
                    {
                        int naturalDist = Pathfinder::GetGroundDistance(
                                gameUnit->prevPos,
                                startingLocation->natBase->getPosition(),
                                BWAPI::UnitTypes::Protoss_Probe,
                                Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                        if (naturalDist != -1 && naturalDist < 640)
                        {
                            setBaseOwner(startingLocationBase, gameUnit->player);
                            break;
                        }
                    }
                }
            }
        }

        bool checkCreep(Base *baseLoc)
        {
            if (!Opponent::canBeRace(BWAPI::Races::Zerg)) return false;

            for (int x = -8; x <= 11; x++)
            {
                if (x == 0) x = 4;
                for (int y = -5; y <= 7; y++)
                {
                    if (y == 0) y = 2;

                    int tileX = baseLoc->getTilePosition().x + x;
                    int tileY = baseLoc->getTilePosition().y + y;
                    if (tileX < 0 || tileX >= mapWidth || tileY < 0 || tileY >= mapHeight) continue;
                    if (BWAPI::Broodwar->hasCreep(tileX, tileY))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        void validateBaseOwnership(Base *baseLoc, Unit recentlyDestroyedBuilding = nullptr)
        {
            if (!baseLoc->owner) return;
            if (baseLoc->lastScouted == -1) return;

            auto isNearbyBuilding = [&recentlyDestroyedBuilding, &baseLoc](const Unit &gameUnit)
            {
                if (recentlyDestroyedBuilding && gameUnit->ident == recentlyDestroyedBuilding->ident) return false;
                if (!gameUnit->type.isBuilding()) return false;
                if (!gameUnit->lastPositionValid) return false;
                return baseLoc->getPosition().getApproxDistance(gameUnit->prevPos) < 320;
            };

            if (baseLoc->owner == BWAPI::Broodwar->self())
            {
                for (auto &gameUnit : UnitMgr::allMine())
                {
                    if (isNearbyBuilding(gameUnit)) return;
                }
            }
            else
            {
                /* For the case where we are doing periodic re-evaluation, assume the base is still owned */
                /* by the enemy if it has creep */
                if (!recentlyDestroyedBuilding && checkCreep(baseLoc)) return;

                for (auto &gameUnit : UnitMgr::allEnemy())
                {
                    if (isNearbyBuilding(gameUnit)) return;
                }
            }

            setBaseOwner(baseLoc, nullptr);
        }

        /* Given a newly-destroyed building, determine whether it infers something about base ownership. */
        /* If the lost unit is a building near a base owned by the unit's owner, change the base ownership */
        /* if the player has no buildings left near the base. */
        void inferBaseOwnershipFromUnitDestroyed(const Unit &gameUnit)
        {
            /* Only consider buildings */
            if (!gameUnit->type.isBuilding()) return;

            /* Ensure there is a base owned by the player near the building */
            auto nearbyBase = baseNear(gameUnit->prevPos);
            if (!nearbyBase || !nearbyBase->owner || nearbyBase->owner != gameUnit->player) return;

            /* Clear dead resource depot */
            if (nearbyBase->resourceDepot == gameUnit) nearbyBase->resourceDepot = nullptr;

            /* Check if the destruction of the unit results in the base ownership changing */
            validateBaseOwnership(nearbyBase, gameUnit);
        }

        void computeNarrowChokeTiles()
        {
            narrowChokeTiles.resize(mapWidth * mapHeight);

            for (const auto &pair : chokes)
            {
                if (!pair.second->isNarrowChoke) continue;

                for (const auto &chokeTile : pair.second->chokeTiles)
                {
                    narrowChokeTiles[chokeTile.x + chokeTile.y * mapWidth] = true;
                }
            }

#if CVIS_HEATMAPS
            /* Dump to CherryVis */
            std::vector<long> narrowChokeTilesCVis(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    narrowChokeTilesCVis[x + y * mapWidth] = narrowChokeTiles[x + y * mapWidth];
                }
            }

            Telemetry::addHeatmap("NarrowChokeTiles", narrowChokeTilesCVis, mapWidth, mapHeight);
#endif
        }

        void computeIslandTiles()
        {
            /* Gather all "island" areas, which are areas not ground-connected to our start location */
            std::set<const BWEM::Area *> islandAreas;
            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                auto span = Pathfinder::GetGroundDistance(BWAPI::Position(area.Top()),
                                                           BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));
                if (span == -1)
                {
                    islandAreas.insert(&area);
                }
            }
            _mapSpecificOverride->addIslandAreas(islandAreas);

            islandTiles.resize(mapWidth * mapHeight);

            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    BWAPI::TilePosition here(x, y);
                    auto area = BWEM::Map::Instance().GetArea(here);
                    if (area)
                    {
                        if (islandAreas.contains(area))
                        {
                            islandTiles[here.x + here.y * mapWidth] = true;
                        }
                    }
                }
            }

#if CVIS_HEATMAPS
            /* Dump to CherryVis */
            std::vector<long> islandTilesCVis(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    islandTilesCVis[x + y * mapWidth] = islandTiles[x + y * mapWidth];
                }
            }

            Telemetry::addHeatmap("IslandTiles", islandTilesCVis, mapWidth, mapHeight);
#endif
        }

        void computeLeafAreaTiles()
        {
            /* Gather all "leaf" areas, which we define as areas that are only connected by narrow or blocked chokes */
            /* We also always add our starting main areas as leaf areas */
            std::set<const BWEM::Area *> leafAreas;
            auto &myStartingMainAreas = startingBaseAreas[getMyMain()];
            leafAreas.insert(myStartingMainAreas.begin(), myStartingMainAreas.end());
            auto isIslandArea = [](const BWEM::Area *area)
            {
                return isOnIsland(BWAPI::TilePosition(area->Top()));
            };
            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                if (isIslandArea(&area)) continue;

                for (const auto &bwemChoke : area.ChokePoints())
                {
                    auto choke = Map::choke(bwemChoke);
                    if (!choke) continue;
                    if (!choke->isNarrowChoke && !isIslandArea(bwemChoke->GetAreas().first) && !isIslandArea(bwemChoke->GetAreas().second))
                    {
                        goto NextArea;
                    }
                }

                leafAreas.insert(&area);

                NextArea:;
            }

            leafAreaTiles.resize(mapWidth * mapHeight);

            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    BWAPI::TilePosition here(x, y);
                    auto area = BWEM::Map::Instance().GetArea(here);
                    if (area)
                    {
                        if (leafAreas.contains(area))
                        {
                            leafAreaTiles[here.x + here.y * mapWidth] = true;
                        }
                    }
                }
            }

#if CVIS_HEATMAPS
            /* Dump to CherryVis */
            std::vector<long> leafAreaTilesCVis(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    leafAreaTilesCVis[x + y * mapWidth] = leafAreaTiles[x + y * mapWidth];
                }
            }

            Telemetry::addHeatmap("LeafAreaTiles", leafAreaTilesCVis, mapWidth, mapHeight);
#endif
        }

        /* Dumps heatmaps for static map things like ground height */
        void dumpStaticHeatmaps()
        {
#if CVIS_HEATMAPS
            /* Ground height is at tile resolution */
            std::vector<long> groundHeight(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    groundHeight[x + y * mapWidth] = BWAPI::Broodwar->getGroundHeight(x, y);
                }
            }

            Telemetry::addHeatmap("GroundHeight", groundHeight, mapWidth, mapHeight);

            /* Buildability is at tile resolution */
            std::vector<long> buildability(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    buildability[x + y * mapWidth] = BWAPI::Broodwar->isBuildable(x, y);
                }
            }

            Telemetry::addHeatmap("Buildable", buildability, mapWidth, mapHeight);

            /* Walkability is at walk tile resolution */
            std::vector<long> walkability(mapWidth * mapHeight * 16);
            for (int y = 0; y < mapHeight * 4; y++)
            {
                for (int x = 0; x < mapWidth * 4; x++)
                {
                    if (BWAPI::Broodwar->isBuildable(x / 4, y / 4))
                    {
                        walkability[x + y * mapWidth * 4] = 2;
                    }
                    else
                    {
                        walkability[x + y * mapWidth * 4] = BWAPI::Broodwar->isWalkable(x, y);
                    }
                }
            }

            Telemetry::addHeatmap("Walkable", walkability, mapWidth * 4, mapHeight * 4);

            /* Altitude is at walk tile resolution */
            std::vector<long> altitude(mapWidth * mapHeight * 16);
            for (int y = 0; y < mapHeight * 4; y++)
            {
                for (int x = 0; x < mapWidth * 4; x++)
                {
                    altitude[x + y * mapWidth * 4] = BWEM::Map::Instance().GetMiniTile(BWAPI::WalkPosition(x, y)).Altitude();
                }
            }

            Telemetry::addHeatmap("Altitude", altitude, mapWidth * 4, mapHeight * 4);

            /* Edge tiles are at tile resolution */
            std::vector<long> edgeTiles(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    edgeTiles[x + y * mapWidth] = ((!edgePositionsToArea.contains(BWAPI::TilePosition(x, y))) ? 0 : 100);
                }
            }

            Telemetry::addHeatmap("EdgeTiles", edgeTiles, mapWidth, mapHeight);

            /* Mineral lines from all bases */
            std::vector<long> mineralLineCvis(mapWidth * mapHeight);
            for (auto &baseLoc : bases)
            {
                for (int y = 0; y < mapHeight; y++)
                {
                    for (int x = 0; x < mapWidth; x++)
                    {
                        BWAPI::TilePosition here(x, y);

                        if (baseLoc->workerDefenseRallyPatch && baseLoc->workerDefenseRallyPatch->tilePos == here)
                        {
                            mineralLineCvis[x + y * mapWidth] = 10;
                        }
                        else if (BWAPI::TilePosition(baseLoc->mineralLineCenter) == here)
                        {
                            mineralLineCvis[x + y * mapWidth] = 10;
                        }
                        else if (baseLoc->isInMineralLine(here))
                        {
                            mineralLineCvis[x + y * mapWidth] = -10;
                        }
                    }
                }
            }

            Telemetry::addHeatmap("MineralLines", mineralLineCvis, mapWidth, mapHeight);

            /* Bases and resources */
            std::vector<long> basesCvis(mapWidth * mapHeight);
            auto setBaseTiles = [&basesCvis](BWAPI::TilePosition tilePos, BWAPI::TilePosition size, long value)
            {
                auto bottomRight = tilePos + size;

                for (int y = tilePos.y; y < bottomRight.y; y++)
                {
                    for (int x = tilePos.x; x < bottomRight.x; x++)
                    {
                        basesCvis[x + y * mapWidth] = value;
                    }
                }
            };
            int value = 500;
            for (auto &baseLoc : bases)
            {
                setBaseTiles(baseLoc->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus.tileSize(), value);
                for (const auto &patch : baseLoc->mineralPatches())
                {
                    setBaseTiles(patch->tilePos, BWAPI::UnitTypes::Resource_Mineral_Field.tileSize(), value);
                }
                for (const auto &geyser : baseLoc->geysersOrRefineries())
                {
                    setBaseTiles(geyser->tilePos, BWAPI::UnitTypes::Resource_Vespene_Geyser.tileSize(), value);
                }
                value += 20;
            }
            Telemetry::addHeatmap("Bases", basesCvis, mapWidth, mapHeight);

            /* Chokes */
            std::vector<long> chokesCvis(mapWidth * mapHeight * 16);
            for (auto &bwemChokeAndChoke : chokes)
            {
                for (auto pt : bwemChokeAndChoke.first->Geometry())
                {
                    chokesCvis[pt.x + pt.y * mapWidth * 4] = 1;
                }

                BWAPI::WalkPosition midpoint(bwemChokeAndChoke.second->midpoint);
                chokesCvis[midpoint.x + midpoint.y * mapWidth * 4] = 30;

                if (bwemChokeAndChoke.second->isNarrowChoke)
                {
                    BWAPI::WalkPosition end1Center(bwemChokeAndChoke.second->end1Center);
                    BWAPI::WalkPosition end2Center(bwemChokeAndChoke.second->end2Center);
                    BWAPI::WalkPosition end1Exit(bwemChokeAndChoke.second->end1Exit);
                    BWAPI::WalkPosition end2Exit(bwemChokeAndChoke.second->end2Exit);

                    chokesCvis[end1Exit.x + end1Exit.y * mapWidth * 4] = 15;
                    chokesCvis[end2Exit.x + end2Exit.y * mapWidth * 4] = 15;
                    chokesCvis[end1Center.x + end1Center.y * mapWidth * 4] = 20;
                    chokesCvis[end2Center.x + end2Center.y * mapWidth * 4] = 20;
                }

                if (bwemChokeAndChoke.second->highElevationTile.isValid())
                {
                    auto highTile = BWAPI::WalkPosition(bwemChokeAndChoke.second->highElevationTile) + BWAPI::WalkPosition(2, 2);
                    chokesCvis[highTile.x + highTile.y * mapWidth * 4] = 30;
                }
            }

            Telemetry::addHeatmap("Chokes", chokesCvis, mapWidth * 4, mapHeight * 4);

            /* Narrow Chokes */
            std::vector<long> narrowChokesCvis(mapWidth * mapHeight * 16);
            for (auto &bwemChokeAndChoke : chokes)
            {
                auto &choke = bwemChokeAndChoke.second;
                if (!choke->isNarrowChoke) continue;

                BWAPI::WalkPosition midpoint(choke->midpoint);
                narrowChokesCvis[midpoint.x + midpoint.y * mapWidth * 4] = 30;

                BWAPI::WalkPosition end1Center(bwemChokeAndChoke.second->end1Center);
                BWAPI::WalkPosition end2Center(bwemChokeAndChoke.second->end2Center);
                BWAPI::WalkPosition end1Exit(bwemChokeAndChoke.second->end1Exit);
                BWAPI::WalkPosition end2Exit(bwemChokeAndChoke.second->end2Exit);
                narrowChokesCvis[end1Exit.x + end1Exit.y * mapWidth * 4] = 15;
                narrowChokesCvis[end2Exit.x + end2Exit.y * mapWidth * 4] = 15;
                narrowChokesCvis[end1Center.x + end1Center.y * mapWidth * 4] = 20;
                narrowChokesCvis[end2Center.x + end2Center.y * mapWidth * 4] = 20;
            }

            Telemetry::addHeatmap("NarrowChokes", narrowChokesCvis, mapWidth * 4, mapHeight * 4);
#endif
        }
    }

    void initialize()
    {
        mapWidth = BWAPI::Broodwar->mapWidth();
        mapHeight = BWAPI::Broodwar->mapHeight();
        mapWidthPixels = mapWidth * 32;
        mapHeightPixels = mapHeight * 32;

        if (_mapSpecificOverride)
        {
            delete _mapSpecificOverride;
            _mapSpecificOverride = nullptr;
        }
        for (auto iterRef = bases.begin(); iterRef != bases.end(); iterRef = bases.erase(iterRef))
        {
            delete *iterRef;
        }
        startingLocations.clear();
        for (auto iterRef = chokes.begin(); iterRef != chokes.end(); iterRef = chokes.erase(iterRef))
        {
            delete iterRef->second;
        }
        _minChokeWidth = 0;
        startingBaseAreas.clear();
        areasToEdgePositions.clear();
        edgePositionsToArea.clear();
        inOwnMineralLine.clear();
        inOwnMineralLine.resize(mapWidth * mapHeight);
        narrowChokeTiles.clear();
        leafAreaTiles.clear();
        islandTiles.clear();
        tileLastSeen.clear();
        tileLastSeen.assign(mapWidth * mapHeight, -1);

        NoGoAreas::initialize();

#if CVIS_HEATMAPS
        power.clear();
#endif
        playerToPlayerBases.clear();

        /* Initialize BWEM */
        BWEM::Map::ResetInstance();
        BWEM::Map::Instance().Initialize(BWAPI::BroodwarPtr);
        BWEM::Map::Instance().EnableAutomaticPathAnalysis();
        bool bwem = BWEM::Map::Instance().FindBasesForStartingLocations();
        AppLog::Debug() << "Initialized BWEM: " << bwem;

        /* Select map-specific overrides */
        if (BWAPI::Broodwar->mapHash() == "83320e505f35c65324e93510ce2eafbaa71c9aa1")
        {
            /* TODO: Add openbw hash */
            AppLog::Get() << "Using map-specific override for Fortress";
            _mapSpecificOverride = new Fortress();
        }
        else if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" ||
                 BWAPI::Broodwar->mapHash() == "8b3e8ed9ce9620a606319ba6a593ed5c894e51df")
        {
            AppLog::Get() << "Using map-specific override for Plasma";
            _mapSpecificOverride = new Plasma();
        }
        else if (BWAPI::Broodwar->mapHash() == "8000dc6116e405ab878c14bb0f0cde8efa4d640c" ||
                 BWAPI::Broodwar->mapHash() == "9e5770c62b523042e8af590c8dc267e6c12637fc")
        {
            AppLog::Get() << "Using map-specific override for Alchemist";
            _mapSpecificOverride = new Alchemist();
        }
        else if (BWAPI::Broodwar->mapHash() == "63a94b3a878c912f2fa5e31700491a60ac3f29d9" ||
                 BWAPI::Broodwar->mapHash() == "99324782b01af58f6b25aea13e2d62aa83564de0")
        {
            AppLog::Get() << "Using map-specific override for Outsider";
            _mapSpecificOverride = new Outsider();
        }
        else if (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b" ||
                 BWAPI::Broodwar->mapHash() == "e39c1c81740a97a733d227e238bd11df734eaf96")
        {
            AppLog::Get() << "Using map-specific override for Destination";
            _mapSpecificOverride = new Destination();
        }
        else if (BWAPI::Broodwar->mapHash() == "0a41f144c6134a2204f3d47d57cf2afcd8430841" ||
                 BWAPI::Broodwar->mapHash() == "7e14d53b944b1365973f2d8768c75358c6b28a8f")
        {
            AppLog::Get() << "Using map-specific override for Match Point";
            _mapSpecificOverride = new MatchPoint();
        }
        else if (BWAPI::Broodwar->mapHash() == "442e456721c94fd085ecd10230542960d57928d9" ||
                 BWAPI::Broodwar->mapHash() == "83cc5c3944a80915a68190d7b87714d8c0cf8a2f")
        {
            AppLog::Get() << "Using map-specific override for Arcadia";
            _mapSpecificOverride = new Arcadia();
        }
        else if (BWAPI::Broodwar->mapHash() == "bde51d09a2ad733db9e5492354798045db2ced3e" ||
                 BWAPI::Broodwar->mapHash() == "699c33ca1ff8f486d93b288371e57db49a0c403f")
        {
            AppLog::Get() << "Using map-specific override for Colosseum";
            _mapSpecificOverride = new Colosseum();
        }
        else if (BWAPI::Broodwar->mapHash() == "97944269ea55365d13c310f46c9337f5e873dc6c" ||
                 BWAPI::Broodwar->mapHash() == "a8fff0bad1956dba03e234744f2e12f7941a8f8a")
        {
            AppLog::Get() << "Using map-specific override for CrossingField";
            _mapSpecificOverride = new CrossingField();
        }
        else if (BWAPI::Broodwar->mapHash() == "2acb8e8cc2ec9b0911a73f2f29dcce424862dddd" ||
                 BWAPI::Broodwar->mapHash() == "0d592ab56bd5c9230444e48177f150e58fce0a91")
        {
            AppLog::Get() << "Using map-specific override for GodsGarden";
            _mapSpecificOverride = new GodsGarden();
        }
        else if (BWAPI::Broodwar->mapHash() == "2f69eaa1a73bb743934d55e7ea12186fe340e656" ||
                 BWAPI::Broodwar->mapHash() == "a19d3ed890c2919c81a9aff55732d3f602a3323e")
        {
            AppLog::Get() << "Using map-specific override for JudgmentDay";
            _mapSpecificOverride = new JudgmentDay();
        }
        else if (BWAPI::Broodwar->mapHash() == "444b805a88f971c6b2c5f8d2a467de3c1fb2f001" ||
                 BWAPI::Broodwar->mapHash() == "625b16a3a6c861bc6c2f91b709329edb7e8e28aa")
        {
            AppLog::Get() << "Using map-specific override for Katrina";
            _mapSpecificOverride = new Katrina();
        }
        else if (BWAPI::Broodwar->mapHash() == "5386ec02cc3ee913acc55181896c287ae9d5b5c6" ||
                 BWAPI::Broodwar->mapHash() == "2f55a45d8b9cb7ef86c2f688aeeb3c738406beb9")
        {
            AppLog::Get() << "Using map-specific override for Roadkill";
            _mapSpecificOverride = new Roadkill();
        }
        else if (BWAPI::Broodwar->mapHash() == "dbd844012e678b23ca8ef21b3b62008589a554b5" ||
                 BWAPI::Broodwar->mapHash() == "1be2d1d778131323d3a0e7dd2301285c5a9887c9")
        {
            AppLog::Get() << "Using map-specific override for Neo Sylphid";
            _mapSpecificOverride = new NeoSylphid();
        }
        else
        {
            _mapSpecificOverride = new MapSpecificOverride();
        }

        borderingMineralPatch.clear();
        borderingMineralPatch.resize(mapWidth * mapHeight);
        for (auto neutral : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (!neutral->getType().isMineralField()) continue;

            int top = neutral->getTilePosition().y - 1;
            int bottom = neutral->getTilePosition().y + 1;
            for (int x = (neutral->getTilePosition().x - 1); x < (neutral->getTilePosition().x + 3); x++)
            {
                if (x < 0 || x >= mapWidth) continue;
                if (top >= 0) borderingMineralPatch[x + top * mapWidth] = true;
                if (bottom < mapHeight) borderingMineralPatch[x + bottom * mapWidth] = true;
            }

            int left = neutral->getTilePosition().x - 1;
            int right = neutral->getTilePosition().x + 2;
            if (left >= 0) borderingMineralPatch[left + neutral->getTilePosition().y * mapWidth] = true;
            if (right < mapWidth) borderingMineralPatch[right + neutral->getTilePosition().y * mapWidth] = true;
        }
#if CVIS_HEATMAPS
        /* Dump to CherryVis */
        std::vector<long> borderingMineralPatchCVis(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                borderingMineralPatchCVis[x + y * mapWidth] = borderingMineralPatch[x + y * mapWidth] ? 1 : 0;
            }
        }

        Telemetry::addHeatmap("BorderingMineralPatch", borderingMineralPatchCVis, mapWidth, mapHeight);
#endif

        initializeWalkability();

        /* Analyze chokepoints */
        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            for (const BWEM::ChokePoint *choke : area.ChokePoints())
            {
                if (!chokes.contains(choke))
                    chokes.emplace(choke, new Choke(choke));
            }
        }
        _mapSpecificOverride->initializeChokes(chokes);

        /* Compute the minimum choke width */
        _minChokeWidth = INT_MAX;
        for (const auto &pair : chokes)
        {
            if (pair.second->width < _minChokeWidth)
                _minChokeWidth = pair.second->width;
        }

        computeIslandTiles();

        /* Initialize bases */
        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            for (const auto &baseLoc : area.Bases())
            {
                auto newBase = new Base(baseLoc.Location(), &baseLoc);
                bases.push_back(newBase);
            }
        }
        _mapSpecificOverride->modifyBases(bases);
        for (auto &baseLoc : bases)
        {
            if (baseLoc->isStartingBase())
            {
                auto natBase = getNaturalForStartLocation(baseLoc->getTilePosition());
                auto mainChoke = computeMainChoke(baseLoc, natBase);
                auto naturalChoke = computeNaturalChoke(baseLoc, natBase, mainChoke);

                auto startingLocation = std::make_unique<StartingLocation>(baseLoc, natBase, mainChoke, naturalChoke);
                _mapSpecificOverride->modifyStartingLocation(startingLocation);

                startingLocations.emplace_back(std::move(startingLocation));
                if (baseLoc->getTilePosition() == BWAPI::Broodwar->self()->getStartLocation())
                {
                    setBaseOwner(baseLoc, BWAPI::Broodwar->self());
                }
            }
        }
        AppLog::Debug() << "Found " << bases.size() << " bases";

        if (playerToPlayerBases[BWAPI::Broodwar->self()].startingMainChoke)
        {
            playerToPlayerBases[BWAPI::Broodwar->self()].startingMainChoke->setAsMainChoke();
        }

        /* Compute areas for all starting location bases */
        for (auto &startingLocation : startingLocations)
        {
            /* Initialize with the base area */
            startingBaseAreas[startingLocation->main].insert(startingLocation->main->getArea());

            /* Add any areas where the path to the natural goes through the main choke */
            /* TODO: Doesn't work for Alchemist 3 o'clock where we don't have a main choke or natural */

            if (!startingLocation->natBase) continue;
            if (!startingLocation->mainChoke) continue;
            if (_mapSpecificOverride->hasBackdoorNatural()) continue;

            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                if (isOnIsland(BWAPI::TilePosition(area.Top()))) continue;

                bool hasMainChoke = false;
                for (const auto &choke : Pathfinder::GetChokePointPath(
                        BWAPI::Position(area.Top()),
                        startingLocation->natBase->getPosition(),
                        BWAPI::UnitTypes::Protoss_Dragoon,
                        Pathfinder::PathFindingOptions::UseNearestBWEMArea))
                {
                    if (choke == startingLocation->mainChoke->choke)
                    {
                        hasMainChoke = true;
                        break;
                    }
                }

                if (hasMainChoke) startingBaseAreas[startingLocation->main].insert(&area);
            }
        }

        /* Gather the edges of all areas on the map */
        for (int y = 0; y < mapHeight * 4; y++)
        {
            for (int x = 0; x < mapWidth * 4; x++)
            {
                if (!BWAPI::Broodwar->isWalkable(x, y)) continue;

                BWAPI::WalkPosition wp(x, y);

                auto &miniTile = BWEM::Map::Instance().GetMiniTile(wp);
                if (miniTile.Altitude() >= 24 && miniTile.Altitude() < 32 && miniTile.AreaId() > 0)
                {
                    auto area = BWEM::Map::Instance().GetArea(miniTile.AreaId());
                    if (area->MaxAltitude() < 128) continue; // Exclude small / narrow areas

                    BWAPI::TilePosition tp(wp);
                    areasToEdgePositions[area].insert(tp);
                    edgePositionsToArea[tp] = area;
                }
            }
        }

        computeNarrowChokeTiles();
        computeLeafAreaTiles();
        dumpStaticHeatmaps();

        update();
    }

    void onUnitCreated(const Unit &gameUnit)
    {
        /* Whenever we see a new building, determine if it infers a change in base ownership */
        inferBaseOwnershipFromUnitCreated(gameUnit);

        onUnitCreated_Walkability(gameUnit);
    }

    void onUnitDestroy(const Unit &gameUnit)
    {
        /* Whenever a building is lost, determine if it infers a change in base ownership */
        inferBaseOwnershipFromUnitDestroyed(gameUnit);

        onUnitDestroy_Walkability(gameUnit);
    }

    void onUnitDestroy(BWAPI::Unit gameUnit)
    {
        if (gameUnit->getType().isMineralField())
            BWEM::Map::Instance().OnMineralDestroyed(gameUnit);
        else if (gameUnit->getType().isSpecialBuilding())
            BWEM::Map::Instance().OnStaticBuildingDestroyed(gameUnit);

        onUnitDestroy_Walkability(gameUnit);

        _mapSpecificOverride->onUnitDestroy(gameUnit);
    }

    void update()
    {
        /* Update the last seen frame for all visible tiles */
        /* This is fairly expensive and we don't need super high resolution updates, so we split the update over two frames */
        int startY, endY;
        if (frameNow % 2 == 0)
        {
            startY = 0;
            endY = mapHeight >> 1;
        }
        else
        {
            startY = mapHeight >> 1;
            endY = mapHeight;
        }
        for (int y = startY; y < endY; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                if (BWAPI::Broodwar->isVisible(x, y))
                {
                    tileLastSeen[x + y * mapWidth] = frameNow;
                }
            }
        }

        /* Update bases, base scouting and resource depot */
        for (auto &baseLoc : bases)
        {
            baseLoc->update();

            /* If the resource depot is killed, reset it */
            if (baseLoc->resourceDepot && !baseLoc->resourceDepot->exists())
            {
                baseLoc->resourceDepot = nullptr;
            }

            /* Owned bases were scouted when we last saw each of the tiles around where the resource depot should be */
            /* Unowned bases were scouted when we last saw one of the center tiles of where the resource depot should be */
            auto tilePos = baseLoc->getTilePosition();
            if (baseLoc->owner)
            {
                baseLoc->lastScouted = std::min({
                     tileLastSeen[tilePos.x - 1 + (tilePos.y - 1) * mapWidth],
                     tileLastSeen[tilePos.x + (tilePos.y - 1) * mapWidth],
                     tileLastSeen[tilePos.x + 1 + (tilePos.y - 1) * mapWidth],
                     tileLastSeen[tilePos.x + 2 + (tilePos.y - 1) * mapWidth],
                     tileLastSeen[tilePos.x + 3 + (tilePos.y - 1) * mapWidth],
                     tileLastSeen[tilePos.x + 4 + (tilePos.y - 1) * mapWidth],
                     tileLastSeen[tilePos.x - 1 + tilePos.y * mapWidth],
                     tileLastSeen[tilePos.x + 4 + tilePos.y * mapWidth],
                     tileLastSeen[tilePos.x - 1 + (tilePos.y + 1) * mapWidth],
                     tileLastSeen[tilePos.x + 4 + (tilePos.y + 1) * mapWidth],
                     tileLastSeen[tilePos.x - 1 + (tilePos.y + 2) * mapWidth],
                     tileLastSeen[tilePos.x + 4 + (tilePos.y + 2) * mapWidth],
                     tileLastSeen[tilePos.x - 1 + (tilePos.y + 3) * mapWidth],
                     tileLastSeen[tilePos.x + (tilePos.y + 3) * mapWidth],
                     tileLastSeen[tilePos.x + 1 + (tilePos.y + 3) * mapWidth],
                     tileLastSeen[tilePos.x + 2 + (tilePos.y + 3) * mapWidth],
                     tileLastSeen[tilePos.x + 3 + (tilePos.y + 3) * mapWidth],
                     tileLastSeen[tilePos.x + 4 + (tilePos.y + 3) * mapWidth],
                });
            }
            else
            {
                baseLoc->lastScouted = std::max(
                        tileLastSeen[tilePos.x + 1 + (tilePos.y + 1) * mapWidth],
                        tileLastSeen[tilePos.x + 2 + (tilePos.y + 1) * mapWidth]);

                /* Check for creep */
                if ((baseLoc->ownedSince == -1 || baseLoc->ownedSince < (frameNow - 2500)) && checkCreep(baseLoc))
                {
                    setBaseOwner(baseLoc, BWAPI::Broodwar->enemy());
                }
            }
        }

        /* Infer single-enemy base ownership from which starting locations are scouted */
        if (BWAPI::Broodwar->enemies().size() == 1 &&
            !playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMain)
        {
            auto unscouted = unscoutedStartingLocations();
            if (unscouted.size() == 1)
            {
                setBaseOwner(*unscouted.begin(), BWAPI::Broodwar->enemy());
            }
        }

        /* Periodically check if base ownership is out-of-sync for any bases */
        if (frameNow % 24 == 17)
        {
            for (auto &baseLoc : bases)
            {
                validateBaseOwnership(baseLoc);
            }
        }

        dumpWalkability();

        NoGoAreas::update();
    }

    MapSpecificOverride *mapSpecificOverride()
    {
        return _mapSpecificOverride;
    }

    std::vector<Base *> &allBases()
    {
        return bases;
    }

    std::set<Base *> &getMyBases(BWAPI::Player player)
    {
        return playerToPlayerBases[player].allOwned;
    }

    std::set<Base *> getEnemyBases(BWAPI::Player player)
    {
        std::set<Base *> outcome;
        for (auto &baseLoc : bases)
        {
            if (!baseLoc->owner) continue;
            if (baseLoc->owner->isEnemy(player)) outcome.insert(baseLoc);
        }

        return outcome;
    }

    std::vector<Base *> &getUntakenExpansions(BWAPI::Player player)
    {
        return playerToPlayerBases[player].probableExpansions;
    }

    std::vector<Base *> &getUntakenIslandExpansions(BWAPI::Player player)
    {
        return playerToPlayerBases[player].islandExpansions;
    }

    Base *getMyMain()
    {
        return playerToPlayerBases[BWAPI::Broodwar->self()].startingMain;
    }

    Base *getMyNatural()
    {
        return playerToPlayerBases[BWAPI::Broodwar->self()].startingNatural;
    }

    void setMyNatural(Base *baseLoc)
    {
        playerToPlayerBases[BWAPI::Broodwar->self()].startingNatural = baseLoc;
        AppLog::Get() << "Set my natural to " << baseLoc->getTilePosition();
    }

    Base *getEnemyStartingMain()
    {
        return playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMain;
    }

    Base *getEnemyStartingNatural()
    {
        return playerToPlayerBases[BWAPI::Broodwar->enemy()].startingNatural;
    }

    Base *getEnemyMain()
    {
        return playerToPlayerBases[BWAPI::Broodwar->enemy()].main;
    }

    void setEnemyStartingMain(Base *baseLoc)
    {
        if (baseLoc->owner) return;

        setBaseOwner(baseLoc, BWAPI::Broodwar->enemy());
    }

    void setEnemyStartingNatural(Base *baseLoc)
    {
        playerToPlayerBases[BWAPI::Broodwar->enemy()].startingNatural = baseLoc;
        AppLog::Get() << "Set enemy natural to " << baseLoc->getTilePosition();
    }

    Base *getHiddenBase()
    {
        auto myMain = playerToPlayerBases[BWAPI::Broodwar->self()].startingMain;
        auto enemyMain = playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMain;
        if (!enemyMain) return nullptr;

        Base *best = nullptr;
        int bestScore = -1;
        for (auto &baseLoc : bases)
        {
            if (baseLoc->owner) continue;
            if (baseLoc->gas < 2000) continue;

            int ourDist = Pathfinder::GetGroundDistance(
                    baseLoc->getPosition(),
                    myMain->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    Pathfinder::PathFindingOptions::UseNearestBWEMArea);
            if (ourDist == -1) continue;

            int enemyDist = Pathfinder::GetGroundDistance(
                    baseLoc->getPosition(),
                    enemyMain->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    Pathfinder::PathFindingOptions::UseNearestBWEMArea);
            if (enemyDist == -1) enemyDist = 10000;

            /* Pick the base with the longest total distance */
            int score = ourDist + enemyDist;
            if (score > bestScore)
            {
                bestScore = score;
                best = baseLoc;
            }
        }

        return best;
    }

    Base *baseNear(BWAPI::Position position)
    {
        auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(position));
        if (!area) return nullptr;

        int closestDist = INT_MAX;
        Base *outcome = nullptr;

        for (auto &baseLoc : bases)
        {
            if (baseLoc->getArea() != area) continue;

            int span = baseLoc->getPosition().getApproxDistance(position);
            if (span < 500 && span < closestDist)
            {
                closestDist = span;
                outcome = baseLoc;
            }
        }

        return outcome;
    }

    std::vector<Base *> allStartingLocations()
    {
        std::vector<Base *> outcome;
        for (auto &startingLocation : startingLocations)
        {
            outcome.push_back(startingLocation->main);
        }
        return outcome;
    }

    std::set<Base *> unscoutedStartingLocations()
    {
        std::set<Base *> outcome;
        for (auto &startingLocation : startingLocations)
        {
            if (startingLocation->main->lastScouted >= 0) continue;

            outcome.insert(startingLocation->main);
        }

        return outcome;
    }

    std::vector<Choke *> allChokes()
    {
        std::vector<Choke *> outcome;
        outcome.reserve(chokes.size());
        for (auto &pair : chokes)
        {
            outcome.push_back(pair.second);
        }
        return outcome;
    }

    Choke *chokeNear(BWAPI::Position pt, int maxDistance)
    {
        int bestDist = maxDistance + 1;
        Choke *best = nullptr;
        for (auto &[_, choke] : chokes)
        {
            int span = choke->midpoint.getApproxDistance(pt);
            if (span < bestDist)
            {
                bestDist = span;
                best = choke;
            }
        }
        return best;
    }

    Choke *choke(const BWEM::ChokePoint *bwemChoke)
    {
        if (!bwemChoke) return nullptr;
        auto iterRef = chokes.find(bwemChoke);
        return iterRef == chokes.end()
               ? nullptr
               : iterRef->second;
    }

    Choke *getMyMainChoke()
    {
        return playerToPlayerBases[BWAPI::Broodwar->self()].startingMainChoke;
    }

    Choke *getMyNaturalChoke()
    {
        return playerToPlayerBases[BWAPI::Broodwar->self()].startingNaturalChoke;
    }

    void setMyMainChoke(Choke *choke)
    {
        choke->setAsMainChoke();
        playerToPlayerBases[BWAPI::Broodwar->self()].startingMainChoke = choke;
        AppLog::Get() << "Set my main choke to " << BWAPI::TilePosition(choke->midpoint);
        BuildingPlacement::onMainChokeChanged();
    }

    Choke *getEnemyMainChoke()
    {
        return playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMainChoke;
    }

    void setEnemyMainChoke(Choke *choke)
    {
        playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMainChoke = choke;
        AppLog::Get() << "Set enemy main choke to " << BWAPI::TilePosition(choke->midpoint);
    }

    int minChokeWidth()
    {
        return _minChokeWidth;
    }

    std::set<const BWEM::Area *> &getMyMainAreas()
    {
        return startingBaseAreas[getMyMain()];
    }

    std::set<const BWEM::Area *> &getStartingBaseAreas(Base *baseLoc)
    {
        return startingBaseAreas[baseLoc];
    }

    Base *getStartingBaseNatural(Base *baseLoc)
    {
        for (auto &startingLocation : startingLocations)
        {
            if (startingLocation->main != baseLoc) continue;

            return startingLocation->natBase;
        }

        return nullptr;
    }

    std::pair<Choke*, Choke*> getStartingBaseChokes(Base *baseLoc)
    {
        for (auto &startingLocation : startingLocations)
        {
            if (startingLocation->main != baseLoc) continue;

            return std::make_pair(startingLocation->mainChoke, startingLocation->naturalChoke);
        }

        return std::make_pair<Choke *, Choke *>(nullptr, nullptr);
    }

    std::map<const BWEM::Area *, std::set<BWAPI::TilePosition>> &getAreasToEdgePositions()
    {
        return areasToEdgePositions;
    }

    std::map<BWAPI::TilePosition, const BWEM::Area *> &getEdgePositionsToArea()
    {
        return edgePositionsToArea;
    }

    void dumpPowerHeatmap()
    {
#if CVIS_HEATMAPS
        std::vector<long> newPower(mapWidth * mapHeight, 0);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                if (BWAPI::Broodwar->hasPower(x, y, BWAPI::UnitTypes::Protoss_Photon_Cannon))
                    newPower[x + y * mapWidth] = 1;
            }
        }

        if (newPower != power)
        {
            Telemetry::addHeatmap("Power", newPower, mapWidth, mapHeight);
            power = newPower;
        }
#endif
    }

    bool isInOwnMineralLine(BWAPI::TilePosition tilePos)
    {
        return isInOwnMineralLine(tilePos.x, tilePos.y);
    }

    bool isInOwnMineralLine(int x, int y)
    {
        return inOwnMineralLine[x + y * mapWidth];
    }

    bool bordersMineralPatch(int x, int y)
    {
        return borderingMineralPatch[x + y * mapWidth];
    }

    bool isInNarrowChoke(BWAPI::TilePosition pt)
    {
        return narrowChokeTiles[pt.x + pt.y * mapWidth];
    }

    bool isInLeafArea(BWAPI::TilePosition pt)
    {
        return leafAreaTiles[pt.x + pt.y * mapWidth];
    }

    bool isOnIsland(BWAPI::TilePosition pt)
    {
        return islandTiles[pt.x + pt.y * mapWidth];
    }

    int lastSeen(BWAPI::TilePosition tilePos)
    {
        return tileLastSeen[tilePos.x + tilePos.y * mapWidth];
    }

    int lastSeen(int x, int y)
    {
        return tileLastSeen[x + y * mapWidth];
    }

    void makePositionValid(int &x, int &y)
    {
        x = std::clamp(x, 0, mapWidthPixels - 1);
        y = std::clamp(y, 0, mapHeightPixels - 1);
    }
}
