#pragma once
#include <BWAPI.h>
#include <set>

namespace McRave
{
    class PlayerInfo
    {
        std::set<BWAPI::UpgradeType> playerUpgrades;
        std::set<BWAPI::TechType> playerTechs;
        std::set<std::shared_ptr<UnitInfo>> units;
        std::map<BWAPI::UnitType, int> visibleTypeCounts;
        std::map<BWAPI::UnitType, int> completeTypeCounts;
        std::map<BWAPI::UnitType, int> totalTypeCounts;

        BWAPI::Player thisPlayer;
        BWAPI::Race startRace, currentRace;
        BWAPI::TilePosition startLocation;
        std::string build, opener, transition;

        Strength pStrength;
        PlayerState pState;

        bool alive;
        std::map<BWAPI::Race, int> raceSupply;
    public:
        PlayerInfo() {
            thisPlayer = nullptr;
            currentRace = BWAPI::Races::None;
            startRace = BWAPI::Races::None;
            alive = true;
            pState = PlayerState::None;
            build = "Unknown";
        }

        bool operator== (PlayerInfo const& p) const {
            return thisPlayer == p.player();
        }

        bool operator< (PlayerInfo const& p) const {
            return thisPlayer < p.player();
        }

        void update();

        BWAPI::TilePosition getStartingLocation() { return startLocation; }
        BWAPI::Race getCurrentRace() { return currentRace; }
        BWAPI::Race getStartRace() { return startRace; }
        BWAPI::Player player() const { return thisPlayer; }
        PlayerState getPlayerState() { return pState; }
        Strength& getStrength() { return pStrength; }
        std::set<std::shared_ptr<UnitInfo>>& getUnits() { return units; }
        std::string getBuild() { return build; }

        std::map<BWAPI::UnitType, int>& getVisibleTypeCounts() { return visibleTypeCounts; }
        std::map<BWAPI::UnitType, int>& getCompleteTypeCounts() { return completeTypeCounts; }
        std::map<BWAPI::UnitType, int>& getTotalTypeCounts() { return totalTypeCounts; }

        bool isAlive() { return alive; }
        bool isEnemy() { return pState == PlayerState::Enemy; }
        bool isAlly() { return pState == PlayerState::Ally; }
        bool isSelf() { return pState == PlayerState::Self; }
        bool isNeutral() { return pState == PlayerState::Neutral; }

        int getSupply(BWAPI::Race race) { return raceSupply[race]; }

        void setCurrentRace(BWAPI::Race newRace) { currentRace = newRace; }
        void setStartRace(BWAPI::Race newRace) { startRace = newRace; }
        void setPlayer(BWAPI::Player newPlayer) { thisPlayer = newPlayer; }
        void setAlive(bool newState) { alive = newState; }
        void setBuild(std::string newBuild) { build = newBuild; }
        void setOpener(std::string newOpener) { opener = newOpener; }
        void setTransition(std::string newTransition) { transition = newTransition; }

        bool hasUpgrade(BWAPI::UpgradeType upgrade) {
            if (playerUpgrades.find(upgrade) != playerUpgrades.end())
                return true;
            return false;
        }
        bool hasTech(BWAPI::TechType tech) {
            if (playerTechs.find(tech) != playerTechs.end())
                return true;
            return false;
        }
    };
}