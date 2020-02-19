#include "CherryVis.h"

#include <BWAPI.h>
#include <nlohmann/json.hpp>
#include <utility>
#include <zstdstream/zstdstream.hpp>
#include <filesystem>

namespace CherryVis
{
    namespace
    {
#if CHERRYVIS_ENABLED

        struct HeatmapFilePart
        {
            int firstFrame;
            std::string filename;

            HeatmapFilePart(int firstFrame, std::string filename) : firstFrame(firstFrame), filename(std::move(filename)) {}
        };

        struct HeatmapFile
        {
            std::vector<HeatmapFilePart> parts;

            HeatmapFile(std::string heatmapName, int size) : heatmapName(std::move(heatmapName)), size(size), count(0), stream(nullptr) {}

            void writeFrameData(nlohmann::json &frameData)
            {
                if (count * size >= 26214400)
                {
                    close();
                    count = 0;
                }

                if (count == 0)
                {
                    createPart();
                }
                else
                {
                    (*stream) << ",";
                }

                count++;

                (*stream)
                        << "\"" << BWAPI::Broodwar->getFrameCount() << "\":"
                        << frameData.dump(-1, ' ', true);
            }

            void close()
            {
                (*stream) << "}";
                stream->close();
                delete stream;
            }

        private:
            std::string heatmapName;
            int size;
            int count;
            zstd::ofstream *stream{};

            void createPart()
            {
                std::ostringstream filenameBuilder;
                filenameBuilder << "heatmap_" << heatmapName << "_" << BWAPI::Broodwar->getFrameCount() << ".json.zstd";

                parts.emplace_back(BWAPI::Broodwar->getFrameCount(), filenameBuilder.str());

                stream = new zstd::ofstream("bwapi-data/write/cvis/" + filenameBuilder.str());
                (*stream) << "{";
            }
        };

        nlohmann::json boardUpdates = nlohmann::json::object();
        nlohmann::json frameBoardUpdates = nlohmann::json::object();
        bool frameHasBoardUpdates = false;
        std::unordered_map<std::string, std::string> boardKeyToLastValue;
        std::unordered_map<std::string, size_t> boardListToLastCount;

        std::unordered_map<std::string, std::vector<nlohmann::json>> frameToUnitsFirstSeen;
        std::unordered_map<std::string, std::unordered_map<std::string, nlohmann::json>> unitIdToFrameToUnitUpdate;

        std::vector<nlohmann::json> logEntries;
        std::vector<std::string> frameLogMessages;

        std::unordered_map<std::string, std::vector<nlohmann::json>> unitIdToLogEntries;
        std::vector<std::pair<int, std::string>> frameUnitLogMessages;

        std::unordered_map<std::string, HeatmapFile> heatmapNameToHeatmapFile;

        void log(const std::string &str, int unitId)
        {
            if (unitId == -1)
            {
                frameLogMessages.push_back(str);
            }
            else
            {
                frameUnitLogMessages.emplace_back(unitId, str);
            }
        }

#endif
    }

#if CHERRYVIS_ENABLED

    LogWrapper::LogWrapper(int unitId)
            : os(new std::ostringstream)
            , refCount(new int(1))
            , unitId(unitId)
    {
    }

    LogWrapper::LogWrapper(const LogWrapper &other)
            : os(other.os)
            , refCount(other.refCount)
            , unitId(other.unitId)
    {
        ++*refCount;
    }

    LogWrapper::~LogWrapper()
    {
        --*refCount;

        if (*refCount == 0)
        {
            log(os->str(), unitId);
            delete os;
            delete refCount;
        }
    }

#else

    LogWrapper::LogWrapper(int unitId) {}

    LogWrapper::LogWrapper(const LogWrapper &other) {}

    LogWrapper::~LogWrapper() {}

#endif

    void initialize()
    {
#if CHERRYVIS_ENABLED
        boardUpdates = nlohmann::json::object();
        frameBoardUpdates = nlohmann::json::object();
        frameHasBoardUpdates = false;
        boardKeyToLastValue.clear();
        boardListToLastCount.clear();
        frameToUnitsFirstSeen.clear();
        unitIdToFrameToUnitUpdate.clear();
        logEntries.clear();
        frameLogMessages.clear();
        unitIdToLogEntries.clear();
        frameUnitLogMessages.clear();
        heatmapNameToHeatmapFile.clear();

        std::filesystem::create_directories("bwapi-data/write/cvis");
#endif
    }

    void setBoardValue(const std::string &key, const std::string &value)
    {
#if CHERRYVIS_ENABLED
        if (boardKeyToLastValue.find(key) == boardKeyToLastValue.end() ||
            boardKeyToLastValue[key] != value)
        {
            boardKeyToLastValue.insert_or_assign(key, value);
            frameBoardUpdates[key] = value;
            frameHasBoardUpdates = true;
        }
#endif
    }

    void setBoardListValue(const std::string &key, std::vector<std::string> &values)
    {
#if CHERRYVIS_ENABLED
        size_t limit = std::max(boardListToLastCount[key], values.size());
        for (size_t i = 1; i <= limit; i++)
        {
            std::ostringstream valueKey;
            valueKey << key << "_" << std::setfill('0') << std::setw(3) << i;

            setBoardValue(valueKey.str(), (i <= values.size() ? values.at(i - 1) : ""));
        }

        boardListToLastCount[key] = values.size();
#endif
    }

    void unitFirstSeen(BWAPI::Unit unit)
    {
#if CHERRYVIS_ENABLED
        int frame = BWAPI::Broodwar->getFrameCount();
        if (frame == 0) frame = 1;

        frameToUnitsFirstSeen[std::to_string(frame)].push_back({
                                                                       {"id",   unit->getID()},
                                                                       {"type", unit->getType().getID()},
                                                                       {"x",    unit->getPosition().x},
                                                                       {"y",    unit->getPosition().y}
                                                               });

        unitIdToFrameToUnitUpdate[std::to_string(unit->getID())][std::to_string(frame)] = {
                {"type", unit->getType().getID()}
        };
#endif
    }

    LogWrapper log(int unitId)
    {
        return LogWrapper(unitId);
    }

    void addHeatmap(const std::string &key, const std::vector<long> &data, int sizeX, int sizeY)
    {
#if CHERRYVIS_ENABLED
        long max = 0;
        long min = LONG_MAX;
        long long sum = 0;
        for (long val : data)
        {
            max = std::max(max, val);
            min = std::min(min, val);
            sum += val;
        }

        double mean = (double) sum / data.size();
        double variance = 0.0;
        for (long val : data)
        {
            double diff = val - mean;
            variance += diff * diff;
        }
        double stddev = std::sqrt(variance / data.size());

        nlohmann::json frameData = {
                {"data",           data},
                {"dimension",      {sizeY,                                         sizeX}},
                {"scaling",        {(BWAPI::Broodwar->mapHeight() * 32.0) / sizeY, (BWAPI::Broodwar->mapWidth() * 32.0) / sizeX}},
                {"top_left_pixel", {0,                                             0}},
                {"summary",        {
                                    {"hist", {
                                                     {"max", max},
                                                     {"min", min},
                                                     {"num_buckets", 1},
                                                     {"values", {0}},
                                             }},
                                                                                   {"max", max},
                                           {"min", min},
                                           {"mean", mean},
                                           {"median", 0},
                                           {"name", key},
                                           {"shape", {sizeY, sizeX}},
                                           {"std", stddev}
                                   }}
        };

        auto heatmap = heatmapNameToHeatmapFile.try_emplace(key, key, sizeX * sizeY);
        heatmap.first->second.writeFrameData(frameData);
#endif
    }

    void frameEnd(int frame)
    {
#if CHERRYVIS_ENABLED
        if (frameHasBoardUpdates)
        {
            boardUpdates[std::to_string(frame)] = std::move(frameBoardUpdates);
            frameBoardUpdates = nlohmann::json::object();
            frameHasBoardUpdates = false;
        }

        if (!frameLogMessages.empty())
        {
            for (const auto &msg : frameLogMessages)
            {
                logEntries.push_back({
                                             {"message", msg},
                                             {"frame",   frame}
                                     });
            }

            frameLogMessages.clear();
        }

        if (!frameUnitLogMessages.empty())
        {
            for (const auto &unitIdAndMsg : frameUnitLogMessages)
            {
                unitIdToLogEntries[std::to_string(unitIdAndMsg.first)].push_back({
                                                                                         {"message", unitIdAndMsg.second},
                                                                                         {"frame",   frame}
                                                                                 });
            }

            frameUnitLogMessages.clear();
        }
#endif
    }

    void gameEnd()
    {
#if CHERRYVIS_ENABLED
        std::vector<nlohmann::json> heatmaps;
        for (auto heatmapNameAndHeatmapFile : heatmapNameToHeatmapFile)
        {
            heatmapNameAndHeatmapFile.second.close();

            for (const auto &part : heatmapNameAndHeatmapFile.second.parts)
            {
                std::ostringstream name;
                name << heatmapNameAndHeatmapFile.first << "_" << part.firstFrame;

                heatmaps.push_back({
                                           {"filename",    part.filename},
                                           {"first_frame", part.firstFrame},
                                           {"name",        name.str()}
                                   });
            }
        }

        std::unordered_map<std::string, std::string> buildTypesToName;
        for (auto type : BWAPI::UnitTypes::allUnitTypes())
        {
            buildTypesToName[std::to_string(type.getID())] = type.getName();
        }

        nlohmann::json trace = {
                {"types_names",      buildTypesToName},
                {"board_updates",    boardUpdates},
                {"units_first_seen", frameToUnitsFirstSeen},
                {"units_updates",    unitIdToFrameToUnitUpdate},
                {"logs",             logEntries},
                {"units_logs",       unitIdToLogEntries},
                {"heatmaps",         heatmaps}
        };

        zstd::ofstream traceFile("bwapi-data/write/cvis/trace.json");
        traceFile << trace.dump(-1, ' ', true);
        traceFile.close();
#endif
    }
}
