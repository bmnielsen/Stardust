#include "BulletImpl.h"
#include <BW/BWData.h>
#include <BW/Constants.h>
#include <BWAPI/Client/BulletData.h>
#include <Util/Convenience.h>

#include "GameImpl.h"
#include "PlayerImpl.h"
#include "UnitImpl.h"

#include "../../../Debug.h"

namespace BWAPI
{
  //int BulletImpl::nextId = 0;

  //---------------------------------------------- CONSTRUCTOR -----------------------------------------------
  BulletImpl::BulletImpl(BW::Bullet bwbullet)
      : bwbullet(bwbullet)
  {
  }
  //----------------------------------------------- SET EXISTS -----------------------------------------------
  void BulletImpl::setExists(bool exists)
  {
    __exists = exists;
  }
  //---------------------------------------------- SAVE EXISTS -----------------------------------------------
  void BulletImpl::saveExists()
  {
    if ( !lastExists && __exists)
      id = BroodwarImpl.bulletNextId++;
    lastExists = __exists;
  }
  void BulletImpl::updateData()
  {
    bool _exists = __exists && bwbullet;
    if ( _exists )
    {
      for(int i = 0; i < 9; ++i)
      {
        PlayerImpl* player = static_cast<PlayerImpl*>(Broodwar->getPlayer(i));

        if ( !bwbullet.hasSprite() || !player )
          self->isVisible[i] = false;
        else if ( BWAPI::BroodwarImpl.isFlagEnabled(Flag::CompleteMapInformation) )
          self->isVisible[i] = true;
        else
          self->isVisible[i] = Broodwar->isVisible( TilePosition(bwbullet.spritePosition()) );
      }
    }
    else
    {
      MemZero(self->isVisible);
    }

    if ( _exists && 
         (BWAPI::BroodwarImpl.isFlagEnabled(Flag::CompleteMapInformation) ||
          BroodwarImpl.isReplay() || 
          isVisible()) )
    {
      UnitImpl *_getSource = BWAPI::BroodwarImpl.getUnitFromBWUnit(bwbullet.sourceUnit());
      UnitImpl *_getTarget = BWAPI::BroodwarImpl.getUnitFromBWUnit(bwbullet.attackTargetUnit());
      Player   _getPlayer = _getSource ? _getSource->_getPlayer : nullptr;

      // id, player, type, source
      self->id      = id;
      self->player  = (_getSource && _getSource->isAlive && _getPlayer) ? _getPlayer->getID() : -1;
      self->type    = bwbullet.type();
      self->source  = (_getSource && _getSource->exists()) ? _getSource->getID() : -1;

      // position
      self->positionX = bwbullet.position().x;
      self->positionY = bwbullet.position().y;

      // angle, velocity
      int d = (int)bwbullet.currentDirection() - 64;
      if ( d < 0 )
        d += 256;
      self->angle     = (double)d * 3.14159265358979323846 / 128.0;
      self->velocityX = (double)(bwbullet.current_speed_x() / 256.0);
      self->velocityY = (double)(bwbullet.current_speed_y() / 256.0);

      // target, targetPosition
      self->target          = (_getTarget && _getTarget->exists()) ? _getTarget->getID() : -1;
      self->targetPositionX = bwbullet.targetPosition().x;
      self->targetPositionY = bwbullet.targetPosition().y;

      // removeTimer
      self->removeTimer = bwbullet.time_remaining();

      // exists
      self->exists  = true;
    }
    else
    {
      self->id              = -1;
      self->player          = -1;
      self->type            = BulletTypes::Unknown;
      self->source          = -1;
      self->positionX       = Positions::Unknown.x;
      self->positionY       = Positions::Unknown.y;
      self->angle           = 0;
      self->velocityX       = 0;
      self->velocityY       = 0;
      self->target          = -1;
      self->targetPositionX = Positions::Unknown.x;
      self->targetPositionY = Positions::Unknown.y;
      self->removeTimer     = 0;
      self->exists          = false;
    }

  }
}
