#pragma once
#include <string>
#include <BWAPI/Force.h>
#include <BWAPI/Playerset.h>

#ifdef COMPAT
#include "CompatGameImpl.h"
#endif

namespace BWAPI
{
  // Forwards
  class PlayerInterface;
  typedef PlayerInterface *Player;

  class ForceImpl : public ForceInterface
  {
    public:
      ForceImpl(std::string const & name);
      virtual int getID() const override;
      virtual std::string getName() const override;
      virtual Playerset getPlayers() const override;
      Playerset players;
      std::string name;
      void setID(int newID);
      
#ifdef COMPAT
      CompatForceImpl compatForceImpl{this};
#endif
    private:
      int id = -1;
  };
};
