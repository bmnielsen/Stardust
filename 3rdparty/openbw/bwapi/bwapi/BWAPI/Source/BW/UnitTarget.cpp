#include "UnitTarget.h"

#include <BWAPI/UnitImpl.h>
#include <BWAPI/GameImpl.h>

#include "../../../Debug.h"

namespace BW
{
  //---------------------------------------------- CONSTRUCTOR -----------------------------------------------
  UnitTarget::UnitTarget() {}

  UnitTarget::UnitTarget(BWAPI::Unit target)
  {
    this->targetID = static_cast<BWAPI::UnitImpl*>(target)->bwunit.getUnitID();
  }
  UnitTarget::UnitTarget(BW::Unit target)
  {
    this->targetID = target.getUnitID();
  }
  //-------------------------------------------------- GETTARGET ---------------------------------------------
  u16 UnitTarget::getTarget() const
  {
    return this->targetID;
  }
  //----------------------------------------------------------------------------------------------------------

}
