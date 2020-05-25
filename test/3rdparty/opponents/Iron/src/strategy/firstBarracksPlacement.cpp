//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "firstBarracksPlacement.h"
#include "strategy.h"
#include "workerRush.h"
#include "zerglingRush.h"
#include "cannonRush.h"
#include "walling.h"
#include "../units/cc.h"
#include "../behavior/fleeing.h"
#include "../behavior/constructing.h"
#include "../behavior/walking.h"
#include "../behavior/defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class FirstBarracksPlacement
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


FirstBarracksPlacement::FirstBarracksPlacement()
{
}


FirstBarracksPlacement::~FirstBarracksPlacement()
{
	if (m_pBuilder)
		if (m_pBuilder->GetBehavior()->IsWalking())
			m_pBuilder->ChangeBehavior<DefaultBehavior>(m_pBuilder);
}


string FirstBarracksPlacement::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


void FirstBarracksPlacement::OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit)
{
	if (m_pBuilder == pBWAPIUnit)
		m_pBuilder = nullptr;
}



void FirstBarracksPlacement::OnFrame_v()
{
	const bool mayBeZerg = !(him().IsProtoss() || him().IsTerran());

	if (!me().Buildings(Terran_Barracks).empty()) return Discard();
//	if (!him().IsProtoss()) return Discard();
//	if (!(me().GetWall() && me().GetWall()->Active())) return Discard();

	if (ai()->GetStrategy()->Detected<WorkerRush>()) return Discard();
	
	if (m_active)
	{
		if (m_pBuilder)
		{
			if (!(m_pBuilder->GetBehavior()->IsWalking() ||
				m_pBuilder->GetBehavior()->IsConstructing() && (m_pBuilder->GetBehavior()->IsConstructing()->Location() == m_location)))
				// m_pBuilder may be fleeing
				return Discard();

			// Walking agents do not check for threats themselves.
			if (m_pBuilder->GetBehavior()->IsWalking())
				if (!findThreats(m_pBuilder, 3*32).empty())
					return m_pBuilder->ChangeBehavior<Fleeing>(m_pBuilder);
		}
		else
		{
			if (My<Terran_SCV> * pWorker = findFreeWorker(me().StartingVBase()))
			{
				for (;;)
				{
					int distance = roundedDist(center(m_location), pWorker->Pos());

					if (My<Terran_SCV> * pCloserWorker = findFreeWorker(me().StartingVBase(), [this, distance](const My<Terran_SCV> * pSCV)
						{ return roundedDist(center(m_location), pSCV->Pos()) < distance - 5; }))
						pWorker = pCloserWorker;
					else break;
				} 


				pWorker->ChangeBehavior<Walking>(pWorker, Position(m_location + UnitType(Terran_Barracks).tileSize()/2), __FILE__ + to_string(__LINE__));
				m_pBuilder = pWorker;
				return;
			}
		}
	}
	else
	{
//		if (me().SupplyUsed() >= (ai()->GetStrategy()->Detected<ZerglingRush>() ? 10 : 11))
		if (me().SupplyUsed() >= (mayBeZerg ? 10 : 11))
		{
			if (m_location == TilePositions::None) m_location = Constructing::FindFirstBarracksPlacement();
//			int distance = roundedDist(m_location, TilePosition(me().StartingBase()->Center()));
//			if (ai()->Me().MineralsAvailable() > 150-4*distance)
			if (ai()->GetStrategy()->Active<Walling>() && !mayBeZerg ||
				(ai()->Me().MineralsAvailable() > 100))
			{
			///	ai()->SetDelay(100);
				m_active = true;
				m_activeSince = ai()->Frame();
				return;
			}
		}
	}
}


} // namespace iron



