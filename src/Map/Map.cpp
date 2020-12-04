#include "Map.h"

#include "MapSpecificOverrides/Fortress.h"
#include "MapSpecificOverrides/Plasma.h"
#include "MapSpecificOverrides/Alchemist.h"

#include "Units.h"
#include "PathFinding.h"
#include "Geo.h"

namespace Map
{
    namespace
    {
        MapSpecificOverride *_mapSpecificOverride;
        std::vector<Base *> bases;
        std::vector<Base *> startingLocationBases;
        std::map<const BWEM::ChokePoint *, Choke *> chokes;
        int _minChokeWidth;

        std::set<const BWEM::Area *> myStartingMainAreas;

        std::vector<bool> tileWalkability;
        std::vector<int> tileUnwalkableProximity;
        std::vector<BWAPI::Position> tileCollisionVector;
        bool tileWalkabilityUpdated;

        std::vector<bool> inOwnMineralLine;

        std::vector<bool> narrowChokeTiles;
        std::vector<bool> leafAreaTiles;

        std::vector<int> tileLastSeen;

        std::vector<int> noGoAreaTiles;
        bool noGoAreaTilesUpdated;

#if CHERRYVIS_ENABLED
        std::vector<long> visibility;
        std::vector<long> power;
#endif

        struct PlayerBases
        {
            Base *startingMain;
            Base *startingNatural;
            Choke *startingMainChoke;

            Base *main;
            std::set<Base *> allOwned;
            std::vector<Base *> probableExpansions;
            std::vector<Base *> islandExpansions;

            PlayerBases() : startingMain(nullptr), startingNatural(nullptr), main(nullptr) {}
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

            for (auto it = path.rbegin(); it != path.rend(); it++)
            {
                auto c = choke(*it);
                if (c->isRamp) return c;
            }

            return choke(*path.begin());
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
            for (auto base : bases)
            {
                if (base->getTilePosition() == startLocation) continue;
                if (base->gas() == 0) continue;

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
            for (auto base : bases)
            {
                // Skip bases that are already taken
                // We consider any based owned by a different player than us to be taken
                // We consider bases owned by us to be taken if the resource depot exists
                if (base->owner && (player != BWAPI::Broodwar->self() || (base->resourceDepot && base->resourceDepot->exists()))) continue;

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
                score += base->minerals() / 100;
                score += base->gas() / 50;

                scoredBases.emplace_back(std::make_tuple(score, base, island));
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
                        inOwnMineralLine[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] = false;
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
            base->ownedSince = BWAPI::Broodwar->getFrameCount();
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
                    ownerBases.startingNatural = getNaturalForStartLocation(base->getTilePosition());
                    ownerBases.startingMainChoke = computeMainChoke(base, ownerBases.startingNatural);
                    ownerBases.main = base;

                    if (owner == BWAPI::Broodwar->enemy())
                    {
                        _mapSpecificOverride->enemyStartingMainDetermined();
                    }
                }

                // If this player had a starting main, but doesn't currently have a main, set this base as the main
                if (ownerBases.startingMain && !ownerBases.main)
                {
                    ownerBases.main = base;
                }

                // If this is our base, add the mineral line tiles as blocking the navigation grids
                if (owner == BWAPI::Broodwar->self())
                {
                    for (auto tile : base->mineralLineTiles)
                    {
                        inOwnMineralLine[tile.x + tile.y * BWAPI::Broodwar->mapWidth()] = true;
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
            // Only consider non-neutral buildings
            if (unit->player == BWAPI::Broodwar->neutral()) return;
            if (!unit->type.isBuilding()) return;

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
                    if (nearbyBase->resourceDepot && nearbyBase->resourceDepot->exists())
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
                for (auto startingLocationBase : startingLocationBases)
                {
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

                    auto natural = getNaturalForStartLocation(startingLocationBase->getTilePosition());
                    if (natural)
                    {
                        int naturalDist = PathFinding::GetGroundDistance(
                                unit->lastPosition,
                                natural->getPosition(),
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
            for (int x = -8; x <= 11; x++)
            {
                if (x == 0) x = 4;
                for (int y = -5; y <= 7; y++)
                {
                    if (y == 0) y = 2;

                    BWAPI::TilePosition tile = {base->getTilePosition().x + x, base->getTilePosition().y + y};
                    if (!tile.isValid()) continue;
                    if (BWAPI::Broodwar->hasCreep(tile))
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

            // Check if the destruction of the unit results in the base ownership changing
            validateBaseOwnership(nearbyBase, unit);
        }

        // Writes the tile walkability grid to CherryVis
        void dumpTileWalkability()
        {
#if CHERRYVIS_ENABLED
            // Dump to CherryVis
            std::vector<long> tileWalkabilityCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    tileWalkabilityCVis[x + y * BWAPI::Broodwar->mapWidth()] = tileWalkability[x + y * BWAPI::Broodwar->mapWidth()];
                }
            }

            CherryVis::addHeatmap("TileWalkable", tileWalkabilityCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

            std::vector<long> tileUnwalkableProximityCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    tileUnwalkableProximityCVis[x + y * BWAPI::Broodwar->mapWidth()] = tileUnwalkableProximity[x + y * BWAPI::Broodwar->mapWidth()];
                }
            }

            CherryVis::addHeatmap("TileUnwalkableProximity", tileUnwalkableProximityCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

            std::vector<long> tileCollisionVectorXCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    tileCollisionVectorXCVis[x + y * BWAPI::Broodwar->mapWidth()] = tileCollisionVector[x + y * BWAPI::Broodwar->mapWidth()].x;
                }
            }

            CherryVis::addHeatmap("TileCollisionVectorX", tileCollisionVectorXCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

            std::vector<long> tileCollisionVectorYCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    tileCollisionVectorYCVis[x + y * BWAPI::Broodwar->mapWidth()] = tileCollisionVector[x + y * BWAPI::Broodwar->mapWidth()].y;
                }
            }

            CherryVis::addHeatmap("TileCollisionVectorY", tileCollisionVectorYCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
        }

        // Writes the tile no go areas to CherryVis
        void dumpNoGoAreaTiles()
        {
#if CHERRYVIS_ENABLED
            // Dump to CherryVis
            std::vector<long> noGoAreaTilesCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    noGoAreaTilesCVis[x + y * BWAPI::Broodwar->mapWidth()] = noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()];
                }
            }

            CherryVis::addHeatmap("NoGoAreas", noGoAreaTilesCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
        }

        void updateWalkabilityProximity(int tileX, int tileY)
        {
            auto tileValid = [](int x, int y)
            {
                return x >= 0 && y >= 0 && x < BWAPI::Broodwar->mapWidth() && y < BWAPI::Broodwar->mapHeight();
            };

            if (!tileValid(tileX, tileY)) return;

            auto visit = [&tileValid](int x, int y, int val, int *result)
            {
                if (!tileValid(x, y)) return false;

                if (!tileWalkability[x + y * BWAPI::Broodwar->mapWidth()])
                {
                    *result = val;
                    return true;
                }

                return false;
            };

            int result = 3;
            visit(tileX - 1, tileY, 0, &result) ||
            visit(tileX + 1, tileY, 0, &result) ||
            visit(tileX, tileY - 1, 0, &result) ||
            visit(tileX, tileY + 1, 0, &result) ||
            visit(tileX - 1, tileY - 1, 1, &result) ||
            visit(tileX + 1, tileY - 1, 1, &result) ||
            visit(tileX - 1, tileY + 1, 1, &result) ||
            visit(tileX + 1, tileY + 1, 1, &result) ||
            visit(tileX - 2, tileY, 2, &result) ||
            visit(tileX + 2, tileY, 2, &result) ||
            visit(tileX, tileY - 2, 2, &result) ||
            visit(tileX, tileY + 2, 2, &result);

            tileUnwalkableProximity[tileX + tileY * BWAPI::Broodwar->mapWidth()] = result;
        }

        void updateCollisionVectors(int tileX, int tileY, bool walkable)
        {
            auto updateTile = [&tileX, &tileY](int offsetX, int offsetY, int deltaX, int deltaY)
            {
                int x = tileX + offsetX;
                if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) return;

                int y = tileY + offsetY;
                if (y < 0 || y >= BWAPI::Broodwar->mapHeight()) return;

                auto &pos = tileCollisionVector[x + y * BWAPI::Broodwar->mapWidth()];
                pos.x += deltaX;
                pos.y += deltaY;
            };

            if (walkable)
            {
                updateTile(-1, -1, 10, 10);
                updateTile(-1, 0, 14, 0);
                updateTile(-1, 1, 10, -10);
                updateTile(0, 1, 0, -14);
                updateTile(1, 1, -10, -10);
                updateTile(1, 0, -14, 0);
                updateTile(1, -1, -10, 10);
                updateTile(0, -1, 0, 14);
            }
            else
            {
                updateTile(-1, -1, -10, -10);
                updateTile(-1, 0, -14, 0);
                updateTile(-1, 1, -10, 10);
                updateTile(0, 1, 0, 14);
                updateTile(1, 1, 10, 10);
                updateTile(1, 0, 14, 0);
                updateTile(1, -1, 10, -10);
                updateTile(0, -1, 0, -14);
            }
        }

        // Updates the tile walkability grid based on appearance or disappearance of a building, mineral field, etc.
        bool updateTileWalkability(BWAPI::TilePosition tile, BWAPI::TilePosition size, bool walkable)
        {
            bool updated = false;

            for (int x = 0; x < size.x; x++)
            {
                for (int y = 0; y < size.y; y++)
                {
                    if (tileWalkability[(tile.x + x) + (tile.y + y) * BWAPI::Broodwar->mapWidth()] != walkable)
                    {
                        tileWalkability[(tile.x + x) + (tile.y + y) * BWAPI::Broodwar->mapWidth()] = walkable;
                        updateCollisionVectors(tile.x + x, tile.y + y, walkable);
                        updated = true;
                    }
                }
            }

            // Update walkability proximity for unwalkable
            if (updated && !walkable)
            {
                auto tileValid = [](int x, int y)
                {
                    return x >= 0 && y >= 0 && x < BWAPI::Broodwar->mapWidth() && y < BWAPI::Broodwar->mapHeight();
                };

                // Zero the tiles in and directly touching
                for (int x = -1; x <= size.x; x++)
                {
                    for (int y = (x == -1 || x == size.x ? 0 : -1); y < (x == -1 || x == size.x ? size.y : (size.y + 1)); y++)
                    {
                        if (tileValid(tile.x + x, tile.y + y))
                        {
                            tileUnwalkableProximity[(tile.x + x) + (tile.y + y) * BWAPI::Broodwar->mapWidth()] = 0;
                        }
                    }
                }

                auto visit = [&tileValid](int x, int y, int val)
                {
                    if (!tileValid(x, y)) return;

                    tileUnwalkableProximity[x + y * BWAPI::Broodwar->mapWidth()] =
                            std::min(tileUnwalkableProximity[x + y * BWAPI::Broodwar->mapWidth()], val);
                };

                // Corners
                visit(tile.x - 1, tile.y - 1, 1);
                visit(tile.x - 1, tile.y + size.y, 1);
                visit(tile.x + size.x, tile.y - 1, 1);
                visit(tile.x + size.x, tile.y + size.y, 1);

                // Left and right sides two tiles away
                for (int x = -2; x < size.x + 2; x++)
                {
                    for (int y = 0; y < size.y; y++)
                    {
                        visit(tile.x + x, tile.y + y, 2);
                    }
                }

                // Top and bottom sides two tiles away
                for (int y = -2; y < size.y + 2; y++)
                {
                    for (int x = 0; x < size.x; x++)
                    {
                        visit(tile.x + x, tile.y + y, 2);
                    }
                }
            }

            // Walkable is more difficult, so reprocess each tile individually
            if (updated && walkable)
            {
                for (int x = -2; x < size.x + 2; x++)
                {
                    for (int y = -2; y < size.y + 2; y++)
                    {
                        updateWalkabilityProximity(tile.x + x, tile.y + y);
                    }
                }
            }

            return updated;
        }

        // Computes walkability at a tile level
        void computeTileWalkability()
        {
            tileWalkability.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());

            // Start by checking the normal BWAPI walkability
            for (int tileX = 0; tileX < BWAPI::Broodwar->mapWidth(); tileX++)
            {
                for (int tileY = 0; tileY < BWAPI::Broodwar->mapHeight(); tileY++)
                {
                    bool walkable = true;
                    for (int walkX = 0; walkX < 4; walkX++)
                    {
                        for (int walkY = 0; walkY < 4; walkY++)
                        {
                            if (!BWAPI::Broodwar->isWalkable((tileX << 2U) + walkX, (tileY << 2U) + walkY))
                            {
                                walkable = false;
                                updateCollisionVectors(tileX, tileY, false);
                                goto breakInnerLoop;
                            }
                        }
                    }
                    breakInnerLoop:
                    tileWalkability[tileX + tileY * BWAPI::Broodwar->mapWidth()] = walkable;
                }
            }

            // For collision vectors, mark the edges of the map as unwalkable
            for (int tileX = -1; tileX <= BWAPI::Broodwar->mapWidth(); tileX++)
            {
                updateCollisionVectors(tileX, -1, false);
                updateCollisionVectors(tileX, BWAPI::Broodwar->mapHeight(), false);
            }
            for (int tileY = 0; tileY <= BWAPI::Broodwar->mapHeight(); tileY++)
            {
                updateCollisionVectors(-1, tileY, false);
                updateCollisionVectors(BWAPI::Broodwar->mapWidth(), tileY, false);
            }

            // TODO: Consider map-specific overrides for maps with very narrow ramps, like Plasma

            // Compute proximity to unwalkable tiles
            // We use this to prioritize pathing
            tileUnwalkableProximity.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int tileX = 0; tileX < BWAPI::Broodwar->mapWidth(); tileX++)
            {
                for (int tileY = 0; tileY < BWAPI::Broodwar->mapHeight(); tileY++)
                {
                    updateWalkabilityProximity(tileX, tileY);
                }
            }

            // Add our start position
            updateTileWalkability(BWAPI::Broodwar->self()->getStartLocation(), BWAPI::UnitTypes::Protoss_Nexus.tileSize(), false);

            // Add static neutrals
            for (auto neutral : BWAPI::Broodwar->getStaticNeutralUnits())
            {
                updateTileWalkability(neutral->getInitialTilePosition(), neutral->getType().tileSize(), false);
            }

            dumpTileWalkability();
        }

        void computeNarrowChokeTiles()
        {
            narrowChokeTiles.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());

            for (const auto &pair : chokes)
            {
                if (!pair.second->isNarrowChoke) continue;

                for (const auto &chokeTile : pair.second->chokeTiles)
                {
                    narrowChokeTiles[chokeTile.x + chokeTile.y * BWAPI::Broodwar->mapWidth()] = true;
                }
            }

#if CHERRYVIS_ENABLED
            // Dump to CherryVis
            std::vector<long> narrowChokeTilesCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    narrowChokeTilesCVis[x + y * BWAPI::Broodwar->mapWidth()] = narrowChokeTiles[x + y * BWAPI::Broodwar->mapWidth()];
                }
            }

            CherryVis::addHeatmap("NarrowChokeTiles", narrowChokeTilesCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
        }

        void computeLeafAreaTiles()
        {
            // Gather all "leaf" areas, which we define as areas that are only connected by narrow chokes
            // We also always add our starting main areas as leaf areas
            std::set<const BWEM::Area *> leafAreas;
            leafAreas.insert(myStartingMainAreas.begin(), myStartingMainAreas.end());
            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                for (const auto &bwemChoke : area.ChokePoints())
                {
                    auto choke = Map::choke(bwemChoke);
                    if (!choke) continue;
                    if (!choke->isNarrowChoke) goto NextArea;
                }

                leafAreas.insert(&area);

                NextArea:;
            }

            leafAreaTiles.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());

            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    BWAPI::TilePosition here(x, y);
                    auto area = BWEM::Map::Instance().GetArea(here);
                    if (area && leafAreas.find(area) != leafAreas.end())
                    {
                        leafAreaTiles[here.x + here.y * BWAPI::Broodwar->mapWidth()] = true;
                    }
                }
            }

#if CHERRYVIS_ENABLED
            // Dump to CherryVis
            std::vector<long> leafAreaTilesCVis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    leafAreaTilesCVis[x + y * BWAPI::Broodwar->mapWidth()] = leafAreaTiles[x + y * BWAPI::Broodwar->mapWidth()];
                }
            }

            CherryVis::addHeatmap("LeafAreaTiles", leafAreaTilesCVis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
#endif
        }

        // Dumps heatmaps for static map things like ground height
        void dumpStaticHeatmaps()
        {
#if CHERRYVIS_ENABLED
            // Ground height is at tile resolution
            std::vector<long> groundHeight(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    groundHeight[x + y * BWAPI::Broodwar->mapWidth()] = BWAPI::Broodwar->getGroundHeight(x, y);
                }
            }

            CherryVis::addHeatmap("GroundHeight", groundHeight, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

            // Buildability is at tile resolution
            std::vector<long> buildability(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                {
                    buildability[x + y * BWAPI::Broodwar->mapWidth()] = BWAPI::Broodwar->isBuildable(x, y);
                }
            }

            CherryVis::addHeatmap("Buildable", buildability, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

            // Walkability is at walk tile resolution
            std::vector<long> walkability(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 16);
            for (int x = 0; x < BWAPI::Broodwar->mapWidth() * 4; x++)
            {
                for (int y = 0; y < BWAPI::Broodwar->mapHeight() * 4; y++)
                {
                    walkability[x + y * BWAPI::Broodwar->mapWidth() * 4] = BWAPI::Broodwar->isWalkable(x, y);
                }
            }

            CherryVis::addHeatmap("Walkable", walkability, BWAPI::Broodwar->mapWidth() * 4, BWAPI::Broodwar->mapHeight() * 4);

            // Mineral lines from all bases
            std::vector<long> mineralLineCvis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            for (auto &base : bases)
            {
                for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
                {
                    for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
                    {
                        BWAPI::TilePosition here(x, y);

                        if (base->workerDefenseRallyPatch && base->workerDefenseRallyPatch->getTilePosition() == here)
                        {
                            mineralLineCvis[x + y * BWAPI::Broodwar->mapWidth()] = 10;
                        }
                        else if (BWAPI::TilePosition(base->mineralLineCenter) == here)
                        {
                            mineralLineCvis[x + y * BWAPI::Broodwar->mapWidth()] = 10;
                        }
                        else if (base->isInMineralLine(here))
                        {
                            mineralLineCvis[x + y * BWAPI::Broodwar->mapWidth()] = -10;
                        }
                    }
                }
            }

            CherryVis::addHeatmap("MineralLines", mineralLineCvis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

            // Bases and resources
            std::vector<long> basesCvis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
            auto setBaseTiles = [&basesCvis](BWAPI::TilePosition tile, BWAPI::TilePosition size, long value)
            {
                for (int x = 0; x < size.x; x++)
                {
                    for (int y = 0; y < size.y; y++)
                    {
                        basesCvis[tile.x + x + (tile.y + y) * BWAPI::Broodwar->mapWidth()] = value;
                    }
                }
            };
            int val = 500;
            for (auto &base : bases)
            {
                setBaseTiles(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus.tileSize(), val);
                for (const auto &patch : base->mineralPatches())
                {
                    setBaseTiles(patch->getInitialTilePosition(), BWAPI::UnitTypes::Resource_Mineral_Field.tileSize(), val);
                }
                for (const auto &geyser : base->geysers())
                {
                    setBaseTiles(geyser->getInitialTilePosition(), BWAPI::UnitTypes::Resource_Vespene_Geyser.tileSize(), val);
                }
                val += 20;
            }
            CherryVis::addHeatmap("Bases", basesCvis, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());

            // Chokes
            std::vector<long> chokesCvis(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight() * 16);
            for (auto &bwemChokeAndChoke : chokes)
            {
                for (auto pos : bwemChokeAndChoke.first->Geometry())
                {
                    chokesCvis[pos.x + pos.y * BWAPI::Broodwar->mapWidth() * 4] = 1;
                }

                BWAPI::WalkPosition center(bwemChokeAndChoke.second->center);
                chokesCvis[center.x + center.y * BWAPI::Broodwar->mapWidth() * 4] = 30;

                if (bwemChokeAndChoke.second->isNarrowChoke)
                {
                    BWAPI::WalkPosition end1Center(bwemChokeAndChoke.second->end1Center);
                    BWAPI::WalkPosition end2Center(bwemChokeAndChoke.second->end2Center);

                    chokesCvis[end1Center.x + end1Center.y * BWAPI::Broodwar->mapWidth() * 4] = 20;
                    chokesCvis[end2Center.x + end2Center.y * BWAPI::Broodwar->mapWidth() * 4] = 20;
                }

                if (bwemChokeAndChoke.second->highElevationTile.isValid())
                {
                    auto highTile = BWAPI::WalkPosition(bwemChokeAndChoke.second->highElevationTile) + BWAPI::WalkPosition(2, 2);
                    chokesCvis[highTile.x + highTile.y * BWAPI::Broodwar->mapWidth() * 4] = 30;
                }
            }

            CherryVis::addHeatmap("Chokes", chokesCvis, BWAPI::Broodwar->mapWidth() * 4, BWAPI::Broodwar->mapHeight() * 4);
#endif
        }
    }

    void initialize()
    {
        _mapSpecificOverride = nullptr;
        bases.clear();
        startingLocationBases.clear();
        chokes.clear();
        _minChokeWidth = 0;
        myStartingMainAreas.clear();
        tileWalkability.clear();
        tileUnwalkableProximity.clear();
        tileCollisionVector.clear();
        tileCollisionVector.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), BWAPI::Position(0, 0));
        tileWalkabilityUpdated = false;
        inOwnMineralLine.clear();
        inOwnMineralLine.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        narrowChokeTiles.clear();
        leafAreaTiles.clear();
        tileLastSeen.clear();
        tileLastSeen.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        noGoAreaTiles.clear();
        noGoAreaTiles.resize(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight());
        noGoAreaTilesUpdated = true; // So we get an initial null state
#if CHERRYVIS_ENABLED
        visibility.clear();
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
        else
        {
            _mapSpecificOverride = new MapSpecificOverride();
        }

        computeTileWalkability();

        // Analyze chokepoints
        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            for (const BWEM::ChokePoint *choke : area.ChokePoints())
            {
                if (chokes.find(choke) == chokes.end())
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

        // Initialize bases
        for (const auto &area : BWEM::Map::Instance().Areas())
        {
            for (const auto &base : area.Bases())
            {
                auto newBase = new Base(base.Location(), &base);
                bases.push_back(newBase);
            }
        }
        for (auto &base : bases)
        {
            if (base->isStartingBase())
            {
                startingLocationBases.push_back(base);
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

        myStartingMainAreas.insert(getMyMain()->getArea());
        if (playerToPlayerBases[BWAPI::Broodwar->self()].startingMainChoke &&
            playerToPlayerBases[BWAPI::Broodwar->self()].startingNatural)
        {
            // Main areas consist of all areas where the path to the natural goes through the main choke
            // TODO: Doesn't work for Alchemist 3 o'clock where we don't have a main choke or natural
            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                bool hasMainChoke = false;
                for (const auto &choke : PathFinding::GetChokePointPath(
                        BWAPI::Position(area.Top()),
                        getMyNatural()->getPosition(),
                        BWAPI::UnitTypes::Protoss_Dragoon,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea))
                {
                    if (choke == playerToPlayerBases[BWAPI::Broodwar->self()].startingMainChoke->choke)
                    {
                        hasMainChoke = true;
                        break;
                    }
                }

                if (hasMainChoke) myStartingMainAreas.insert(&area);
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

        // Units that affect tile walkability
        // Skip on frame 0, since we handle static neutrals and our base explicitly
        // Skip refineries, since creation of a refinery does not affect tile walkability (there was already a geyser)
        if (BWAPI::Broodwar->getFrameCount() > 0 && unit->type.isBuilding() && !unit->type.isRefinery())
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->type.tileSize(), false))
            {
                PathFinding::addBlockingObject(unit->type, unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }

        // FIXME: Check tile walkability for mineral fields
    }

    void onUnitDiscover(BWAPI::Unit unit)
    {
        // Update tile walkability for discovered mineral fields
        // TODO: Is this even needed?
        if (BWAPI::Broodwar->getFrameCount() > 0 && unit->getType().isMineralField())
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->getType().tileSize(), false))
            {
                PathFinding::addBlockingObject(unit->getType(), unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }
    }

    void onUnitDestroy(const Unit &unit)
    {
        // Whenever a building is lost, determine if it infers a change in base ownership
        inferBaseOwnershipFromUnitDestroyed(unit);

        // Units that affect tile walkability
        // Skip refineries, since destruction of a refinery does not affect tile walkability (there will still be a geyser)
        if (unit->type.isBuilding() && !unit->type.isRefinery())
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->type.tileSize(), true))
            {
                PathFinding::removeBlockingObject(unit->type, unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }
    }

    void onUnitDestroy(BWAPI::Unit unit)
    {
        if (unit->getType().isMineralField())
            BWEM::Map::Instance().OnMineralDestroyed(unit);
        else if (unit->getType().isSpecialBuilding())
            BWEM::Map::Instance().OnStaticBuildingDestroyed(unit);

        // Units that affect tile walkability
        if (unit->getType().isMineralField() ||
            (unit->getType().isBuilding() && !unit->getType().isRefinery()) ||
            (unit->getType() == BWAPI::UnitTypes::Zerg_Egg && unit->getPlayer() == BWAPI::Broodwar->neutral()))
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->getType().tileSize(), true))
            {
                PathFinding::removeBlockingObject(unit->getType(), unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }

        _mapSpecificOverride->onUnitDestroy(unit);
    }

    void onBuildingLifted(BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        if (updateTileWalkability(tile, type.tileSize(), true))
        {
            PathFinding::removeBlockingObject(type, tile);
            tileWalkabilityUpdated = true;
        }
    }

    void onBuildingLanded(BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        if (updateTileWalkability(tile, type.tileSize(), false))
        {
            PathFinding::addBlockingObject(type, tile);
            tileWalkabilityUpdated = true;
        }
    }

    void update()
    {
        // Update base scouting
        for (auto &base : bases)
        {
            // Is the center of where the resource depot should be visible?
            if (BWAPI::Broodwar->isVisible(base->getTilePosition().x + 1, base->getTilePosition().y + 1) ||
                BWAPI::Broodwar->isVisible(base->getTilePosition().x + 2, base->getTilePosition().y + 1))
            {
                base->lastScouted = BWAPI::Broodwar->getFrameCount();
            }

                // If the base hasn't been owned for a while, and the enemy could be zerg, can we see creep?
            else if (
                    (base->ownedSince == -1 || base->ownedSince < (BWAPI::Broodwar->getFrameCount() - 2500)) &&
                    BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran &&
                    BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss &&
                    checkCreep(base))
            {
                setBaseOwner(base, BWAPI::Broodwar->enemy());

                // Don't set lastScouted as this refers to when the depot location was seen
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
        if (BWAPI::Broodwar->getFrameCount() % 24 == 17)
        {
            for (auto &base : bases)
            {
                validateBaseOwnership(base);
            }
        }

        if (tileWalkabilityUpdated)
        {
            dumpTileWalkability();
            tileWalkabilityUpdated = false;
        }

        if (noGoAreaTilesUpdated)
        {
            dumpNoGoAreaTiles();
            noGoAreaTilesUpdated = false;
        }

        // Update the last seen frame for all visible tiles
        for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
        {
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                if (BWAPI::Broodwar->isVisible(x, y))
                {
                    tileLastSeen[x + y * BWAPI::Broodwar->mapWidth()] = BWAPI::Broodwar->getFrameCount();
                }
            }
        }
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
        for (auto base : bases)
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

    std::set<Base *> unscoutedStartingLocations()
    {
        std::set<Base *> result;
        for (auto base : startingLocationBases)
        {
            if (base->owner) continue;
            if (base->lastScouted >= 0) continue;

            result.insert(base);
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
        return myStartingMainAreas;
    }

    void dumpVisibilityHeatmap()
    {
#if CHERRYVIS_ENABLED
        std::vector<long> newVisibility(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);
        for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
        {
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                if (BWAPI::Broodwar->isVisible(x, y))
                    newVisibility[x + y * BWAPI::Broodwar->mapWidth()] = 1;
            }
        }

        if (newVisibility != visibility)
        {
            CherryVis::addHeatmap("FogOfWar", newVisibility, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
            visibility = newVisibility;
        }

        std::vector<long> newPower(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);
        for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
        {
            for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
            {
                if (BWAPI::Broodwar->hasPower(x, y, BWAPI::UnitTypes::Protoss_Photon_Cannon))
                    newPower[x + y * BWAPI::Broodwar->mapWidth()] = 1;
            }
        }

        if (newPower != power)
        {
            CherryVis::addHeatmap("Power", newPower, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
            power = newPower;
        }
#endif
    }

    bool isWalkable(BWAPI::TilePosition pos)
    {
        return isWalkable(pos.x, pos.y);
    }

    bool isWalkable(int x, int y)
    {
        return tileWalkability[x + y * BWAPI::Broodwar->mapWidth()];
    }

    int unwalkableProximity(int x, int y)
    {
        return tileUnwalkableProximity[x + y * BWAPI::Broodwar->mapWidth()];
    }

    BWAPI::Position collisionVector(int x, int y)
    {
        return tileCollisionVector[x + y * BWAPI::Broodwar->mapWidth()];
    }

    bool isInOwnMineralLine(BWAPI::TilePosition tile)
    {
        return isInOwnMineralLine(tile.x, tile.y);
    }

    bool isInOwnMineralLine(int x, int y)
    {
        return inOwnMineralLine[x + y * BWAPI::Broodwar->mapWidth()];
    }

    bool isInNarrowChoke(BWAPI::TilePosition pos)
    {
        return narrowChokeTiles[pos.x + pos.y * BWAPI::Broodwar->mapWidth()];
    }

    bool isInLeafArea(BWAPI::TilePosition pos)
    {
        return leafAreaTiles[pos.x + pos.y * BWAPI::Broodwar->mapWidth()];
    }

    int lastSeen(BWAPI::TilePosition tile)
    {
        return tileLastSeen[tile.x + tile.y * BWAPI::Broodwar->mapWidth()];
    }

    int lastSeen(int x, int y)
    {
        return tileLastSeen[x + y * BWAPI::Broodwar->mapWidth()];
    }

    void addNoGoArea(BWAPI::TilePosition topLeft, BWAPI::TilePosition size)
    {
        for (int x = topLeft.x; x < topLeft.x + size.x; x++)
        {
            if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

            for (int y = topLeft.y; y < topLeft.y + size.y; y++)
            {
                if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

                noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()]++;
            }
        }

        noGoAreaTilesUpdated = true;
    }

    void removeNoGoArea(BWAPI::TilePosition topLeft, BWAPI::TilePosition size)
    {
        for (int x = topLeft.x; x < topLeft.x + size.x; x++)
        {
            if (x < 0 || x >= BWAPI::Broodwar->mapWidth()) continue;

            for (int y = topLeft.y; y < topLeft.y + size.y; y++)
            {
                if (y < 0 || y >= BWAPI::Broodwar->mapWidth()) continue;

                noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()]--;
            }
        }

        noGoAreaTilesUpdated = true;
    }

    bool isInNoGoArea(BWAPI::TilePosition pos)
    {
        return noGoAreaTiles[pos.x + pos.y * BWAPI::Broodwar->mapWidth()] > 0;
    }

    bool isInNoGoArea(int x, int y)
    {
        return noGoAreaTiles[x + y * BWAPI::Broodwar->mapWidth()] > 0;
    }
}
