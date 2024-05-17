//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "shop.h"
#include "production.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/zealotRush.h"
#include "../strategy/marineRush.h"
#include "../strategy/dragoonRush.h"
#include "../strategy/cannonRush.h"
#include "../strategy/wraithRush.h"
#include "../strategy/terranFastExpand.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


static bool researchMinesBeforeIonThrusters()
{
	if (him().IsZerg())
		return him().MayHydraOrLurker();

	if (ai()->GetStrategy()->Detected<WraithRush>())
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Machine_Shop>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructingAddon<Terran_Machine_Shop> : public ConstructingAddonExpert
{
public:
						ExpertInConstructingAddon() : ConstructingAddonExpert(Terran_Machine_Shop) {}

	void				UpdateConstructingAddonPriority() override;

private:
};


void ExpertInConstructingAddon<Terran_Machine_Shop>::UpdateConstructingAddonPriority()
{
	if (Interactive::moreTanks) { m_priority = 590; return; }

	if (ai()->GetStrategy()->Detected<MarineRush>())
		{ m_priority = 0; return; }

	if (me().Buildings(Terran_Machine_Shop).size() == 1)
		if (auto * pDragoonRush = ai()->GetStrategy()->Detected<DragoonRush>())
			if (!pDragoonRush->ConditionToStartSecondShop())
			{
				m_priority = 0;
				return;
			}

	if (me().Buildings(Terran_Machine_Shop).size() == 0)
	{
		if (ai()->GetStrategy()->TimeToBuildFirstShop())
			m_priority = 10000;
		else
			m_priority = 0;

		return;
	}

	if (me().Buildings(Terran_Machine_Shop).size() < 2)
	{
		m_priority = 590;

		if (him().IsTerran())
		{
			//if (!ai()->GetStrategy()->Detected<TerranFastExpand>())
			//if (!ai()->GetStrategy()->Detected<WraithRush>())
				if (me().Units(Terran_Wraith).size() < 2)
				if (me().Units(Terran_Goliath).size() < 2)
					m_priority = 0;
			
		}

		if (him().ZerglingPressure())
			m_priority = 0;

		return;
	}

	if (me().Buildings(Terran_Machine_Shop).size() < me().Buildings(Terran_Factory).size() / 2)
	{
		m_priority = 500;
		return;
	}

	m_priority = 0;
}



template<>
class ExpertInResearching<TechTypes::Enum::Spider_Mines> : public ResearchingExpert
{
public:
						ExpertInResearching(MyBuilding * pWhere) : ResearchingExpert(TechTypes::Enum::Spider_Mines, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInResearching<TechTypes::Enum::Spider_Mines>::UpdateResearchingPriority()
{
	if (him().ZerglingPressure())
		{ m_priority = 390; return; }

	if ((him().AllUnits(Zerg_Mutalisk).size() > 0) || (him().Buildings(Zerg_Spire).size() > 0))
		if (me().Buildings(Terran_Command_Center).size() < 2)
			{ m_priority = 0; return; }

	if (him().IsProtoss())
	{
		if (!him().MayDarkTemplar() && (me().Units(Terran_Vulture).size() < 2))
	//	if (!me().HasResearched(TechTypes::Tank_Siege_Mode))
			{ m_priority = 0; return; }

		if (me().Units(Terran_Vulture).size() < 5)
			if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*2 < (int)him().AllUnits(Protoss_Dragoon).size()*1)
				{ m_priority = 0; return; }
	}

	m_priority = researchMinesBeforeIonThrusters() ? 580 : 570;

	if (him().IsProtoss())
		if (me().Production().GasAvailable() < TaskCost().Gas()*3/4)
		{ m_priority = 385; return; }

	if (!(me().HasResearched(TechTypes::Tank_Siege_Mode) || me().Player()->isResearching(TechTypes::Tank_Siege_Mode)) &&
		!(me().HasResearched(TechTypes::Spider_Mines) || me().Player()->isResearching(TechTypes::Spider_Mines)) &&
		!(me().HasUpgraded(UpgradeTypes::Ion_Thrusters) || me().Player()->isUpgrading(UpgradeTypes::Ion_Thrusters)))
		m_priority += 10000;
}



template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Ion_Thrusters> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Ion_Thrusters, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Ion_Thrusters>::UpdateResearchingPriority()
{
	if (him().HasCannons() || him().MayReaver())
		if (!me().HasResearched(TechTypes::Tank_Siege_Mode))
			{ m_priority = 0; return; }

	if (him().IsProtoss())
	{
		if (me().Units(Terran_Vulture).size() < 3)
			{ m_priority = 0; return; }

		if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode)*4 < (int)him().AllUnits(Protoss_Dragoon).size()*3)
			{ m_priority = 0; return; }
	}

	m_priority = researchMinesBeforeIonThrusters() ? 570 : 580;

	if (him().IsProtoss())
		if (me().Production().GasAvailable() < TaskCost().Gas()*3/4)
		{ m_priority = 385; return; }

	if (!(me().HasResearched(TechTypes::Tank_Siege_Mode) || me().Player()->isResearching(TechTypes::Tank_Siege_Mode)) &&
		!(me().HasResearched(TechTypes::Spider_Mines) || me().Player()->isResearching(TechTypes::Spider_Mines)) &&
		!(me().HasUpgraded(UpgradeTypes::Ion_Thrusters) || me().Player()->isUpgrading(UpgradeTypes::Ion_Thrusters)))
		m_priority += 10000;
}



template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Charon_Boosters> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Charon_Boosters, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Charon_Boosters>::UpdateResearchingPriority()
{
	if (!(me().HasResearched(TechTypes::Tank_Siege_Mode) ||
			me().HasResearched(TechTypes::Spider_Mines) ||
			me().HasUpgraded(UpgradeTypes::Ion_Thrusters)))
		{ m_priority = 0; return; }

	if (me().Units(Terran_Goliath).size() == 0)
		{ m_priority = 0; return; }

	if (Interactive::moreGoliaths)
		m_priority = 585;

	m_priority = 550;
}



template<>
class ExpertInResearching<TechTypes::Enum::Tank_Siege_Mode> : public ResearchingExpert
{
public:
						ExpertInResearching(MyBuilding * pWhere) : ResearchingExpert(TechTypes::Enum::Tank_Siege_Mode, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInResearching<TechTypes::Enum::Tank_Siege_Mode>::UpdateResearchingPriority()
{
	if (ai()->GetStrategy()->Detected<CannonRush>())
		{ m_priority = 20000; return; }

	if (him().IsZerg())
		if (!him().HydraPressure())
			if (me().Buildings(Terran_Command_Center).size() < 2)
				{ m_priority = 0; return; }

	if (him().IsProtoss())
		if (me().SupplyMax() == 26)
			if (!me().BuildingsBeingTrained(Terran_Supply_Depot))
				{ m_priority = 0; return; }

	m_priority = 560;

	if (me().Units(Terran_Siege_Tank_Tank_Mode).size() == 0)
		{ m_priority = 0; return; }


	if (him().IsProtoss())
		m_priority = 585;

	if (Interactive::moreTanks)
		m_priority = 585;

	if (him().IsProtoss())
		if (me().Production().GasAvailable() < TaskCost().Gas()*3/4)
		{ m_priority = 385; return; }

	if (!(me().HasResearched(TechTypes::Tank_Siege_Mode) || me().Player()->isResearching(TechTypes::Tank_Siege_Mode)) &&
		!(me().HasResearched(TechTypes::Spider_Mines) || me().Player()->isResearching(TechTypes::Spider_Mines)) &&
		!(me().HasUpgraded(UpgradeTypes::Ion_Thrusters) || me().Player()->isUpgrading(UpgradeTypes::Ion_Thrusters)))
		m_priority += 10000;
}


ExpertInConstructingAddon<Terran_Machine_Shop>	My<Terran_Machine_Shop>::m_ConstructingAddonExpert;


ConstructingAddonExpert * My<Terran_Machine_Shop>::GetConstructingAddonExpert() { return &m_ConstructingAddonExpert; }

My<Terran_Machine_Shop>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Machine_Shop);

	AddResearchingExpert<TechTypes::Enum::Spider_Mines>();
	AddResearchingExpert<TechTypes::Enum::Tank_Siege_Mode>();
	AddUpgragingExpert<UpgradeTypes::Enum::Ion_Thrusters>();
	AddUpgragingExpert<UpgradeTypes::Enum::Charon_Boosters>();


	m_ConstructingAddonExpert.OnBuildingCreated();
}


ConstructingAddonExpert * My<Terran_Machine_Shop>::ConstructingThisAddonExpert()
{CI(this);
	return &m_ConstructingAddonExpert;
}


void My<Terran_Machine_Shop>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}


	
} // namespace iron



