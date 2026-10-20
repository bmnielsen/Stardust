#include "MyCarrier.h"

MyCarrier::MyCarrier(BWAPI::Unit gameUnit) : MyUnitImpl(gameUnit)
{
}

void MyCarrier::update(BWAPI::Unit gameUnit)
{
    if (!gameUnit || !gameUnit->exists()) return;

    MyUnitImpl::update(gameUnit);

    /* Build interceptors as needed */
    if (!gameUnit->isTraining() && gameUnit->canTrain(BWAPI::UnitTypes::Protoss_Interceptor))
    {
        gameUnit->train(BWAPI::UnitTypes::Protoss_Interceptor);
    }
}
