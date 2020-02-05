#include <BWAPI/Forceset.h>
#include <BWAPI/Force.h>
#include <BWAPI/Playerset.h>

#include <utility>

bool std::less<BWAPI::Force>::operator()(const BWAPI::Force &a, const BWAPI::Force &b) const
{
    return a == nullptr || (b != nullptr && a->getID() < b->getID());
}

namespace BWAPI
{
  Playerset Forceset::getPlayers() const
  {
    Playerset rset;
    for (auto &f : *this)
    {
      auto players = f->getPlayers();
      rset.insert(players.begin(), players.end());
    }
    return rset;
  }
}

