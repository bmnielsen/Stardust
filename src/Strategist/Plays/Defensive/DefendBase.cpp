#include "DefendBase.h"

#include "Map.h"
#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Workers.h"

/*
 * General approach:
 *
 * - If the mineral line is under attack, do worker defense
 * - Otherwise, determine the level of risk to the base.
 * - If the base cannot be saved, evacuate workers
 * - If high risk (enemy units are here or expected to come soon), reserve some units for protection
 * - If at medium risk, get some static defense
 * - If at low risk, do nothing
 */

DefendBase::DefendBase(Base *base) : base(base), squad(std::make_shared<DefendBaseSquad>(base))
{
    General::addSquad(squad);
}

void DefendBase::update()
{
    // TODO
}
