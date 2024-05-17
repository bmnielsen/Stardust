//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "academy.h"
#include "army.h"
#include "../behavior/defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/shallowTwo.h"
#include "../strategy/zealotRush.h"
#include "../strategy/expand.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Academy>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructing<Terran_Academy> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Academy) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Academy>::UpdateConstructingPriority()
{
	const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });

	if (!(him().IsZerg() || him().IsProtoss()))
	{
		// disabled for Terran
		m_priority = 0;

		// unless clocked wraith threat
		if (Buildings() == 0)
			if (me().CompletedBuildings(Terran_Command_Center) >= 2)
			if (me().CompletedBuildings(Terran_Armory) >= 1)
				if ((him().AllUnits(Terran_Wraith).size() >= 2) ||
					(him().AllUnits(Terran_Ghost).size() >= 1) ||
					(me().Buildings(Terran_Command_Center).size() >= 3))
					m_priority = 650;

		return;
	}

	if (me().CompletedBuildings(Terran_Barracks) == 0) { m_priority = 0; return; }

//	if ((him().IsProtoss() ? me().Buildings(Terran_Refinery).size() : me().CompletedBuildings(Terran_Refinery))
//		== 0) { m_priority = 0; return; }

	if (ai()->GetStrategy()->Detected<ZealotRush>())
		{ m_priority = 0; return; }

	if (auto s = ai()->GetStrategy()->Active<Expand>())
		if (him().IsProtoss())
			if (activeBases < 2)
				{ m_priority = 0; return; }

	if (him().IsZerg())
		if (me().CompletedBuildings(Terran_Engineering_Bay) == 0) { m_priority = 0; return; }	// Ebay first

	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (Buildings() == 0)
	{
/*
		if (him().IsProtoss() && !him().HasCannons())
//			if (me().SupplyUsed() >= 24)
				{ m_priority = 610; return; }
*/
		if (me().Buildings(Terran_Command_Center).size() >= 2)
		{
			if (me().Army().HisInvisibleUnits() >= 1)
				if (me().Army().GroundLead())
					{ m_priority = 500; return; }

			if (him().IsProtoss())
			{
				if (me().Buildings(Terran_Engineering_Bay).empty() ||
					any_of(me().Buildings(Terran_Factory).begin(), me().Buildings(Terran_Factory).end(),
							[](const unique_ptr<MyBuilding> & b){ return b->Completed() && !b->Unit()->isTraining(); }))
					{ m_priority = 0; return; }
			}

			static frame_t startingFrame = ai()->Frame();
			m_priority = min(500, 100 + 50*me().Army().HisInvisibleUnits() + (ai()->Frame() - startingFrame)/10); return;
		}
	}

	m_priority = 0;
}


ExpertInConstructing<Terran_Academy>	My<Terran_Academy>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Academy>::GetConstructingExpert() { return &m_ConstructingExpert; }




template<>
class ExpertInResearching<TechTypes::Enum::Stim_Packs> : public ResearchingExpert
{
public:
						ExpertInResearching(MyBuilding * pWhere) : ResearchingExpert(TechTypes::Enum::Stim_Packs, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInResearching<TechTypes::Enum::Stim_Packs>::UpdateResearchingPriority()
{
	if (him().IsProtoss())
	{/*
		if (me().GasAvailable() < Cost(TechType(TechTypes::Enum::Stim_Packs)).Gas() - 25)
			m_priority = 550;
		else
			m_priority = 1100;
		return;*/
	}

	m_priority = 0;
}



template<>
class ExpertInResearching<TechTypes::Enum::Optical_Flare> : public ResearchingExpert
{
public:
						ExpertInResearching(MyBuilding * pWhere) : ResearchingExpert(TechTypes::Enum::Optical_Flare, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInResearching<TechTypes::Enum::Optical_Flare>::UpdateResearchingPriority()
{
/*
	if (me().Units(Terran_Medic).size() >= 1)
		if (me().Bases().size() >= 3)
			if (me().SupplyUsed() >= 150)
			if (me().MineralsAvailable() >= 800)
			if (me().GasAvailable() >= 400)
			{
				m_priority = 500;
				return;
			}
*/
	m_priority = 0;
}




template<>
class ExpertInUpgraging<UpgradeTypes::Enum::U_238_Shells> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::U_238_Shells, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::U_238_Shells>::UpdateResearchingPriority()
{
/*
	if (him().IsProtoss())
	{
		if (me().GasAvailable() < Cost(UpgradeType(UpgradeTypes::Enum::U_238_Shells)).Gas() - 25)
			m_priority = 300;
		else
		{
			m_priority = 900;

			if (ai()->GetStrategy()->Has<ShallowTwo>() && ai()->GetStrategy()->Has<ShallowTwo>()->DelayExpansion())
			{
				int idleBarracks = count_if(me().Buildings(Terran_Barracks).begin(), me().Buildings(Terran_Barracks).end(),						
						[](const unique_ptr<MyBuilding> & b){ return b->Completed() && !b->Unit()->isTraining(); });
				if (!((idleBarracks == 0) ||
					(idleBarracks == 1) && (me().MineralsAvailable() >= Cost(UpgradeType(UpgradeTypes::Enum::U_238_Shells)).Minerals() - 25)))
					m_priority = 300;
			}
		}

		return;
	}
*/
	m_priority = 0;
}



My<Terran_Academy>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Academy);

	AddUpgragingExpert<UpgradeTypes::Enum::U_238_Shells>();
	AddResearchingExpert<TechTypes::Enum::Stim_Packs>();
	AddResearchingExpert<TechTypes::Enum::Optical_Flare>();


	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Academy>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}

	
} // namespace iron



