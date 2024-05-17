//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "armory.h"
#include "../strategy/strategy.h"
#include "../strategy/wraithRush.h"
#include "../behavior/defaultBehavior.h"
#include "../interactive.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Armory>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructing<Terran_Armory> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Armory) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Armory>::UpdateConstructingPriority()
{
	const int activeBases = count_if(me().Bases().begin(), me().Bases().end(), [](VBase * b){ return b->Active(); });

	if (me().CompletedBuildings(Terran_Factory) == 0) { m_priority = 0; return; }

	if (Interactive::moreGoliaths)
		if (Buildings() < 1)
			{ m_priority = 590; return; }

	if (him().IsProtoss())
		if (me().CompletedBuildings(Terran_Engineering_Bay) == 0) { m_priority = 0; return; }

	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (Buildings() < 1)
	{
		if (ai()->GetStrategy()->Detected<WraithRush>())
			if (me().Buildings(Terran_Command_Center).size() >= 2)
			{
				m_priority = 450;
				return;
			}

		if (him().AllUnits(Protoss_Carrier).size() > 0)
		{
			m_priority = 500;
			return;
		}

		if (him().IsProtoss() && (activeBases >= 2) ||
			!him().IsProtoss() && me().Buildings(Terran_Command_Center).size() >= 2)
			if (!him().IsTerran() ||
				him().MayWraith() ||
				him().AllUnits(Terran_Siege_Tank_Tank_Mode).empty() && !him().AllUnits(Terran_Goliath).empty() ||
				(me().SupplyUsed() >= 80))
			{
				static frame_t startingFrame = ai()->Frame();
				m_priority = min(500, (him().MayMuta() ? 350 : 350) + (ai()->Frame() - startingFrame)/10);
				
				if (him().MayMuta())
					if ((me().Buildings(Terran_Engineering_Bay).size() > 0) && (me().CompletedBuildings(Terran_Engineering_Bay) == 0))
						m_priority = 500;
				return;
			}
	}
	else if (Buildings() < 2)
	{
		if (me().Buildings(Terran_Command_Center).size() >= 3)
			{ m_priority = 350; return; }
	}
	else if (Buildings() < 4)
	{
		if (all_of(me().Buildings(Terran_Armory).begin(), me().Buildings(Terran_Armory).end(), [](const unique_ptr<MyBuilding> & b)
				{ return b->TimeToResearch() > 0; }))
		{
			const int flyingForce =
				me().Units(Terran_Battlecruiser).size() +
				me().Units(Terran_Valkyrie).size() + 
				me().Units(Terran_Wraith).size();

			if (flyingForce >= 3)
				{ m_priority = 350; return; }
		}
	}

	m_priority = 0;
}


ExpertInConstructing<Terran_Armory>	My<Terran_Armory>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Armory>::GetConstructingExpert() { return &m_ConstructingExpert; }




template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Terran_Vehicle_Weapons> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Terran_Vehicle_Weapons, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Terran_Vehicle_Weapons>::UpdateResearchingPriority()
{
	if (ai()->GetStrategy()->Detected<WraithRush>()) { m_priority = 0; return; }

	m_priority = 410 - 50*me().Player()->getUpgradeLevel(UpgradeTypes::Enum::Terran_Vehicle_Weapons);
	
	if (me().CountMechGroundFighters() >= 20) m_priority += me().CountMechGroundFighters()*2;
}



template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Terran_Vehicle_Plating> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Terran_Vehicle_Plating, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Terran_Vehicle_Plating>::UpdateResearchingPriority()
{
	if (ai()->GetStrategy()->Detected<WraithRush>()) { m_priority = 0; return; }

	m_priority = 409 - 50*me().Player()->getUpgradeLevel(UpgradeTypes::Enum::Terran_Vehicle_Plating);
	
	if (me().CountMechGroundFighters() >= 20) m_priority += me().CountMechGroundFighters()*2;
}



template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Terran_Ship_Weapons> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Terran_Ship_Weapons, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Terran_Ship_Weapons>::UpdateResearchingPriority()
{
	if (ai()->GetStrategy()->Detected<WraithRush>()) { m_priority = 0; return; }

	const int flyingForce =
		me().Units(Terran_Battlecruiser).size() +
		me().Units(Terran_Valkyrie).size() + 
		me().Units(Terran_Wraith).size();

	m_priority = min(450, 50*flyingForce) / (1 + me().Player()->getUpgradeLevel(UpgradeTypes::Enum::Terran_Ship_Weapons));
}



template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Terran_Ship_Plating> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Terran_Ship_Plating, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Terran_Ship_Plating>::UpdateResearchingPriority()
{
	if (ai()->GetStrategy()->Detected<WraithRush>()) { m_priority = 0; return; }

	const int flyingForce =
		me().Units(Terran_Battlecruiser).size() +
		me().Units(Terran_Valkyrie).size() + 
		me().Units(Terran_Wraith).size();

	m_priority = max(0, (min(450, 50*flyingForce) / (1 + me().Player()->getUpgradeLevel(UpgradeTypes::Enum::Terran_Ship_Plating)) - 1));
}



My<Terran_Armory>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Armory);

	AddUpgragingExpert<UpgradeTypes::Enum::Terran_Vehicle_Weapons>();
	AddUpgragingExpert<UpgradeTypes::Enum::Terran_Vehicle_Plating>();
	AddUpgragingExpert<UpgradeTypes::Enum::Terran_Ship_Weapons>();
	AddUpgragingExpert<UpgradeTypes::Enum::Terran_Ship_Plating>();

	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Armory>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;
}

	
} // namespace iron



