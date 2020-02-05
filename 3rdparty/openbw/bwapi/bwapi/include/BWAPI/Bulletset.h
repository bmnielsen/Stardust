#pragma once
#include "SetContainer.h"
#include <BWAPI/Bullet.h>

namespace BWAPI
{
    // Forward Declarations
    class BulletInterface;
    typedef BulletInterface *Bullet;
}

namespace std
{
    template<>
    struct less<BWAPI::Bullet>{
        bool operator()(const BWAPI::Bullet& a, const BWAPI::Bullet& b) const;
    };
}

namespace BWAPI
{
  /// <summary>A container for a set of Bullet objects.</summary>
  class Bulletset : public SetContainer<Bullet, std::hash<void*>>
  {
  public:
  };
}

