#include "Map.h"

#include "MapSpecificOverrides/Fortress.h"
#include "MapSpecificOverrides/Plasma.h"

#include "Units.h"
#include "PathFinding.h"

/*
Base ownership:
- Set our main base on startup
- Set enemy main base on startup if there are only two starting locations
- Don't set enemy main base if it takes another main
- Set enemy main base if we see a building near it
*/

namespace Map
{
    namespace
    {
        MapSpecificOverride *_mapSpecificOverride;
        std::vector<Base *> bases;
        std::vector<Base *> startingLocationBases;
        std::vector<std::vector<Base *>> baseClusters;
        std::map<const BWEM::ChokePoint *, Choke *> chokes;
        int _minChokeWidth;

        std::vector<bool> tileWalkability;
        std::vector<int> tileUnwalkableProximity;
        bool tileWalkabilityUpdated;

        std::vector<bool> narrowChokeTiles;
        std::vector<bool> leafAreaTiles;

#if CHERRYVIS_ENABLED
        std::vector<long> visibility;
#endif

        struct PlayerBases
        {
            Base *startingMain;
            Base *startingNatural;
            Base *main;
            std::set<Base *> allOwned;
            std::vector<Base *> probableExpansions;

            PlayerBases() : startingMain(nullptr), startingNatural(nullptr), main(nullptr) {}
        };

        std::map<BWAPI::Player, PlayerBases> playerToPlayerBases;

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

        void computeProbableExpansions(BWAPI::Player player, PlayerBases &playerBases)
        {
            playerBases.probableExpansions.clear();

            // If we don't yet know the location of this player's starting base, we can't guess where they will expand
            if (!playerBases.startingMain) return;

            auto myBases = getMyBases(player);
            auto enemyBases = getEnemyBases(player);

            std::vector<std::pair<int, Base *>> scoredBases;
            for (auto base : bases)
            {
                if (base->owner) continue;

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
                if (distanceFromUs == -1) continue;

                // Want to be far from the enemy base.
                int distanceFromEnemy = closestBaseDistance(base, enemyBases);
                if (distanceFromEnemy == -1) distanceFromEnemy = 10000;

                // Initialize score based on distances
                int score = (distanceFromEnemy * 3) / 2 - distanceFromUs;

                // Increase score based on available resources
                score += base->minerals() / 100;
                score += base->gas() / 50;

                scoredBases.emplace_back(std::make_pair(score, base));
            }

            if (scoredBases.empty()) return;

            std::sort(scoredBases.begin(), scoredBases.end());
            std::reverse(scoredBases.begin(), scoredBases.end());
            for (auto &pair : scoredBases)
            {
                playerBases.probableExpansions.push_back(pair.second);
            }
        }

        void setBaseOwner(Base *base, BWAPI::Player owner)
        {
            if (base->owner == owner) return;

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
            }

            auto playerLabel = [](BWAPI::Player player)
            {
                return player ? player->getName() : "(none)";
            };
            CherryVis::log() << "Changing base " << base->getTilePosition() << " owner from " << playerLabel(base->owner) << " to "
                             << playerLabel(owner);

            base->owner = owner;
            base->ownedSince = BWAPI::Broodwar->getFrameCount();
            base->resourceDepot = nullptr;

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
                    ownerBases.main = base;
                }

                // If this player had a starting main, but doesn't currently have a main, set this base as the main
                if (ownerBases.startingMain && !ownerBases.main)
                {
                    ownerBases.main = base;
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
            if (!unit->type.isBuilding() && !unit->type.isAddon()) return;

            // Check if there is a base near the building
            auto nearbyBase = baseNear(unit->lastPosition);
            if (nearbyBase)
            {
                // Is this unit a resource depot that is closer than the existing resource depot registered for this base?
                bool isCloserDepot = false;
                if (unit->type.isResourceDepot())
                {
                    if (nearbyBase->resourceDepot && nearbyBase->resourceDepot->exists())
                    {
                        int existingDist = nearbyBase->resourceDepot->lastPosition.getApproxDistance(nearbyBase->getPosition());
                        int newDist = unit->lastPosition.getApproxDistance(nearbyBase->getPosition());
                        isCloserDepot = newDist < existingDist;
                    }
                    else
                        isCloserDepot = true;
                }

                // If the base was previously unowned, or this is a closer depot, change the ownership
                if (!nearbyBase->owner || isCloserDepot)
                {
                    setBaseOwner(nearbyBase, unit->player);

                    if (isCloserDepot)
                    {
                        nearbyBase->resourceDepot = unit;
                        if (unit->player == BWAPI::Broodwar->self())
                        {
                            unit->bwapiUnit->setRallyPoint(nearbyBase->mineralLineCenter);
                        }
                    }
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
                }
            }
        }

        // Given a newly-destroyed building, determine whether it infers something about base ownership.
        // If the lost unit is a building near a base owned by the unit's owner, change the base ownership
        // if the player has no buildings left near the base.
        void inferBaseOwnershipFromUnitDestroyed(Unit unit)
        {
            // Only consider buildings
            if (!unit->type.isBuilding()) return;

            // Ensure there is an owned base near the building
            auto nearbyBase = baseNear(unit->lastPosition);
            if (!nearbyBase || !nearbyBase->owner) return;

            // Ignore if the building wasn't owned by the base owner
            if (nearbyBase->owner != unit->player)
            {
                return;
            }

            // If the player still has a building near the base, don't update ownership
            auto isNearbyBuilding = [&unit, &nearbyBase](const Unit &other)
            {
                if (other->id == unit->id) return false;
                if (!other->type.isBuilding()) return false;
                if (!other->lastPositionValid) return false;
                return nearbyBase->getPosition().getApproxDistance(other->lastPosition) < 320;
            };

            if (nearbyBase->owner == BWAPI::Broodwar->self())
            {
                for (auto &other : Units::allMine())
                {
                    if (isNearbyBuilding(other)) return;
                }
            }
            else
            {
                for (auto &other : Units::allEnemy())
                {
                    if (isNearbyBuilding(other)) return;
                }
            }

            // The player has lost the base
            setBaseOwner(nearbyBase, nullptr);
        }

        void checkCreep(Base *base)
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
                        setBaseOwner(base, BWAPI::Broodwar->enemy());
                        base->lastScouted = BWAPI::Broodwar->getFrameCount();
                        return;
                    }
                }
            }
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
                                goto breakInnerLoop;
                            }
                        }
                    }
                    breakInnerLoop:
                    tileWalkability[tileX + tileY * BWAPI::Broodwar->mapWidth()] = walkable;
                }
            }

            // Add all mineral lines
            for (auto &base : allBases())
            {
                for (auto mineralLineTile : base->mineralLineTiles)
                {
                    tileWalkability[mineralLineTile.x + mineralLineTile.y * BWAPI::Broodwar->mapWidth()] = false;
                }
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
                if (pair.second->width > 96) continue;

                BWAPI::TilePosition chokeCenter(pair.second->Center());

                for (int x = -5; x <= 5; x++)
                {
                    for (int y = -5; y <= 5; y++)
                    {
                        BWAPI::TilePosition here(chokeCenter.x + x, chokeCenter.y + y);
                        if (!here.isValid()) continue;

                        narrowChokeTiles[here.x + here.y * BWAPI::Broodwar->mapWidth()] = true;
                    }
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
            std::set<const BWEM::Area *> leafAreas;
            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                for (const auto &bwemChoke : area.ChokePoints())
                {
                    auto choke = Map::choke(bwemChoke);
                    if (!choke) continue;
                    if (choke->width > 96) goto NextArea;
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
#endif
        }
    }

    void initialize()
    {
        _mapSpecificOverride = nullptr;
        bases.clear();
        startingLocationBases.clear();
        baseClusters.clear();
        chokes.clear();
        _minChokeWidth = 0;
        tileWalkability.clear();
        tileUnwalkableProximity.clear();
        tileWalkabilityUpdated = false;
        narrowChokeTiles.clear();
        leafAreaTiles.clear();
#if CHERRYVIS_ENABLED
        visibility.clear();
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
            _mapSpecificOverride = new Fortress();
        else if (BWAPI::Broodwar->mapHash() == "6f5295624a7e3887470f3f2e14727b1411321a67")
            _mapSpecificOverride = new Plasma();
        else
            _mapSpecificOverride = new MapSpecificOverride();

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

        // Initialize base clusters
        // These are groups of bases that are close enough together that we can freely transfer probes between them
        // On most maps this will be the main/natural pairs

        // Start by adding each base to its own cluster
        for (auto &base : bases)
        {
            std::vector<Base *> cluster{base};
            baseClusters.emplace_back(std::move(cluster));
        }

        // Now continually try to combine clusters until there are no more to combine
        for (auto clusterIt = baseClusters.begin(); clusterIt != baseClusters.end();)
        {
            auto otherClusterIt = clusterIt;
            otherClusterIt++;
            for (; otherClusterIt != baseClusters.end(); otherClusterIt++)
            {
                // Combine the two clusters if any of the bases are within 300 frames of worker travel from each other
                for (auto first : *clusterIt)
                {
                    for (auto second : *otherClusterIt)
                    {
                        int time = PathFinding::ExpectedTravelTime(first->getPosition(),
                                                                   second->getPosition(),
                                                                   BWAPI::UnitTypes::Protoss_Probe);
                        if (time <= 300)
                        {
                            clusterIt->insert(clusterIt->end(),
                                              std::make_move_iterator(otherClusterIt->begin()),
                                              std::make_move_iterator(otherClusterIt->end()));
                            baseClusters.erase(otherClusterIt);
                            goto continueOuterLoop;
                        }
                    }
                }
            }
            clusterIt++;
            continueOuterLoop:;
        }

        computeTileWalkability();
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
        if (unit->getType().isMineralField() || (unit->getType().isBuilding() && !unit->getType().isRefinery()))
        {
            if (updateTileWalkability(unit->getTilePosition(), unit->getType().tileSize(), true))
            {
                PathFinding::removeBlockingObject(unit->getType(), unit->getTilePosition());
                tileWalkabilityUpdated = true;
            }
        }
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
        // Update unowned base scouting
        for (auto base : bases)
        {
            if (base->owner) continue;

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
                    BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss)
            {
                checkCreep(base);
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

        if (tileWalkabilityUpdated)
        {
            dumpTileWalkability();
            tileWalkabilityUpdated = false;
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

    std::vector<std::vector<Base *>> &allBaseClusters()
    {
        return baseClusters;
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

    Base *getMyMain()
    {
        return playerToPlayerBases[BWAPI::Broodwar->self()].startingMain;
    }

    Base *getMyNatural()
    {
        return playerToPlayerBases[BWAPI::Broodwar->self()].startingNatural;
    }

    Base *getEnemyStartingMain()
    {
        return playerToPlayerBases[BWAPI::Broodwar->enemy()].startingMain;
    }

    Base *getEnemyMain()
    {
        return playerToPlayerBases[BWAPI::Broodwar->enemy()].main;
    }

    Base *baseNear(BWAPI::Position position)
    {
        int closestDist = INT_MAX;
        Base *result = nullptr;

        for (auto &base : bases)
        {
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

    Base *getNaturalForStartLocation(BWAPI::TilePosition startLocation)
    {
        auto startPosition = BWAPI::Position(startLocation) + BWAPI::Position(64, 48);

        // The natural base is the closest base that:
        // - has gas
        // - is closer (by ground) to at least one other start position

        // Pick a natural base
        Base *bestNatural = nullptr;
        int bestDist = INT_MAX;
        for (auto base : bases)
        {
            if (base->getTilePosition() == startLocation) continue;
            if (base->gas() == 0) continue;

            int dist = PathFinding::GetGroundDistance(
                    startPosition,
                    base->getPosition(),
                    BWAPI::UnitTypes::Protoss_Zealot,
                    PathFinding::PathFindingOptions::UseNearestBWEMArea);
            if (dist == -1 || dist > bestDist) continue;

            bool closerToOtherStartLocation = false;
            for (auto otherStartLocation : BWAPI::Broodwar->getStartLocations())
            {
                if (startLocation == otherStartLocation) continue;
                auto otherPosition = BWAPI::Position(otherStartLocation) + BWAPI::Position(64, 48);
                int distFromMain = PathFinding::GetGroundDistance(
                        startPosition,
                        otherPosition,
                        BWAPI::UnitTypes::Protoss_Zealot,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
                int distFromBase = PathFinding::GetGroundDistance(
                        base->getPosition(),
                        otherPosition,
                        BWAPI::UnitTypes::Protoss_Zealot,
                        PathFinding::PathFindingOptions::UseNearestBWEMArea);
                if (distFromBase != -1 && distFromMain > distFromBase)
                {
                    closerToOtherStartLocation = true;
                    break;
                }
            }

            if (closerToOtherStartLocation)
            {
                bestNatural = base;
                bestDist = dist;
            }
        }

        return bestNatural;
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
        auto main = getMyMain();
        auto natural = getMyNatural();
        if (!main || !natural) return nullptr;

        // Main choke is defined as the last choke traversed in a path from the main to the natural
        auto path = PathFinding::GetChokePointPath(
                main->getPosition(),
                natural->getPosition(),
                BWAPI::UnitTypes::Protoss_Dragoon,
                PathFinding::PathFindingOptions::UseNearestBWEMArea);
        if (path.empty()) return nullptr;

        return choke(*path.rbegin());
    }

    bool nearNarrowChokepoint(BWAPI::Position position)
    {
        for (auto choke : chokes)
        {
            if (choke.second->width < 64 && choke.second->Center().getApproxDistance(position) < 64)
            {
                return true;
            }
        }

        return false;
    }

    int minChokeWidth()
    {
        return _minChokeWidth;
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

    bool isInNarrowChoke(BWAPI::TilePosition pos)
    {
        return narrowChokeTiles[pos.x + pos.y * BWAPI::Broodwar->mapWidth()];
    }

    bool isInLeafArea(BWAPI::TilePosition pos)
    {
        return leafAreaTiles[pos.x + pos.y * BWAPI::Broodwar->mapWidth()];
    }
}
