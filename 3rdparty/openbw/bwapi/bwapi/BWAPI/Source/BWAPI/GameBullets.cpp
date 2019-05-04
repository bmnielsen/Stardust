#include "GameImpl.h"

#include <cassert>

#include <BWAPI/BulletImpl.h>

#include "../../../Debug.h"

namespace BWAPI
{
  //----------------------------------------------- GET BULLET FROM INDEX ------------------------------------
  BulletImpl* GameImpl::getBulletFromIndex(int index)
  {
    index &= 0x7F;
    if (static_cast<unsigned>(index) < bulletArray.size())
      return this->bulletArray[index];
    return nullptr;
  }
  //--------------------------------------------- UPDATE BULLETS ---------------------------------------------
  void GameImpl::updateBullets()
  {
    // Reset bullets information
    for (BulletImpl* b : bulletArray)
      b->setExists(false);
    bullets.clear();

    // Repopulate bullets set
    for ( auto curritem = bwgame.BulletNodeTable_begin(); 
          curritem != bwgame.BulletNodeTable_end(); 
          ++curritem )
    {
      BulletImpl*& b = this->bulletArray.at(curritem->getIndex());
      b->bwbullet = *curritem;
      b->setExists(true);
      this->bullets.insert(b);
    }

    // Update all bullets info
    for (BulletImpl* b : bulletArray)
    {
      b->saveExists();
      b->updateData();
    }
  }

}

