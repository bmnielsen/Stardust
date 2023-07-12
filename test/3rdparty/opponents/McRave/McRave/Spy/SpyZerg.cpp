#include "..\McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Spy::Zerg {

    namespace {

        void enemyZergBuilds(PlayerInfo& player, StrategySpy& theSpy)
        {
            auto enemyHatchCount = Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Hive);

            for (auto &u : player.getUnits()) {
                UnitInfo &unit =*u;

                // If this is our first time seeing a Zergling, see how soon until it arrives
                if (unit.getType() == UnitTypes::Zerg_Zergling) {
                    if (unit.timeArrivesWhen().minutes > 0 && unit.timeArrivesWhen() < theSpy.rushArrivalTime)
                        theSpy.rushArrivalTime = unit.timeArrivesWhen();
                }
            }

            // Zerg builds
            if ((theSpy.expand.possible || enemyHatchCount > 1) && completesBy(2, Zerg_Hatchery, Time(3, 15)))
                theSpy.build.name = "HatchPool";
            else if ((theSpy.expand.possible || enemyHatchCount > 1) && completesBy(2, Zerg_Hatchery, Time(4, 00)))
                theSpy.build.name = "PoolHatch";
            else if (!theSpy.expand.possible && enemyHatchCount == 1 && completesBy(1, Zerg_Lair, Time(3, 45)))
                theSpy.build.name = "PoolLair";

            // Build based on the opener seen
            if (theSpy.opener.name == "4Pool" || theSpy.opener.name == "9Pool" || theSpy.opener.name == "OverPool" || theSpy.opener.name == "12Pool") {
                if (enemyHatchCount > 1)
                    theSpy.build.name = "PoolHatch";
                else if (completesBy(1, Zerg_Lair, Time(3, 45)))
                    theSpy.build.name = "PoolLair";
            }
            if (enemyHatchCount > 1 && (theSpy.opener.name == "9Hatch" || theSpy.opener.name == "10Hatch" || theSpy.opener.name == "12Hatch"))
                theSpy.build.name = "HatchPool";

            // Build based on the transition seen
            if (theSpy.transition.name == "1HatchMuta" || theSpy.transition.name == "1HatchLurker")
                theSpy.build.name = "PoolLair";
        }

        void enemyZergOpeners(PlayerInfo& player, StrategySpy& theSpy)
        {
            auto enemyHatchCount = Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Hive);

            // Pool timing
            if (theSpy.build.name == "PoolHatch" || theSpy.build.name == "PoolLair" || (vis(Zerg_Zergling) > 0 && Util::getTime() < Time(3, 45))) {
                if (theSpy.rushArrivalTime < Time(2, 45)
                    || completesBy(1, Zerg_Zergling, Time(2, 05))
                    || completesBy(1, Zerg_Spawning_Pool, Time(1, 40))
                    || arrivesBy(8, Zerg_Zergling, Time(3, 00)))
                    theSpy.opener.name = "4Pool";
                else if (theSpy.rushArrivalTime < Time(3, 00)
                    || completesBy(1, Zerg_Zergling, Time(2, 15))
                    || completesBy(1, Zerg_Spawning_Pool, Time(1, 50)))
                    theSpy.opener.name = "7Pool";
                else if (theSpy.rushArrivalTime < Time(3, 15)
                    || completesBy(1, Zerg_Zergling, Time(2, 40))
                    || completesBy(8, Zerg_Zergling, Time(3, 00))
                    || completesBy(10, Zerg_Zergling, Time(3, 20))
                    || completesBy(12, Zerg_Zergling, Time(3, 30))
                    || completesBy(14, Zerg_Zergling, Time(3, 40))
                    || completesBy(1, Zerg_Spawning_Pool, Time(2, 00))
                    || completesBy(1, Zerg_Lair, Time(3, 45)))
                    theSpy.opener.name = "9Pool";
                else if (theSpy.rushArrivalTime < Time(3, 50)
                    || theSpy.expand.possible
                    || completesBy(1, Zerg_Zergling, Time(3, 00))
                    || completesBy(1, Zerg_Spawning_Pool, Time(2, 00)))
                    theSpy.opener.name = "12Pool";
            }

            // Hatch timing
            if (theSpy.build.name == "HatchPool") {
                if (completesBy(2, Zerg_Hatchery, Time(2, 50)))
                    theSpy.opener.name = "9Hatch";
                if (completesBy(2, Zerg_Hatchery, Time(3, 05)))
                    theSpy.opener.name = "10Hatch";
            }
        }

        void enemyZergTransitions(PlayerInfo& player, StrategySpy& theSpy)
        {
            auto enemyHatchCount = Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair)
                + Players::getVisibleCount(PlayerState::Enemy, Zerg_Hive);

            // Ling rush detection
            if (theSpy.opener.name == "4Pool"
                || (!Players::ZvZ() && theSpy.opener.name == "9Pool" && Util::getTime() > Time(3, 30) && Util::getTime() < Time(4, 30) && Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) == 0 && enemyHatchCount == 1))
                theSpy.transition.name = "LingRush";

            // Zergling all-in transitions
            if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Spire) == 0 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) == 0 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) == 0) {
                if (Players::ZvZ() && ((theSpy.opener.name == "10Hatch" && Util::getTime() > Time(3, 45)) || (Players::getVisibleCount(PlayerState::Enemy, Zerg_Evolution_Chamber) == 0) && Util::getTime() > Time(4, 30)))
                    theSpy.transition.name = "2HatchSpeedling";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 3 && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 12 && Util::getTime() > Time(3, 45))
                    theSpy.transition.name = "3HatchSpeedling";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Evolution_Chamber) > 0 && !theSpy.expand.possible && Players::getTotalCount(PlayerState::Enemy, Zerg_Zergling) >= 12 && Util::getTime() < Time(4, 00))
                    theSpy.transition.name = "+1Ling";
            }

            // Hydralisk/Lurker build detection
            if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Spire) == 0 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) > 0) {
                if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 3)
                    theSpy.transition.name = "3HatchHydra";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 2)
                    theSpy.transition.name = "2HatchHydra";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 1)
                    theSpy.transition.name = "1HatchHydra";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) + Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 2)
                    theSpy.transition.name = "2HatchLurker";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) == 1 && Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) == 0)
                    theSpy.transition.name = "1HatchLurker";
                else if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hatchery) >= 4)
                    theSpy.transition.name = "5Hatch";
            }

            // Mutalisk transition detection
            if (Players::getVisibleCount(PlayerState::Enemy, Zerg_Hydralisk_Den) == 0) {
                if ((Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) > 0) && enemyHatchCount == 3)
                    theSpy.transition.name = "3HatchMuta";
                else if ((Util::getTime() < Time(3, 30) && Players::getVisibleCount(PlayerState::Enemy, Zerg_Lair) > 0 && (enemyHatchCount == 2 || theSpy.expand.possible)))
                    theSpy.transition.name = "2HatchMuta";
                else if ((Spy::getEnemyBuild() == "PoolLair" && Players::ZvZ())
                    || (completesBy(1, Zerg_Spire, Time(5, 15))))
                    theSpy.transition.name = "1HatchMuta";
            }
        }
    }

    void updateZerg(StrategySpy& theSpy)
    {
        for (auto &p : Players::getPlayers()) {
            PlayerInfo &player = p.second;
            if (player.isEnemy() && player.getCurrentRace() == Races::Zerg) {
                if (!theSpy.build.confirmed || theSpy.build.changeable)
                    enemyZergBuilds(player, theSpy);
                if (!theSpy.opener.confirmed || theSpy.opener.changeable)
                    enemyZergOpeners(player, theSpy);
                if (!theSpy.transition.confirmed || theSpy.transition.changeable)
                    enemyZergTransitions(player, theSpy);
            }
        }
    }
}