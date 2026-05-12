#include "BWTest.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"
#include "MiningOptimizationTraining/DataTransformation/DataTransformer.h"

using namespace MiningOptimizationTraining;

namespace
{
    void transform(const std::string &mapHash)
    {
        MapData data;
        InitialWorkerMapData initialWorkerData;

        Serialization::setGameParameters(mapHash);
        std::cout << "Loading path data for " << mapHash << "..." << std::endl;
        Serialization::readMapData(data);
        std::cout << "Loading initial split data for " << mapHash << "..." << std::endl;
        Serialization::readMapData(initialWorkerData);

        DataTransformer::transform(data, initialWorkerData);
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
