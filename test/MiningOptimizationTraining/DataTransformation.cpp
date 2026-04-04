#include "BWTest.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"
#include "MiningOptimizationTraining/DataTransformation/DataTransformer.h"

using namespace MiningOptimizationTraining;

namespace
{
    void transform(const std::string &mapHash)
    {
        MapData data;

        Serialization::setGameParameters(mapHash);
        Serialization::readMapData(data);

        DataTransformer::transform(data);
    }
}

TEST(DataTransformation, Vermeer)
{
    transform(Maps::GetOne("Vermeer")->openbwHash);
}

TEST(DataTransformation, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        transform(test.map->openbwHash);
    });
}
