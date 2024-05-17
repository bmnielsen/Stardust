//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "dragoonRush.h"
#include "../units/him.h"
#include "../units/my.h"
#include "../behavior/mining.h"
#include "../behavior/chasing.h"
#include "../behavior/repairing.h"
#include "../territory/stronghold.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class DragoonRush
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


DragoonRush::DragoonRush()
{
}


string DragoonRush::StateDescription() const
{
	if (!m_detected) return "-";
	if (m_detected) return "detected";

	return "-";
}


bool DragoonRush::ConditionToStartSecondShop() const
{
	assert_throw(m_detected);

	return me().Units(Terran_Vulture).size() >= 4;
}


void DragoonRush::OnFrame_v()
{
	if (him().IsTerran() || him().IsZerg()) return Discard();

	const vector<const Area *> & MyEnlargedArea = ext(me().GetArea())->EnlargedArea();
	vector<const Area *> MyTerritory = MyEnlargedArea;
	for (const Area * area : MyEnlargedArea)
		for (const Area * neighbour : area->AccessibleNeighbours())
			push_back_if_not_found(MyTerritory, neighbour);

	const int dragoonsNearMe = count_if(him().Units().begin(), him().Units().end(),
				[&MyTerritory](const unique_ptr<HisUnit> & u)
				{
					return u->Is(Protoss_Dragoon) &&
						contains(MyTerritory, u->GetArea()) &&
						groundDist(u->Pos(), me().StartingBase()->Center()) < 64*32;
				});

	const bool detectedCondition = (dragoonsNearMe >= 1) && ((int)me().Units(Terran_Vulture).size() < dragoonsNearMe + 2);
	if (!detectedCondition)
	{
		if (me().CompletedUnits(Terran_Vulture) >= 4) return Discard();
		if ((me().CompletedUnits(Terran_Vulture) >= 2) && me().HasResearched(TechTypes::Spider_Mines)) return Discard();
	}


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
								multimap<int, My<Terran_SCV> *> Candidates;
								for (My<Terran_SCV> * pSCV : me().StartingVBase()->GetStronghold()->SCVs())
									if (pSCV->Completed() && !pSCV->Loaded())
										if (pSCV->Life() >= 51)
											if (!pSCV->GetBehavior()->IsConstructing())
											if (!pSCV->GetBehavior()->IsChasing())
											if (!pSCV->GetBehavior()->IsWalking())
											if (!(pSCV->GetBehavior()->IsRepairing() &&
													pSCV->GetBehavior()->IsRepairing()->TargetX() &&
													pSCV->GetBehavior()->IsRepairing()->TargetX()->Is(Terran_Siege_Tank_Siege_Mode)
												))
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
		if (detectedCondition)
		{
//			ai()->SetDelay(100);
			m_detected = true;
			m_detetedSince = ai()->Frame();
			return;
		}
	}
}


} // namespace iron



