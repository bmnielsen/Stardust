#include "MyCarrier.h"

MyCarrier::MyCarrier(BWAPI::Unit unit) : MyUnitImpl(unit)
{
}

void MyCarrier::update(BWAPI::Unit unit)
{
    if (!unit || !unit->exists()) return;

    MyUnitImpl::update(unit);

    // Build interceptors as needed
    if (!unit->isTraining() && unit->canTrain(BWAPI::UnitTypes::Protoss_Interceptor))
    {
        unit->train(BWAPI::UnitTypes::Protoss_Interceptor);
    }
}
