#include "..\McRave.h"

using namespace BWAPI;
using namespace std;
using namespace UnitTypes;

namespace McRave::Spy::Protoss {

    namespace {

        void enemyProtossBuilds(PlayerInfo& player, StrategySpy& theSpy)
        {
            for (auto &u : player.getUnits()) {
                UnitInfo &unit = *u;

                // Zealot timing
                if (unit.getType() == Protoss_Zealot) {
                    if (unit.timeArrivesWhen().minutes > 0 && unit.timeArrivesWhen() < theSpy.rushArrivalTime)
                        theSpy.rushArrivalTime = unit.timeArrivesWhen();
                }

                // CannonRush
                if (unit.getType() == Protoss_Forge && Scouts::gotFullScout() && Terrain::getEnemyStartingPosition().isValid() && unit.getPosition().getDistance(Terrain::getEnemyStartingPosition()) < 200.0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Nexus) <= 1) {
                    theSpy.build.name = "CannonRush";
                    theSpy.proxy.possible = true;
                }
                if ((unit.getType() == Protoss_Forge || unit.getType() == Protoss_Photon_Cannon) && unit.isProxy()) {
                    theSpy.build.name = "CannonRush";
                    theSpy.proxy.possible = true;
                }
                if (unit.getType() == Protoss_Pylon && unit.isProxy())
                    theSpy.early.possible = true;

                // FFE
                if (Util::getTime() < Time(4, 00) && Terrain::getEnemyNatural() && (unit.getType() == Protoss_Photon_Cannon || unit.getType() == Protoss_Forge)) {
                    if (unit.getPosition().getDistance(Position(Terrain::getEnemyNatural()->getChokepoint()->Center())) < 320.0 || unit.getPosition().getDistance(Position(Terrain::getEnemyNatural()->getBase()->Center())) < 320.0) {
                        theSpy.build.name = "FFE";
                        theSpy.expand.possible = true;
                    }
                }
            }

            if (Players::getTotalCount(PlayerState::Enemy, Protoss_Forge) == 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Photon_Cannon) == 0) {

                // 1GateCore - Gas estimation
                if ((completesBy(1, Protoss_Assimilator, Time(2, 15)) && !theSpy.steal.possible)
                    || (Players::getTotalCount(PlayerState::Enemy, Protoss_Gateway) > 0 && completesBy(1, Protoss_Assimilator, Time(2, 50)) && !theSpy.steal.possible))
                    theSpy.build.name = "1GateCore";

                // 1GateCore - Core estimation
                if (completesBy(1, Protoss_Cybernetics_Core, Time(3, 25)))
                    theSpy.build.name = "1GateCore";

                // 1GateCore - Goon estimation
                if (completesBy(1, Protoss_Dragoon, Time(4, 10))
                    || completesBy(2, Protoss_Dragoon, Time(4, 30)))
                    theSpy.build.name = "1GateCore";

                // 1GateCore - Tech estimation
                if (completesBy(1, Protoss_Scout, Time(5, 15))
                    || completesBy(1, Protoss_Corsair, Time(5, 15))
                    || completesBy(1, Protoss_Stargate, Time(4, 10)))
                    theSpy.build.name = "1GateCore";

                // 1GateCore - Turtle estimation
                if (Players::getTotalCount(PlayerState::Enemy, Protoss_Forge) > 0
                    && Players::getTotalCount(PlayerState::Enemy, Protoss_Gateway) > 0
                    && Players::getTotalCount(PlayerState::Enemy, Protoss_Cybernetics_Core) > 0
                    && Util::getTime() < Time(4, 00))
                    theSpy.build.name = "1GateCore";

                // 1GateCore - fallback assumption
                if (Util::getTime() > Time(3, 45) && Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 2)
                    theSpy.build.name = "1GateCore";

                // 2Gate Proxy - No info estimation
                if (Scouts::gotFullScout() && Util::getTime() < Time(3, 30) && Players::getVisibleCount(PlayerState::Enemy, Protoss_Forge) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Nexus) <= 1) {
                    theSpy.build.name = "2Gate";
                    theSpy.proxy.possible = true;
                }

                // 2Gate - Zealot estimation
                if ((Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 5 && Util::getTime() < Time(4, 0))
                    || (Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 3 && Util::getTime() < Time(3, 30))
                    || (theSpy.proxy.possible == true && Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) > 0)
                    || arrivesBy(2, Protoss_Zealot, Time(3, 25))
                    || arrivesBy(3, Protoss_Zealot, Time(4, 00))
                    || arrivesBy(4, Protoss_Zealot, Time(4, 20))
                    || completesBy(2, Protoss_Gateway, Time(2, 55))
                    || completesBy(3, Protoss_Zealot, Time(3, 20))) {
                    theSpy.build.name = "2Gate";
                }
            }
        }

        void enemyProtossOpeners(PlayerInfo& player, StrategySpy& theSpy)
        {
            // 2Gate Openers
            if (theSpy.build.name == "2Gate" || theSpy.build.name == "CannonRush") {
                if (theSpy.proxy.possible || arrivesBy(1, Protoss_Zealot, Time(2, 50)) || arrivesBy(2, Protoss_Zealot, Time(3, 00)) || arrivesBy(4, Protoss_Zealot, Time(3, 25))) {
                    theSpy.opener.name = "Proxy";
                    theSpy.proxy.possible = true;
                }
                else if (arrivesBy(2, Protoss_Zealot, Time(3, 25)) || arrivesBy(4, Protoss_Zealot, Time(4, 00)) || arrivesBy(5, Protoss_Zealot, Time(4, 10)) || completesBy(2, Protoss_Gateway, Time(2, 15)))
                    theSpy.opener.name = "9/9";
                else if ((completesBy(3, Protoss_Zealot, Time(3, 10)) && arrivesBy(3, Protoss_Zealot, Time(4, 05))) || arrivesBy(4, Protoss_Zealot, Time(4, 20)))
                    theSpy.opener.name = "10/12";
                else if ((completesBy(3, Protoss_Zealot, Time(3, 35)) && arrivesBy(3, Protoss_Zealot, Time(4, 20))) || arrivesBy(2, Protoss_Dragoon, Time(5, 00)) || completesBy(1, Protoss_Cybernetics_Core, Time(3, 40)))
                    theSpy.opener.name = "10/17";
            }

            // FFE Openers - need timings for when Nexus/Forge/Gate complete
            if (theSpy.build.name == "FFE") {
                if (startedEarlier(Protoss_Nexus, Protoss_Forge))
                    theSpy.opener.name = "Nexus";
                else if (startedEarlier(Protoss_Gateway, Protoss_Forge))
                    theSpy.opener.name = "Gateway";
                else
                    theSpy.opener.name = "Forge";
            }

            // 1Gate Openers -  need timings for when Core completes
            if (theSpy.build.name == "1GateCore") {
                if (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) == 0)
                    theSpy.opener.name = "0Zealot";
                else if (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) == 1)
                    theSpy.opener.name = "1Zealot";
                else if (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) > 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 2)
                    theSpy.opener.name = "2Zealot";
            }
        }

        void enemyProtossTransitions(PlayerInfo& player, StrategySpy& theSpy)
        {
            // Rush detection
            if (theSpy.build.name == "2Gate") {
                if (theSpy.opener.name == "Proxy"
                    || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Assimilator) == 0 && Players::getCompleteCount(PlayerState::Enemy, Protoss_Zealot) >= 8 && Util::getTime() < Time(4, 00))
                    || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Assimilator) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) == 0 && Util::getTime() > Time(6, 00))
                    || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Assimilator) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Dragoon) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) == 0 && completesBy(3, Protoss_Gateway, Time(4, 30)))
                    || completesBy(3, Protoss_Gateway, Time(3, 30)))
                    theSpy.transition.name = "ZealotRush";
            }

            if (theSpy.workersNearUs >= 3 && Util::getTime() < Time(3, 00))
                theSpy.transition.name = "WorkerRush";

            // 2Gate
            if (theSpy.build.name == "2Gate" || theSpy.build.name == "1GateCore") {

                // DT
                if (!theSpy.expand.possible) {
                    if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Citadel_of_Adun) >= 1 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) > 0)
                        || Players::getVisibleCount(PlayerState::Enemy, Protoss_Templar_Archives) >= 1
                        || Players::getVisibleCount(PlayerState::Enemy, Protoss_Dark_Templar) >= 1
                        || (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) < 2 && !theSpy.expand.possible && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) >= 1 && Util::getTime() > Time(6, 45)))
                        theSpy.transition.name = "DT";
                }

                // Speedlot
                if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Zealot) >= 8 && theSpy.build.name == "1GateCore")
                    || theSpy.upgradeLevel[UpgradeTypes::Leg_Enhancements] > 0)
                    theSpy.transition.name = "Speedlot";

                // Corsair
                if (Players::getVisibleCount(PlayerState::Enemy, Protoss_Stargate) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) > 0
                    || completesBy(1, Protoss_Scout, Time(5, 15))
                    || completesBy(1, Protoss_Corsair, Time(5, 15)))
                    theSpy.transition.name = "Corsair";

                // Robo
                if (Players::getVisibleCount(PlayerState::Enemy, Protoss_Shuttle) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Reaver) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Observer) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Robotics_Facility) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Robotics_Support_Bay) > 0
                    || Players::getVisibleCount(PlayerState::Enemy, Protoss_Observatory) > 0)
                    theSpy.transition.name = "Robo";

                // 4Gate
                if (Players::PvP()) {
                    if ((Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 7 && Util::getTime() < Time(6, 30))
                        || (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 11 && Util::getTime() < Time(7, 15))
                        || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) >= 4 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) >= 1 && Util::getTime() < Time(5, 30)))
                        theSpy.transition.name = "4Gate";
                }
                if (Players::ZvP()) {

                    if (Players::getVisibleCount(PlayerState::Enemy, Protoss_Corsair) == 0 && Players::getVisibleCount(PlayerState::Enemy, Protoss_Stargate) == 0) {
                        if ((!theSpy.expand.possible && Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 4 && Util::getTime() < Time(6, 00))
                            || (theSpy.typeUpgrading.find(Protoss_Cybernetics_Core) != theSpy.typeUpgrading.end() && Util::getTime() < Time(4, 15))
                            || (Players::getPlayerInfo(Broodwar->enemy())->hasUpgrade(UpgradeTypes::Singularity_Charge) && Util::getTime() < Time(6, 00))
                            || (arrivesBy(2, Protoss_Dragoon, Time(5, 30)))
                            || (arrivesBy(3, Protoss_Dragoon, Time(5, 45)))
                            || (arrivesBy(5, Protoss_Dragoon, Time(6, 05)))
                            || (arrivesBy(9, Protoss_Dragoon, Time(6, 40)))
                            || (arrivesBy(12, Protoss_Dragoon, Time(7, 25)))
                            || (completesBy(3, Protoss_Gateway, Time(4, 00)) && Players::getVisibleCount(PlayerState::Enemy, Protoss_Assimilator) > 0)
                            || (Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) >= 2 && arrivesBy(2, Protoss_Dragoon, Time(5, 00)))
                            || (completesBy(4, Protoss_Gateway, Time(5, 30)) && Players::getVisibleCount(PlayerState::Enemy, Protoss_Cybernetics_Core) >= 1))
                            theSpy.transition.name = "4Gate";
                    }
                }

                // 5ZealotExpand
                if (Players::PvP() && theSpy.expand.possible && (Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) >= 2 || Players::getTotalCount(PlayerState::Enemy, Protoss_Zealot) >= 5) && Util::getTime() < Time(4, 45))
                    theSpy.transition.name = "5ZealotExpand";
            }

            // FFE transitions
            if (Players::ZvP() && theSpy.build.name == "FFE") {
                if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Gateway) >= 4 && Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 1 && Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) == 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Stargate) == 0)
                    || (Players::getTotalCount(PlayerState::Enemy, Protoss_Dragoon) >= 4 && Players::getTotalCount(PlayerState::Enemy, Protoss_Corsair) == 0 && Players::getTotalCount(PlayerState::Enemy, Protoss_Stargate) == 0))
                    theSpy.transition.name = "5GateGoon";
                else if (completesBy(1, Protoss_Stargate, Time(5, 45)) && completesBy(1, Protoss_Citadel_of_Adun, Time(5, 45)) && theSpy.typeUpgrading.find(Protoss_Forge) != theSpy.typeUpgrading.end())
                    theSpy.transition.name = "NeoBisu";
                else if (completesBy(1, Protoss_Stargate, Time(5, 45)) && completesBy(1, Protoss_Robotics_Facility, Time(6, 30)))
                    theSpy.transition.name = "CorsairReaver";
                else if (theSpy.upgradeLevel[UpgradeTypes::Singularity_Charge] > 0 && completesBy(1, Protoss_Corsair, Time(7, 00)))
                    theSpy.transition.name = "CorsairGoon";
                else if (completesBy(1, Protoss_Templar_Archives, Time(7, 30)))
                    theSpy.transition.name = "ZealotArchon";
                else if ((Players::getVisibleCount(PlayerState::Enemy, Protoss_Stargate) <= 0 && theSpy.typeUpgrading.find(Protoss_Forge) != theSpy.typeUpgrading.end() && theSpy.typeUpgrading.find(Protoss_Cybernetics_Core) == theSpy.typeUpgrading.end() && Util::getTime() < Time(5, 30))
                    || (completesBy(1, Protoss_Citadel_of_Adun, Time(5, 30)) && completesBy(2, Protoss_Gateway, Time(5, 45)))
                    || (Util::getTime() < Time(7,30) && theSpy.upgradeLevel[UpgradeTypes::Leg_Enhancements] > 0 && theSpy.upgradeLevel[UpgradeTypes::Protoss_Ground_Weapons] > 0))
                    theSpy.transition.name = "Speedlot";
                else if (completesBy(2, Protoss_Stargate, Time(7, 00)))
                    theSpy.transition.name = "DoubleStargate";
                else if (completesBy(1, Protoss_Fleet_Beacon, Time(9, 30)) || completesBy(1, Protoss_Carrier, Time(12, 00)))
                    theSpy.transition.name = "Carriers";
            }
        }
    }

    void updateProtoss(StrategySpy& theSpy)
    {
        for (auto &p : Players::getPlayers()) {
            PlayerInfo &player = p.second;
            if (player.isEnemy() && player.getCurrentRace() == Races::Protoss) {
                if (!theSpy.build.confirmed || theSpy.build.changeable)
                    enemyProtossBuilds(player, theSpy);
                if (!theSpy.opener.confirmed || theSpy.opener.changeable)
                    enemyProtossOpeners(player, theSpy);
                if (!theSpy.transition.confirmed || theSpy.transition.changeable)
                    enemyProtossTransitions(player, theSpy);
            }
        }
    }
}