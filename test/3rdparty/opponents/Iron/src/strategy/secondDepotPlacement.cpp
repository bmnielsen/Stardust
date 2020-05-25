//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "secondDepotPlacement.h"
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
//                                  class SecondDepotPlacement
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


SecondDepotPlacement::SecondDepotPlacement()
{
}


SecondDepotPlacement::~SecondDepotPlacement()
{
	if (m_pBuilder)
		if (m_pBuilder->GetBehavior()->IsWalking())
			m_pBuilder->ChangeBehavior<DefaultBehavior>(m_pBuilder);
}


string SecondDepotPlacement::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "active";

	return "-";
}


void SecondDepotPlacement::OnBWAPIUnitDestroyed(BWAPIUnit * pBWAPIUnit)
{
	if (m_pBuilder == pBWAPIUnit)
		m_pBuilder = nullptr;
}



void SecondDepotPlacement::OnFrame_v()
{
	if (him().IsProtoss() || him().IsTerran()) return Discard();
	if (me().Buildings(Terran_Supply_Depot).size() >= 2) return Discard();
	if (!ai()->GetStrategy()->Active<Walling>()) return Discard();
	if (ai()->GetStrategy()->Active<Walling>()->GetWall()->Size() < 3) return Discard();
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


				pWorker->ChangeBehavior<Walking>(pWorker, Position(m_location + UnitType(Terran_Supply_Depot).tileSize()/2), __FILE__ + to_string(__LINE__));
				m_pBuilder = pWorker;
				return;
			}
		}
	}
	else
	{
		if (me().SupplyUsed() >= 13)
		{
			if (m_location == TilePositions::None) m_location = Constructing::FindSecondDepotPlacement();
			//int distance = roundedDist(m_location, TilePosition(me().StartingBase()->Center()));
//			int distance = roundedDist(m_location, TilePosition(me().StartingBase()->Center()));
//			if (ai()->Me().MineralsAvailable() > 50)
//				for (const auto & b : me().Buildings(Terran_Command_Center))
//					if (b->Unit()->isTraining())
//						if (b->TimeToTrain() < 200)
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



