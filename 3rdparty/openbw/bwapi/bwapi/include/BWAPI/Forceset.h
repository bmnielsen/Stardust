#pragma once
#include "SetContainer.h"

namespace BWAPI
{
    // Forward Declarations
    class ForceInterface;
    typedef ForceInterface *Force;
    class Playerset;
}

namespace std
{
    template<>
    struct less<BWAPI::Force>{
        bool operator()(const BWAPI::Force& a, const BWAPI::Force& b) const;
    };
}

namespace BWAPI
{
  /// <summary>A container that holds a group of Forces.</summary>
  ///
  /// @see BWAPI::Force
  class Forceset : public SetContainer<BWAPI::Force, std::hash<void*>>
  {
  public:

    /// @copydoc ForceInterface::getPlayers
    Playerset getPlayers() const;
  };
}

