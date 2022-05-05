#include "CherryVis.h"

#if CHERRYVIS_ENABLED
#include <utility>
#include <zstdstream/zstdstream.hpp>
#include <filesystem>
#endif

namespace CherryVis
{
    namespace
    {
#if CHERRYVIS_ENABLED

        enum DataFileType {
            Array,
            ArrayPerFrame,
            ObjectPerFrame
        };

        struct DataFilePart
        {
            int firstFrame;
            std::string filename;

            DataFilePart(int firstFrame, std::string filename) : firstFrame(firstFrame), filename(std::move(filename))
            {}
        };

        struct DataFile
        {
            explicit DataFile(std::string filename, DataFileType type, int partitionedObjectSize = 0)
            : filename(std::move(filename))
            , type(type)
            , lastFrame(-1)
            , partitionedObjectSize(partitionedObjectSize)
            , count(0)
            , stream(nullptr)
            {
            }

            std::vector<DataFilePart> parts;

            void writeEntry(const nlohmann::json &entry)
            {
                if (count * partitionedObjectSize >= 26214400)
                {
                    close();
                }

                if (count == 0)
                {
                    createPart();
                }
                else
                {
                    switch (type)
                    {
                        case Array:
                            (*stream) << ",";
                            break;
                        case ArrayPerFrame:
                            if (lastFrame == BWAPI::Broodwar->getFrameCount())
                            {
                                (*stream) << ",";
                            }
                            else
                            {
                                (*stream) << "],\"" << BWAPI::Broodwar->getFrameCount() << "\":[";
                                lastFrame = BWAPI::Broodwar->getFrameCount();
                            }
                            break;
                        case ObjectPerFrame:
                            (*stream) << ",\"" << BWAPI::Broodwar->getFrameCount() << "\":";
                            break;
                    }
                }

                count++;

                // For some reason this sometimes throws a resource unavailable exception, so retry if that happens
                while (true)
                {
                    try
                    {
                        errno = 0;
                        (*stream) << entry;
                        break;
                    }
                    catch (...)
                    {
                        if (errno == 0x23) continue;
                        throw;
                    }
                }
            }

            void close()
            {
                switch (type)
                {
                    case Array:
                        (*stream) << "]";
                        break;
                    case ArrayPerFrame:
                        if (lastFrame != -1)
                        {
                            (*stream) << "]}";
                        }
                        else
                        {
                            (*stream) << "}";
                        }
                        break;
                    case ObjectPerFrame:
                        (*stream) << "}";
                        break;
                }
                stream->close();
                delete stream;
                count = 0;
            }

        private:
            std::string filename;
            DataFileType type;
            int lastFrame;
            int partitionedObjectSize;
            int count;
            zstd::ofstream *stream{};

            void createPart()
            {
                std::ostringstream filenameBuilder;
                filenameBuilder << filename;
                if (partitionedObjectSize > 0) filenameBuilder << "_" << BWAPI::Broodwar->getFrameCount();
                filenameBuilder << ".json.zstd";

                parts.emplace_back(BWAPI::Broodwar->getFrameCount(), filenameBuilder.str());

                stream = new zstd::ofstream("bwapi-data/write/cvis/" + filenameBuilder.str());

                switch (type)
                {
                    case Array:
                        (*stream) << "[";
                        break;
                    case ArrayPerFrame:
                        (*stream) << "{\"" << BWAPI::Broodwar->getFrameCount() << "\":[";
                        break;
                    case ObjectPerFrame:
                        (*stream) << "{\"" << BWAPI::Broodwar->getFrameCount() << "\":";
                        break;
                }
            }
        };

        nlohmann::json boardUpdates = nlohmann::json::object();
        nlohmann::json frameBoardUpdates = nlohmann::json::object();
        bool frameHasBoardUpdates = false;
        std::unordered_map<std::string, std::string> boardKeyToLastValue;
        std::unordered_map<std::string, size_t> boardListToLastCount;

        std::unordered_map<std::string, std::vector<nlohmann::json>> frameToUnitsFirstSeen;
        std::unordered_map<std::string, std::unordered_map<std::string, nlohmann::json>> unitIdToFrameToUnitUpdate;

        std::map<int, DataFile> unitIdToLogFile;
        std::map<int, DataFile> unitIdToDrawCommandsFile;
        std::map<std::string, DataFile> labelToDataFile;

        std::unordered_map<std::string, DataFile> heatmapNameToDataFile;

        bool disabled = false;

        void log(const std::string &str, int unitId)
        {
            auto logFileIt = unitIdToLogFile.find(unitId);
            if (logFileIt == unitIdToLogFile.end())
            {
                std::ostringstream filenameBuilder;
                filenameBuilder << "logs";
                if (unitId != -1) filenameBuilder << "_" << unitId;
                logFileIt = unitIdToLogFile.emplace(
                        std::piecewise_construct,
                        std::make_tuple(unitId),
                        std::make_tuple(filenameBuilder.str(), DataFileType::Array)).first;
            }

            logFileIt->second.writeEntry({
                                                 {"message", str},
                                                 {"frame",   BWAPI::Broodwar->getFrameCount()}
                                         });
        }

        void draw(const nlohmann::json &drawCommand, int unitId)
        {
            auto drawCommandsFileIt = unitIdToDrawCommandsFile.find(unitId);
            if (drawCommandsFileIt == unitIdToDrawCommandsFile.end())
            {
                std::ostringstream filenameBuilder;
                filenameBuilder << "drawCommands";
                if (unitId != -1) filenameBuilder << "_" << unitId;
                drawCommandsFileIt = unitIdToDrawCommandsFile.emplace(
                        std::piecewise_construct,
                        std::make_tuple(unitId),
                        std::make_tuple(filenameBuilder.str(), DataFileType::ArrayPerFrame)).first;
            }

            drawCommandsFileIt->second.writeEntry(drawCommand);
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
        unitIdToLogFile.clear();
        unitIdToDrawCommandsFile.clear();
        labelToDataFile.clear();
        heatmapNameToDataFile.clear();

        std::filesystem::create_directories("bwapi-data/write/cvis");

        // Mark all static neutrals as seen
        for (auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            unitFirstSeen(unit);
        }
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

        double mean = (double)sum / data.size();
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

        std::ostringstream filenameBuilder;
        filenameBuilder << "heatmap_" << key;
        auto heatmap = heatmapNameToDataFile.try_emplace(key, filenameBuilder.str(), DataFileType::ObjectPerFrame, sizeX * sizeY);
        heatmap.first->second.writeEntry(frameData);
#endif
    }

    void drawLine(int x1, int y1, int x2, int y2, DrawColor color, int unitId)
    {
#if CHERRYVIS_ENABLED
        draw({
                 {"code", 20},
                 {"args", nlohmann::json::array({x1, y1, x2, y2, (int)color})},
                 {"str",  "."}
             }, unitId);
#endif
    }

    void drawCircle(int x, int y, int radius, DrawColor color, int unitId)
    {
#if CHERRYVIS_ENABLED
        draw({
                 {"code", 23},
                 {"args", nlohmann::json::array({x, y, radius, (int)color})},
                 {"str",  "."}
             }, unitId);
#endif
    }

    void drawText(int x, int y, const std::string &text, int unitId)
    {
#if CHERRYVIS_ENABLED
        draw({
                 {"code", 25},
                 {"args", nlohmann::json::array({x, y})},
                 {"str",  text}
             }, unitId);
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
#endif
    }

    void gameEnd()
    {
#if CHERRYVIS_ENABLED
        if (disabled) return;

        std::vector<nlohmann::json> heatmaps;
        for (auto heatmapNameAndDataFile : heatmapNameToDataFile)
        {
            heatmapNameAndDataFile.second.close();

            for (const auto &part : heatmapNameAndDataFile.second.parts)
            {
                std::ostringstream name;
                name << heatmapNameAndDataFile.first << "_" << part.firstFrame;

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

        std::unordered_map<std::string, std::string> orderTypesToName;
        for (auto type : BWAPI::Orders::allOrders())
        {
            orderTypesToName[std::to_string(type.getID())] = type.getName();
        }

        nlohmann::json trace = {
                {"types_names",      buildTypesToName},
                {"orders_names",     orderTypesToName},
                {"board_updates",    boardUpdates},
                {"units_first_seen", frameToUnitsFirstSeen},
                {"units_updates",    unitIdToFrameToUnitUpdate},
                {"units_logs",       nlohmann::json::object()},
                {"units_draw",       nlohmann::json::object()},
                {"heatmaps",         heatmaps}
        };

        if (unitIdToDrawCommandsFile.find(-1) == unitIdToDrawCommandsFile.end())
        {
            trace["draw_commands"] = nlohmann::json::array();
        }

        std::unordered_map<std::string, std::string> unitIdToDrawCommandsFilePath;
        for (auto &drawCommandsFile : unitIdToDrawCommandsFile)
        {
            if (drawCommandsFile.first == -1)
            {
                trace["draw_commands"] = drawCommandsFile.second.parts[0].filename;
            }
            else
            {
                unitIdToDrawCommandsFilePath[(std::ostringstream() << drawCommandsFile.first).str()] = drawCommandsFile.second.parts[0].filename;
            }
            drawCommandsFile.second.close();
        }
        trace["units_draw"] = unitIdToDrawCommandsFilePath;

        if (unitIdToLogFile.find(-1) == unitIdToLogFile.end())
        {
            trace["logs"] = nlohmann::json::array();
        }

        std::unordered_map<std::string, std::string> unitIdToLogFilePath;
        for (auto &logFile : unitIdToLogFile)
        {
            if (logFile.first == -1)
            {
                trace["logs"] = logFile.second.parts[0].filename;
            }
            else
            {
                unitIdToLogFilePath[(std::ostringstream() << logFile.first).str()] = logFile.second.parts[0].filename;
            }
            logFile.second.close();
        }
        trace["units_logs"] = unitIdToLogFilePath;

        for (auto &labelAndDataFile : labelToDataFile)
        {
            trace[labelAndDataFile.first] = labelAndDataFile.second.parts[0].filename;
            labelAndDataFile.second.close();
        }

        zstd::ofstream traceFile("bwapi-data/write/cvis/trace.json");
        traceFile << trace.dump(-1, ' ', true);
        traceFile.close();
#endif
    }
}

void CherryVis::disable()
{
#if CHERRYVIS_ENABLED
    disabled = true;
#endif
}

void CherryVis::writeFrameData(const std::string &label, const nlohmann::json &entry)
{
    auto fileIt = labelToDataFile.find(label);
    if (fileIt == labelToDataFile.end())
    {
        fileIt = labelToDataFile.emplace(
                std::piecewise_construct,
                std::make_tuple(label),
                std::make_tuple(label, DataFileType::ObjectPerFrame)).first;
    }

    fileIt->second.writeEntry(entry);
}
