#include "Opponent.h"

#include <fstream>
#include <filesystem>
#include <ranges>

#include <nlohmann.h>

#include "Map.h"
#include "Units.h"
#include "PathFinding.h"

namespace Opponent
{
    namespace
    {
        std::string name;
        bool raceUnknown;
        std::vector<nlohmann::json> previousGames;
        nlohmann::json currentGame;
        std::set<std::string> setKeys;

        std::vector<std::string> dataLoadPaths = {
                "bwapi-data/read/",
                "bwapi-data/write/",
                "bwapi-data/AI/"
        };
        std::string dataWritePath = "bwapi-data/write/";

        std::string opponentDataFilename(bool writing = false)
        {
            if (writing)
            {
                return (std::ostringstream() << dataWritePath << "stardust_" << name << ".json").str();
            }

            for (auto &path : dataLoadPaths)
            {
                auto filename = (std::ostringstream() << path << "stardust_" << name << ".json").str();
                if (std::filesystem::exists(filename)) return filename;
            }

            return "";
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
                Log::Get() << "Exception caught parsing previous game: " << ex.what() << "; line: " << line;
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
                // Just skip this game result
            }
            return false;
        }
    }

    void initialize()
    {
        setKeys.clear();

        name = BWAPI::Broodwar->enemy()->getName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        name.erase(std::remove_if(name.begin(), name.end(), ::isspace), name.end());

        raceUnknown =
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Protoss &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Terran &&
                BWAPI::Broodwar->enemy()->getRace() != BWAPI::Races::Zerg;

        previousGames.clear();
        currentGame = nlohmann::json::object();

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
                Log::Get() << "Exception caught attempting to read opponent data: " << ex.what();
            }
        }

        Log::Get() << "Read " << previousGames.size() << " previous game result(s)";

        currentGame["mapHash"] = BWAPI::Broodwar->mapHash();

        // Default values for some items
        currentGame["pylonInOurMain"] = INT_MAX;
        currentGame["firstDarkTemplarCompleted"] = INT_MAX;
        currentGame["firstMutaliskCompleted"] = INT_MAX;
        currentGame["firstLurkerAtOurMain"] = INT_MAX;
        currentGame["sneakAttack"] = INT_MAX;
        currentGame["elevatoredUnits"] = 0;
        currentGame["myStrategy"] = nlohmann::json::array();
        currentGame["enemyStrategy"] = nlohmann::json::array();
    }

    void update()
    {
        // Detect sneak attacks
        // A sneak attack is when the enemy has at least 4 units in our main base in the early game while our army is out on the map
        // This might happen because of a runaround or a drop
        if (currentFrame < 12000 && !isGameValueSet("sneakAttack"))
        {
            auto mainBase = Map::getMyMain();
            auto enemyMain = Map::getEnemyStartingMain();
            if (mainBase && enemyMain && Units::enemyAtBase(mainBase).size() >= 4)
            {
                // Verify we have at least four combat units closer to the enemy's main than ours
                int combatUnitsOnMap = 0;
                auto countType = [&](BWAPI::UnitType type)
                {
                    for (auto &unit: Units::allMineCompletedOfType(type))
                    {
                        if (!unit->exists()) continue;
                        if (!unit->completed) continue;
                        int distOurs = PathFinding::GetGroundDistance(unit->lastPosition,
                                                                      mainBase->getPosition(),
                                                                      unit->type,
                                                                      PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
                        int distTheirs = PathFinding::GetGroundDistance(unit->lastPosition,
                                                                        enemyMain->getPosition(),
                                                                        unit->type,
                                                                        PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
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
                    setGameValue("sneakAttack", currentFrame);
                }
            }
        }
    }

    void gameEnd(bool isWinner)
    {
        currentGame["won"] = isWinner;

        previousGames.push_back(currentGame);

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

        Log::Get() << "Enemy identified as " << BWAPI::Broodwar->enemy()->getRace();
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
            CherryVis::log() << "Set game value " << key << " to " << value;
        }
#endif
#if LOGGING_ENABLED
        if (currentGame[key] != value)
        {
            Log::Get() << "Set game value " << key << " to " << value;
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
        CherryVis::log() << "Incremented game value " << key << " from " << currentValue << " to " << currentGame[key];
#endif
#if LOGGING_ENABLED
        Log::Get() << "Incremented game value " << key << " from " << currentValue << " to " << currentGame[key];
#endif
        setKeys.insert(key);
    }

    bool isGameValueSet(const std::string &key)
    {
        return setKeys.contains(key);
    }

    void addMyStrategyChange(const std::string &strategy)
    {
        currentGame["myStrategy"].push_back(nlohmann::json::array({currentFrame, strategy}));
    }

    void addEnemyStrategyChange(const std::string &strategy)
    {
        currentGame["enemyStrategy"].push_back(nlohmann::json::array({currentFrame, strategy}));
    }

    int minValueInPreviousGames(const std::string &key, int defaultNoData, int maxCount, int minCount)
    {
        if (previousGames.size() < minCount) return defaultNoData;

        int result = INT_MAX;

        int count = 0;
        for (auto &previousGame : std::ranges::reverse_view(previousGames))
        {
            if (count >= maxCount) break;

            try
            {
                auto valIt = previousGame.find(key);

                // If we hit a record where this data point isn't recorded, this means it was played by a previous version of the bot
                // Apply the minimum game count restriction and return
                if (valIt == previousGame.end())
                {
                    if (count < minCount) return defaultNoData;
                    return result;
                }

                result = std::min(result, valIt->get<int>());
            }
            catch (std::exception &ex)
            {
                // Just skip this game result
            }

            count++;
        }

        return result;
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
                // Just skip this game result
            }

            count++;
        }

        if (wins == 0 && losses == 0) return defaultValue;
        return (double)wins / (double)(wins + losses);
    }

    std::string selectOpeningUCB1(const std::vector<std::string> &openings, double decayFactor)
    {
        // Gather wins, losses, reward, and potential for previous games
        // To account for the opponent changing strategies, we weight recent results above older ones using the decay factor
        // We also give a bonus weighting to results on the same map
        std::map<std::string, std::tuple<int, int, double, double>> resultsByOpening;
        double totalPotential = 0.0;
        int count = 0;
        for (auto &previousGame : std::ranges::reverse_view(previousGames))
        {
            try
            {
                auto wonIt = previousGame.find("won");
                if (wonIt == previousGame.end()) continue;
                auto won = wonIt->get<bool>();

                auto myStrategiesIt = previousGame.find("myStrategy");
                if (myStrategiesIt == previousGame.end()) continue;
                auto initialStrategy = (*myStrategiesIt)[0];
                if (initialStrategy[0] != 0) continue;

                auto &[wins, losses, reward, potential] = resultsByOpening[initialStrategy[1]];

                double thisPotential = (isPreviousGameOnThisMap(previousGame) ? 2.0 : 1.0);
                thisPotential *= std::exp(decayFactor * -1 * (count + 1));
                potential += thisPotential;
                totalPotential += thisPotential;
                if (won)
                {
                    wins++;
                    reward += thisPotential;
                }
                else
                {
                    losses++;
                }
            }
            catch (std::exception &ex)
            {
                // Just skip this game result
            }

            count++;
        }

#if LOGGING_ENABLED
        Log::Get() << "Previous opening results for past " << count << " games:";
        for (const auto &[opening, results] : resultsByOpening)
        {
            auto &[wins, losses, reward, potential] = results;
            Log::Get() << opening << ": " << wins << " won " << losses << " lost"
                << "; weighted result " << std::fixed << std::setprecision(1) << (100.0 * reward / potential) << "%";
        }
#endif

        // If there is one, select the first opening that has either never lost or hasn't been attempted
        for (const auto &opening : openings)
        {
            auto resultsIt = resultsByOpening.find(opening);
            if (resultsIt == resultsByOpening.end()) return opening;

            auto &[wins, losses, reward, potential] = resultsIt->second;
            if (losses == 0) return opening;
        }

        // Run UCB1 on each opening
        double bestScore = 0.0;
        std::string bestOpening;
        for (const auto &opening : openings)
        {
            auto &[wins, losses, reward, potential] = resultsByOpening[opening];

            double score = (reward / potential) + std::sqrt(2.0 * std::log(totalPotential) / potential);

            if (score > bestScore)
            {
                bestScore = score;
                bestOpening = opening;
            }
        }

        return bestOpening;
    }
}
