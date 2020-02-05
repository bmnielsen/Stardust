#include <BWAPI/Regionset.h>
#include <BWAPI/Region.h>
#include <BWAPI/Unitset.h>

#include <utility>

bool std::less<BWAPI::Region>::operator()(const BWAPI::Region &a, const BWAPI::Region &b) const
{
    return a == nullptr || (b != nullptr && a->getID() < b->getID());
}

namespace BWAPI
{
  Position Regionset::getCenter() const
  {
    Position sum(0,0); // The sum of all positions
    int count = 0; // The number of valid positions

    for ( auto &r : *this )
    {
      Position p = r->getCenter();
      if ( p )  // Only use if position is valid/known
      {
        sum += p;
        ++count;
      }
    }
    
    if (count > 0)
      sum /= count;

    return sum;
  }

  Unitset Regionset::getUnits(const UnitFilter &pred) const
  {
    Unitset result;
    for (auto &r : *this)
    {
      auto units = r->getUnits(pred);
      result.insert(units.begin(), units.end());
    }
    return result;
  }
}

