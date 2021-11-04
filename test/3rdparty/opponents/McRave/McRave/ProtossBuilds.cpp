//#include "McRave.h"
//#include "BuildOrder.h"
//
//using namespace BWAPI;
//using namespace std;
//using namespace UnitTypes;
//using namespace McRave::BuildOrder::All;
//#define s Units::getSupply()
//
//namespace McRave::BuildOrder::Protoss {
//
//    namespace {
//
//        string enemyBuild() { return Strategy::getEnemyBuild(); }
//
//        bool goonRange() {
//            return Broodwar->self()->isUpgrading(UpgradeTypes::Singularity_Charge) || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Singularity_Charge);
//        }
//
//        bool addGates() {
//            return goonRange() && Broodwar->self()->minerals() >= 100;
//        }
//
//        void defaultPvT() {
//            firstUpgrade =		UpgradeTypes::Singularity_Charge;
//            firstTech =			TechTypes::None;
//            scout =				vis(Protoss_Cybernetics_Core) > 0;
//            gasLimit =			INT_MAX;
//            zealotLimit =		0;
//            dragoonLimit =		INT_MAX;
//        }
//
//        void defaultPvZ() {
//            firstUpgrade =		UpgradeTypes::Protoss_Ground_Weapons;
//            firstTech =			TechTypes::None;
//            scout =				vis(Protoss_Pylon) > 0;
//            gasLimit =			INT_MAX;
//            zealotLimit =		INT_MAX;
//            dragoonLimit =		0;
//        }
//
//        void defaultPvP() {
//            firstUpgrade =		UpgradeTypes::Singularity_Charge;
//            firstTech =			TechTypes::None;
//            scout =				vis(Protoss_Gateway) > 0;
//            gasLimit =			INT_MAX;
//            zealotLimit =		1;
//            dragoonLimit =		INT_MAX;
//        }
//    }
//
//    void Reaction2GateDefensive() {
//        gasLimit =			(com(Protoss_Cybernetics_Core) && s >= 50) ? INT_MAX : 0;
//        getOpening =		vis(Protoss_Dark_Templar) == 0;
//        playPassive	=		Players::vZ() ? vis(Protoss_Corsair) == 0 : vis(Protoss_Dark_Templar) == 0;
//        firstUpgrade =		UpgradeTypes::None;
//        firstTech =			TechTypes::None;
//        fastExpand =		false;
//
//        zealotLimit =		INT_MAX;
//        dragoonLimit =		(vis(Protoss_Templar_Archives) > 0 || Players::vT()) ? INT_MAX : 0;
//
//        if (Players::vZ()) {
//            if (com(Protoss_Cybernetics_Core) > 0 && techList.find(Protoss_Corsair) == techList.end() && s >= 80)
//                techUnit = Protoss_Corsair;
//        }
//        else {
//            if (com(Protoss_Cybernetics_Core) > 0 && techList.find(Protoss_Dark_Templar) == techList.end() && s >= 80)
//                techUnit = Protoss_Dark_Templar;
//        }
//
//        itemQueue[Protoss_Nexus] =					Item(1);
//        itemQueue[Protoss_Pylon] =					Item((s >= 14) + (s >= 30), (s >= 16) + (s >= 30));
//        itemQueue[Protoss_Gateway] =				Item((s >= 20) + (s >= 24) + (s >= 66));
//        itemQueue[Protoss_Assimilator] =			Item(s >= 40);
//        itemQueue[Protoss_Shield_Battery] =			Item(vis(Protoss_Zealot) >= 2 && vis(Protoss_Pylon) >= 2);
//        itemQueue[Protoss_Cybernetics_Core] =		Item(s >= 58);
//    }
//
//    void PFFE()
//    {
//        fastExpand = true;
//        wallNat = true;
//        defaultPvZ();
//
//        auto min100 = Broodwar->self()->minerals() >= 100;
//        auto cannonCount = int(com(Protoss_Forge) > 0) + (Units::getEnemyCount(Zerg_Zergling) >= 6) + (Units::getEnemyCount(Zerg_Zergling) >= 12) + (Units::getEnemyCount(Zerg_Zergling) >= 24);
//
//        // TODO: If scout died, go to 2 cannons, if next scout dies, go 3 cannons		
//        if (enemyBuild() == "Z2HatchHydra")
//            cannonCount = 5;
//        else if (enemyBuild() == "Z3HatchHydra")
//            cannonCount = 4;
//        else if (enemyBuild() == "Z2HatchMuta")
//            cannonCount = 7;
//        else if (enemyBuild() == "Z3HatchMuta")
//            cannonCount = 8;
//
//        // Reactions
//        if (!lockedTransition) {
//            if ((enemyBuild() == "Unknown" && !Terrain::getEnemyStartingPosition().isValid()) || enemyBuild() == "Z9Pool")
//                false;// currentTransition = "Defensive";
//            else if (enemyBuild() == "Z5Pool")
//                currentTransition =	"Defensive";
//            else if (enemyBuild() == "Z2HatchHydra" || enemyBuild() == "Z3HatchHydra")
//                currentTransition =	"StormRush";
//            else if (enemyBuild() == "Z2HatchMuta" || enemyBuild() == "Z3HatchMuta")
//                currentTransition =	"DoubleStargate";
//        }
//
//        // Openers
//        if (currentOpener == "Forge") {
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 28));
//            itemQueue[Protoss_Pylon] =				Item((s >= 14) + (s >= 30), (s >= 16) + (s >= 30));
//            itemQueue[Protoss_Assimilator] =		Item((vis(Protoss_Gateway) >= 1) + (vis(Protoss_Stargate) >= 1));
//            itemQueue[Protoss_Gateway] =			Item((s >= 32) + (s >= 46));
//            itemQueue[Protoss_Forge] =				Item(s >= 20);
//        }
//        else if (currentOpener == "Nexus") {
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 24));
//            itemQueue[Protoss_Pylon] =				Item((s >= 14) + (s >= 30), (s >= 16) + (s >= 30));
//            itemQueue[Protoss_Assimilator] =		Item((vis(Protoss_Gateway) >= 1) + (vis(Protoss_Stargate) >= 1));
//            itemQueue[Protoss_Gateway] =			Item(vis(Protoss_Forge) > 0);
//            itemQueue[Protoss_Forge] =				Item(vis(Protoss_Nexus) >= 2);
//        }
//        else if (currentOpener == "Gate") {
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 42));
//            itemQueue[Protoss_Pylon] =				Item((s >= 14) + (s >= 24), (s >= 16) + (s >= 24));
//            itemQueue[Protoss_Assimilator] =		Item((s >= 50) + (s >= 64));
//            itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 46));
//            itemQueue[Protoss_Forge] =				Item(s >= 60);
//        }
//
//        // If we want Cannons but have no Forge
//        if (cannonCount > 0 && com(Protoss_Forge) == 0) {
//            cannonCount = 0;
//            itemQueue[Protoss_Forge] = Item(1);
//        }
//
//        // Transitions
//        if (currentTransition == "StormRush") {
//            firstUpgrade =		UpgradeTypes::None;
//            firstTech =			TechTypes::Psionic_Storm;
//            firstUnit =         Protoss_High_Templar;
//            lockedTransition =  com(Protoss_Cybernetics_Core) > 0;
//
//            itemQueue[Protoss_Photon_Cannon] =		Item(cannonCount);
//            itemQueue[Protoss_Assimilator] =		Item((s >= 38) + (s >= 60));
//            itemQueue[Protoss_Cybernetics_Core] =	Item((s >= 42));
//            itemQueue[Protoss_Stargate] =           Item(0);
//        }
//        else if (currentTransition == "DoubleStargate") {
//            firstUpgrade =		UpgradeTypes::Protoss_Air_Weapons;
//            firstTech =			TechTypes::None;
//            firstUnit =         Protoss_Corsair;
//            lockedTransition =  vis(Protoss_Stargate) >= 2;
//
//            itemQueue[Protoss_Photon_Cannon] =		Item(cannonCount);
//            itemQueue[Protoss_Assimilator] =		Item((s >= 38) + (s >= 60));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 40);
//            itemQueue[Protoss_Citadel_of_Adun] =	Item(0);
//            itemQueue[Protoss_Templar_Archives] =	Item(0);
//            itemQueue[Protoss_Stargate] =			Item((vis(Protoss_Corsair) > 0) + (vis(Protoss_Cybernetics_Core) > 0));
//        }
//        else {
//            getOpening =		s < 100;
//            currentTransition =	"NeoBisu";
//            firstUpgrade =		UpgradeTypes::Protoss_Air_Weapons;
//            firstUnit =         Protoss_Corsair;
//
//            itemQueue[Protoss_Photon_Cannon] =		Item(cannonCount);
//            itemQueue[Protoss_Cybernetics_Core] =	Item(vis(Protoss_Zealot) >= 1);
//            itemQueue[Protoss_Citadel_of_Adun] =	Item(vis(Protoss_Assimilator) >= 2);
//            itemQueue[Protoss_Stargate] =			Item(com(Protoss_Cybernetics_Core) >= 1);
//            itemQueue[Protoss_Templar_Archives] =	Item(Broodwar->self()->isUpgrading(UpgradeTypes::Leg_Enhancements) || Broodwar->self()->getUpgradeLevel(UpgradeTypes::Leg_Enhancements));
//        }
//    }
//
//    void P2Gate()
//    {
//        zealotLimit = 3;
//        proxy = currentOpener == "Proxy";
//        wallNat = currentOpener == "Natural";
//        scout = Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) >= 1 : vis(Protoss_Gateway) >= 2;
//
//        // Openers
//        if (currentOpener == "Proxy") {
//            itemQueue[Protoss_Pylon] =					Item((s >= 12), (s >= 16));
//            itemQueue[Protoss_Gateway] =				Item((vis(Protoss_Pylon) > 0) + (vis(Protoss_Gateway) > 0), 2 * (s >= 18));
//        }
//        else if (currentOpener == "Natural") {
//            if (Players::vT()) {
//                itemQueue[Protoss_Pylon] =				Item((s >= 14), (s >= 16));
//                itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 30));
//            }
//            else if (Broodwar->getStartLocations().size() >= 3) {
//                itemQueue[Protoss_Pylon] =				Item((s >= 14) + (s >= 30), (s >= 16) + (s >= 30));
//                itemQueue[Protoss_Gateway] =			Item((vis(Protoss_Pylon) > 0) + (s >= 20), (s >= 18) + (s >= 20));
//            }
//            else {
//                itemQueue[Protoss_Pylon] =				Item((s >= 14) + (s >= 30), (s >= 16) + (s >= 30));
//                itemQueue[Protoss_Gateway] =			Item((vis(Protoss_Pylon) > 0) + (vis(Protoss_Gateway) > 0), 2 * (s >= 18));
//            }
//        }
//        else if (currentOpener == "Main") {
//            if (Broodwar->getStartLocations().size() >= 3) {
//                itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 30));
//                itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 24));
//            }
//            else {
//                itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 30));
//                itemQueue[Protoss_Gateway] =			Item((vis(Protoss_Pylon) > 0) + (s >= 20));
//            }
//        }
//
//        // Transitions
//        if (!lockedTransition) {
//            if (!Players::vT() && Strategy::enemyRush())
//                currentTransition = "Panic";
//            else if (Strategy::enemyPressure() && currentOpener == "Natural")
//                currentTransition = "Defensive";
//            else if (Players::vT() && Strategy::enemyFastExpand())
//                currentTransition = "DT";
//            else if (Units::getEnemyCount(UnitTypes::Zerg_Sunken_Colony) >= 2)
//                currentTransition = "Expand";
//        }
//
//        // Builds
//        if (Players::vT()) {
//            // https://liquipedia.net/starcraft/10/15_Gates_(vs._Terran)
//            defaultPvT();
//            playPassive = false;
//
//            if (currentTransition == "DT") {
//                getOpening =		s < 70;
//                firstUnit =			vis(Protoss_Dragoon) >= 3 ? Protoss_Dark_Templar : UnitTypes::None;
//
//                itemQueue[Protoss_Nexus] =				Item(1);
//                itemQueue[Protoss_Assimilator] =		Item(s >= 22);
//                itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//            }
//            else if (currentTransition == "Reaver") {
//                getOpening =		s < 70;
//                firstUnit =			com(Protoss_Dragoon) >= 3 ? Protoss_Reaver : UnitTypes::None;
//
//                itemQueue[Protoss_Nexus] =				Item(1);
//                itemQueue[Protoss_Assimilator] =		Item(s >= 22);
//                itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//            }
//            else if (currentTransition == "Expand") {
//                getOpening =		s < 100;
//
//                itemQueue[Protoss_Nexus] =				Item(1 + (s >= 50));
//                itemQueue[Protoss_Assimilator] =		Item(s >= 22);
//                itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//            }
//            else if (currentTransition == "DoubleExpand") {
//                getOpening =		s < 120;
//
//                itemQueue[Protoss_Nexus] =				Item(1 + (s >= 50) + (s >= 50));
//                itemQueue[Protoss_Assimilator] =		Item(s >= 22);
//                itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//            }
//            else if (currentTransition == "Defensive" || currentTransition == "Panic")
//                Reaction2GateDefensive();
//        }
//        else
//        {
//            // Versus Zerg
//            if (Players::vZ()) {
//                defaultPvZ();
//
//                if (currentTransition == "Expand") {
//                    getOpening =		s < 100;
//
//                    itemQueue[Protoss_Assimilator] =		Item(s >= 76);
//                    itemQueue[Protoss_Nexus] =				Item(1 + (s >= 42));
//                    itemQueue[Protoss_Forge] =				Item(s >= 62);
//                    itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 70);
//                    itemQueue[Protoss_Photon_Cannon] =		Item(2 * (com(Protoss_Forge) > 0));
//                }
//                else if (currentTransition == "Defensive") {
//                    getOpening =		s < 80;
//
//                    itemQueue[Protoss_Forge] =				Item(1);
//                    itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 70);
//                    itemQueue[Protoss_Photon_Cannon] =		Item(2 * (com(Protoss_Forge) > 0));
//                }
//                else if (currentTransition == "Panic") {
//                    getOpening =		s < 80;
//
//                    itemQueue[Protoss_Nexus] =				Item(1);
//                    itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 24) + (s >= 62) + (s >= 70));
//                    itemQueue[Protoss_Assimilator] =		Item(s >= 64);
//                    itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 66);
//                    gasLimit = 1;
//                }
//                else if (currentTransition == "4Gate") {
//                    // https://liquipedia.net/starcraft/4_Gate_Goon_(vs._Protoss)
//                    getOpening = s < 120;
//
//                    itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 24) + (s >= 62) + (s >= 70));
//                    itemQueue[Protoss_Assimilator] =		Item(s >= 44);
//                    itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 50);
//                }
//            }
//
//            // Versus Protoss
//            else if (Players::vP()) {
//
//                defaultPvP();
//                zealotLimit =		5;
//
//                if (currentTransition == "Defensive") {
//                    playPassive =		com(Protoss_Gateway) < 5;
//                    zealotLimit	=		INT_MAX;
//                    firstUnit =			Protoss_Dark_Templar;
//
//                    itemQueue[Protoss_Assimilator] =        Item(s >= 64);
//                    itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 66);
//                    itemQueue[Protoss_Forge] =				Item(1);
//                    itemQueue[Protoss_Photon_Cannon] =		Item(6 * (com(Protoss_Forge) > 0));
//                    itemQueue[Protoss_Nexus] =				Item(1 + (s >= 56));
//                }
//                else if (currentTransition == "DT") {
//                    // https://liquipedia.net/starcraft/2_Gateway_Dark_Templar_(vs._Protoss)
//                    hideTech =			currentOpener == "Main" && com(Protoss_Dark_Templar) < 1;
//                    getOpening =		s < 70;
//                    firstUnit =			vis(Protoss_Zealot) >= 3 ? Protoss_Dark_Templar : UnitTypes::None;
//
//                    itemQueue[Protoss_Nexus] =				Item(1);
//                    itemQueue[Protoss_Assimilator] =		Item(s >= 44);
//                    itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 56);
//                }
//                else if (currentTransition == "Standard") {
//                    // https://liquipedia.net/starcraft/2_Gate_(vs._Protoss)#10.2F12_Gateway_Expand
//
//                    itemQueue[Protoss_Assimilator] =		Item(s >= 58);
//                    itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 60);
//                    itemQueue[Protoss_Forge] =				Item(s >= 70);
//                    itemQueue[Protoss_Nexus] =				Item(1 + (s >= 56));
//                    itemQueue[Protoss_Photon_Cannon] =		Item(2 * (com(Protoss_Forge) > 0));
//                }
//                else if (currentTransition == "Reaver") {
//                    // https://liquipedia.net/starcraft/2_Gate_Reaver_(vs._Protoss)
//                    getOpening =		s < 70;
//                    firstUnit =			com(Protoss_Robotics_Facility) >= 1 ? (Strategy::needDetection() ? Protoss_Observer : Protoss_Reaver) : UnitTypes::None;
//
//                    itemQueue[Protoss_Nexus] =				Item(1);
//                    itemQueue[Protoss_Assimilator] =		Item(s >= 44);
//                    itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 56);
//                    itemQueue[Protoss_Robotics_Facility] =	Item(com(Protoss_Dragoon) >= 2);
//                }
//                else if (currentTransition == "Panic")
//                    Reaction2GateDefensive();
//            }
//        }
//    }
//
//    void P1GateCore()
//    {
//        // https://liquipedia.net/starcraft/1_Gate_Core_(vs._Protoss)
//        // https://liquipedia.net/starcraft/1_Gate_Core_(vs._Terran)
//        firstUpgrade =		UpgradeTypes::Singularity_Charge;
//        firstTech =			TechTypes::None;
//        scout =				Broodwar->getStartLocations().size() >= 3 ? vis(Protoss_Gateway) > 0 : vis(Protoss_Pylon) > 0;
//        gasLimit =			INT_MAX;
//
//        bool addGas = Broodwar->getStartLocations().size() >= 3 ? (s >= 22) : (s >= 24);
//
//        if (!lockedTransition) {
//            if (Strategy::enemyRush())
//                currentTransition = "Defensive";
//            else if (Players::vT() && Strategy::enemyFastExpand())
//                currentTransition = "DT";
//        }
//
//        // Openers
//        if (currentOpener == "0Zealot") {
//            zealotLimit = 0;
//
//            itemQueue[Protoss_Nexus] =				Item(1);
//            itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 30));
//            itemQueue[Protoss_Gateway] =			Item((s >= 20) + (2 * addGates()));
//            itemQueue[Protoss_Assimilator] =		Item((addGas || Strategy::enemyScouted()));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//        }
//        else if (currentOpener == "1Zealot") {
//            zealotLimit = 1;
//
//            itemQueue[Protoss_Nexus] =				Item(1);
//            itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 30));
//            itemQueue[Protoss_Gateway] =			Item((s >= 20) + (2 * addGates()));
//            itemQueue[Protoss_Assimilator] =		Item((addGas || Strategy::enemyScouted()));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 34);
//        }
//        else if (currentOpener == "2Zealot") {
//            zealotLimit = 2;
//
//            itemQueue[Protoss_Nexus] =				Item(1);
//            itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 30));
//            itemQueue[Protoss_Gateway] =			Item((s >= 20) + (2 * addGates()));
//            itemQueue[Protoss_Assimilator] =		Item((addGas || Strategy::enemyScouted()));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 40);
//        }
//
//        // Transitions
//        if (currentTransition == "3GateRobo") {
//            getOpening = s < 80;
//            dragoonLimit = INT_MAX;
//
//            itemQueue[Protoss_Robotics_Facility] =	Item(s >= 52);
//            itemQueue[Protoss_Gateway] =			Item((s >= 20) + (2 * addGates()));
//
//            // Decide whether to Reaver first or Obs first
//            if (com(Protoss_Robotics_Facility) > 0) {
//                if (vis(Protoss_Observer) == 0 && Players::vP() && (Units::getEnemyCount(UnitTypes::Protoss_Dragoon) <= 2 || enemyBuild() == "P1GateDT")) {
//                    if (techList.find(Protoss_Observer) == techList.end())
//                        techUnit = Protoss_Observer;
//                }
//                else if (techList.find(Protoss_Reaver) == techList.end())
//                    techUnit = Protoss_Reaver;
//            }
//        }
//        else if (currentTransition == "Reaver") {
//            // http://liquipedia.net/starcraft/1_Gate_Reaver
//            getOpening =		s < 60;
//            dragoonLimit =		INT_MAX;
//
//            if (Players::vP()) {
//                playPassive =		!Strategy::enemyFastExpand() && com(Protoss_Reaver) < 2;
//                getOpening =		(Players::vP() && Strategy::enemyPressure()) ? vis(Protoss_Reaver) < 3 : s < 70;
//                zealotLimit =		com(Protoss_Robotics_Facility) >= 1 ? 6 : zealotLimit;
//
//                itemQueue[Protoss_Gateway] =				Item((s >= 20) + (s >= 60) + (s >= 62));
//                itemQueue[Protoss_Assimilator] =			Item((addGas || Strategy::enemyScouted()));
//                itemQueue[Protoss_Robotics_Facility] =		Item(s >= 52);
//            }
//            else {
//                hideTech =			com(Protoss_Reaver) == 0;
//                getOpening =		(com(Protoss_Reaver) < 1);
//
//                itemQueue[Protoss_Nexus] =					Item(1 + (s >= 74));
//                itemQueue[Protoss_Gateway] =				Item((s >= 20) + (s >= 60) + (s >= 62));
//                itemQueue[Protoss_Assimilator] =			Item((addGas || Strategy::enemyScouted()));
//                itemQueue[Protoss_Robotics_Facility] =		Item(s >= 52);
//            }
//
//            // Decide whether to Reaver first or Obs first
//            if (com(Protoss_Robotics_Facility) > 0) {
//                if (vis(Protoss_Observer) == 0 && Players::vP() && (Units::getEnemyCount(UnitTypes::Protoss_Dragoon) <= 2 || enemyBuild() == "P1GateDT")) {
//                    if (techList.find(Protoss_Observer) == techList.end())
//                        techUnit = Protoss_Observer;
//                }
//                else if (techList.find(Protoss_Reaver) == techList.end())
//                    techUnit = Protoss_Reaver;
//            }
//        }
//        else if (currentTransition == "Corsair") {
//            getOpening =		s < 60;
//            firstUpgrade =		UpgradeTypes::Protoss_Air_Weapons;
//            firstTech =			TechTypes::None;
//            dragoonLimit =		0;
//            zealotLimit	=		INT_MAX;
//            playPassive =		com(Protoss_Stargate) == 0;
//
//            if (techList.find(Protoss_Corsair) == techList.end())
//                techUnit = Protoss_Corsair;
//
//            if (Strategy::enemyRush()) {
//                itemQueue[Protoss_Gateway] =			Item((s >= 18) * 2);
//                itemQueue[Protoss_Forge] =				Item(com(Protoss_Stargate) >= 1);
//                itemQueue[Protoss_Assimilator] =		Item(s >= 54);
//                itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 50);
//                itemQueue[Protoss_Stargate] =			Item(com(Protoss_Cybernetics_Core) > 0);
//            }
//            else {
//                itemQueue[Protoss_Gateway] =			Item((s >= 18) + vis(Protoss_Stargate) > 0);
//                itemQueue[Protoss_Forge] =				Item(vis(Protoss_Gateway) >= 2);
//                itemQueue[Protoss_Assimilator] =		Item(s >= 40);
//                itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 36);
//                itemQueue[Protoss_Stargate] =			Item(com(Protoss_Cybernetics_Core) > 0);
//            }
//        }
//        else if (currentTransition == "4Gate") {
//            // https://liquipedia.net/starcraft/4_Gate_Goon_(vs._Protoss)
//            if (Players::vT()) {
//                getOpening = s < 80;
//
//                itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 30) + (2 * (s >= 62)));
//                itemQueue[Protoss_Assimilator] =		Item(s >= 22);
//                itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//            }
//            else {
//                getOpening = s < 140;
//
//                itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 54) + (2 * (s >= 62)));
//                itemQueue[Protoss_Assimilator] =		Item(s >= 32);
//                itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 34);
//            }
//        }
//        else if (currentTransition == "DT") {
//            if (Players::vT()) {
//                // https://liquipedia.net/starcraft/DT_Fast_Expand_(vs._Terran)
//                firstUpgrade =		UpgradeTypes::Khaydarin_Core;
//                getOpening =		s < 60;
//                hideTech =			com(Protoss_Dark_Templar) < 1;
//                dragoonLimit =		INT_MAX;
//
//                itemQueue[Protoss_Citadel_of_Adun] =	Item(s >= 36);
//                itemQueue[Protoss_Templar_Archives] =	Item(s >= 48);
//            }
//            else if (Players::vZ()) {
//                // Experimental build from Best
//                firstUpgrade =		UpgradeTypes::None;
//                firstTech =			TechTypes::Psionic_Storm;
//                getOpening =		s < 70;
//                hideTech =			com(Protoss_Dark_Templar) < 1;
//                dragoonLimit =		1;
//                zealotLimit =		vis(Protoss_Dark_Templar) >= 2 ? INT_MAX : 2;
//
//                if (techList.find(Protoss_Dark_Templar) == techList.end())
//                    techUnit =			Protoss_Dark_Templar;
//                if (com(Protoss_Dark_Templar) >= 2 && techList.find(Protoss_High_Templar) == techList.end())
//                    techUnit =			Protoss_High_Templar;
//
//                itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 42));
//                itemQueue[Protoss_Cybernetics_Core] =	Item(com(Protoss_Gateway) > 0);
//                itemQueue[Protoss_Citadel_of_Adun] =	Item(s >= 34);
//                itemQueue[Protoss_Templar_Archives] =	Item(vis(Protoss_Gateway) >= 2);
//            }
//            //else if (Players::vP())
//                // https://liquipedia.net/starcraft/2_Gate_DT_(vs._Protoss) -- is actually 1 Gate
//        }
//        else if (currentTransition == "Defensive")
//            Reaction2GateDefensive();
//    }
//
//    void PNexusGate()
//    {
//        // 12 Nexus - "http://liquipedia.net/starcraft/12_Nexus"
//        fastExpand =		true;
//        playPassive =		Units::getEnemyCount(Terran_Siege_Tank_Tank_Mode) == 0 && Units::getEnemyCount(Terran_Siege_Tank_Siege_Mode) == 0 && Strategy::enemyPressure() ? vis(Protoss_Dragoon) < 12 : !firstReady();
//        firstUpgrade =		vis(Protoss_Dragoon) >= 2 ? UpgradeTypes::Singularity_Charge : UpgradeTypes::None;
//        firstTech =			TechTypes::None;
//        scout =				vis(Protoss_Cybernetics_Core) >= 1;
//        wallNat =			vis(Protoss_Nexus) >= 2 ? true : false;
//        cutWorkers =		Production::hasIdleProduction() && com(Protoss_Cybernetics_Core) > 0;
//
//        gasLimit =			goonRange() && com(Protoss_Nexus) < 2 ? 2 : INT_MAX;
//        zealotLimit =		0;
//        dragoonLimit =		INT_MAX;
//
//        // Reactions
//        if (Terrain::isIslandMap())
//            currentTransition =	"Island"; // TODO: Island stuff		
//        else if (Strategy::enemyFastExpand() || enemyBuild() == "TSiegeExpand")
//            currentTransition =	"DoubleExpand";
//        else if (!Strategy::enemyFastExpand() && currentTransition == "DoubleExpand")
//            currentTransition = "Standard";
//
//        // Openers
//        if (currentOpener == "Zealot") {
//            zealotLimit = 1;
//
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 24));
//            itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 48));
//            itemQueue[Protoss_Assimilator] =		Item((s >= 30) + (s >= 60));
//            itemQueue[Protoss_Gateway] =			Item((s >= 28) + (s >= 34) + (s >= 80));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(vis(Protoss_Gateway) >= 2);
//        }
//        else if (currentOpener == "Dragoon") {
//            zealotLimit = 0;
//
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 24));
//            itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 48));
//            itemQueue[Protoss_Assimilator] =		Item((s >= 28) + (s >= 60));
//            itemQueue[Protoss_Gateway] =			Item((s >= 26) + (s >= 32) + (s >= 80));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 30);
//        }
//
//        // Transitions
//        if (currentTransition == "Island") {
//            firstUpgrade =		UpgradeTypes::Gravitic_Drive;
//            getOpening =		s < 50;
//            scout =				0;
//
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 24));
//            itemQueue[Protoss_Pylon] =				Item((s >= 14) + (s >= 48), (s >= 16) + (s >= 48));
//            itemQueue[Protoss_Gateway] =			Item(vis(Protoss_Nexus) > 1);
//            itemQueue[Protoss_Assimilator] =		Item(s >= 26);
//        }
//        else if (currentTransition == "DoubleExpand") {
//            getOpening =		s < 140;
//
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 24) + (com(Protoss_Cybernetics_Core) > 0));
//            itemQueue[Protoss_Assimilator] =		Item(s >= 28);
//        }
//        else if (currentTransition == "Standard") {
//            getOpening =		s < 90;
//        }
//        else if (currentTransition == "ReaverCarrier") {
//            getOpening =		s < 120;
//
//            if (techList.find(Protoss_Reaver) != techList.end())
//                techUnit = Protoss_Reaver;
//            if (com(Protoss_Reaver) && techList.find(Protoss_Carrier) != techList.end())
//                techUnit = Protoss_Carrier;
//
//            itemQueue[Protoss_Gateway] =			Item((vis(Protoss_Pylon) > 1) + (vis(Protoss_Nexus) > 1) + (s >= 70) + (s >= 80));
//        }
//    }
//
//    void PGateNexus()
//    {
//        // 1 Gate - "http://liquipedia.net/starcraft/21_Nexus"
//        // 2 Gate - "https://liquipedia.net/starcraft/2_Gate_Range_Expand"      
//
//        fastExpand =		true;
//        playPassive =		Strategy::enemyPressure() ? vis(Protoss_Observer) == 0 : !firstReady();
//        firstUpgrade =		UpgradeTypes::Singularity_Charge;
//        firstTech =			TechTypes::None;
//        scout =				Broodwar->getStartLocations().size() == 4 ? vis(Protoss_Pylon) > 0 : vis(Protoss_Pylon) > 0;
//        wallNat =			com(Protoss_Nexus) >= 2 ? true : false;
//
//        // Pull 1 probe when researching goon range, add 1 after we have a Nexus, then add 3 when 2 gas
//        gasLimit =			goonRange() && com(Protoss_Nexus) < 2 ? 2 : INT_MAX;
//        zealotLimit =		0;
//        dragoonLimit =		vis(Protoss_Nexus) >= 2 ? INT_MAX : 1;
//
//        // Reactions
//        if (!lockedTransition) {
//
//            // Change Transition
//            if (Strategy::enemyFastExpand() || enemyBuild() == "TSiegeExpand")
//                currentTransition =	"DoubleExpand";
//            else if ((!Strategy::enemyFastExpand() && Terrain::foundEnemy() && currentTransition == "DoubleExpand") || Strategy::enemyPressure())
//                currentTransition = "Standard";
//
//            // Change Opener
//            if (Units::getEnemyCount(Terran_Factory) >= 2)
//                currentOpener = "2Gate";
//
//            // Change Build
//            if (s < 42 && Strategy::enemyRush()) {
//                currentBuild = "2Gate";
//                currentOpener = "Main";
//                currentTransition = "DT";
//            }
//        }
//
//        // Openers
//        if (currentOpener == "1Gate") {
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 42));
//            itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 30));
//            itemQueue[Protoss_Gateway] =			Item((s >= 20) + (vis(Protoss_Nexus) >= 2) + (s >= 76));
//        }
//        else if (currentOpener == "2Gate") {
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 40));
//            itemQueue[Protoss_Pylon] =				Item((s >= 16) + (s >= 30));
//            itemQueue[Protoss_Gateway] =			Item((s >= 20) + (s >= 36) + (s >= 76));
//        }
//
//        // Transitions
//        if (currentTransition == "DoubleExpand") {
//            getOpening =		s < 140;
//            playPassive =		com(Protoss_Nexus) < 3;
//            lockedTransition =  vis(Protoss_Nexus) >= 3;
//            gasLimit =          s >= 80 ? INT_MAX : 3;
//
//            itemQueue[Protoss_Nexus] =				Item(1 + (s >= 42) + (s >= 70));
//            itemQueue[Protoss_Assimilator] =		Item((s >= 24) + (s >= 80));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//        }
//        else if (currentTransition == "Standard") {
//            getOpening =		s < 80;
//            firstUnit =         com(Protoss_Nexus) >= 2 ? Protoss_Observer : UnitTypes::None;
//            lockedTransition =  com(Protoss_Nexus) >= 2;
//
//            itemQueue[Protoss_Assimilator] =		Item((s >= 24) + (s >= 50));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//        }
//        else if (currentTransition == "Carrier") {
//            getOpening =		s < 80;
//            firstUnit =         com(Protoss_Nexus) >= 2 ? Protoss_Carrier : UnitTypes::None;
//            lockedTransition =  com(Protoss_Nexus) >= 2;
//
//            itemQueue[Protoss_Assimilator] =		Item((s >= 24) + (s >= 50));
//            itemQueue[Protoss_Cybernetics_Core] =	Item(s >= 26);
//        }
//    }
//}