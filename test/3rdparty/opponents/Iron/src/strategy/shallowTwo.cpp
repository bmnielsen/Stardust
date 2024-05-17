//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "shallowTwo.h"
/*
#include "strategy.h"
#include "expand.h"
#include "scan.h"
#include "lateCore.h"
#include "../units/him.h"
#include "../units/comsat.h"
#include "../behavior/kiting.h"
#include "../behavior/fighting.h"
#include "../behavior/scouting.h"
#include "../behavior/walking.h"
#include "../behavior/defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }




namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class ShallowTwo
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


ShallowTwo::ShallowTwo()
{
}


ShallowTwo::~ShallowTwo()
{
}


string ShallowTwo::StateDescription() const
{
	if (!m_keepMarinesAtHome) return "-";
	if (m_delayExpansion) return "delayExpansion";
	if (m_keepMarinesAtHome) return "keepMarinesAtHome";

	return "-";
}




void ShallowTwo::OnFrame_v()
{
	if (him().IsZerg() || him().IsTerran()) return Discard();

	const Area * pAreaToHold = homeAreaToHold();

	if (m_started)
	{
		if (m_keepMarinesAtHome)
		{
			if (him().MayDarkTemplar())
				if (me().TotalAvailableScans() >= min(3, min(2*(int)me().Buildings(Terran_Comsat_Station).size(), 1 + (int)him().AllUnits(Protoss_Dark_Templar).size()/2)))
				{
				///	bw << "stop keepMarinesAtHome" << endl;

					m_keepMarinesAtHome = false;
					for (UnitType t : {Terran_Marine, Terran_Medic, Terran_Siege_Tank_Tank_Mode})
						for (const auto & m : me().Units(t))
							if (m->Completed())
								if (m->GetStronghold())
									m->ChangeBehavior<DefaultBehavior>(m.get());
				}
		}
		else
		{
			if (him().MayDarkTemplar())
				if (me().TotalAvailableScans() == 0)
					if (ai()->Frame() - lastScan() > 100)
						if (none_of(him().Units(Protoss_Dark_Templar).begin(), him().Units(Protoss_Dark_Templar).end(),
							[](HisUnit * u){ return u->Unit()->isDetected(); }))
				{
				///	bw << "keepMarinesAtHome again" << endl;

					m_keepMarinesAtHome = true;
				}
		}
	}
	else
	{
		int hisUnitsHere = 0;
		for (const auto & u : him().Units())
			if (u->Is(Protoss_Zealot) || u->Is(Protoss_Dragoon))
				if (u->GetArea(check_t::no_check) == me().GetArea())
					if (groundDist(u->Pos(), me().StartingBase()->Center()) < 30*32)
						++hisUnitsHere;


		if (hisUnitsHere == 0)
		if ((count_if(me().Units(Terran_Medic).begin(), me().Units(Terran_Medic).end(), [pAreaToHold](const unique_ptr<MyUnit> & u)
				{ return u->Completed() && (u->GetArea() == pAreaToHold); }) >= 2) ||
			(me().CompletedUnits(Terran_Medic) >= 3) ||
	//		(me().CompletedUnits(Terran_Marine) >= 6) ||
			(him().HasCannons() && (me().Units(Terran_Siege_Tank_Tank_Mode).size() >= 1)) ||
			him().MayCarrier() ||
			//((him().CreationCount(Protoss_Zealot) >= 4) && (him().CreationCount(Protoss_Dragoon) == 0)) ||
			ai()->GetStrategy()->Active<LateCore>())
		{
			DO_ONCE
			{
				m_mayDragoonOnly = true;

				if (ai()->GetStrategy()->Active<LateCore>() ||
					him().HasCannons() ||
					him().MayCarrier() ||
					//(him().Buildings(Protoss_Forge).size() > 0) ||
					(him().Buildings(Protoss_Robotics_Facility).size() > 0) ||
					(him().Buildings(Protoss_Robotics_Support_Bay).size() > 0) ||
					(him().Buildings(Protoss_Observatory).size() > 0) ||
					(him().Buildings(Protoss_Stargate).size() > 0) ||
					//(him().CreationCount(Protoss_Zealot) >= 8) ||
					him().MayDarkTemplar() ||
					//(him().Buildings(Protoss_Gateway).size() < 2) && !him().MayDragoon() ||
					(him().Buildings(Protoss_Nexus).size() >= 2) ||
					((him().Buildings(Protoss_Nexus).size() == 1) &&
						!contains(ai()->GetMap().StartingLocations(), him().Buildings(Protoss_Nexus).front()->TopLeft())))
						m_mayDragoonOnly = false;

				if (m_mayDragoonOnly)
				{
					m_lastTimeHisNaturalWasChecked = ai()->Frame();
					if (him().Natural())
						if (me().BestComsat() && me().BestComsat()->CanAcceptCommand())
							me().BestComsat()->Scan(him().Natural()->Center());
				}
			}

			if (m_mayDragoonOnly && !m_delayExpansion)
			{
				if (ai()->Frame() - m_lastTimeHisNaturalWasChecked < 30) return;

				DO_ONCE
					if (him().Buildings(Protoss_Nexus).size() < 2)
					{
						m_expansionWasDelayed = true;
						m_delayExpansion = true;
					}
			}

	//		m_expansionWasDelayed = true;
	//		m_delayExpansion = true;

			if (m_delayExpansion)
			{
				if (me().HasUpgraded(UpgradeTypes::U_238_Shells))
					if (him().AllUnits(Protoss_Dragoon).size() > him().AllUnits(Protoss_Zealot).size())
					{
						if (me().HasResearched(TechTypes::Tank_Siege_Mode) && (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) >= 2))
							m_delayExpansion = false;
					}
					else
					{
						if (me().CompletedUnits(Terran_Vulture) >= 2)
							m_delayExpansion = false;
					}
			}

			if (!m_delayExpansion)
				DO_ONCE
				{
					m_started = true;
					m_keepMarinesAtHome = false;
					for (UnitType t : {Terran_Marine, Terran_Medic, Terran_Siege_Tank_Tank_Mode})
						for (const auto & m : me().Units(t))
							if (m->Completed())
								if (m->GetStronghold())
									m->ChangeBehavior<DefaultBehavior>(m.get());

					return;
				}
		}
	}

}


} // namespace iron

*/

