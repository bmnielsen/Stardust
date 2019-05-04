#include "RegionImpl.h"

#include <BW/BWData.h>

#include <BWAPI/GameImpl.h>

#include "../../../Debug.h"

namespace BWAPI
{
  RegionImpl::RegionImpl(int id)
  {
    BW::Region r = BroodwarImpl.bwgame.getRegion(id);

    // Assign common region properties
    self->islandID        = r.groupIndex();
    self->center_x        = r.getCenter().x;
    self->center_y        = r.getCenter().y;

    self->isAccessible    = r.accessabilityFlags() != 0x1FFD;
    self->isHigherGround  = r.accessabilityFlags() == 0x1FF9;
    //self->priority        = r.defencePriority & 0x7F;
    self->priority        = 0;
    self->leftMost        = r.rgnBox_left();
    self->rightMost       = r.rgnBox_right();
    self->topMost         = r.rgnBox_top();
    self->bottomMost      = r.rgnBox_bottom();

    // Connect the BWAPI Region and BW Region two ways
    self->id  = id;
    
    this->closestAccessibleRgn    = nullptr;
    this->closestInaccessibleRgn  = nullptr;
  }
  void RegionImpl::UpdateRegionRelations()
  {
    // Assuming this is called via GameInternals, so no checks are made

    BW::Region r = BroodwarImpl.bwgame.getRegion(self->id);

    // Assign region neighbors
    this->neighbors.clear();

    int accessibleBestDist    = 99999;
    int inaccessibleBestDist  = 99999;
    for ( size_t n = 0; n != r.neighborCount(); ++n )
    {
      BW::Region neighbor = r.getNeighbor(n);
      BWAPI::Region bwapiNeighbor = Broodwar->getRegion(neighbor.getIndex());

      // continue if this is null (but it shouldn't be)
      if ( !bwapiNeighbor )
        continue;

      // add our neighbor
      this->neighbors.insert(bwapiNeighbor);

      // Obtain the closest accessible and inaccessible Regions from their Region center
      int dst = r.getCenter().getApproxDistance(neighbor.getCenter());
      if ( r.groupIndex() == neighbor.groupIndex() )
      {
        if ( dst < accessibleBestDist )
        {
          accessibleBestDist = dst;
          this->closestAccessibleRgn = bwapiNeighbor;
        }
      }
      else if ( dst < inaccessibleBestDist )
      {
        inaccessibleBestDist = dst;
        this->closestInaccessibleRgn = bwapiNeighbor;
      }

      // Client compatibility for neighbors
      ++self->neighborCount;
      self->neighbors[n] = neighbor.getIndex();
    }
  }
  RegionData *RegionImpl::getData()
  {
    return self;
  }
};
