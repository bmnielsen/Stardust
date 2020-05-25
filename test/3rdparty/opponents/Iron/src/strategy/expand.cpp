//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "expand.h"
#include "strategy.h"
#include "walling.h"
#include "zerglingRush.h"
#include "zealotRush.h"
#include "dragoonRush.h"
#include "marineRush.h"
#include "cannonRush.h"
#include "wraithRush.h"
#include "baseDefense.h"
#include "shallowTwo.h"
#include "terranFastExpand.h"
#include "../units/cc.h"
#include "../units/army.h"
#include "../behavior/mining.h"
#include "../behavior/exploring.h"
#include "../behavior/constructing.h"
#include "../behavior/defaultBehavior.h"
#include "../territory/stronghold.h"
#include "../territory/vgridMap.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


VBase * findNatural(const VBase * pMain)
{
	assert_throw(pMain);
	VBase * natural = nullptr;

	int minLength = numeric_limits<int>::max();
	for (VBase & base : ai()->GetVMap().Bases())
		if (&base != pMain)
			if (!base.BWEMPart()->Minerals().empty() && !base.BWEMPart()->Geysers().empty())
			{
				int length;
				ai()->GetMap().GetPath(base.BWEMPart()->Center(), pMain->BWEMPart()->Center(), &length);
				if ((length > 0) && (length < minLength))
				{
					minLength = length;
					natural = &base;
				}
			}

	return natural;
}


VBase * findNewBase()
{
	vector<pair<VBase *, int>> Candidates;

	for (VBase & base : ai()->GetVMap().Bases())
		if (!contains(me().Bases(), &base))
		if (him().StartingVBase() != &base)
		{
			TilePosition baseCenter = base.BWEMPart()->Location() + UnitType(Terran_Command_Center).tileSize()/2;
			auto & Cell = ai()->GetGridMap().GetCell(baseCenter);
			if (Cell.HisUnits.empty())
			if (Cell.HisBuildings.empty())
				if (all_of(him().Buildings().begin(), him().Buildings().end(), [baseCenter](const unique_ptr<HisBuilding> & b)
								{ return dist(b->Pos(), center(baseCenter)) > 10*32; }))
					if (!base.BWEMPart()->Minerals().empty())// && !base.BWEMPart()->Geysers().empty())
					{
						int distToMyMain;
						ai()->GetMap().GetPath(base.BWEMPart()->Center(), me().StartingBase()->Center(), &distToMyMain);

						int distToHisMain = 1000000;
						if (him().StartingBase())
							ai()->GetMap().GetPath(base.BWEMPart()->Center(), him().StartingBase()->Center(), &distToHisMain);

						if (distToMyMain > 0)
						{
							int ressourcesScore = base.MineralsAmount() + base.GasAmount();
							int score = ressourcesScore/50 + distToHisMain - distToMyMain;
							Candidates.emplace_back(&base, score);
						}
					}
		}

	if (Candidates.empty()) return nullptr;

	sort(Candidates.begin(), Candidates.end(),
		[](const pair<VBase *, int> & a, const pair<VBase *, int> & b){ return a.second > b.second; });

	return Candidates.front().first;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Expand
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


Expand::Expand()
{
}


Expand::~Expand()
{
}


string Expand::StateDescription() const
{
	if (!m_active) return "-";
	if (m_active) return "base #" + to_string(me().Bases().size());

	return "-";
}


bool Expand::ConditionOnUnits() const
{
	const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });

	if (him().IsProtoss())
	{
		if (activeBases == 1)
		{

		}
		else
		{
			if (me().Units(Terran_Vulture).size() < 3)
				return false;

			if (me().Units(Terran_Siege_Tank_Tank_Mode).size() < 1)
				return false;

			if (me().Units(Terran_Vulture).size() + me().Units(Terran_Siege_Tank_Tank_Mode).size()  < 10)
				return false;

			if (me().Army().KeepTanksAtHome())
				return false;
		}
/*
		if (activeBases == 1)
			if (ai()->GetStrategy()->Has<ShallowTwo>())
			{
				if (him().Buildings(Protoss_Photon_Cannon).size() >= 3)
				{
					if (me().Units(Terran_Siege_Tank_Tank_Mode).size() < 1)
						return false;
				}
				else if (him().MayDarkTemplar())
				{
					//if (me().Army().MyGroundUnitsAhead() < me().Army().HisGroundUnitsAhead() + 4)
					//	return false;
				}
				else
				{
					if (me().Army().MyGroundUnitsAhead() < me().Army().HisGroundUnitsAhead() + 16)
						return false;
				}

				if (him().MayReaver())
				{
					if (me().CompletedUnits(Terran_Wraith) < 1)
						return false;
				//	if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) < 2)
				//		return false;
				}

			}
*/


	}
	else if (him().IsTerran())
	{
		int minVultures = 5;

		if (me().Bases().size() == 1)
		{
			minVultures = 13;

			if (him().Buildings(Terran_Bunker).size() >= 1) minVultures = 5;

			if (him().Buildings(Terran_Command_Center).size() >= 2) minVultures = 5;

			for (HisBuilding * b : him().Buildings(Terran_Command_Center))
				if (b->GetArea() != him().GetArea())
					minVultures = 5;

			if (ai()->GetStrategy()->Detected<WraithRush>()) minVultures = 3;

			if (ai()->GetStrategy()->Detected<TerranFastExpand>()) minVultures = 3;
		}

		if ((int)me().Units(Terran_Vulture).size() < minVultures)
			return false;
	}
	else
	{
		int minVultures = 5;
/*
		if (me().Bases().size() == 1)
			if ((him().AllUnits(Zerg_Mutalisk).size() > 0) || (him().Buildings(Zerg_Spire).size() > 0))
				minVultures = 2;
*/
		if ((int)me().Units(Terran_Vulture).size() < minVultures)
			return false;

		if (me().Bases().size() >= 2)
			if (me().MineralsAvailable() < 500)
				if (him().HydraPressure_needVultures())
					if (me().Army().KeepTanksAtHome())
						return false;
	}

	return true;
}


bool Expand::ConditionOnActivity() const
{
	const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });

	if (activeBases >= 2)
	{
		if (me().MineralsAvailable() <= 600)
		{
			if (me().FactoryActivity() >= 9) return false;
			if (me().BarrackActivity() >= 9) return false;
		}
	}

	return true;
}


bool Expand::ConditionOnTech() const
{
	const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });

	const int mechProductionSites =
		me().Buildings(Terran_Factory).size() +
		me().Buildings(Terran_Starport).size();

	const int bioProductionSites = me().Buildings(Terran_Barracks).size();

	if (him().IsProtoss())
		if (activeBases == 1)
		{
//			if (ai()->GetStrategy()->Active<Walling>())
//				return false;

			if (me().Buildings(Terran_Factory).empty())
				return false;

			if (!(me().MineralsAvailable() >= 450))
				if (me().Buildings(Terran_Machine_Shop).size() < 1)
					return false;

	//		if (me().Buildings(Terran_Starport).size() == 0)
	//			return false;

		}

	if (activeBases >= 2)
		if (me().MineralsAvailable() <= 600)
		{
			if (!(me().Buildings(Terran_Missile_Turret).size() >= 8* me().Bases().size()))
			{
				if (mechProductionSites < min(12, 1*activeBases + 2)) return false;
			}
		}

	if (him().IsTerran())
	{
		if (ai()->GetStrategy()->Detected<WraithRush>())
		{
//			return false;

			if (me().Buildings(Terran_Engineering_Bay).size() == 0)
				return false;

//			if (me().Buildings(Terran_Armory).size() == 0)
//				return false;
		}
		else if (ai()->GetStrategy()->Detected<TerranFastExpand>())
		{

		}
		else 
		{
			if (me().Buildings(Terran_Starport).size() == 0)
				return false;
		}
	}
	else
	{
		if (him().IsZerg())
			if (!(
					(me().Buildings(Terran_Engineering_Bay).size() >= 1)
				))
				return false;

		if ((activeBases >= 3))
			if (!(
					(me().Buildings(Terran_Engineering_Bay).size() >= 1) &&
					(me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 1)
				))
				return false;
	}
	return true;
}


bool Expand::Condition() const
{//return false;

	if (Interactive::expand) return true;

	if (me().MineralsAvailable() > 800)
		if (me().SupplyUsed() > 150)
			return true;

	if (me().Bases().size() <= 2)
		if (me().StartingBase()->Minerals().size() < 6)
			if (!me().Army().KeepTanksAtHome())
			if (!me().Army().KeepGoliathsAtHome())
				return true;

	if (ai()->GetStrategy()->Detected<ZerglingRush>() ||
		ai()->GetStrategy()->Detected<ZealotRush>() ||
		ai()->GetStrategy()->Detected<DragoonRush>() ||
		ai()->GetStrategy()->Detected<CannonRush>() ||
		ai()->GetStrategy()->Detected<MarineRush>() ||
		ai()->GetStrategy()->Active<BaseDefense>() && (me().Bases().size() <= 2))
		return false;

	if (ConditionOnUnits())
//	if (ConditionOnActivity() || (me().MineralsAvailable() >= 350))
	if (me().Army().GoodValueInTime() ||
		(me().MineralsAvailable() >= 350) ||
		ai()->GetStrategy()->Detected<WraithRush>() && (me().Bases().size() == 1) ||
		ai()->GetStrategy()->Detected<TerranFastExpand>() && (me().Bases().size() == 1) ||
		me().Buildings(Terran_Missile_Turret).size() >= 8 * me().Bases().size() ||
		him().HydraPressure() &&
			(me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 1) &&
			(me().CompletedBuildings(Terran_Factory) >= 3))

//	if (ConditionOnTech() || (me().MineralsAvailable() >= 350))
	if (ConditionOnTech() || ((me().Bases().size() >= 3) && (me().MineralsAvailable() >= 1000)))
	{
		if (me().Bases().size() == 1)
		{
			if (him().HydraPressure())
				if (!(//me().Army().GroundLead() ||
						((me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 1) &&
						(me().CompletedBuildings(Terran_Factory) >= 3))))
					return false;

//			if (me().Army().GroundLead() || (me().MineralsAvailable() >= 350))
			//if (me().Army().GoodValueInTime() || (me().MineralsAvailable() >= 350))
			{
				//if (him().IsZerg() ||
				//	(me().Army().MyGroundUnitsAhead() >= me().Army().HisGroundUnitsAhead() + 3) ||
				//	(me().SupplyAvailable() >= 60) && (me().MineralsAvailable() >= 350) ||
				//	(me().SupplyAvailable() >= 70) ||
				//	(me().MineralsAvailable() >= 350))
					return true;
			}
		}
		else
		{
			if (ai()->Frame() - me().Bases().back()->CreationTime() > 1000)
				if (me().Army().GroundLead())
				{
				///	bw << "Expand::Condition 1" << endl;
					return true;
				}

			if (ai()->Frame() - me().Bases().back()->CreationTime() > 1000)
				if ((me().FactoryActivity() >= 9) && !ai()->GetStrategy()->Active<BaseDefense>())
				{
				///	bw << "Expand::Condition 2a" << endl;
					return true;
				}

			if (ai()->Frame() - me().Bases().back()->CreationTime() > 1000)
				if ((me().BarrackActivity() >= 9) && !ai()->GetStrategy()->Active<BaseDefense>())
				{
				///	bw << "Expand::Condition 2b" << endl;
					return true;
				}

			if (me().Bases().size() == 2)
				if (ai()->Frame() - me().Bases().back()->CreationTime() > 1000)
					if ((me().Army().Value() >= (him().IsProtoss() ? 2000 : 2500)) && !ai()->GetStrategy()->Active<BaseDefense>())
					{
					///	bw << "Expand::Condition 3" << endl;
						return true;
					}

			if (ai()->Frame() - me().Bases().back()->CreationTime() > 10000)
				if (Mining::Instances().size() < 20)
				{
				///	bw << "Expand::Condition 4" << endl;
					return true;
				}

			if (ai()->Frame() - me().Bases().back()->CreationTime() > 2000)
				if (me().MineralsAvailable() > 600)
				{
				///	bw << "Expand::Condition 5" << endl;
					return true;
				}

			if (ai()->Frame() - me().Bases().back()->CreationTime() > 1000)
				if (me().MineralsAvailable() > 600)
					if (me().SupplyUsed() > 150)
					{
					///	bw << "Expand::Condition 6" << endl;
						return true;
					}

			if (ai()->Frame() - me().Bases().back()->CreationTime() > 500)
				if (me().MineralsAvailable() > 1000)
				{
				///	bw << "Expand::Condition 7" << endl;
					return true;
				}

			if (me().MineralsAvailable() > 2000)
			{
			///	bw << "Expand::Condition 8" << endl;
				return true;
			}
		}
	}

	return false;
}


VBase * Expand::FindNewBase() const
{
	if (me().Bases().size() == 1)
	{
		return findNatural(me().StartingVBase());
	}
	else if (me().Bases().size() >= 2)
	{
		if (ai()->Frame() - me().Bases().back()->CreationTime() > 1000)
			return findNewBase();
	}

	return nullptr;
}


bool Expand::LiftCC() const
{
	return him().IsProtoss() && (me().Bases().size() == 2);
}


void Expand::OnFrame_v()
{
	if (m_active)
	{
		VBase * newBase = me().Bases().back();

		if (newBase->Active())
		{
			vector<My<Terran_SCV> *> RemainingExploringSCVs;
			for (Exploring * pExplorer : Exploring::Instances())
				if (My<Terran_SCV> * pSCV = pExplorer->Agent()->IsMy<Terran_SCV>())
					if (pSCV->GetStronghold() == newBase->GetStronghold())
						RemainingExploringSCVs.push_back(pSCV);

			for (My<Terran_SCV> * pSCV : RemainingExploringSCVs)
				pSCV->ChangeBehavior<DefaultBehavior>(pSCV);

			m_active = false;
			return;
		}

		if (LiftCC())
		{

		}
		else
		{
			if (newBase->GetStronghold()->SCVs().size() < 3)
				if (none_of(Exploring::Instances().begin(), Exploring::Instances().end(), [newBase](const Exploring * e)
									{ return e->Agent()->Is(Terran_SCV) &&
											(e->Agent()->GetStronghold() == newBase->GetStronghold()); }))
				if (him().IsZerg() || ai()->GetStrategy()->Detected<WraithRush>() ||
					none_of(Constructing::Instances().begin(), Constructing::Instances().end(), [newBase](const Constructing * c)
									{ return c->Agent()->Is(Terran_SCV) &&
											(c->Agent()->GetStronghold() == newBase->GetStronghold()) &&
											(c->Type() == Terran_Command_Center); }))
					if (My<Terran_SCV> * pSCV = findFreeWorker(me().StartingVBase()))
					{
						pSCV->ChangeBehavior<Exploring>(pSCV, newBase->GetArea()->BWEMPart());
						pSCV->LeaveStronghold();
						pSCV->EnterStronghold(newBase->GetStronghold());
					}
		}
	}
	else
	{
		if (Condition())
		{
			if (VBase * newBase = FindNewBase())
			{
				newBase->SetCreationTime();
				me().AddBase(newBase);
				assert_throw(me().Bases().size() >= 2);
				me().AddBaseStronghold(me().Bases().back());

			///	bw << Name() << " started!" << endl; ai()->SetDelay(100);

				m_active = true;
				m_activeSince = ai()->Frame();
				return;
			}
		}
	}
}


} // namespace iron



