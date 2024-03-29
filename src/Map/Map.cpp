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
#include "MapSpecificOverrides/Outsider.h"
#include "MapSpecificOverrides/Plasma.h"

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

        Choke *computeMainChoke(Base *main, Base *natural)
        {
            if (!main || !natural) return nullptr;

            // Main choke is defined as the last ramp traversed in the path from the main to the natural, or the first
            // choke if a ramp is not found
            auto path = PathFinding::GetChokePointPath(
                    main->getPosition(),
                    natural->getPosition(),
                    BWAPI::UnitTypes::Protoss_Dragoon,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (path.empty()) return nullptr;

            for (auto &bwemChoke : std::ranges::reverse_view(path))
            {
                auto c = choke(bwemChoke);
                if (c->isRamp) return c;
            }

            return choke(*path.begin());
        }

        Choke *computeNaturalChoke(Base *main, Base *natural, Choke *mainChoke)
        {
            if (!main || !natural || !mainChoke) return nullptr;

            // Finds the choke out of the natural area that:
            // - is not the main choke
            // - is not blocked or very narrow
            // - goes to the area closest to the map center
            // Ties are broken by closest choke to our main

            auto naturalArea = natural->getArea();
            auto mapCenter = BWEM::Map::Instance().Center();

            // Find the closest choke to the area closest to the center
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

        int closestBaseDistance(Base *base, const std::set<Base *> &otherBases)
        {
            int closestDistance = -1;
            for (auto otherBase : otherBases)
            {
                int dist = PathFinding::GetGroundDistance(
                        base->getPosition(),
                        otherBase->getPosition(),
                        BWAPI::UnitTypes::Protoss_Probe,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
                if (dist >= 0 && (dist < closestDistance || closestDistance == -1))
                    closestDistance = dist;
            }

            return closestDistance;
        }

        Base *getNaturalForStartLocation(BWAPI::TilePosition startLocation)
        {
            auto startPosition = BWAPI::Position(startLocation) + BWAPI::Position(64, 48);

            // Natural is the closest base that has gas, unless it is further from a start position than our main
            // In this case the map either has a backdoor expansion or two exits from the main, both of which we handle
            // via map-specific overrides

            // Get the closest base
            Base *bestNatural = nullptr;
            int bestDist = INT_MAX;
            for (auto &base : bases)
            {
                if (base->getTilePosition() == startLocation) continue;
                if (base->gas == 0) continue;

                int dist = PathFinding::GetGroundDistance(
                        startPosition,
                        base->getPosition(),
                        BWAPI::UnitTypes::Protoss_Probe,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
                if (dist == -1 || dist > bestDist) continue;

                bestDist = dist;
                bestNatural = base;
            }
            if (!bestNatural) return nullptr;

            // Verify it is closer than the main to all other start locations
            for (auto otherStartLocation : BWAPI::Broodwar->getStartLocations())
            {
                if (startLocation == otherStartLocation) continue;
                auto otherPosition = BWAPI::Position(otherStartLocation) + BWAPI::Position(64, 48);
                int distFromMain = PathFinding::GetGroundDistance(
                        startPosition,
                        otherPosition,
                        BWAPI::UnitTypes::Protoss_Probe,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
                int distFromBase = PathFinding::GetGroundDistance(
                        bestNatural->getPosition(),
                        otherPosition,
                        BWAPI::UnitTypes::Protoss_Probe,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea);

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

            // If we don't yet know the location of this player's starting base, we can't guess where they will expand
            if (!playerBases.startingMain) return;

            auto myBases = getMyBases(player);
            auto enemyBases = getEnemyBases(player);

            std::vector<std::tuple<int, Base *, bool>> scoredBases;
            for (auto &base : bases)
            {
                // Skip bases that are already taken
                // We consider any based owned by a different player than us to be taken
                // We consider bases owned by us to be taken if the resource depot exists
                if (base->owner &&
                    (base->owner != BWAPI::Broodwar->self() || player != BWAPI::Broodwar->self()
                     || (base->resourceDepot)))
                {
                    continue;
                }

                // Want to be close to our own base
                int distanceFromUs = closestBaseDistance(base, myBases);

                // If the player has no known bases, assume they expanded based on proximity to their starting main
                if (distanceFromUs == -1)
                {
                    distanceFromUs = PathFinding::GetGroundDistance(
                            base->getPosition(),
                            playerBases.startingMain->getPosition(),
                            BWAPI::UnitTypes::Protoss_Probe,
                            PathFinding::PathFindingOptions::UseNearestBWEMArea);
                }

                // Might be an island base
                bool island = (distanceFromUs == -1);
                if (island) distanceFromUs = playerBases.startingMain->getPosition().getApproxDistance(base->getPosition());

                // Want to be far from the enemy base.
                int distanceFromEnemy = closestBaseDistance(base, enemyBases);
                if (distanceFromEnemy == -1) distanceFromEnemy = 10000;

                // Initialize score based on distances
                int score = (distanceFromEnemy * 3) / 2 - distanceFromUs;

                // Increase score based on available resources
                score += base->minerals / 100;
                score += base->gas / 50;

                scoredBases.emplace_back(score, base, island);
            }

            if (scoredBases.empty()) return;

            std::sort(scoredBases.begin(), scoredBases.end());
            std::reverse(scoredBases.begin(), scoredBases.end());
            for (auto &scoredBase : scoredBases)
            {
                (std::get<2>(scoredBase) ? playerBases.islandExpansions : playerBases.probableExpansions).push_back(std::get<1>(scoredBase));
            }
        }

        void setBaseOwner(Base *base, BWAPI::Player owner, Unit resourceDepot = nullptr)
        {
            auto setResourceDepot = [&]()
            {
                if (!resourceDepot) return;

                base->resourceDepot = resourceDepot;
                if (resourceDepot->player == BWAPI::Broodwar->self())
                {
                    resourceDepot->bwapiUnit->setRallyPoint(base->mineralLineCenter);
                }
            };

            if (base->owner == owner)
            {
                setResourceDepot();
                return;
            }

            // If the base previously had an owner, remove it from their list of owned bases
            if (base->owner)
            {
                auto &playerBases = playerToPlayerBases[base->owner];
                playerBases.allOwned.erase(base);

                // If this was the player's main base, pick the oldest of the player's known owned bases as their new main base
                if (base == playerBases.main)
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

                // If this was our base, remove the mineral line tiles
                if (base->owner == BWAPI::Broodwar->self())
                {
                    for (auto tile : base->mineralLineTiles)
                    {
                        inOwnMineralLine[tile.x + tile.y * mapWidth] = false;
                    }
                    PathFinding::removeBlockingTiles(base->mineralLineTiles);
                }
            }

            auto playerLabel = [](BWAPI::Player player)
            {
                return player ? player->getName() : "(none)";
            };
            CherryVis::log() << "Changing base " << base->getTilePosition() << " owner from " << playerLabel(base->owner) << " to "
                             << playerLabel(owner);
            Log::Get() << "Changing base " << base->getTilePosition() << " owner from " << playerLabel(base->owner) << " to "
                       << playerLabel(owner);

            base->owner = owner;
            base->ownedSince = currentFrame;
            base->resourceDepot = nullptr;
            setResourceDepot();

            // If the base has a new owner, add it to their list of owned bases
            if (owner)
            {
                // Add to new owner's bases
                auto &ownerBases = playerToPlayerBases[owner];
                ownerBases.allOwned.insert(base);

                // If this base is at a start position and we haven't registered a starting main base for the new owner, do so now
                if (!ownerBases.startingMain && base->isStartingBase())
                {
                    ownerBases.startingMain = base;
                    ownerBases.main = base;

                    for (auto &startingLocation : startingLocations)
                    {
                        if (startingLocation->main != base) continue;

                        ownerBases.startingNatural = startingLocation->natural;
                        ownerBases.startingMainChoke = startingLocation->mainChoke;
                        ownerBases.startingNaturalChoke = startingLocation->naturalChoke;
                        break;
                    }

                    if (owner == BWAPI::Broodwar->enemy())
                    {
                        _mapSpecificOverride->enemyStartingMainDetermined();
                    }
                }

                // If this player had a starting main, but doesn't currently have a main, set this base as the main
                if (ownerBases.startingMain && !ownerBases.main && !base->island)
                {
                    ownerBases.main = base;
                }

                // If this is our base, add the mineral line tiles as blocking the navigation grids
                if (owner == BWAPI::Broodwar->self())
                {
                    for (auto tile : base->mineralLineTiles)
                    {
                        inOwnMineralLine[tile.x + tile.y * mapWidth] = true;
                    }
                    PathFinding::addBlockingTiles(base->mineralLineTiles);
                }
            }

            // Location of bases affects where all players might expand, so recompute the probable expansions for all players
            for (auto &playerAndBases : playerToPlayerBases)
            {
                computeProbableExpansions(playerAndBases.first, playerAndBases.second);
            }
        }

        // Given a newly-discovered building, determine whether it infers something about base ownership
        // The general logic is:
        // - If we see a building within 10 tiles of the expected position of a resource depot, we consider the base to be
        //   owned by that player
        // - We keep track of the resource depot itself when we see it, so if two players have buildings in the same base,
        //   the player owning the resource depot is considered to be the owner of the base
        // - We infer unknown start positions if we see buildings nearby
        void inferBaseOwnershipFromUnitCreated(const Unit &unit)
        {
            // Only consider non-neutral non-flying buildings
            if (unit->player == BWAPI::Broodwar->neutral()) return;
            if (!unit->type.isBuilding()) return;
            if (unit->isFlying) return;

            // Only consider base ownership for self with depots
            if (unit->player == BWAPI::Broodwar->self() && !unit->type.isResourceDepot()) return;

            // Check if there is a base near the building
            auto nearbyBase = baseNear(unit->lastPosition);
            if (nearbyBase)
            {
                // If this is an enemy building and the base is our natural, only change the ownership if the building overlaps the depot position
                // or is a resource depot
                if (nearbyBase == Map::getMyNatural() && !unit->type.isResourceDepot() && unit->player != BWAPI::Broodwar->self())
                {
                    if (!Geo::Overlaps(BWAPI::UnitTypes::Protoss_Nexus, nearbyBase->getPosition(), unit->type, unit->lastPosition)) return;
                }

                // Is this unit a resource depot that is closer than the existing resource depot registered for this base?
                Unit depot = nullptr;
                if (unit->type.isResourceDepot())
                {
                    if (nearbyBase->resourceDepot)
                    {
                        int existingDist = nearbyBase->resourceDepot->lastPosition.getApproxDistance(nearbyBase->getPosition());
                        int newDist = unit->lastPosition.getApproxDistance(nearbyBase->getPosition());
                        if (newDist < existingDist) depot = unit;
                    }
                    else
                        depot = unit;
                }

                // If the base was previously unowned, or this is a closer depot, change the ownership
                if (!nearbyBase->owner || depot)
                {
                    setBaseOwner(nearbyBase, unit->player, depot);
                }
            }

            // If we haven't found the player's main base yet, determine if this building infers the start location
            auto &playerBases = playerToPlayerBases[unit->player];
            if (!playerBases.startingMain)
            {
                for (auto &startingLocation : startingLocations)
                {
                    auto &startingLocationBase = startingLocation->main;

                    if (startingLocationBase->owner) continue;

                    int dist = PathFinding::GetGroundDistance(
                            unit->lastPosition,
                            startingLocationBase->getPosition(),
                            BWAPI::UnitTypes::Protoss_Probe,
                            PathFinding::PathFindingOptions::UseNearestBWEMArea);
                    if (dist != -1 && dist < 1500)
                    {
                        setBaseOwner(startingLocationBase, unit->player);
                        break;
                    }

                    if (startingLocation->natural)
                    {
                        int naturalDist = PathFinding::GetGroundDistance(
                                unit->lastPosition,
                                startingLocation->natural->getPosition(),
                                BWAPI::UnitTypes::Protoss_Probe,
                                PathFinding::PathFindingOptions::UseNearestBWEMArea);
                        if (naturalDist != -1 && naturalDist < 640)
                        {
                            setBaseOwner(startingLocationBase, unit->player);
                            break;
                        }
                    }
                }
            }
        }

        bool checkCreep(Base *base)
        {
            if (!Opponent::canBeRace(BWAPI::Races::Zerg)) return false;

            for (int x = -8; x <= 11; x++)
            {
                if (x == 0) x = 4;
                for (int y = -5; y <= 7; y++)
                {
                    if (y == 0) y = 2;

                    int tileX = base->getTilePosition().x + x;
                    int tileY = base->getTilePosition().y + y;
                    if (tileX < 0 || tileX >= mapWidth || tileY < 0 || tileY >= mapHeight) continue;
                    if (BWAPI::Broodwar->hasCreep(tileX, tileY))
                    {
                        return true;
                    }
                }
            }

            return false;
        }

        void validateBaseOwnership(Base *base, Unit recentlyDestroyedBuilding = nullptr)
        {
            if (!base->owner) return;
            if (base->lastScouted == -1) return;

            auto isNearbyBuilding = [&recentlyDestroyedBuilding, &base](const Unit &unit)
            {
                if (recentlyDestroyedBuilding && unit->id == recentlyDestroyedBuilding->id) return false;
                if (!unit->type.isBuilding()) return false;
                if (!unit->lastPositionValid) return false;
                return base->getPosition().getApproxDistance(unit->lastPosition) < 320;
            };

            if (base->owner == BWAPI::Broodwar->self())
            {
                for (auto &unit : Units::allMine())
                {
                    if (isNearbyBuilding(unit)) return;
                }
            }
            else
            {
                // For the case where we are doing periodic re-evaluation, assume the base is still owned
                // by the enemy if it has creep
                if (!recentlyDestroyedBuilding && checkCreep(base)) return;

                for (auto &unit : Units::allEnemy())
                {
                    if (isNearbyBuilding(unit)) return;
                }
            }

            setBaseOwner(base, nullptr);
        }

        // Given a newly-destroyed building, determine whether it infers something about base ownership.
        // If the lost unit is a building near a base owned by the unit's owner, change the base ownership
        // if the player has no buildings left near the base.
        void inferBaseOwnershipFromUnitDestroyed(const Unit &unit)
        {
            // Only consider buildings
            if (!unit->type.isBuilding()) return;

            // Ensure there is a base owned by the player near the building
            auto nearbyBase = baseNear(unit->lastPosition);
            if (!nearbyBase || !nearbyBase->owner || nearbyBase->owner != unit->player) return;

            // Clear dead resource depot
            if (nearbyBase->resourceDepot == unit) nearbyBase->resourceDepot = nullptr;

            // Check if the destruction of the unit results in the base ownership changing
            validateBaseOwnership(nearbyBase, unit);
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
            // Dump to CherryVis
            std::vector<long> narrowChokeTilesCVis(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    narrowChokeTilesCVis[x + y * mapWidth] = narrowChokeTiles[x + y * mapWidth];
                }
            }

            CherryVis::addHeatmap("NarrowChokeTiles", narrowChokeTilesCVis, mapWidth, mapHeight);
#endif
        }

        void computeIslandTiles()
        {
            // Gather all "island" areas, which are areas not ground-connected to our start location
            std::set<const BWEM::Area *> islandAreas;
            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                auto dist = PathFinding::GetGroundDistance(BWAPI::Position(area.Top()),
                                                           BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));
                if (dist == -1)
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
            // Dump to CherryVis
            std::vector<long> islandTilesCVis(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    islandTilesCVis[x + y * mapWidth] = islandTiles[x + y * mapWidth];
                }
            }

            CherryVis::addHeatmap("IslandTiles", islandTilesCVis, mapWidth, mapHeight);
#endif
        }

        void computeLeafAreaTiles()
        {
            // Gather all "leaf" areas, which we define as areas that are only connected by narrow or blocked chokes
            // We also always add our starting main areas as leaf areas
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
            // Dump to CherryVis
            std::vector<long> leafAreaTilesCVis(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    leafAreaTilesCVis[x + y * mapWidth] = leafAreaTiles[x + y * mapWidth];
                }
            }

            CherryVis::addHeatmap("LeafAreaTiles", leafAreaTilesCVis, mapWidth, mapHeight);
#endif
        }

        // Dumps heatmaps for static map things like ground height
        void dumpStaticHeatmaps()
        {
#if CVIS_HEATMAPS
            // Ground height is at tile resolution
            std::vector<long> groundHeight(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    groundHeight[x + y * mapWidth] = BWAPI::Broodwar->getGroundHeight(x, y);
                }
            }

            CherryVis::addHeatmap("GroundHeight", groundHeight, mapWidth, mapHeight);

            // Buildability is at tile resolution
            std::vector<long> buildability(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    buildability[x + y * mapWidth] = BWAPI::Broodwar->isBuildable(x, y);
                }
            }

            CherryVis::addHeatmap("Buildable", buildability, mapWidth, mapHeight);

            // Walkability is at walk tile resolution
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

            CherryVis::addHeatmap("Walkable", walkability, mapWidth * 4, mapHeight * 4);

            // Altitude is at walk tile resolution
            std::vector<long> altitude(mapWidth * mapHeight * 16);
            for (int y = 0; y < mapHeight * 4; y++)
            {
                for (int x = 0; x < mapWidth * 4; x++)
                {
                    altitude[x + y * mapWidth * 4] = BWEM::Map::Instance().GetMiniTile(BWAPI::WalkPosition(x, y)).Altitude();
                }
            }

            CherryVis::addHeatmap("Altitude", altitude, mapWidth * 4, mapHeight * 4);

            // Edge tiles are at tile resolution
            std::vector<long> edgeTiles(mapWidth * mapHeight);
            for (int y = 0; y < mapHeight; y++)
            {
                for (int x = 0; x < mapWidth; x++)
                {
                    edgeTiles[x + y * mapWidth] = ((!edgePositionsToArea.contains(BWAPI::TilePosition(x, y))) ? 0 : 100);
                }
            }

            CherryVis::addHeatmap("EdgeTiles", edgeTiles, mapWidth, mapHeight);

            // Mineral lines from all bases
            std::vector<long> mineralLineCvis(mapWidth * mapHeight);
            for (auto &base : bases)
            {
                for (int y = 0; y < mapHeight; y++)
                {
                    for (int x = 0; x < mapWidth; x++)
                    {
                        BWAPI::TilePosition here(x, y);

                        if (base->workerDefenseRallyPatch && base->workerDefenseRallyPatch->tile == here)
                        {
                            mineralLineCvis[x + y * mapWidth] = 10;
                        }
                        else if (BWAPI::TilePosition(base->mineralLineCenter) == here)
                        {
                            mineralLineCvis[x + y * mapWidth] = 10;
                        }
                        else if (base->isInMineralLine(here))
                        {
                            mineralLineCvis[x + y * mapWidth] = -10;
                        }
                    }
                }
            }

            CherryVis::addHeatmap("MineralLines", mineralLineCvis, mapWidth, mapHeight);

            // Bases and resources
            std::vector<long> basesCvis(mapWidth * mapHeight);
            auto setBaseTiles = [&basesCvis](BWAPI::TilePosition tile, BWAPI::TilePosition size, long value)
            {
                auto bottomRight = tile + size;

                for (int y = tile.y; y < bottomRight.y; y++)
                {
                    for (int x = tile.x; x < bottomRight.x; x++)
                    {
                        basesCvis[x + y * mapWidth] = value;
                    }
                }
            };
            int val = 500;
            for (auto &base : bases)
            {
                setBaseTiles(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus.tileSize(), val);
                for (const auto &patch : base->mineralPatches())
                {
                    setBaseTiles(patch->tile, BWAPI::UnitTypes::Resource_Mineral_Field.tileSize(), val);
                }
                for (const auto &geyser : base->geysersOrRefineries())
                {
                    setBaseTiles(geyser->tile, BWAPI::UnitTypes::Resource_Vespene_Geyser.tileSize(), val);
                }
                val += 20;
            }
            CherryVis::addHeatmap("Bases", basesCvis, mapWidth, mapHeight);

            // Chokes
            std::vector<long> chokesCvis(mapWidth * mapHeight * 16);
            for (auto &bwemChokeAndChoke : chokes)
            {
                for (auto pos : bwemChokeAndChoke.first->Geometry())
                {
                    chokesCvis[pos.x + pos.y * mapWidth * 4] = 1;
                }

                BWAPI::WalkPosition center(bwemChokeAndChoke.second->center);
                chokesCvis[center.x + center.y * mapWidth * 4] = 30;

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

            CherryVis::addHeatmap("Chokes", chokesCvis, mapWidth * 4, mapHeight * 4);

            // Narrow Chokes
            std::vector<long> narrowChokesCvis(mapWidth * mapHeight * 16);
            for (auto &bwemChokeAndChoke : chokes)
            {
                auto &choke = bwemChokeAndChoke.second;
                if (!choke->isNarrowChoke) continue;

                BWAPI::WalkPosition center(choke->center);
                narrowChokesCvis[center.x + center.y * mapWidth * 4] = 30;

                BWAPI::WalkPosition end1Center(bwemChokeAndChoke.second->end1Center);
                BWAPI::WalkPosition end2Center(bwemChokeAndChoke.second->end2Center);
                BWAPI::WalkPosition end1Exit(bwemChokeAndChoke.second->end1Exit);
                BWAPI::WalkPosition end2Exit(bwemChokeAndChoke.second->end2Exit);
                narrowChokesCvis[end1Exit.x + end1Exit.y * mapWidth * 4] = 15;
                narrowChokesCvis[end2Exit.x + end2Exit.y * mapWidth * 4] = 15;
                narrowChokesCvis[end1Center.x + end1Center.y * mapWidth * 4] = 20;
                narrowChokesCvis[end2Center.x + end2Center.y * mapWidth * 4] = 20;
            }

            CherryVis::addHeatmap("NarrowChokes", narrowChokesCvis, mapWidth * 4, mapHeight * 4);
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
        for (auto it = bases.begin(); it != bases.end(); it = bases.erase(it))
        {
            delete *it;
        }
        startingLocations.clear();
        for (auto it = chokes.begin(); it != chokes.end(); it = chokes.erase(it))
        {
            delete it->second;
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

        // Initialize BWEM
        BWEM::Map::ResetInstance();
        BWEM::Map::Instance().Initialize(BWAPI::BroodwarPtr);
        BWEM::Map::Instance().EnableAutomaticPathAnalysis();
        bool bwem = BWEM::Map::Instance().FindBasesForStartingLocations();
        Log::Debug() << "Initialized BWEM: " << bwem;

        // Select map-specific overrides
        if (BWAPI::Broodwar->mapHash() == "83320e505f35c65324e93510ce2eafbaa71c9aa1")
        {
            // TODO: Add openbw hash
            Log::Get() << "Using map-specific override for Fortress";
            _mapSpecificOverride = new Fortress();
        }
        else if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67" ||
                 BWAPI::Broodwar->mapHash() == "8b3e8ed9ce9620a606319ba6a593ed5c894e51df")
        {
            Log::Get() << "Using map-specific override for Plasma";
            _mapSpecificOverride = new Plasma();
        }
        else if (BWAPI::Broodwar->mapHash() == "8000dc6116e405ab878c14bb0f0cde8efa4d640c" ||
                 BWAPI::Broodwar->mapHash() == "9e5770c62b523042e8af590c8dc267e6c12637fc")
        {
            Log::Get() << "Using map-specific override for Alchemist";
            _mapSpecificOverride = new Alchemist();
        }
        else if (BWAPI::Broodwar->mapHash() == "63a94b3a878c912f2fa5e31700491a60ac3f29d9" ||
                 BWAPI::Broodwar->mapHash() == "99324782b01af58f6b25aea13e2d62aa83564de0")
        {
            Log::Get() << "Using map-specific override for Outsider";
            _mapSpecificOverride = new Outsider();
        }
        else if (BWAPI::Broodwar->mapHash() == "4e24f217d2fe4dbfa6799bc57f74d8dc939d425b" ||
                 BWAPI::Broodwar->mapHash() == "e39c1c81740a97a733d227e238bd11df734eaf96")
        {
            Log::Get() << "Using map-specific override for Destination";
            _mapSpecificOverride = new Destination();
        }
        else if (BWAPI::Broodwar->mapHash() == "0a41f144c6134a2204f3d47d57cf2afcd8430841" ||
                 BWAPI::Broodwar->mapHash() == "7e14d53b944b1365973f2d8768c75358c6b28a8f")
        {
            Log::Get() << "Using map-specific override for Match Point";
            _mapSpecificOverride = new MatchPoint();
        }
        else if (BWAPI::Broodwar->mapHash() == "442e456721c94fd085ecd10230542960d57928d9" ||
                 BWAPI::Broodwar->mapHash() == "83cc5c3944a80915a68190d7b87714d8c0cf8a2f")
        {
            Log::Get() << "Using map-specific override for Arcadia";
            _mapSpecificOverride = new Arcadia();
        }
        else if (BWAPI::Broodwar->mapHash() == "bde51d09a2ad733db9e5492354798045db2ced3e" ||
                 BWAPI::Broodwar->mapHash() == "699c33ca1ff8f486d93b288371e57db49a0c403f")
        {
            Log::Get() << "Using map-specific override for Colosseum";
            _mapSpecificOverride = new Colosseum();
        }
        else if (BWAPI::Broodwar->mapHash() == "97944269ea55365d13c310f46c9337f5e873dc6c" ||
                 BWAPI::Broodwar->mapHash() == "a8fff0bad1956dba03e234744f2e12f7941a8f8a")
        {
            Log::Get() << "Using map-specific override for CrossingField";
            _mapSpecificOverride = new CrossingField();
        }
        else if (BWAPI::Broodwar->mapHash() == "2acb8e8cc2ec9b0911a73f2f29dcce424862dddd" ||
                 BWAPI::Broodwar->mapHash() == "0d592ab56bd5c9230444e48177f150e58fce0a91")
        {
            Log::Get() << "Using map-specific override for GodsGarden";
            _mapSpecificOverride = new GodsGarden();
        }
        else if (BWAPI::Broodwar->mapHash() == "2f69eaa1a73bb743934d55e7ea12186fe340e656" ||
                 BWAPI::Broodwar->mapHash() == "a19d3ed890c2919c81a9aff55732d3f602a3323e")
        {
            Log::Get() << "Using map-specific override for JudgmentDay";
            _mapSpecificOverride = new JudgmentDay();
        }
        else if (BWAPI::Broodwar->mapHash() == "444b805a88f971c6b2c5f8d2a467de3c1fb2f001" ||
                 BWAPI::Broodwar->mapHash() == "625b16a3a6c861bc6c2f91b709329edb7e8e28aa")
        {
            Log::Get() << "Using map-specific override for Katrina";
            _mapSpecificOverride = new Katrina();
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
        // Dump to CherryVis
        std::vector<long> borderingMineralPatchCVis(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++)
        {
            for (int x = 0; x < mapWidth; x++)
            {
                borderingMineralPatchCVis[x + y * mapWidth] = borderingMineralPatch[x + y * mapWidth] ? 1 : 0;
            }
        }

        CherryVis::addHeatmap("BorderingMineralPatch", borderingMineralPatchCVis, mapWidth, mapHeight);
#endif

        initializeWalkability();

        // Analyze chokepoints
        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            for (const BWEM::ChokePoint *choke : area.ChokePoints())
            {
                if (!chokes.contains(choke))
                    chokes.emplace(choke, new Choke(choke));
            }
        }
        _mapSpecificOverride->initializeChokes(chokes);

        // Compute the minimum choke width
        _minChokeWidth = INT_MAX;
        for (const auto &pair : chokes)
        {
            if (pair.second->width < _minChokeWidth)
                _minChokeWidth = pair.second->width;
        }

        computeIslandTiles();

        // Initialize bases
        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            for (const auto &base : area.Bases())
            {
                auto newBase = new Base(base.Location(), &base);
                bases.push_back(newBase);
            }
        }
        _mapSpecificOverride->modifyBases(bases);
        for (auto &base : bases)
        {
            if (base->isStartingBase())
            {
                auto natural = getNaturalForStartLocation(base->getTilePosition());
                auto mainChoke = computeMainChoke(base, natural);
                auto naturalChoke = computeNaturalChoke(base, natural, mainChoke);

                auto startingLocation = std::make_unique<StartingLocation>(base, natural, mainChoke, naturalChoke);
                _mapSpecificOverride->modifyStartingLocation(startingLocation);

                startingLocations.emplace_back(std::move(startingLocation));
                if (base->getTilePosition() == BWAPI::Broodwar->self()->getStartLocation())
                {
                    setBaseOwner(base, BWAPI::Broodwar->self());
                }
            }
        }
        Log::Debug() << "Found " << bases.size() << " bases";

        if (playerToPlayerBases[BWAPI::Broodwar->self()].startingMainChoke)
        {
            playerToPlayerBases[BWAPI::Broodwar->self()].startingMainChoke->setAsMainChoke();
        }

        // Compute areas for all starting location bases
        for (auto &startingLocation : startingLocations)
        {
            // Initialize with the base area
            startingBaseAreas[startingLocation->main].insert(startingLocation->main->getArea());

            // Add any areas where the path to the natural goes through the main choke
            // TODO: Doesn't work for Alchemist 3 o'clock where we don't have a main choke or natural

            if (!startingLocation->natural) continue;
            if (!startingLocation->mainChoke) continue;
            if (_mapSpecificOverride->hasBackdoorNatural()) continue;

            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                if (isOnIsland(BWAPI::TilePosition(area.Top()))) continue;

                bool hasMainChoke = false;
                for (const auto &choke : PathFinding::GetChokePointPath(
                        BWAPI::Position(area.Top()),
                        startingLocation->natural->getPosition(),
                        BWAPI::UnitTypes::Protoss_Dragoon,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea))
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

        // Gather the edges of all areas on the map
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

    void onUnitCreated(const Unit &unit)
    {
        // Whenever we see a new building, determine if it infers a change in base ownership
        inferBaseOwnershipFromUnitCreated(unit);

        onUnitCreated_Walkability(unit);
    }

    void onUnitDestroy(const Unit &unit)
    {
        // Whenever a building is lost, determine if it infers a change in base ownership
        inferBaseOwnershipFromUnitDestroyed(unit);

        onUnitDestroy_Walkability(unit);
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        if (unit->getType().isMineralField())
            BWEM::Map::Instance().OnMineralDestroyed(unit);
        else if (unit->getType().isSpecialBuilding())
            BWEM::Map::Instance().OnStaticBuildingDestroyed(unit);

        onUnitDestroy_Walkability(unit);

        _mapSpecificOverride->onUnitDestroy(unit);
    }

    void update()
    {
        // Update the last seen frame for all visible tiles
        // This is fairly expensive and we don't need super high resolution updates, so we split the update over two frames
        int startY, endY;
        if (currentFrame % 2 == 0)
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
                    tileLastSeen[x + y * mapWidth] = currentFrame;
                }
            }
        }

        // Update bases, base scouting and resource depot
        for (auto &base : bases)
        {
            base->update();

            // If the resource depot is killed, reset it
            if (base->resourceDepot && !base->resourceDepot->exists())
            {
                base->resourceDepot = nullptr;
            }

            // Owned bases were scouted when we last saw each of the tiles around where the resource depot should be
            // Unowned bases were scouted when we last saw one of the center tiles of where the resource depot should be
            auto tile = base->getTilePosition();
            if (base->owner)
            {
                base->lastScouted = std::min({
                     tileLastSeen[tile.x - 1 + (tile.y - 1) * mapWidth],
                     tileLastSeen[tile.x + (tile.y - 1) * mapWidth],
                     tileLastSeen[tile.x + 1 + (tile.y - 1) * mapWidth],
                     tileLastSeen[tile.x + 2 + (tile.y - 1) * mapWidth],
                     tileLastSeen[tile.x + 3 + (tile.y - 1) * mapWidth],
                     tileLastSeen[tile.x + 4 + (tile.y - 1) * mapWidth],
                     tileLastSeen[tile.x - 1 + tile.y * mapWidth],
                     tileLastSeen[tile.x + 4 + tile.y * mapWidth],
                     tileLastSeen[tile.x - 1 + (tile.y + 1) * mapWidth],
                     tileLastSeen[tile.x + 4 + (tile.y + 1) * mapWidth],
                     tileLastSeen[tile.x - 1 + (tile.y + 2) * mapWidth],
                     tileLastSeen[tile.x + 4 + (tile.y + 2) * mapWidth],
                     tileLastSeen[tile.x - 1 + (tile.y + 3) * mapWidth],
                     tileLastSeen[tile.x + (tile.y + 3) * mapWidth],
                     tileLastSeen[tile.x + 1 + (tile.y + 3) * mapWidth],
                     tileLastSeen[tile.x + 2 + (tile.y + 3) * mapWidth],
                     tileLastSeen[tile.x + 3 + (tile.y + 3) * mapWidth],
                     tileLastSeen[tile.x + 4 + (tile.y + 3) * mapWidth],
                });
            }
            else
            {
                base->lastScouted = std::max(
                        tileLastSeen[tile.x + 1 + (tile.y + 1) * mapWidth],
                        tileLastSeen[tile.x + 2 + (tile.y + 1) * mapWidth]);

                // Check for creep
                if ((base->ownedSince == -1 || base->ownedSince < (currentFrame - 2500)) && checkCreep(base))
                {
                    setBaseOwner(base, BWAPI::Broodwar->enemy());
                }
            }
        }

        // Infer single-enemy base ownership from which starting locations are scouted
        if (BWAPI::Broodwar->enemies().size() == 1 &&
            !playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMain)
        {
            auto unscouted = unscoutedStartingLocations();
            if (unscouted.size() == 1)
            {
                setBaseOwner(*unscouted.begin(), BWAPI::Broodwar->enemy());
            }
        }

        // Periodically check if base ownership is out-of-sync for any bases
        if (currentFrame % 24 == 17)
        {
            for (auto &base : bases)
            {
                validateBaseOwnership(base);
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
        std::set<Base *> result;
        for (auto &base : bases)
        {
            if (!base->owner) continue;
            if (base->owner->isEnemy(player)) result.insert(base);
        }

        return result;
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

    void setMyNatural(Base *base)
    {
        playerToPlayerBases[BWAPI::Broodwar->self()].startingNatural = base;
        Log::Get() << "Set my natural to " << base->getTilePosition();
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

    void setEnemyStartingMain(Base *base)
    {
        if (base->owner) return;

        setBaseOwner(base, BWAPI::Broodwar->enemy());
    }

    void setEnemyStartingNatural(Base *base)
    {
        playerToPlayerBases[BWAPI::Broodwar->enemy()].startingNatural = base;
        Log::Get() << "Set enemy natural to " << base->getTilePosition();
    }

    Base *getHiddenBase()
    {
        auto myMain = playerToPlayerBases[BWAPI::Broodwar->self()].startingMain;
        auto enemyMain = playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMain;
        if (!enemyMain) return nullptr;

        Base *best = nullptr;
        int bestScore = -1;
        for (auto &base : bases)
        {
            if (base->owner) continue;
            if (base->gas < 2000) continue;

            int ourDist = PathFinding::GetGroundDistance(
                    base->getPosition(),
                    myMain->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (ourDist == -1) continue;

            int enemyDist = PathFinding::GetGroundDistance(
                    base->getPosition(),
                    enemyMain->getPosition(),
                    BWAPI::UnitTypes::Protoss_Probe,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (enemyDist == -1) enemyDist = 10000;

            // Pick the base with the longest total distance
            int score = ourDist + enemyDist;
            if (score > bestScore)
            {
                bestScore = score;
                best = base;
            }
        }

        return best;
    }

    Base *baseNear(BWAPI::Position position)
    {
        auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(position));
        if (!area) return nullptr;

        int closestDist = INT_MAX;
        Base *result = nullptr;

        for (auto &base : bases)
        {
            if (base->getArea() != area) continue;

            int dist = base->getPosition().getApproxDistance(position);
            if (dist < 500 && dist < closestDist)
            {
                closestDist = dist;
                result = base;
            }
        }

        return result;
    }

    std::vector<Base *> allStartingLocations()
    {
        std::vector<Base *> result;
        for (auto &startingLocation : startingLocations)
        {
            result.push_back(startingLocation->main);
        }
        return result;
    }

    std::set<Base *> unscoutedStartingLocations()
    {
        std::set<Base *> result;
        for (auto &startingLocation : startingLocations)
        {
            if (startingLocation->main->lastScouted >= 0) continue;

            result.insert(startingLocation->main);
        }

        return result;
    }

    std::vector<Choke *> allChokes()
    {
        std::vector<Choke *> result;
        result.reserve(chokes.size());
        for (auto &pair : chokes)
        {
            result.push_back(pair.second);
        }
        return result;
    }

    Choke *chokeNear(BWAPI::Position pos, int maxDistance)
    {
        int bestDist = maxDistance + 1;
        Choke *best = nullptr;
        for (auto &[_, choke] : chokes)
        {
            int dist = choke->center.getApproxDistance(pos);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = choke;
            }
        }
        return best;
    }

    Choke *choke(const BWEM::ChokePoint *bwemChoke)
    {
        if (!bwemChoke) return nullptr;
        auto it = chokes.find(bwemChoke);
        return it == chokes.end()
               ? nullptr
               : it->second;
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
        Log::Get() << "Set my main choke to " << BWAPI::TilePosition(choke->center);
        BuildingPlacement::onMainChokeChanged();
    }

    Choke *getEnemyMainChoke()
    {
        return playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMainChoke;
    }

    void setEnemyMainChoke(Choke *choke)
    {
        playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMainChoke = choke;
        Log::Get() << "Set enemy main choke to " << BWAPI::TilePosition(choke->center);
    }

    int minChokeWidth()
    {
        return _minChokeWidth;
    }

    std::set<const BWEM::Area *> &getMyMainAreas()
    {
        return startingBaseAreas[getMyMain()];
    }

    std::set<const BWEM::Area *> &getStartingBaseAreas(Base *base)
    {
        return startingBaseAreas[base];
    }

    Base *getStartingBaseNatural(Base *base)
    {
        for (auto &startingLocation : startingLocations)
        {
            if (startingLocation->main != base) continue;

            return startingLocation->natural;
        }

        return nullptr;
    }

    std::pair<Choke*, Choke*> getStartingBaseChokes(Base *base)
    {
        for (auto &startingLocation : startingLocations)
        {
            if (startingLocation->main != base) continue;

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
            CherryVis::addHeatmap("Power", newPower, mapWidth, mapHeight);
            power = newPower;
        }
#endif
    }

    bool isInOwnMineralLine(BWAPI::TilePosition tile)
    {
        return isInOwnMineralLine(tile.x, tile.y);
    }

    bool isInOwnMineralLine(int x, int y)
    {
        return inOwnMineralLine[x + y * mapWidth];
    }

    bool bordersMineralPatch(int x, int y)
    {
        return borderingMineralPatch[x + y * mapWidth];
    }

    bool isInNarrowChoke(BWAPI::TilePosition pos)
    {
        return narrowChokeTiles[pos.x + pos.y * mapWidth];
    }

    bool isInLeafArea(BWAPI::TilePosition pos)
    {
        return leafAreaTiles[pos.x + pos.y * mapWidth];
    }

    bool isOnIsland(BWAPI::TilePosition pos)
    {
        return islandTiles[pos.x + pos.y * mapWidth];
    }

    int lastSeen(BWAPI::TilePosition tile)
    {
        return tileLastSeen[tile.x + tile.y * mapWidth];
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
