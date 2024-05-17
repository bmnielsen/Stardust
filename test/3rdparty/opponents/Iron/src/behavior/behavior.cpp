//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "behavior.h"
#include "../Iron.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"

namespace { auto & bw = Broodwar; }


namespace iron
{



bool canWalkOnTile(const TilePosition & t)
{
	if (ai()->GetMap().Valid(t))
	{
		const auto & tile = ai()->GetMap().GetTile(t, check_t::no_check);
		if (tile.Walkable())
		if (!tile.GetNeutral())
		if (!tile.Ptr())
			return true;
	}

	return false;
}




//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class IBehavior
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

IBehavior::IBehavior(MyBWAPIUnit * pAgent, behavior_t behaviorType)
	: m_pAgent(pAgent), m_behaviorType(behaviorType), m_justArrivedInState(true), m_since(ai()->Frame())
{
}


IBehavior::~IBehavior()
{
	ResetSubBehavior();
	m_pAgent->SetLastBehaviorType(GetBehaviorType());
}


void IBehavior::ResetSubBehavior()
{CI(this);
	if (m_pSubBehavior) m_pSubBehavior.reset();
	
	if (!AgentBeingDestroyed() && Agent()->Unit()->exists() && !Agent()->Unit()->isIdle() &&
		Agent()->CanAcceptCommand() && Agent()->Unit()->canStop())
		Agent()->Stop();
}


MyBWAPIUnit * IBehavior::Agent() const
{CI(this);
	return CI(m_pAgent);
}


bool IBehavior::CheckStuckBuilding()
{CI(this);
	if (Agent()->Pos() == Agent()->PrevPos(4) && Agent()->Pos() == Agent()->PrevPos(8))
	{
		if (Agent()->Is(Terran_Marine))
			if (him().IsZerg())
				if (const Walling * s = ai()->GetStrategy()->Active<Walling>())
					if (!s->GetWall()->Open())
						if (ai()->GetVMap().InsideWall(ai()->GetMap().GetTile(TilePosition(Agent()->Pos()))))
							return false;

		auto pos = TilePosition(Agent()->Pos());
		for (TilePosition delta : {	TilePosition(+1,  0), TilePosition(0, -1),  TilePosition(0, +1),  TilePosition(-1,  0),
									TilePosition(-1, -1), TilePosition(-1, +1), TilePosition(+1, -1), TilePosition(+1, +1)})
		{
			TilePosition t = pos + delta;
			if (ai()->GetMap().Valid(t))
			{
				const Tile & tile = ai()->GetMap().GetTile(t, check_t::no_check);
				if (ai()->GetVMap().GetBuildingOn(tile) || tile.GetNeutral())
				{
				///	bw << Agent()->NameWithId() << " bloqued!" << endl; ai()->SetDelay(500);
					Position dest = Agent()->Pos() - Position(delta*4);
					if (!ai()->GetMap().Valid(dest) || (ai()->Frame() - m_goAwayFromStuckingBuildingUntil < 20)) dest = ai()->GetMap().RandomPosition();
#if DEV
					bw->drawCircleMap(dest, 10, Colors::Red, true);
#endif
					Agent()->Move(dest);
					m_goAwayFromStuckingBuildingUntil = ai()->Frame() + 10;
					return true;
				}
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Behavior
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////



	
} // namespace iron



