#include "Opponent.h"

#include <fstream>
#include <filesystem>

#include <nlohmann/json.hpp>

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

    bool isGameValueSet(const std::string &key)
    {
        return setKeys.find(key) != setKeys.end();
    }

    int minValueInPreviousGames(const std::string &key, int defaultNoData, int maxCount, int minCount)
    {
        if (previousGames.size() < minCount) return defaultNoData;

        int result = INT_MAX;

        int count = 0;
        for (auto it = previousGames.rbegin(); it != previousGames.rend(); it++)
        {
            if (count >= maxCount) break;

            try
            {
                auto valIt = it->find(key);

                // If we hit a record where this data point isn't recorded, this means it was played by a previous version of the bot
                // Apply the minimum game count restriction and return
                if (valIt == it->end())
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
        for (auto it = previousGames.rbegin(); it != previousGames.rend(); it++)
        {
            if (count >= maxCount) break;

            try
            {
                auto valIt = it->find("won");
                if (valIt != it->end())
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
}
