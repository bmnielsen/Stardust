#include "BWTest.h"

#include "MiningOptimizationTraining/DataModel/Serialization.h"
#include "MiningOptimizationTraining/DataTransformation/DataTransformer.h"

namespace
{
    void transform(const std::string &mapHash)
    {
        MiningOptimizationTraining::MapData data;

        MiningOptimizationTraining::Serialization::setGameParameters(mapHash);
        MiningOptimizationTraining::Serialization::readMapData(data);

        MiningOptimizationTraining::DataTransformer::transform(data);
    }
}

TEST(DataTransformation, Vermeer)
{
    transform(Maps::GetOne("Vermeer")->openbwHash);
}
