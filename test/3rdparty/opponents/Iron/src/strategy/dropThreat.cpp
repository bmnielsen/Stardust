//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "dropThreat.h"
#include "../units/him.h"
#include "../units/my.h"
#include "../behavior/mining.h"
#include "../behavior/scouting.h"
#include "../behavior/harassing.h"
#include "../behavior/exploring.h"
#include "../behavior/chasing.h"
#include "../territory/stronghold.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class DropThreat
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


DropThreat::DropThreat()
{
}


string DropThreat::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected";

	return "-";
}


void DropThreat::OnFrame_v()
{/*
	if (m_detected)
	{
		// cancel second Machine Shop if there is a lack of vultures
		if (ai()->Frame() % 10 == 0)
			if (me().Buildings(Terran_Machine_Shop).size() == 2)
				if (!ConditionToStartSecondShop())
				{
					MyBuilding * pSecondShop = nullptr;
					int maxTimeToComplete = numeric_limits<int>::min();
					for (const auto & b : me().Buildings(Terran_Machine_Shop))
						if (!b->Completed())
							if (b->RemainingBuildTime() > maxTimeToComplete)
							{
								pSecondShop = b.get();
								maxTimeToComplete = b->RemainingBuildTime();
							}

					if (pSecondShop && (maxTimeToComplete > 150))
						if (pSecondShop->CanAcceptCommand())
							return pSecondShop->CancelConstruction();
				}

		// recrut SCVs to defend the base
		if (Mining::Instances().size() >= 8)
			for (const auto & u : him().Units())
				if (!u->InFog())
					if (u->Is(Protoss_Dragoon))
						if (u->GetArea() == me().GetArea())
							if (u->Chasers().empty())
							{
								if (u->Unit()->isResearching
								multimap<int, My<Terran_SCV> *> Candidates;
								for (My<Terran_SCV> * pSCV : me().StartingVBase()->GetStronghold()->SCVs())
									if (pSCV->Completed())
										if (pSCV->Life() >= 51)
											if (!pSCV->GetBehavior()->IsConstructing())
											if (!pSCV->GetBehavior()->IsChasing())
											if (!pSCV->GetBehavior()->IsWalking())
												Candidates.emplace(squaredDist(u->Pos(), pSCV->Pos()), pSCV);

								if (Candidates.size() >= 4)
								{
									auto end = Candidates.begin();
									advance(end, 4);
									for (auto it = Candidates.begin() ; it != end ; ++it)
										it->second->ChangeBehavior<Chasing>(it->second, u.get(), bool("insist"));
									return;
								}
							}
	}
	else
	{
		if (him().MayDrop())
		{
			ai()->SetDelay(100);
			m_detected = true;
			m_detetedSince = ai()->Frame();
			return;
		}
	}
*/
}


} // namespace iron



