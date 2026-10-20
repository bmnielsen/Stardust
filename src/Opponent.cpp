#include "Opponent.h"

#include <fstream>
#include <ranges>
#include <chrono>
#include <ctime>
#include <iomanip>

#include <nlohmann.h>

#include "Map.h"
#include "Units.h"
#include "PathFinding.h"
#include "FileTools.h"

namespace Opponent
{
    namespace
    {
        std::string name;
        bool raceUnknown;
        std::vector<nlohmann::json> previousGames;
        nlohmann::json currentGame;
        std::set<std::string> setKeys;
        bool learnModeGate = false; // B2: learning IO redirected to scratch files

        std::string opponentDataFilename(bool writing = false)
        {
            /* Gate mode (bwapi.ini [ai] learn_mode=gate, written by the harness for */
            /* old-vs-new verification matches): both reads and writes go to scratch */
            /* files so the mirrored series never pollutes the live profiles. */
            auto label = (std::ostringstream()
                    << (learnModeGate ? "scratch_shushanking_" : "shushanking_") << name).str();
            return FileTools::getFilePath(label, "json", writing);
        }

        /* One-shot read of bwapi.ini's learn_mode key (unique key, no section */
        /* parsing needed). File lives at the StarCraft working-directory root. */
        bool readLearnModeGate()
        {
            std::ifstream ini("bwapi-data/bwapi.ini");
            if (!ini.good()) return false;
            std::string line;
            while (std::getline(ini, line))
            {
                auto pt = line.find('=');
                if (pt == std::string::npos) continue;
                auto key = line.substr(0, pt);
                auto value = line.substr(pt + 1);
                auto trim = [](std::string &s)
                {
                    while (!s.empty() && isspace((unsigned char) s.front())) s.erase(s.begin());
                    while (!s.empty() && isspace((unsigned char) s.back())) s.pop_back();
                };
                trim(key);
                trim(value);
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                if (key == "learn_mode") return value == "gate";
            }
            return false;
        }

        bool readPreviousGame(std::istream &str)
        {
            std::string line;
            std::getline(str, line);

            if (line.empty()) return false;

            try
            {
                previousGames.emplace_back(nlohmann::json::parse(line));
            }
            catch (std::exception &ex)
            {
                AppLog::Get() << "Exception caught parsing previous game: " << ex.what() << "; line: " << line;
                return false;
            }

            return true;
        }

        bool isPreviousGameOnThisMap(const nlohmann::json &previousGameResult)
        {
            try
            {
                auto mapHashIt = previousGameResult.find("mapHash");
                if (mapHashIt != previousGameResult.end())
                {
                    return mapHashIt->get<std::string>() == BWAPI::Broodwar->mapHash();
                }
            }
            catch (std::exception &ex)
            {
                /* Just skip this game result */
            }
            return false;
        }

        /* Enemy openings that put early pressure on our base, making */
        /* economy-focused openings (ForgeExpandDT) risky */
        const std::set<std::string> AggressiveEnemyOpenings = {
                "WorkerRush", "ProxyRush", "ZealotRush", "ZealotAllIn",
                "TwoGate", "DragoonAllIn"
        };

        /* First strategy of a game (frame must be 0 = the actual opening); */
        /* empty string when the record is missing or malformed */
        std::string initialStrategyOf(const nlohmann::json &previousGame, const char *key)
        {
            auto iterRef = previousGame.find(key);
            if (iterRef == previousGame.end() || iterRef->empty()) return {};
            auto first = (*iterRef)[0];
            if (first[0] != 0) return {};
            return first[1];
        }

        /* P0-2/P0-3 (2026-09-18 report): all non-Unknown enemy labels one game */
        /* recorded - consumed by the label-conditioned opening prior. */
        std::vector<std::string> enemyLabelsOf(const nlohmann::json &previousGame)
        {
            std::vector<std::string> labels;
            auto iterRef = previousGame.find("enemyStrategy");
            if (iterRef == previousGame.end() || !iterRef->is_array()) return labels;
            for (auto &entry : *iterRef)
            {
                if (entry.is_array() && entry.size() >= 2 && entry.back().is_string())
                {
                    auto label = entry.back().get<std::string>();
                    if (label != "Unknown") labels.push_back(label);
                }
            }
            return labels;
        }

        /* Whether this historical game showed an aggressive enemy: either a */
        /* recognized aggressive opening, or an early attack on our main base */
        /* (recorded via enemyFirstAttackFrame). nullopt when no information. */
        /* Threshold 11500 frames ~= observed median first-attack arrival */
        /* (median 11278, min 4425, max 15804 over 52 measured games) */
        std::optional<bool> enemyAggressiveOf(const nlohmann::json &previousGame)
        {
            if (AggressiveEnemyOpenings.contains(initialStrategyOf(previousGame, "enemyStrategy")))
            {
                return true;
            }
            try
            {
                auto iterRef = previousGame.find("enemyFirstAttackFrame");
                if (iterRef != previousGame.end())
                {
                    int tick = iterRef->get<int>();
                    if (tick >= 0 && tick < INT_MAX) return tick < 11500;
                }
            }
            catch (std::exception &ex)
            {
                /* Just skip this game result */
            }
            return std::nullopt;
        }
    }

    void initialize()
    {
        setKeys.clear();
        learnModeGate = readLearnModeGate();

        name = BWAPI::Broodwar->enemy()->getName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());

        raceUnknown =
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg;

        previousGames.clear();
        currentGame = nlohmann::json::object();

        /* B11: per-game start timestamp (new contract field; append-only). Gives the */
        /* harness reconciliation an unambiguous key for consecutive same-map games. */
        {
            std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm tm{};
            localtime_s(&tm, &t);
            std::ostringstream ts;
            ts << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            currentGame["startTime"] = ts.str();
        }

        std::ifstream file;
        file.open(opponentDataFilename());
        if (file.good())
        {
            try
            {
                while (readPreviousGame(file)) {}
            }
            catch (std::exception &ex)
            {
                AppLog::Get() << "Exception caught attempting to read opponent data: " << ex.what();
            }
        }

        AppLog::Get() << "Read " << previousGames.size() << " previous game result(s)";

        currentGame["mapHash"] = BWAPI::Broodwar->mapHash();

        /* Default values for some items */
        currentGame["pylonInOurMain"] = INT_MAX;
        currentGame["firstDarkTemplarCompleted"] = INT_MAX;
        currentGame["firstMutaliskCompleted"] = INT_MAX;
        currentGame["firstLurkerAtOurMain"] = INT_MAX;
        currentGame["sneakAttack"] = INT_MAX;
        currentGame["elevatoredUnits"] = 0;
        currentGame["enemyFirstAttackFrame"] = INT_MAX;
        currentGame["myStrategy"] = nlohmann::json::array();
        currentGame["enemyStrategy"] = nlohmann::json::array();
    }

    void update()
    {
        /* Record the first time an enemy combat unit approaches our main base: */
        /* a robust proxy for the opponent's first-attack timing, even when their */
        /* opening cannot be recognized by name */
        if (frameNow < 20000 && currentGame["enemyFirstAttackFrame"] == INT_MAX)
        {
            auto mainBase = Map::getMyMain();
            if (mainBase)
            {
                auto mainPos = mainBase->getPosition();
                for (auto u : BWAPI::Broodwar->enemy()->getUnits())
                {
                    if (!u || !u->exists() || !u->isCompleted()) continue;
                    auto type = u->getType();
                    if (type.isWorker() || type.isBuilding()) continue;
                    if (u->getDistance(mainPos) < 800)
                    {
                        setGameValue("enemyFirstAttackFrame", frameNow);
                        break;
                    }
                }
            }
        }

        /* Detect sneak attacks */
        /* A sneak attack is when the enemy has at least 4 units in our main base in the early game while our army is out on the map */
        /* This might happen because of a runaround or a drop */
        if (frameNow < 12000 && !isGameValueSet("sneakAttack"))
        {
            auto mainBase = Map::getMyMain();
            auto enemyMain = Map::getEnemyStartingMain();
            if (mainBase && enemyMain && UnitMgr::enemyAtBase(mainBase).size() >= 4)
            {
                /* Verify we have at least four combat units closer to the enemy's main than ours */
                int combatUnitsOnMap = 0;
                auto countType = [&](BWAPI::UnitType type)
                {
                    for (auto &gameUnit: UnitMgr::allMineCompletedOfType(type))
                    {
                        if (!gameUnit->exists()) continue;
                        if (!gameUnit->completed) continue;
                        int distOurs = Pathfinder::GetGroundDistance(gameUnit->prevPos,
                                                                      mainBase->getPosition(),
                                                                      gameUnit->type,
                                                                      Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
                        int distTheirs = Pathfinder::GetGroundDistance(gameUnit->prevPos,
                                                                        enemyMain->getPosition(),
                                                                        gameUnit->type,
                                                                        Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
                        if (distTheirs <= distOurs)
                        {
                            combatUnitsOnMap++;
                        }
                    }
                };
                countType(BWAPI::UnitTypes::Protoss_Zealot);
                countType(BWAPI::UnitTypes::Protoss_Dragoon);
                if (combatUnitsOnMap >= 4)
                {
                    setGameValue("sneakAttack", frameNow);
                }
            }
        }
    }

    void gameEnd(bool isWinner)
    {
        currentGame["won"] = isWinner;

        /* Multi-signal end-state capture (v1.6 二.L5). Arbitration priority when */
        /* signals disagree: endIsVictorious/endIsDefeated > enemyLeft > ourLeaveGame */
        /* (the latter two are set by the module at the corresponding exit paths). */
        currentGame["endIsVictorious"] = BWAPI::Broodwar->self()->isVictorious();
        currentGame["endIsDefeated"] = BWAPI::Broodwar->self()->isDefeated();

        previousGames.push_back(currentGame);

        /* B9: cap the retained history at the most recent 500 games (v1 keeps the */
        /* deterministic newest-500 trim; decay-weighted selection adds nothing */
        /* while the selector already downweights old records). */
        if (previousGames.size() > 500)
        {
            previousGames.erase(previousGames.begin(), previousGames.end() - 500);
        }

        std::ofstream file;
        file.open(opponentDataFilename(true), std::ofstream::trunc);

        bool first = true;
        for (auto &game : previousGames)
        {
            if (first)
            {
                first = false;
            }
            else
            {
                file << "\n";
            }

            file << game.dump(-1, ' ', true);
        }

        file.close();

        /* C9: compact per-opening aggregate (opener -> [wins, losses] over the */
        /* retained history) written alongside the JSONL. Recomputed fresh each */
        /* game so it can never drift from the stream it summarizes. */
        {
            nlohmann::json agg = nlohmann::json::object();
            for (auto &game : previousGames)
            {
                std::string op = initialStrategyOf(game, "myStrategy");
                if (op.empty()) continue;
                auto wonIt = game.find("won");
                if (wonIt == game.end() || !wonIt->is_boolean()) continue;
                auto &slot = agg[op];
                if (!slot.is_array()) slot = nlohmann::json::array({0, 0});
                if (wonIt->get<bool>()) slot[0] = slot[0].get<int>() + 1;
                else slot[1] = slot[1].get<int>() + 1;
            }
            std::ofstream aggFile(FileTools::getFilePath(
                    (std::ostringstream()
                            << (learnModeGate ? "scratch_shushanking_" : "shushanking_")
                            << name << "_agg").str(), "json", true),
                    std::ofstream::trunc);
            if (aggFile.is_open()) aggFile << agg.dump(-1, ' ', true) << "\n";
        }
    }

    bool isUnknownRace()
    {
        return raceUnknown;
    }

    std::string &getName()
    {
        if (!name.empty()) return name;

        name = BWAPI::Broodwar->enemy()->getName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());

        return name;
    }

    bool hasRaceJustBeenDetermined()
    {
        if (!raceUnknown) return false;

        raceUnknown =
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg;

        if (raceUnknown) return false;

        AppLog::Get() << "Enemy identified as " << BWAPI::Broodwar->enemy()->getRace();
        return true;
    }

    bool canBeRace(BWAPI::Race race)
    {
        return raceUnknown || BWAPI::Broodwar->enemy()->getRace() == race;
    }

    void setGameValue(const std::string &key, int value)
    {
#if CHERRYVIS_ENABLED
        if (currentGame[key] != value)
        {
            Telemetry::log() << "Set game value " << key << " to " << value;
        }
#endif
#if LOGGING_ENABLED
        if (currentGame[key] != value)
        {
            AppLog::Get() << "Set game value " << key << " to " << value;
        }
#endif
        currentGame[key] = value;
        setKeys.insert(key);
    }

    void incrementGameValue(const std::string &key, int delta)
    {
        int currentValue = 0;
        if (isGameValueSet(key))
        {
            currentValue = currentGame[key];
        }

        currentGame[key] = currentValue + delta;

#if CHERRYVIS_ENABLED
        Telemetry::log() << "Incremented game value " << key << " from " << currentValue << " to " << currentGame[key];
#endif
#if LOGGING_ENABLED
        AppLog::Get() << "Incremented game value " << key << " from " << currentValue << " to " << currentGame[key];
#endif
        setKeys.insert(key);
    }

    bool isGameValueSet(const std::string &key)
    {
        return setKeys.contains(key);
    }

    void addMyStrategyChange(const std::string &strategy)
    {
        currentGame["myStrategy"].push_back(nlohmann::json::array({frameNow, strategy}));
    }

    void addEnemyStrategyChange(const std::string &strategy)
    {
        currentGame["enemyStrategy"].push_back(nlohmann::json::array({frameNow, strategy}));
    }

    int minValueInPreviousGames(const std::string &key, int defaultNoData, int maxCount, int minCount)
    {
        if (previousGames.size() < minCount) return defaultNoData;

        int outcome = INT_MAX;

        int count = 0;
        for (auto &previousGame : std::ranges::reverse_view(previousGames))
        {
            if (count >= maxCount) break;

            try
            {
                auto valIt = previousGame.find(key);

                /* If we hit a record where this data point isn't recorded, this means it was played by a previous version of the bot */
                /* Apply the minimum game count restriction and return */
                if (valIt == previousGame.end())
                {
                    if (count < minCount) return defaultNoData;
                    return outcome;
                }

                outcome = std::min(outcome, valIt->get<int>());
            }
            catch (std::exception &ex)
            {
                /* Just skip this game result */
            }

            count++;
        }

        return outcome;
    }

    double winLossRatio(double defaultValue, int maxCount)
    {
        int wins = 0;
        int losses = 0;

        int count = 0;
        for (auto &previousGame : std::ranges::reverse_view(previousGames))
        {
            if (count >= maxCount) break;

            try
            {
                auto valIt = previousGame.find("won");
                if (valIt != previousGame.end())
                {
                    (valIt->get<bool>() ? wins : losses)++;
                }
            }
            catch (std::exception &ex)
            {
                /* Just skip this game result */
            }

            count++;
        }

        if (wins == 0 && losses == 0) return defaultValue;
        return (double)wins / (double)(wins + losses);
    }

    std::vector<OpeningGameRecord> openingHistory()
    {
        std::vector<OpeningGameRecord> history;
        for (auto &previousGame : std::ranges::reverse_view(previousGames))
        {
            auto wonIt = previousGame.find("won");
            if (wonIt == previousGame.end() || !wonIt->is_boolean()) continue;
            auto won = wonIt->get<bool>();

            OpeningGameRecord rec{};
            rec.won = won;
            rec.myOpening = initialStrategyOf(previousGame, "myStrategy");
            if (rec.myOpening.empty()) continue;

            rec.onThisMap = isPreviousGameOnThisMap(previousGame);
            rec.enemyAggressive = enemyAggressiveOf(previousGame);
            rec.enemyLabels = enemyLabelsOf(previousGame);

            history.push_back(rec);
        }
        return history;
    }

    std::set<std::string> recentEnemyStrategies(int n)
    {
        std::set<std::string> outcome;
        int taken = 0;
        for (auto &game : std::ranges::reverse_view(previousGames))
        {
            if (taken >= n) break;
            auto iterRef = game.find("enemyStrategy");
            if (iterRef == game.end() || !iterRef->is_array()) continue;
            for (auto &entry : *iterRef)
            {
                /* entries are [frame, "label"]; collect every non-Unknown label */
                if (entry.is_array() && entry.size() >= 2 && entry.back().is_string())
                {
                    auto label = entry.back().get<std::string>();
                    if (label != "Unknown") outcome.insert(label);
                }
            }
            taken++;
        }
        return outcome;
    }
}
