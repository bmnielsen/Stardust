//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "expert.h"
#include "production.h"
#include "../units/cc.h"
#include "../units/turret.h"
#include "../strategy/strategy.h"
#include "../strategy/firstDepotPlacement.h"
#include "../strategy/secondDepotPlacement.h"
#include "../strategy/firstBarracksPlacement.h"
#include "../strategy/firstFactoryPlacement.h"
#include "../strategy/expand.h"
#include "../behavior/constructing.h"
#include "../behavior/mining.h"
#include "../territory/stronghold.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Expert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
	
void Expert::OnFinished()
{
	me().Production().Unselect(this);
}


void Expert::UpdatePriority()
{
	UpdatePriority_specific();

	if (TaskCost().Nul())
		if (m_priority > 0)
			m_priority += 1000000;
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class TrainingExpert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//


TrainingExpert::TrainingExpert(tid_t tid, MyBuilding * pWhere)
: Expert(Cost(tid)),
	m_tid(tid), m_pWhere(pWhere)
{

}


TrainingExpert::~TrainingExpert()
{
	if (!Unselected()) OnFinished();

	me().Production().Remove(this);
}


string TrainingExpert::ToString() const
{
	string s = "train. ";

	switch (m_tid)
	{
	case Terran_Battlecruiser:			s += "Battlecruiser"; break;
	case Terran_Dropship:				s += "Dropship"; break;
	case Terran_Firebat:				s += "Firebat"; break;
	case Terran_Ghost:					s += "Ghost"; break;
	case Terran_Goliath:				s += "Goliath"; break;
	case Terran_Marine:					s += "Marine"; break;
	case Terran_Medic:					s += "Medic"; break;
	case Terran_Nuclear_Missile:		s += "Missile"; break;
	case Terran_SCV:					s += "SCV"; break;
	case Terran_Siege_Tank_Tank_Mode:	s += "Tank"; break;
	case Terran_Valkyrie:				s += "Valkyrie"; break;
	case Terran_Science_Vessel:			s += "Vessel"; break;
	case Terran_Vulture:				s += "Vulture"; break;
	case Terran_Wraith:					s += "Wraith"; break;
	default: s += UnitType(m_tid).getName(); break;
	}

	s += " at #" + to_string(m_pWhere->Unit()->getID());

	return s;
}


bool TrainingExpert::Ready() const
{
	if (!m_pWhere->Completed()) return false;
	if (m_pWhere->Flying()) return false;

	if (m_pWhere->Unit()->getAddon() && m_pWhere->Unit()->getAddon()->isBeingConstructed()) return false;
	
	if (m_tid == Terran_Siege_Tank_Tank_Mode)
		if (!(m_pWhere->Unit()->getAddon() && m_pWhere->Unit()->getAddon()->isCompleted()))
			return false;

	if (m_tid == Terran_Goliath)
		if (!me().CompletedBuildings(Terran_Armory))
			return false;

	if (m_tid == Terran_Valkyrie)
		if (!me().CompletedBuildings(Terran_Armory) || !me().CompletedBuildings(Terran_Control_Tower))
			return false;

	if (m_tid == Terran_Science_Vessel)
		if (!me().CompletedBuildings(Terran_Science_Facility) || !me().CompletedBuildings(Terran_Control_Tower))
			return false;

	if (m_tid == Terran_Dropship)
		if (!me().CompletedBuildings(Terran_Control_Tower))
			return false;

	if (m_tid == Terran_Medic)
		if (!me().CompletedBuildings(Terran_Academy))
			return false;

	return true;
}


bool TrainingExpert::PriorityLowerThanOtherExperts() const
{
	assert_throw(Unselected());
	for (ConstructingAddonExpert * pConstructingAddonExpert : m_pWhere->ConstructingAddonExperts())
		if (pConstructingAddonExpert->Ready())
			if (Priority() <= pConstructingAddonExpert->Priority())
				return true;

	// TODO

	return false;
}


int TrainingExpert::Units() const
{
	return me().Units(m_tid).size();
}


void TrainingExpert::OnFrame()
{
	static frame_t lastTrain = 0;

	if (Selected())
	{
		if (TaskCost().Supply() > me().SupplyAvailable())
		{
			OnFinished();
//			bw << ToString() << " aborted" << endl;
//			ai()->SetDelay(5000);
		}
		else if (!m_pWhere->Unit()->isTraining())
			if (ai()->Frame() > lastTrain + 5)
			if (m_pWhere->CanAcceptCommand())
			if (me().CanPay(TaskCost()))
			{
				lastTrain = ai()->Frame();
				m_pWhere->Train(m_tid, check_t::no_check);
				SetStarted();
			}
	}
	else
	{
		assert_throw(Started());
		OnFinished();
	}
}


void TrainingExpert::UpdatePriority_specific()
{
	if (TaskCost().Supply() + me().SupplyUsed() > 200) { m_priority = 0; return; }

	UpdateTrainingPriority();

	if (m_tid != Terran_SCV)
		if ((m_priority > 0) && !Where()->Unit()->isIdle()) m_priority /= 2;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ResearchingExpert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

ResearchingExpert::~ResearchingExpert()
{
	if (!Unselected()) OnFinished();

	me().Production().Remove(this);
}


string ResearchingExpert::ToString() const
{
	string s = "researching ";
/*
	if (m_type == tech)
		switch (m_techType)
		{
//		case TechTypes::Enum::Spider_Mines:		s += "Spider_Mines"; break;
		default: s += m_techType.getName(); break;
		}
	else if (m_type == upgrade)
		switch (m_upgradeType)
		{
//		case UpgradeTypes::Enum::Ion_Thrusters:		s += "Ion_Thrusters"; break;
		default: s += m_upgradeType.getName(); break;
		}
*/
	if (m_type == tech)
		s += m_techType.getName();
	else if (m_type == upgrade)
		s += m_upgradeType.getName();


	s += " at #" + to_string(m_pWhere->Unit()->getID());
	return s;
}


bool ResearchingExpert::Ready() const
{
	if (m_pWhere->Flying()) return false;
	if (m_pWhere->Unit()->isBeingConstructed()) return false;
	if (!m_pWhere->Unit()->isIdle()) return false;

	if (m_type == tech)
	{
		if (me().HasResearched(m_techType)) return false;
		if (me().Player()->isResearching(m_techType)) return false;
	}
	else
	{
		if (me().Player()->getUpgradeLevel(m_upgradeType) == me().Player()->getMaxUpgradeLevel(m_upgradeType)) return false;
		if (me().Player()->getUpgradeLevel(m_upgradeType) >= 1 && !me().CompletedBuildings(Terran_Science_Facility)) return false;
		if (me().Player()->isUpgrading(m_upgradeType)) return false;

		if (m_upgradeType == UpgradeTypes::Enum::Charon_Boosters)
			if (!me().CompletedBuildings(Terran_Armory))
				return false;
	}

	return true;
}


bool ResearchingExpert::PriorityLowerThanOtherExperts() const
{
	assert_throw(Unselected());
	for (ConstructingAddonExpert * pConstructingAddonExpert : m_pWhere->ConstructingAddonExperts())
		if (pConstructingAddonExpert->Ready())
			if (Priority() <= pConstructingAddonExpert->Priority())
				return true;

	for (const auto & pResearchingExpert : m_pWhere->ResearchingExperts())
		if (pResearchingExpert.get() != this)
			if (pResearchingExpert->Ready())
				if (Priority() <= pResearchingExpert->Priority())
					return true;
	

	return false;
}


void ResearchingExpert::UpdatePriority_specific()
{
	UpdateResearchingPriority();
}


void ResearchingExpert::OnFrame()
{
	if (Selected())
	{
		if (!m_pWhere->Unit()->isResearching() && !m_pWhere->Unit()->isUpgrading())
			if (m_pWhere->CanAcceptCommand())
			{
				if (m_type == tech)	m_pWhere->Research(m_techType, check_t::no_check);
				else				m_pWhere->Research(m_upgradeType, check_t::no_check);
				SetStarted();
			}
	}
	else
	{
		assert_throw(Started());
		OnFinished();
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ConstructingExpert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

void ConstructingExpert::OnBuildingCreated()
{
	m_buildingCreated = true;
	if (m_pBuilder)
	{
		m_pBuilder->GetBehavior()->IsConstructing()->ClearExpert();
		m_pBuilder = nullptr;
	}
}


void ConstructingExpert::OnBuilderChanged(My<Terran_SCV> * pNewBuilder)
{
	assert_throw(pNewBuilder && m_pBuilder);
	assert_throw(pNewBuilder != m_pBuilder);

	m_pBuilder = pNewBuilder;
	++m_builderChanges;
}


string ConstructingExpert::ToString() const
{
	string s = "constr. ";

	if (Free()) s += "free ";

	switch (m_tid)
	{
	case Terran_Academy:			s += "Academy"; break;
	case Terran_Armory:				s += "Armory"; break;
	case Terran_Barracks:			s += "Barracks"; break;
	case Terran_Bunker:				s += "Bunker"; break;
	case Terran_Command_Center:		s += "CC"; break;
	case Terran_Engineering_Bay:	s += "EngineeringBay"; break;
	case Terran_Factory:			s += "Factory"; break;
	case Terran_Missile_Turret:		s += "Turret"; break;
	case Terran_Refinery:			s += "Refinery"; break;
	case Terran_Science_Facility:	s += "ScienceFacility"; break;
	case Terran_Starport:			s += "Starport"; break;
	case Terran_Supply_Depot:		s += "Depot"; break;
	default: s += UnitType(m_tid).getName(); break;
	}

	return s;
}


bool ConstructingExpert::PriorityLowerThanOtherExperts() const
{
	assert_throw(Unselected());

	if (this == My<Terran_Missile_Turret>::GetConstructingExpert())
		if (!My<Terran_Missile_Turret>::GetConstructingFreeExpert()->Unselected())
			return true;

	if (this == My<Terran_Missile_Turret>::GetConstructingFreeExpert())
		if (!My<Terran_Missile_Turret>::GetConstructingExpert()->Unselected())
			return true;

	return false;
}


Cost ConstructingExpert::TaskCost() const
{
//	if (Started())
//		return m_buildingCost;

	const bool inStrongHold = !Free();
//	if (Builders() < BuildingsUncompleted())
	if (inStrongHold)	// TODO : remove
	if (me().FindBuildingNeedingBuilder(m_tid, inStrongHold))
		return Cost();

	return Expert::TaskCost();
}


int ConstructingExpert::Buildings() const
{
	return me().Buildings(m_tid).size();
}


int ConstructingExpert::BuildingsCompleted() const
{
	return me().CompletedBuildings(m_tid);
}


int ConstructingExpert::Builders() const
{
	return beingConstructed(m_tid);
}


VBase * ConstructingExpert::DefaultBase() const
{
	assert_throw(!Free());

	if (const MyBuilding * b = me().FindBuildingNeedingBuilder(m_tid))
		return b->GetStronghold()->HasBase();

	if (m_tid == Terran_Command_Center)
	{
		if (auto s = ai()->GetStrategy()->Active<Expand>())
			if (me().Bases().back()->NeverBuiltYet())
			{
				if (!s->LiftCC())
					return me().Bases().back();
			}

		for (VBase * base : me().Bases())
			if (base->Lost())
				if (base->ShouldRebuild())
					return base;
	}

	if (m_tid == Terran_Refinery)
	{
		for (VBase * base : me().Bases())
			if (base->Active())
				if (!base->BWEMPart()->Geysers().empty())
					if (none_of(me().Buildings(Terran_Refinery).begin(), me().Buildings(Terran_Refinery).end(),
								[base](const unique_ptr<MyBuilding> & b){ return b->GetStronghold() == base->GetStronghold(); }))
						return base;
	}

	if (m_noMoreLocation)
	{
		m_noMoreLocation = false;
		auto p = random_element(me().Bases());
//		int i = 0;
//		for (auto b : me().Bases())
//		{
//			if (b == p)
//				break;
//			i++;
//		}
//		bw << "m_noMoreLocation for " << UnitType(m_tid).getName() << ", try base " << i << endl;
//		ai()->SetDelay(500);
		return p;
	}

	return me().StartingVBase();
}


void ConstructingExpert::UpdatePriority_specific()
{
	if (!Free()) SetBase(DefaultBase());

	UpdateConstructingPriority();

	if (Mining::Instances().size() < 3) m_priority = 0;
}


void ConstructingExpert::OnFrame()
{
//	return pMiner->ChangeBehavior<Constructing>(pMiner, m_tid);
//	OnFinished();
	if (Selected())
	{
		m_pBuilder = nullptr;
		My<Terran_SCV> * pBuilder = GetFreeBuilder();
		if (pBuilder)
		{
			assert_throw(Free());
			assert_throw(!pBuilder->GetStronghold());
		}
		else
		{
			assert_throw(!Free());

			if (!pBuilder)
				if (m_tid == Terran_Supply_Depot)
				{
					if (const FirstDepotPlacement * pFirstDepotPlacement = ai()->GetStrategy()->Active<FirstDepotPlacement>())
						pBuilder = pFirstDepotPlacement->Builder();
					else if (const SecondDepotPlacement * pSecondDepotPlacement = ai()->GetStrategy()->Active<SecondDepotPlacement>())
						pBuilder = pSecondDepotPlacement->Builder();
				}

			if (!pBuilder)
				if (m_tid == Terran_Barracks)
					if (const FirstBarracksPlacement * pFirstBarracksPlacement = ai()->GetStrategy()->Active<FirstBarracksPlacement>())
						pBuilder = pFirstBarracksPlacement->Builder();

			if (!pBuilder)
				if (m_tid == Terran_Factory)
					if (const FirstFactoryPlacement * pFirstFactoryPlacement = ai()->GetStrategy()->Active<FirstFactoryPlacement>())
						pBuilder = pFirstFactoryPlacement->Builder();

			if (!pBuilder)
				pBuilder = findFreeWorker(GetBase_());

			assert_throw(!pBuilder || pBuilder->GetStronghold());
		}

		if (pBuilder)
		{
//			m_buildingCost = TaskCost();
			m_builderChanges = 0;
			m_pBuilder = pBuilder;
			m_pBuilder->ChangeBehavior<Constructing>(m_pBuilder, m_tid, this);
			m_buildingCreated = false;
			SetStarted();
		}
	}
	else
	{
		assert_throw(Started());
		if (m_buildingCreated)
			OnFinished();
	}
}



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ConstructingAddonExpert
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////
//

void ConstructingAddonExpert::OnBuildingCreated()
{
	m_buildingCreated = true;
}


void ConstructingAddonExpert::OnMainBuildingDestroyed(MyBuilding * pDestroyed)
{
	if (pDestroyed == m_pMainBuilding)
	{
		assert_throw(Started());
		m_pMainBuilding = nullptr;
//		if (!Unselected()) OnFinished();
		OnFinished();
	}
}


string ConstructingAddonExpert::ToString() const
{
	string s = "constr. add-on ";

	switch (m_tid)
	{
	case Terran_Comsat_Station:		s += "Comsat"; break;
	case Terran_Control_Tower:		s += "ControlTower"; break;
	case Terran_Covert_Ops:			s += "CovertOps"; break;
	case Terran_Machine_Shop:		s += "MachineShop"; break;
	case Terran_Nuclear_Silo:		s += "Silo"; break;
	case Terran_Physics_Lab:		s += "Lab"; break;
	default: s += UnitType(m_tid).getName(); break;
	}

	if (m_pMainBuilding) s += " at #" + to_string(m_pMainBuilding->Unit()->getID());
	return s;
}


bool ConstructingAddonExpert::Ready() const
{
	return any_of(me().Buildings(MainBuildingType()).begin(), me().Buildings(MainBuildingType()).end(),
		[](const unique_ptr<MyBuilding> & b){ return b->Completed() && !b->Flying() && !b->Unit()->isTraining() && !b->Unit()->getAddon(); });
}


void ConstructingAddonExpert::UpdatePriority_specific()
{
	UpdateConstructingAddonPriority();
}


void ConstructingAddonExpert::OnFrame()
{
//	return pMiner->ChangeBehavior<Constructing>(pMiner, m_tid);
//	OnFinished();
	if (Selected())
	{
		assert_throw(!m_pMainBuilding);

		for (auto & b : me().Buildings(MainBuildingType()))
			if (b->Completed() && !b->Unit()->isTraining() && !b->Unit()->getAddon())
				if (b->CanAcceptCommand())
				{
					m_pMainBuilding = b.get();
					break;
				}

		if (m_pMainBuilding)
		{
			m_pMainBuilding->BuildAddon(m_tid, check_t::no_check);
			m_buildingCreated = false;
			SetStarted();
			m_startingFrame = ai()->Frame();
		}
	}
	else
	{
		assert_throw(Started());
		assert_throw(m_pMainBuilding);

		if (ai()->Frame() - m_startingFrame > 10) return ConstructionAborted();

		if (m_buildingCreated)
		{
			m_pMainBuilding = nullptr;
			OnFinished();
		}
	}

}






} // namespace iron



