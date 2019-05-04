#include "PositionUnitTarget.h"

#include "../../../Debug.h"

#include <BWAPI/GameImpl.h>

namespace BW
{
  //---------------------------------------------- CONSTRUCTOR -----------------------------------------------
  PositionUnitTarget::PositionUnitTarget(const Position& position)
      : position(position)
  {
  }
  PositionUnitTarget::PositionUnitTarget(int x, int y)
    : position(static_cast<s16>(x), static_cast<s16>(y))
  {
  }
  //---------------------------------------------- CONSTRUCTOR -----------------------------------------------
  PositionUnitTarget::PositionUnitTarget(const UnitTarget& target)
      : target(target)
  {
    size_t index = (target.getTarget() & 0x7FF) - 1;
    this->position = BWAPI::BroodwarImpl.bwgame.getUnit(index).position();
  }
  //----------------------------------------------------------------------------------------------------------
}

