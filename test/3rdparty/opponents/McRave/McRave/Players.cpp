#include "McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Players
{
    namespace {
        map <Player, PlayerInfo> thePlayers;
        map <Race, int> raceCount;
        map <PlayerState, map<UnitType, int>> allVisibleTypeCounts;
        map <PlayerState, map<UnitType, int>> allCompleteTypeCounts;
        map <PlayerState, map<UnitType, int>> allTotalTypeCounts;

        void update(PlayerInfo& player)
        {
            // Add up the number of each race - HACK: Don't add self for now
            player.update();
            if (!player.isSelf())
                raceCount[player.getCurrentRace()]++;

            for (auto &[type, cnt] : player.getVisibleTypeCounts())
                allVisibleTypeCounts[player.getPlayerState()][type]+=cnt;
            for (auto &[type, cnt] : player.getCompleteTypeCounts())
                allCompleteTypeCounts[player.getPlayerState()][type]+=cnt;
            for (auto &[type, cnt] : player.getTotalTypeCounts())
                allTotalTypeCounts[player.getPlayerState()][type]+=cnt;
        }
    }

    void onStart()
    {
        // Store all players
        for (auto player : Broodwar->getPlayers()) {
            auto race = player->isNeutral() ? Races::None : player->getRace(); // BWAPI returns Zerg for neutral race

            PlayerInfo &p = thePlayers[player];
            p.setPlayer(player);
            p.setStartRace(race);
            p.update();

            if (!p.isSelf())
                raceCount[p.getCurrentRace()]++;
        }
    }

    void onFrame()
    {
        // Clear race count and recount
        allVisibleTypeCounts.clear();
        allCompleteTypeCounts.clear();
        allTotalTypeCounts.clear();
        raceCount.clear();
        for (auto &[_, player] : thePlayers)
            update(player);
    }

    void storeUnit(Unit bwUnit)
    {
        auto p = getPlayerInfo(bwUnit->getPlayer());
        auto info = make_shared<UnitInfo>(bwUnit);

        if (Units::getUnitInfo(bwUnit))
            return;

        if (bwUnit->getType().isBuilding() && bwUnit->getPlayer() == Broodwar->self()) {
            auto closestWorker = Util::getClosestUnit(bwUnit->getPosition(), PlayerState::Self, [&](auto &u) {
                return u.getRole() == Role::Worker && (u.getType() != Terran_SCV || bwUnit->isCompleted()) && u.getBuildPosition() == bwUnit->getTilePosition();
            });
            if (closestWorker) {
                closestWorker->setBuildingType(None);
                closestWorker->setBuildPosition(TilePositions::Invalid);
            }
        }

        if (p) {

            // Setup the UnitInfo and update
            p->getUnits().insert(info);
            info->update();
            auto &counts = p->getTotalTypeCounts();

            // Increase total counts
            if (Broodwar->getFrameCount() == 0 || !p->isSelf() || (info->getType() != Zerg_Zergling && info->getType() != Zerg_Scourge))
                counts[info->getType()] += 1;
        }
    }

    void removeUnit(Unit bwUnit)
    {
        BWEB::Map::onUnitDestroy(bwUnit);

        // Find the unit
        for (auto &[_, player] : Players::getPlayers()) {
            for (auto &u : player.getUnits()) {
                if (u->unit() == bwUnit) {

                    // Remove assignments and roles
                    if (u->hasTransport())
                        Transports::removeUnit(*u);
                    if (u->hasResource())
                        Workers::removeUnit(*u);
                    if (u->getRole() == Role::Scout)
                        Scouts::removeUnit(*u);

                    // Invalidates iterator, must return
                    player.getUnits().erase(u);
                    return;
                }
            }
        }
    }

    void morphUnit(Unit bwUnit)
    {
        auto p = getPlayerInfo(bwUnit->getPlayer());

        // HACK: Changing players is kind of annoying, so we just remove and re-store
        if (bwUnit->getType().isRefinery()) {
            removeUnit(bwUnit);
            storeUnit(bwUnit);
            BWEB::Map::addUsed(bwUnit->getTilePosition(), bwUnit->getType());   // Storing doesn't seem to re-add the used tiles right now
        }

        // Morphing into a Hatchery
        if (bwUnit->getType() == Zerg_Hatchery)
            Stations::storeStation(bwUnit);

        // Grab the UnitInfo for this unit
        auto info = Units::getUnitInfo(bwUnit);
        if (info) {
            if (info->hasResource())
                Workers::removeUnit(*info);
            if (info->hasTransport())
                Transports::removeUnit(*info);
            if (info->hasTarget())
                info->setTarget(nullptr);
            if (info->getRole() == Role::Scout)
                Scouts::removeUnit(*info);

            auto &counts = p->getTotalTypeCounts();

            // When we morph a larva, store the type of what we are making, rather than the egg
            if (p->isSelf() && info->getType() == Zerg_Larva) {
                counts[bwUnit->getBuildType()] += 1 + (bwUnit->getBuildType() == Zerg_Zergling || bwUnit->getBuildType() == Zerg_Scourge);
                counts[info->getType()]--;
            }

            // When enemy morphs a larva, store the egg type, when the egg hatches, store the morphed type
            if (!p->isSelf() && (info->getType() == Zerg_Larva || info->getType() == Zerg_Egg)) {
                counts[bwUnit->getType()] += 1;
                counts[info->getType()] -= 1;
            }

            // When an existing type morphs again
            if (info->getType() == Zerg_Hydralisk || info->getType() == Zerg_Lurker_Egg || info->getType() == Zerg_Mutalisk || info->getType() == Zerg_Cocoon) {
                counts[bwUnit->getType()] += 1;
                counts[info->getType()] -= 1;
            }

            // When an existing Drone morphs into a building
            if (info->getType() == Zerg_Drone && bwUnit->getType().isBuilding()) {
                counts[bwUnit->getType()] += 1;
                counts[info->getType()] -= 1;
            }

            // When an existing building morphs - use the whatBuilds due to onUnitMorph occuring 1 frame after
            if (info->getType() == Zerg_Sunken_Colony || info->getType() == Zerg_Spore_Colony || info->getType() == Zerg_Lair || info->getType() == Zerg_Hive || info->getType() == Zerg_Greater_Spire) {
                counts[bwUnit->getType()] += 1;
                counts[bwUnit->getType().whatBuilds().first] -= 1;
            }

            info->setBuildingType(None);
            info->setBuildPosition(TilePositions::Invalid);
        }
    }

    int getCompleteCount(PlayerState state, UnitType type)
    {
        // Finds how many of a UnitType this PlayerState currently has completed
        auto &list = allCompleteTypeCounts[state];
        auto itr = list.find(type);
        if (itr != list.end())
            return itr->second;
        return 0;
    }

    int getVisibleCount(PlayerState state, UnitType type)
    {
        // Finds how many of a UnitType this PlayerState currently has visible
        auto &list = allVisibleTypeCounts[state];
        auto itr = list.find(type);
        if (itr != list.end())
            return itr->second;
        return 0;
    }

    int getTotalCount(PlayerState state, UnitType type)
    {
        // Finds how many of a UnitType the PlayerState total has ever had
        auto &list = allTotalTypeCounts[state];
        auto itr = list.find(type);
        if (itr != list.end())
            return itr->second;
        return 0;
    }

    bool hasDetection(PlayerState state)
    {
        return getTotalCount(state, Protoss_Observer) > 0
            || getTotalCount(state, Protoss_Photon_Cannon) > 0
            || getTotalCount(state, Terran_Science_Vessel) > 0
            || getTotalCount(state, Terran_Missile_Turret) > 0
            || getTotalCount(state, Zerg_Overlord) > 0;
    }

    int getSupply(PlayerState state, Race race)
    {
        auto combined = 0;
        for (auto &[_, player] : thePlayers) {
            if (player.getPlayerState() == state)
                combined += player.getSupply(race);
        }
        return combined;
    }

    int getRaceCount(Race race, PlayerState state)
    {
        auto combined = 0;
        for (auto &[_, player] : thePlayers) {
            if (player.getCurrentRace() == race && player.getPlayerState() == state)
                combined += 1;
        }
        return combined;
    }

    Strength getStrength(PlayerState state)
    {
        Strength combined;
        for (auto &[_, player] : thePlayers) {
            if (player.getPlayerState() == state)
                combined += player.getStrength();
        }
        return combined;
    }

    PlayerInfo * getPlayerInfo(Player player)
    {
        for (auto &[p, info] : thePlayers) {
            if (p == player)
                return &info;
        }
        return nullptr;
    }

    map <Player, PlayerInfo>& getPlayers() { return thePlayers; }
    bool vP() { return (thePlayers.size() == 3 && raceCount[Races::Protoss] > 0); }
    bool vT() { return (thePlayers.size() == 3 && raceCount[Races::Terran] > 0); }
    bool vZ() { return (thePlayers.size() == 3 && raceCount[Races::Zerg] > 0); }
}