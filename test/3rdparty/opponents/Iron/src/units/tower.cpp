//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "tower.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Control_Tower>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructingAddon<Terran_Control_Tower> : public ConstructingAddonExpert
{
public:
						ExpertInConstructingAddon() : ConstructingAddonExpert(Terran_Control_Tower) {}

	void				UpdateConstructingAddonPriority() override;

private:
};


void ExpertInConstructingAddon<Terran_Control_Tower>::UpdateConstructingAddonPriority()
{
	if (me().Buildings(Terran_Control_Tower).size() < 1)
		if (me().Buildings(Terran_Command_Center).size() >= 2)
			if (me().Units(Terran_Wraith).size() >= 1)
			{
				m_priority = 500;
				return;
			}

/*
	if (me().Buildings(Terran_Control_Tower).size() < 1)
	{//drop
		if (him().IsProtoss())
			if (me().Units(Terran_Wraith).size() >= 1)
			{
				m_priority = 610;
				return;
			}
*/
/*
		if ((him().AllUnits(Protoss_Carrier).size() > 0)/* ||
			(me().Units(Terran_Wraith).size() > 0)*//*)
		{
			m_priority = 500;
			return;
		}
	}
*/

	m_priority = 0;
}



template<>
class ExpertInResearching<TechTypes::Enum::Cloaking_Field> : public ResearchingExpert
{
public:
						ExpertInResearching(MyBuilding * pWhere) : ResearchingExpert(TechTypes::Enum::Cloaking_Field, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInResearching<TechTypes::Enum::Cloaking_Field>::UpdateResearchingPriority()
{
	if (me().GasAvailable() > TaskCost().Gas() - 50)
	{
		m_priority = 500;
		return;
	}

	m_priority = 0;
}



template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Apollo_Reactor> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Apollo_Reactor, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Apollo_Reactor>::UpdateResearchingPriority()
{
	if (me().Units(Terran_Wraith).size() >= 3)
		if (me().Bases().size() >= 3)
			if (me().SupplyUsed() >= 150)
			if (me().MineralsAvailable() >= 800)
			if (me().GasAvailable() >= 400)
			{
				m_priority = 500;
				return;
			}

	m_priority = 0;
}


ExpertInConstructingAddon<Terran_Control_Tower>	My<Terran_Control_Tower>::m_ConstructingAddonExpert;


ConstructingAddonExpert * My<Terran_Control_Tower>::GetConstructingAddonExpert() { return &m_ConstructingAddonExpert; }

My<Terran_Control_Tower>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Control_Tower);

	AddResearchingExpert<TechTypes::Enum::Cloaking_Field>();
	AddUpgragingExpert<UpgradeTypes::Enum::Apollo_Reactor>();


	m_ConstructingAddonExpert.OnBuildingCreated();
}


ConstructingAddonExpert * My<Terran_Control_Tower>::ConstructingThisAddonExpert()
{CI(this);
	return &m_ConstructingAddonExpert;
}


void My<Terran_Control_Tower>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}


	
} // namespace iron



