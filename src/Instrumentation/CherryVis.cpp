#include "CherryVis.h"

#if CHERRYVIS_ENABLED
#include "Log.h"

#include <utility>
#include <filesystem>

#if INSTRUMENTATION_ENABLED_VERBOSE
#include <zstdstream/zstdstream.hpp>
#define STREAM zstd::ofstream
#define FILE_EXTENSION ".json.zstd"
#else
#include <fstream>
#define STREAM std::ofstream
#define FILE_EXTENSION ".json"
#endif
#endif

namespace CherryVis
{
    namespace
    {
#if CHERRYVIS_ENABLED

        bool disabled = false;

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
            explicit DataFile(std::string filename, DataFileType type, int partitionedObjectSize = 0, int framesPerPartition = 0)
            : filename(std::move(filename))
            , type(type)
            , lastFrame(-1)
            , partitionedObjectSize(partitionedObjectSize)
            , framesPerPartition(framesPerPartition)
            , currentPartition(0)
            , count(0)
            , stream(nullptr)
            {
            }

            std::vector<DataFilePart> parts;

            [[nodiscard]] bool isPartitioned() const
            {
                return partitionedObjectSize > 0 || framesPerPartition > 0;
            }

            [[nodiscard]] std::unordered_map<std::string, std::string> index() const
            {
                std::unordered_map<std::string, std::string> result;
                for (const auto &part : parts)
                {
                    result[std::to_string(part.firstFrame)] = part.filename;
                }
                return result;
            }

            void writeEntry(const nlohmann::json &entry)
            {
                try
                {
                    if (count * partitionedObjectSize >= 26214400)
                    {
                        close();
                    }

                    if (framesPerPartition > 0)
                    {
                        int partition = (BWAPI::Broodwar->getFrameCount() / framesPerPartition) * framesPerPartition;
                        if (partition != currentPartition)
                        {
                            close();
                        }

                        currentPartition = partition;
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
                catch (std::exception &ex)
                {
                    Log::Get() << "Exception caught in DataFile::writeEntry: " << ex.what();
                    disabled = true;
                }
            }

            void close()
            {
                if (count == 0) return;

                try
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
                catch (std::exception &ex)
                {
                    Log::Get() << "Exception caught in DataFile::close: " << ex.what();
                    disabled = true;
                }
            }

            void flush()
            {
                if (count == 0) return;
                stream->flush();
            }

        private:
            std::string filename;
            DataFileType type;
            int lastFrame;
            int partitionedObjectSize;
            int framesPerPartition;
            int currentPartition;
            int count;
            STREAM *stream{};

            void createPart()
            {
                try
                {
                    int startFrame = BWAPI::Broodwar->getFrameCount();
                    std::ostringstream filenameBuilder;
                    filenameBuilder << filename;
                    if (partitionedObjectSize > 0) filenameBuilder << "_" << BWAPI::Broodwar->getFrameCount();
                    if (framesPerPartition > 0)
                    {
                        startFrame = (BWAPI::Broodwar->getFrameCount() / framesPerPartition) * framesPerPartition;
                        filenameBuilder << "_" << startFrame;
                    }
                    filenameBuilder << FILE_EXTENSION;

                    parts.emplace_back(startFrame, filenameBuilder.str());

                    stream = new STREAM("bwapi-data/write/cvis/" + filenameBuilder.str());

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
                catch (std::exception &ex)
                {
                    Log::Get() << "Exception caught in DataFile::createPart: " << ex.what();
                    disabled = true;
                }
            }
        };

        std::unique_ptr<DataFile> boardUpdatesFile;
        nlohmann::json frameBoardUpdates = nlohmann::json::object();
        bool frameHasBoardUpdates = false;
        std::unordered_map<std::string, std::string> boardKeyToLastValue;
        std::unordered_map<std::string, size_t> boardListToLastCount;

#if IS_OPENBW
        std::vector<std::pair<int, int>> unitIds;
#else
        std::unordered_map<std::string, std::vector<nlohmann::json>> frameToUnitsFirstSeen;
#endif
        std::unordered_map<std::string, std::unordered_map<std::string, nlohmann::json>> unitIdToFrameToUnitUpdate;

        std::map<int, DataFile> unitIdToLogFile;
        std::map<int, DataFile> unitIdToDrawCommandsFile;
        std::map<std::string, DataFile> labelToDataFile;

        std::unordered_map<std::string, DataFile> heatmapNameToDataFile;

        void log(const std::string &str, int unitId)
        {
            if (disabled) return;

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
                if (unitId == -1)
                {
                    filenameBuilder << "_all";
                }
                else
                {
                    filenameBuilder << "_" << unitId;
                }
                drawCommandsFileIt = unitIdToDrawCommandsFile.emplace(
                        std::piecewise_construct,
                        std::make_tuple(unitId),
                        std::make_tuple(filenameBuilder.str(), DataFileType::ArrayPerFrame, 0, (unitId != -1) ? 0 : 5000)).first;
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
        if (disabled) return;

        boardUpdatesFile = std::make_unique<DataFile>("board_updates", DataFileType::ObjectPerFrame);
        frameBoardUpdates = nlohmann::json::object();
        frameHasBoardUpdates = false;
        boardKeyToLastValue.clear();
        boardListToLastCount.clear();
#if IS_OPENBW
        unitIds.clear();
#else
        frameToUnitsFirstSeen.clear();
#endif
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
        if (disabled) return;

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
        if (disabled) return;

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
        if (disabled) return;

        int frame = BWAPI::Broodwar->getFrameCount();
        if (frame == 0) frame = 1;

#if IS_OPENBW
        unitIds.emplace_back(unit->getID(), unit->getBWID());
#else
        frameToUnitsFirstSeen[std::to_string(frame)].push_back({
                                                                       {"id",   unit->getID()},
                                                                       {"type", unit->getType().getID()},
                                                                       {"x",    unit->getPosition().x},
                                                                       {"y",    unit->getPosition().y}
                                                               });

#endif

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
        if (disabled) return;

        long max = 0;
        long min = LONG_MAX;
        long long sum = 0;
        for (long val : data)
        {
            max = std::max(max, val);
            min = std::min(min, val);
            sum += val;
        }

        double mean = (double)sum / (double)data.size();
        double variance = 0.0;
        for (long val : data)
        {
            double diff = (double)val - mean;
            variance += diff * diff;
        }
        double stddev = std::sqrt(variance / (double)data.size());

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
        if (disabled) return;

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
        if (disabled) return;

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
        if (disabled) return;

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
        if (disabled) return;

        if (frameHasBoardUpdates)
        {
            boardUpdatesFile->writeEntry(frameBoardUpdates);
            frameBoardUpdates = nlohmann::json::object();
            frameHasBoardUpdates = false;
        }

        // Flush data files every 500 frames
        if (frame % 500 == 0)
        {
            for (auto &[heatmapName, heatmapFile] : heatmapNameToDataFile)
            {
                heatmapFile.flush();
            }

            boardUpdatesFile->flush();

            for (auto &[unitId, drawCommandsFile] : unitIdToDrawCommandsFile)
            {
                drawCommandsFile.flush();
            }

            for (auto &[unitId, logFile] : unitIdToLogFile)
            {
                logFile.flush();
            }

            for (auto &[label, dataFile] : labelToDataFile)
            {
                dataFile.flush();
            }
        }
#endif
    }

    void gameEnd()
    {
#if CHERRYVIS_ENABLED
        if (disabled) return;

        std::vector<nlohmann::json> heatmaps;
        for (auto &heatmapNameAndDataFile : heatmapNameToDataFile)
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

        nlohmann::json trace = {
                {"units_updates",    unitIdToFrameToUnitUpdate},
                {"units_logs",       nlohmann::json::object()},
                {"units_draw",       nlohmann::json::object()},
                {"heatmaps",         heatmaps}
        };

        boardUpdatesFile->close();
        if (boardUpdatesFile->parts.empty())
        {
            trace["board_updates"] = "";
        }
        else
        {
            trace["board_updates"] = boardUpdatesFile->parts[0].filename;
        }

#if IS_OPENBW
        trace["unit_ids"] = unitIds;
#else
        trace["units_first_seen"] = frameToUnitsFirstSeen;
#endif

        if (unitIdToDrawCommandsFile.find(-1) == unitIdToDrawCommandsFile.end())
        {
            trace["draw_commands"] = nlohmann::json::array();
        }

        std::unordered_map<std::string, std::string> unitIdToDrawCommandsFilePath;
        for (auto &drawCommandsFile : unitIdToDrawCommandsFile)
        {
            if (drawCommandsFile.first == -1)
            {
                trace["draw_commands"] = drawCommandsFile.second.index();
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

        for (auto &[label, dataFile] : labelToDataFile)
        {
            if (dataFile.isPartitioned())
            {
                trace[label] = dataFile.index();
            }
            else
            {
                trace[label] = dataFile.parts[0].filename;
            }

            dataFile.close();
        }

        try
        {
            STREAM traceFile((std::ostringstream() << "bwapi-data/write/cvis/trace" << FILE_EXTENSION).str());
            traceFile << trace.dump(-1, ' ', true);
            traceFile.close();
        }
        catch (std::exception &ex)
        {
            Log::Get() << "Exception caught writing trace file: " << ex.what();
        }
#endif
    }
}

void CherryVis::disable()
{
#if CHERRYVIS_ENABLED
    disabled = true;
#endif
}

void CherryVis::writeFrameData(const std::string &label, const nlohmann::json &entry, int framesPerPartition)
{
#if CHERRYVIS_ENABLED
    if (disabled) return;

    auto fileIt = labelToDataFile.find(label);
    if (fileIt == labelToDataFile.end())
    {
        fileIt = labelToDataFile.emplace(
                std::piecewise_construct,
                std::make_tuple(label),
                std::make_tuple(label, DataFileType::ObjectPerFrame, 0, framesPerPartition)).first;
    }

    fileIt->second.writeEntry(entry);
#endif
}
