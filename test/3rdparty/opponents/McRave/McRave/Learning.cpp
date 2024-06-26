#include "McRave.h"
#include "BuildOrder.h"
#include <fstream>
#include <iomanip>

using namespace std;
using namespace BWAPI;
using namespace UnitTypes;
using namespace McRave::BuildOrder::All;

namespace McRave::Learning {
    namespace {

        struct BuildComponent {
            int w = 0;
            int l = 0;
            double ucb1 = 0.0;
            string name = "";
            BuildComponent(string _name) {
                name = _name;
            }
        };

        struct Build {
            int w = 0;
            int l = 0;
            double ucb1 = 0.0;
            string name = "";
            vector<BuildComponent> openers, transitions;
            BuildComponent * getComponent(string name) {
                for (auto &opener : openers) {
                    if (opener.name == name)
                        return &opener;
                }
                for (auto &transition : transitions) {
                    if (transition.name == name)
                        return &transition;
                }
                return nullptr;
            }

            Build(string _name) {
                name = _name;
            }
        };

        bool mapLearning;
        vector<Build> myBuilds;
        stringstream ss;
        string mapName;
        string noStats;
        string myRaceChar, enemyRaceChar;
        string version;
        string learningExtension, gameInfoExtension;
        bool p, z, t, r;

        bool isPBuildAllowed(Race enemy, string build, string opener, string transition)
        {
            const auto buildOkay = [&]() {
                if (build == "1GateCore")
                    return t || p;
                if (build == "2Gate")
                    return true;
                if (build == "2Base")
                    return t;
                if (build == "FFE")
                    return z && !Terrain::isIslandMap();
                return false;
            };

            const auto openerOkay = [&]() {
                if (build == "1GateCore") {
                    if (opener == "1Zealot")
                        return true;
                    if (opener == "2Zealot")
                        return z || r;
                }

                if (build == "2Gate") {
                    if (opener == "Main")
                        return true;
                }

                if (build == "FFE") {
                    if (opener == "Forge")
                        return z;
                }

                if (build == "2Base") {
                    if (opener == "12Nexus" || opener == "21Nexus" || opener == "20Nexus")
                        return t;
                }
                return false;
            };

            const auto transitionOkay = [&]() {
                if (build == "1GateCore") {
                    if (transition == "DT")
                        return (p || t);
                    if (transition == "3Gate")
                        return !Terrain::isNarrowNatural() && (p || r);
                    if (transition == "Robo")
                        return !Terrain::isReverseRamp() && (p || r);
                    if (transition == "4Gate")
                        return !Terrain::isNarrowNatural() && p;
                }

                if (build == "2Gate") {
                    if (transition == "DT")
                        return p || t;
                    if (transition == "Robo")
                        return p || r;
                    if (transition == "Expand")
                        return false;
                    if (transition == "4Gate")
                        return z;
                }

                if (build == "FFE") {
                    if (transition == "NeoBisu" || transition == "2Stargate" || transition == "StormRush" || transition == "5GateGoon")
                        return z;
                }

                if (build == "2Base") {
                    if (transition == "ReaverCarrier" || transition == "Carrier" || transition == "Obs")
                        return t;
                }
                return false;
            };

            return buildOkay() && openerOkay() && transitionOkay();
        }

        bool isTBuildAllowed(Race enemy, string build, string opener, string transition)
        {
            // Uhhhhh
            return true;
        }

        bool isZBuildAllowed(Race enemy, string build, string opener, string transition)
        {
            const auto buildOkay = [&]() {
                if (build == "PoolLair")
                    return z;
                if (build == "HatchPool")
                    return !z;
                if (build == "PoolHatch")
                    return !z;
                return false;
            };

            const auto openerOkay = [&]() {
                if (build == "PoolHatch") {
                    if (opener == "Overpool")
                        return !z;
                    if (opener == "12Pool")
                        return t;
                }
                if (build == "HatchPool") {
                    if (opener == "12Hatch")
                        return !z;
                    if (opener == "10Hatch")
                        return false;
                }
                if (build == "PoolLair") {
                    if (opener == "9Pool")
                        return z;
                }
                return false;
            };

            const auto transitionOkay = [&]() {
                if (build == "PoolLair") {
                    if (transition == "1HatchMuta")
                        return z;
                }
                if (build == "HatchPool") {
                    if (transition == "2HatchMuta")
                        return !z;
                }
                if (build == "PoolHatch") {
                    if (transition == "2HatchMuta")
                        return true;
                    if (transition == "3HatchMuta")
                        return !z;
                    if (transition == "3HatchSpeedling")
                        return false;
                    if (transition == "6HatchHydra")
                        return p;
                    if (transition == "2HatchLurker")
                        return false;
                }
                return false;
            };

            return buildOkay() && openerOkay() && transitionOkay();
        }

        bool isBuildAllowed(Race enemy, string build, string opener, string transition)
        {
            if (Broodwar->self()->getRace() == Races::Protoss)
                return isPBuildAllowed(enemy, build, opener, transition);
            if (Broodwar->self()->getRace() == Races::Terran)
                return isTBuildAllowed(enemy, build, opener, transition);
            if (Broodwar->self()->getRace() == Races::Zerg)
                return isZBuildAllowed(enemy, build, opener, transition);
            return false;
        }

        bool isBuildPossible(string build, string opener)
        {
            vector<UnitType> buildings, defenses;
            auto wallOptional = false;
            auto tight = false;

            // Protoss wall requirements
            if (Broodwar->self()->getRace() == Races::Protoss) {
                if (BWEB::Map::getNaturalArea()->ChokePoints().size() == 1)
                    return Walls::getMainWall();
                if (build == "FFE")
                    return Walls::getNaturalWall();
                return true;
            }

            // Zerg wall requirements
            if (Broodwar->self()->getRace() == Races::Zerg)
                return true;

            // Terran wall requirements
            if (Broodwar->self()->getRace() == Races::Terran)
                return Walls::getMainWall();
            return false;
        }

        void getDefaultBuild()
        {
            // Protoss
            if (Broodwar->self()->getRace() == Races::Protoss) {
                if (Players::PvP())
                    BuildOrder::setLearnedBuild("1GateCore", "1Zealot", "Robo");
                else if (Players::PvZ())
                    BuildOrder::setLearnedBuild("2Gate", "Main", "4Gate");
                else if (Players::PvT())
                    BuildOrder::setLearnedBuild("2Base", "21Nexus", "Obs");
                else if (Players::PvFFA() || Players::PvTVB())
                    BuildOrder::setLearnedBuild("1GateCore", "1Zealot", "3Gate");
                else
                    BuildOrder::setLearnedBuild("2Gate", "Main", "Robo");
            }

            // Zerg
            if (Broodwar->self()->getRace() == Races::Zerg) {
                if (Players::ZvZ())
                    BuildOrder::setLearnedBuild("PoolLair", "9Pool", "1HatchMuta");
                else if (Players::ZvT())
                    BuildOrder::setLearnedBuild("HatchPool", "12Hatch", "2HatchMuta");
                else if (Players::ZvP())
                    BuildOrder::setLearnedBuild("PoolHatch", "Overpool", "2HatchMuta");
                else if (Players::ZvFFA() || Players::ZvTVB())
                    BuildOrder::setLearnedBuild("HatchPool", "10Hatch", "3HatchMuta");
                else
                    BuildOrder::setLearnedBuild("PoolHatch", "Overpool", "2HatchSpeedling");
            }

            // Terran
            if (Broodwar->self()->getRace() == Races::Terran) {
                BuildOrder::setLearnedBuild("RaxFact", "10Rax", "2Fact");
            }
        }

        void getBestBuild()
        {
            int totalWins = 0;
            int totalLoses = 0;
            int totalGames = 0;
            auto enemyRace = Broodwar->enemy()->getRace();

            const auto parseLearningFile = [&](ifstream &file) {
                string buffer, token;
                while (file >> buffer)
                    ss << buffer << " ";

                // Create a copy so we aren't dumping out the information
                stringstream sscopy;
                sscopy << ss.str();

                Build * currentBuild = nullptr;
                while (!sscopy.eof()) {
                    sscopy >> token;

                    if (token == "Total") {
                        sscopy >> totalWins >> totalLoses;
                        totalGames = totalWins + totalLoses;
                        continue;
                    }

                    // Build winrate
                    auto itr = find_if(myBuilds.begin(), myBuilds.end(), [&](const auto& u) { return u.name == token; });
                    if (itr != myBuilds.end()) {
                        currentBuild = &*itr;
                        sscopy >> itr->w >> itr->l;
                    }
                    else if (currentBuild && currentBuild->getComponent(token))
                        sscopy >> currentBuild->getComponent(token)->w >> currentBuild->getComponent(token)->l;

                }
            };

            const auto calculateUCB = [&](int w, int l) {
                if (w + l == 0)
                    return 999.9;
                return (double(w) / double(w + l)) + cbrt(2.0 * log((double)totalGames) / double(w + l));
            };

            // Attempt to read a file from the read directory first, then write directory
            ifstream readFile("bwapi-data/read/" + learningExtension);
            ifstream writeFile("bwapi-data/write/" + learningExtension);
            if (readFile)
                parseLearningFile(readFile);
            else if (writeFile)
                parseLearningFile(writeFile);

            // No file found, create a new one
            else {
                if (!writeFile.good()) {

                    ss << "Total " << noStats;
                    for (auto &build : myBuilds) {
                        ss << build.name << noStats;

                        for (auto &opener : build.openers)
                            ss << opener.name << noStats;
                        for (auto &transition : build.transitions)
                            ss << transition.name << noStats;
                    }
                }
            }

            // Calculate UCB1 values - sort by descending value
            for (auto &build : myBuilds) {
                build.ucb1 = calculateUCB(build.w, build.l);
                for (auto &opener : build.openers)
                    opener.ucb1 = calculateUCB(opener.w, opener.l) + 0.01;
                for (auto &transition : build.transitions)
                    transition.ucb1 = calculateUCB(transition.w, transition.l) + 0.01;

                sort(build.openers.begin(), build.openers.end(), [&](const auto &left, const auto &right) { return left.ucb1 < right.ucb1; });
                sort(build.transitions.begin(), build.transitions.end(), [&](const auto &left, const auto &right) { return left.ucb1 < right.ucb1; });
            }
            sort(myBuilds.begin(), myBuilds.end(), [&](const auto &left, const auto &right) { return left.ucb1 < right.ucb1; });

            // Pick the best build that is allowed
            auto bestBuildUCB1 = 0.0;
            auto bestOpenerUCB1 = 0.0;
            auto bestTransitionUCB1 = 0.0;
            for (auto &build : myBuilds) {
                if (build.ucb1 < bestBuildUCB1)
                    continue;

                bestBuildUCB1 = build.ucb1;
                bestOpenerUCB1 = 0.0;
                bestTransitionUCB1 = 0.0;

                for (auto &opener : build.openers) {
                    if (opener.ucb1 < bestOpenerUCB1)
                        continue;
                    bestOpenerUCB1 = opener.ucb1;
                    bestTransitionUCB1 = 0.0;

                    for (auto &transition : build.transitions) {
                        if (transition.ucb1 < bestTransitionUCB1)
                            continue;
                        bestTransitionUCB1 = transition.ucb1;

                        if (isBuildAllowed(enemyRace, build.name, opener.name, transition.name))
                            BuildOrder::setLearnedBuild(build.name, opener.name, transition.name);
                    }
                }
            }
        }

        void getPermanentBuild()
        {
            // Testing builds if needed
            if (false) {
                if (Players::PvZ()) {
                    BuildOrder::setLearnedBuild("FFE", "Forge", "NeoBisu");
                    return;
                }
                if (Players::PvP()) {
                    BuildOrder::setLearnedBuild("1GateCore", "1Zealot", "DT");
                    return;
                }
                if (Players::PvT()) {
                    BuildOrder::setLearnedBuild("2Base", "21Nexus", "Obs");
                    return;
                }
                if (Players::ZvZ()) {
                    BuildOrder::setLearnedBuild("PoolLair", "9Pool", "1HatchMuta");
                    return;
                }
                if (Players::ZvT()) {
                    BuildOrder::setLearnedBuild("PoolHatch", "12Pool", "3HatchMuta");
                    return;
                }
                if (Players::ZvP()) {
                    BuildOrder::setLearnedBuild("HatchPool", "12Hatch", "2HatchMuta");
                    return;
                }
            }
        }

        void createBuildMaps()
        {
            // Protoss builds, openers and transitions
            if (Broodwar->self()->getRace() == Races::Protoss) {
                Build OneGateCore("1GateCore");
                OneGateCore.openers ={ BuildComponent("0Zealot"), BuildComponent("1Zealot"), BuildComponent("2Zealot") };
                OneGateCore.transitions ={ BuildComponent("DT"), BuildComponent("Robo"), BuildComponent("4Gate"), BuildComponent("3Gate") };

                Build TwoGate("2Gate");
                TwoGate.openers ={ BuildComponent("Main") };
                TwoGate.transitions ={ BuildComponent("DT"), BuildComponent("Robo"), BuildComponent("4Gate"), BuildComponent("Expand"), BuildComponent("DoubleExpand") };

                Build FFE("FFE");
                FFE.openers ={ BuildComponent("Forge") };
                FFE.transitions ={ BuildComponent("NeoBisu"), BuildComponent("2Stargate"), BuildComponent("StormRush"), BuildComponent("4GateArchon"), BuildComponent("CorsairReaver"), BuildComponent("5GateGoon") };

                Build TwoBase("2Base");
                TwoBase.openers ={ BuildComponent("12Nexus"), BuildComponent("20Nexus"), BuildComponent("21Nexus") };
                TwoBase.transitions ={ BuildComponent("Obs"), BuildComponent("Carrier"), BuildComponent("ReaverCarrier") };
                
                myBuilds ={ OneGateCore, TwoGate, FFE, TwoBase };
            }

            if (Broodwar->self()->getRace() == Races::Zerg) {

                Build PoolHatch("PoolHatch");
                PoolHatch.openers ={ BuildComponent("4Pool"), BuildComponent("9Pool"), BuildComponent("Overpool"), BuildComponent("12Pool") };
                PoolHatch.transitions={ BuildComponent("2HatchMuta"), BuildComponent("2.5HatchMuta"), BuildComponent("3HatchMuta"), BuildComponent("2HatchSpeedling"), BuildComponent("3HatchSpeedling"), BuildComponent("6HatchHydra"), BuildComponent("2HatchLurker") };

                Build HatchPool("HatchPool");
                HatchPool.openers ={ BuildComponent("9Hatch"), BuildComponent("10Hatch"), BuildComponent("12Hatch") };
                HatchPool.transitions={ BuildComponent("2HatchMuta"), BuildComponent("2.5HatchMuta"), BuildComponent("3HatchMuta"), BuildComponent("2HatchSpeedling"), BuildComponent("6HatchHydra") };

                Build PoolLair("PoolLair");
                PoolLair.openers ={ BuildComponent("9Pool"), BuildComponent("12Pool") };
                PoolLair.transitions={ BuildComponent("1HatchMuta") };

                myBuilds ={ PoolHatch, HatchPool, PoolLair };
            }

            if (Broodwar->self()->getRace() == Races::Terran) {
                BuildOrder::setLearnedBuild("RaxFact", "10Rax", "2Fact");
                isBuildPossible(BuildOrder::getCurrentBuild(), BuildOrder::getCurrentOpener());
                return;// Don't know what to do yet
            }
        }
    }

    void onEnd(bool isWinner)
    {
        if (!Broodwar->enemy() || Players::ZvFFA() || Players::ZvTVB())
            return;

        // HACK: Don't touch records if we play islands, since islands aren't fully implemented yet
        if (Terrain::isIslandMap())
            return;

        // Write into the write directory 3 tokens at a time (4 if we detect a dash)
        ofstream config("bwapi-data/write/" + learningExtension);
        string token;
        bool foundLearning = false;

        // For each token, check if we should increment the wins or losses then shove it into the config file
        while (ss >> token) {
            int w, l;
            ss >> w >> l;
            if (BuildOrder::getCurrentBuild() == token)
                foundLearning = true;
            if (BuildOrder::getCurrentBuild() == token || token == "Total" || (foundLearning && (BuildOrder::getCurrentOpener() == token || BuildOrder::getCurrentTransition() == token)))
                isWinner ? w++ : l++;
            if (BuildOrder::getCurrentTransition() == token)
                foundLearning = false;
            config << token << " " << w << " " << l << endl;
        }

        const auto copyFile = [&](string source, string destination) {
            std::ifstream src(source, std::ios::binary);
            std::ofstream dest(destination, std::ios::binary);
            dest << src.rdbuf();
            return src && dest;
        };

        // Write into the write directory information about what we saw the enemy do
        ifstream readFile("bwapi-data/read/" + gameInfoExtension);
        if (readFile)
            copyFile("bwapi-data/read/" + gameInfoExtension, "bwapi-data/write/" + gameInfoExtension);
        ofstream gameLog("bwapi-data/write/" + gameInfoExtension, std::ios_base::app);

        // Who won on what map in how long
        gameLog << (isWinner ? "Won" : "Lost") << ","
            << mapName << ","
            << std::setfill('0') << Util::getTime().minutes << ":" << std::setw(2) << Util::getTime().seconds << ",";

        // What strategies were used/detected
        gameLog
            << Spy::getEnemyBuild() << ","
            << Spy::getEnemyOpener() << ","
            << Spy::getEnemyTransition() << ","
            << currentBuild << "," << currentOpener << "," << currentTransition << ",";

        // When did we detect the enemy strategy
        gameLog << std::setfill('0') << Spy::getEnemyBuildTime().minutes << ":" << std::setw(2) << Spy::getEnemyBuildTime().seconds << ",";
        gameLog << std::setfill('0') << Spy::getEnemyOpenerTime().minutes << ":" << std::setw(2) << Spy::getEnemyOpenerTime().seconds << ",";
        gameLog << std::setfill('0') << Spy::getEnemyTransitionTime().minutes << ":" << std::setw(2) << Spy::getEnemyTransitionTime().seconds;

        // Store a list of total units everyone made
        for (auto &type : UnitTypes::allUnitTypes()) {
            if (!type.isBuilding()) {
                auto cnt = Players::getTotalCount(PlayerState::Self, type);
                if (cnt > 0)
                    gameLog << "," << cnt << "," << type.c_str();
            }
        }
        for (auto &type : UnitTypes::allUnitTypes()) {
            if (!type.isBuilding()) {
                auto cnt = Players::getTotalCount(PlayerState::Enemy, type);
                if (cnt > 0)
                    gameLog << "," << cnt << "," << type.c_str();
            }
        }

        gameLog << endl;
    }

    void onStart()
    {
        if (!Broodwar->enemy() || Players::vFFA() || Players::vTVB()) {
            getDefaultBuild();
            return;
        }

        // Enemy race bools
        p = Broodwar->enemy()->getRace() == Races::Protoss;
        z = Broodwar->enemy()->getRace() == Races::Zerg;
        t = Broodwar->enemy()->getRace() == Races::Terran;
        r = Broodwar->enemy()->getRace() == Races::Unknown || Broodwar->enemy()->getRace() == Races::Random;

        // Grab only the alpha characters from the map name to remove version numbers
        for (auto &c : Broodwar->mapFileName()) {
            if (isalpha(c))
                mapName.push_back(c);
            if (c == '.')
                break;
        }
        McRave::easyWrite("New game on " + mapName);

        // File extension including our race initial;
        mapLearning         = false;
        myRaceChar          ={ *Broodwar->self()->getRace().c_str() };
        enemyRaceChar       ={ *Broodwar->enemy()->getRace().c_str() };
        version             = "CoG2022";
        noStats             = " 0 0 ";
        learningExtension   = mapLearning ? myRaceChar + "v" + enemyRaceChar + " " + Broodwar->enemy()->getName() + " " + mapName + ".txt" : myRaceChar + "v" + enemyRaceChar + " " + Broodwar->enemy()->getName() + " " + version + ".txt";
        gameInfoExtension   = myRaceChar + "v" + enemyRaceChar + " " + Broodwar->enemy()->getName() + " " + version + " Info.txt";

        createBuildMaps();
        getDefaultBuild();
        getBestBuild();
        getPermanentBuild();
    }
}