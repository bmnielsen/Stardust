#include <filesystem>

#include "BWTest.h"
#include "FileTools.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"
#include "MiningOptimizationTraining/DataTransformation/DataTransformer.h"

using namespace MiningOptimizationTraining;

#define EXPORT_PATH "/Users/bmnielsen/BW/mining-data/dist/"

namespace
{
    void transform(const std::string &mapHash)
    {
        Log::initialize();
        Log::SetOutputToConsole(true);

        MapData data;
        InitialWorkerMapData initialWorkerData;

        Serialization::setGameParameters(mapHash);
        std::cout << "Loading path data for " << mapHash << "..." << std::endl;
        Serialization::readMapData(data);
        std::cout << "Loading initial split data for " << mapHash << "..." << std::endl;
        Serialization::readMapData(initialWorkerData);

        DataTransformer::transform(data, initialWorkerData);
    }

    void exportDataFile(const std::string &openbwMapHash, const std::string &mapHash)
    {
        std::ostringstream sourceFilenameBuilder;
        sourceFilenameBuilder << "mining-optimization";
        sourceFilenameBuilder << "_" << openbwMapHash;
        auto sourceFilename = FileTools::getFilePath(sourceFilenameBuilder.str(), "bin.zstd", false);

        std::ostringstream destinationFilenameBuilder;
        destinationFilenameBuilder << EXPORT_PATH;
        destinationFilenameBuilder << "mining-optimization";
        destinationFilenameBuilder << "_" << mapHash;
        destinationFilenameBuilder << ".bin.zstd";
        auto destinationFilename = destinationFilenameBuilder.str();

        std::filesystem::copy(sourceFilename, destinationFilename, std::filesystem::copy_options::overwrite_existing);
        std::cout << "Copied " << sourceFilename << " to " << destinationFilename << std::endl;
    }
}

TEST(DataTransformation, Vermeer)
{
    transform(Maps::GetOne("Vermeer")->openbwHash);
}

TEST(DataTransformation, Benzene)
{
    transform(Maps::GetOne("Benzene")->openbwHash);
}

TEST(DataTransformation, Destination)
{
    transform(Maps::GetOne("sscai/(2)Destination")->openbwHash);
}

TEST(DataTransformation, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        transform(test.map->openbwHash);
    });
}

TEST(ExportDataFiles, Benzene)
{
    auto map = Maps::GetOne("Benzene");
    exportDataFile(map->openbwHash, map->hash);
}

TEST(ExportDataFiles, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        exportDataFile(test.map->openbwHash, test.map->hash);
    });
}
