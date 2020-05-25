//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "ebay.h"
#include "army.h"
#include "../strategy/strategy.h"
#include "../strategy/wraithRush.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Engineering_Bay>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructing<Terran_Engineering_Bay> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Engineering_Bay) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Engineering_Bay>::UpdateConstructingPriority()
{
	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (Buildings() == 0)
	{
		if (him().IsTerran())
		{
//			if (ai()->GetStrategy()->Detected<WraithRush>()) { m_priority = 0; return; }

			if (him().MayWraith() || ai()->GetStrategy()->Detected<WraithRush>())
//				if (me().Buildings(Terran_Command_Center).size() >= 2)
					{ m_priority = 500; return; }

			if (me().Army().HisInvisibleUnits() >= 1)
				{ m_priority = 500; return; }

			if (me().Buildings(Terran_Command_Center).size() >= 3)
				if (me().SupplyUsed() >= 100)
					if (me().Army().GroundLead())
						{ m_priority = 500; return; }
		}
		else if (him().IsProtoss())
		{
			if (him().MayCarrier() || !him().AllUnits(Protoss_Scout).empty())
					{ m_priority = 500; return; }

			if (him().MayDarkTemplar())
				if (me().Buildings(Terran_Command_Center).size() >= 2)
					{ m_priority = 500; return; }
		
			if (him().HasCannons())
				if (me().Buildings(Terran_Academy).size() >= 1)
					if (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 1)
					if (me().Buildings(Terran_Factory).size() >= 1)
						if (me().Army().GoodValueInTime())
							{ m_priority = 500; return; }

			if (me().CompletedBuildings(Terran_Command_Center) >= 3)
				if ((me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 3) || (me().Buildings(Terran_Barracks).size() >= 5))
					if (me().Army().GroundLead() ||
						him().MayShuttleOrObserver() && me().Army().GoodValueInTime())
						{ m_priority = 500; return; }

			if (me().Buildings(Terran_Factory).size() >= 2)
				if (me().Buildings(Terran_Command_Center).size() >= 2)
					if (him().AllUnits(Protoss_Dragoon).size() + 2*him().AllUnits(Protoss_Reaver).size() <= me().Units(Terran_Siege_Tank_Siege_Mode).size())
					if (him().AllUnits(Protoss_Zealot).size() <= me().Units(Terran_Vulture).size())
						{ m_priority = 50; return; }

			// new
			if (me().Buildings(Terran_Factory).size() >= 1)
				if (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 2)
					if (me().HasResearched(TechTypes::Tank_Siege_Mode) || me().Player()->isResearching(TechTypes::Tank_Siege_Mode))
					{
						if (me().Buildings(Terran_Factory).size() >= 2)
							{ m_priority = 149; return; }
						else
							{ m_priority = 500; return; }
					}
		}
		else
		{
//			if (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 1)
			if (me().Buildings(Terran_Factory).size() >= 2)
			{
				if (!(him().Units(Zerg_Mutalisk).empty() && him().Buildings(Zerg_Spire).empty()) ||
					!him().Buildings(Zerg_Lair).empty() && him().Buildings(Zerg_Lair).front()->Completed())
					{ m_priority = 700; return; }

				if (me().Units(Terran_Vulture).size() >= 5)
				{
					m_priority = 500;

					if (him().ZerglingPressure())
						m_priority = 400;

					if (him().HydraPressure() && (me().Units(Terran_Siege_Tank_Tank_Mode).size() == 0))
						m_priority = 400;
				
					return;
				}
			}
		}
	}

	m_priority = 0;
}


ExpertInConstructing<Terran_Engineering_Bay>	My<Terran_Engineering_Bay>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Engineering_Bay>::GetConstructingExpert() { return &m_ConstructingExpert; }




template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Terran_Infantry_Weapons> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Terran_Infantry_Weapons, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Terran_Infantry_Weapons>::UpdateResearchingPriority()
{
	if (him().IsProtoss())
	{/*
		m_priority = 430;
		if (him().MayDarkTemplar()) m_priority = 230;
		return;*/
	}

	m_priority = 0;
}



template<>
class ExpertInUpgraging<UpgradeTypes::Enum::Terran_Infantry_Armor> : public ResearchingExpert
{
public:
						ExpertInUpgraging(MyBuilding * pWhere) : ResearchingExpert(UpgradeTypes::Enum::Terran_Infantry_Armor, pWhere) {}

	void				UpdateResearchingPriority() override;

private:
};


void ExpertInUpgraging<UpgradeTypes::Enum::Terran_Infantry_Armor>::UpdateResearchingPriority()
{
	if (me().Player()->getUpgradeLevel(UpgradeTypes::Enum::Terran_Infantry_Armor) == 0)
		/*if (him().IsProtoss())
		{
			if (me().Player()->getUpgradeLevel(UpgradeTypes::Enum::Terran_Infantry_Weapons) >= 1)
			{
				m_priority = 420;
				if (him().MayDarkTemplar()) m_priority = 220;
				return;
			}
		}
		else*/
		{
			if ((int)me().Bases().size() >= (him().IsProtoss() ? 3 : 2))
				{ m_priority = 300; return; }
		}

	m_priority = 0;
}



My<Terran_Engineering_Bay>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Engineering_Bay);

	AddUpgragingExpert<UpgradeTypes::Enum::Terran_Infantry_Weapons>();
	AddUpgragingExpert<UpgradeTypes::Enum::Terran_Infantry_Armor>();


	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Engineering_Bay>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;

	if (!him().IsProtoss())  return;

	if (Unit()->isTraining()) return;
	if (me().Bases().size() < 2) return;

	const bool underAttack = Life() < PrevLife(63);
	const Base * pMyStartingBase = me().GetBase(0);
	const Base * pMyNatural = me().GetBase(1);

	static frame_t lastTimeUnderAttack = 0;
	if (underAttack) lastTimeUnderAttack = ai()->Frame();

	{
		if (Flying())
		{
			if (underAttack)
			{
				if (CanAcceptCommand())
					return Move(pMyStartingBase->Center());
			}
			else
				if (ai()->Frame() % 10 == 0)
					if (CanAcceptCommand())
					{
						int naturalCPs = (int)pMyNatural->GetArea()->ChokePoints().size();
						int n = (ai()->Frame() / 200) % (naturalCPs + 1);
						if (n == naturalCPs)
							Move(pMyNatural->Center());
						else
						{
							if (me().StartingVBase()->GetWall() == &ext(pMyNatural->GetArea()->ChokePoints()[n])->GetWall())
								n = (n+1) % naturalCPs;
							Move(center(pMyNatural->GetArea()->ChokePoints()[n]->Center()));
						}
					}
		}
		else
		{
			return Lift(check_t::no_check);
		}

		return;
	}
}

	
} // namespace iron



