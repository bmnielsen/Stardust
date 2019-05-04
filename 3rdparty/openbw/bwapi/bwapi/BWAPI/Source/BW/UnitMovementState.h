#pragma once

namespace BW
{
  enum UnitMovementState : u8
  {
    // Names are official
    UM_Init          = 0x00,
    UM_InitSeq       = 0x01,
    UM_Lump          = 0x02,
    UM_Turret        = 0x03,
    UM_Bunker        = 0x04,
    UM_BldgTurret    = 0x05,
    UM_Hidden        = 0x06,
    UM_Flyer         = 0x07,
    UM_FakeFlyer     = 0x08,
    UM_AtRest        = 0x09,
    UM_Dormant       = 0x0A,
    UM_AtMoveTarget  = 0x0B,
    UM_CheckIllegal  = 0x0C,
    UM_MoveToLegal   = 0x0D,
    UM_LumpWannabe   = 0x0E,
    UM_FailedPath    = 0x0F,
    UM_RetryPath     = 0x10,
    UM_StartPath     = 0x11,
    UM_UIOrderDelay  = 0x12,
    UM_TurnAndStart  = 0x13,
    UM_FaceTarget    = 0x14,
    UM_NewMoveTarget = 0x15,
    UM_AnotherPath   = 0x16,
    UM_Repath        = 0x17,
    UM_RepathMovers  = 0x18,
    UM_FollowPath    = 0x19,
    UM_ScoutPath     = 0x1A,
    UM_ScoutFree     = 0x1B,
    UM_FixCollision  = 0x1C,
    UM_WaitFree      = 0x1D,
    UM_GetFree       = 0x1E,
    UM_SlidePrep     = 0x1F,
    UM_SlideFree     = 0x20,
    UM_ForceMoveFree = 0x21,
    UM_FixTerrain    = 0x22,
    UM_TerrainSlide  = 0x23
  };
};
