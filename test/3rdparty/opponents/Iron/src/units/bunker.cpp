//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "bunker.h"
#include "../territory/stronghold.h"
#include "../behavior/sniping.h"
#include "../behavior/repairing.h"
#include "../behavior/blocking.h"
#include "../strategy/strategy.h"
#include "../strategy/zerglingRush.h"
#include "../strategy/marineRush.h"
#include "../strategy/cannonRush.h"
#include "../strategy/walling.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{



//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class My<Terran_Bunker>
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////


template<>
class ExpertInConstructing<Terran_Bunker> : public ConstructingExpert
{
public:
						ExpertInConstructing() : ConstructingExpert(Terran_Bunker) {}

	void				UpdateConstructingPriority() override;

private:
};


void ExpertInConstructing<Terran_Bunker>::UpdateConstructingPriority()
{
	if (me().CompletedBuildings(Terran_Barracks) == 0) { m_priority = 0; return; }

	if (Builders() < BuildingsUncompleted()) { m_priority = 590; return; }

	if (auto s = ai()->GetStrategy()->Detected<ZerglingRush>())
		if (!s->NoLocationForBunkerForDefenseCP())
			if (ai()->Me().GetVArea()->DefenseCP())
			{
				if (Buildings() < ((int)me().Units(Terran_Marine).size() + 3) / 4)
				{
					m_priority = Buildings() == 0 ? 606 : 621;
					return;
				}
			}
/*
	if (him().IsZerg())
		if (auto s = ai()->GetStrategy()->Active<Walling>())
			if (!s->GetWall()->Open())
				if (me().Units(Terran_Marine).size() >= 3)
					if (him().AllUnits(Zerg_Zergling).size() >= 9)
						if (Buildings() < 1)
						{
							m_priority = 621;
							return;
						}
*/
	if (him().HydraPressure() ||
		him().ZerglingPressure() && !me().StartingVBase()->GetWall() /*||
		(him().AllUnits(Zerg_Zergling).size() >= 8) && ai()->GetStrategy()->Active<Walling>() ||
		(him().AllUnits(Zerg_Zergling).size() >= 15) && (me().Buildings(Terran_Command_Center).size() < 2)*/)
		if (ai()->Me().GetVArea()->DefenseCP())
		{
			if (none_of(him().Units(Zerg_Zergling).begin(), him().Units(Zerg_Zergling).end(), [](const HisUnit * u)
					{ return u->GetArea(check_t::no_check) == me().GetArea(); }))
			if (none_of(him().Units(Zerg_Hydralisk).begin(), him().Units(Zerg_Hydralisk).end(), [](const HisUnit * u)
					{ return u->GetArea(check_t::no_check) == me().GetArea(); }))
			if (Buildings() < ((int)me().Units(Terran_Marine).size() + 3) / 4)
			{
				m_priority = 622;
				return;
			}
		}

	if (auto s = ai()->GetStrategy()->Detected<MarineRush>())
		if (Buildings() == 0)
			if (all_of(him().Units().begin(), him().Units().end(), [](const unique_ptr<HisUnit> & u)
				{ return !u->Is(Terran_Marine) || (groundDist(u->Pos(), me().StartingBase()->Center()) > 32*12); }))
			{
				m_priority = 604;
				return;
			}

/*
	if (ai()->GetStrategy()->Detected<CannonRush>())
		if (Buildings() == 0)
		{
			m_priority = 30000;
			return;
		}
*/
	m_priority = 0;
}


ExpertInConstructing<Terran_Bunker>	My<Terran_Bunker>::m_ConstructingExpert;

ConstructingExpert * My<Terran_Bunker>::GetConstructingExpert() { return &m_ConstructingExpert; }


My<Terran_Bunker>::My(BWAPI::Unit u)
	: MyBuilding(u, make_unique<DefaultBehavior>(this))
{
	assert_throw(u->getType() == Terran_Bunker);

	m_ConstructingExpert.OnBuildingCreated();
}


void My<Terran_Bunker>::DefaultBehaviorOnFrame()
{CI(this);
	if (DefaultBehaviorOnFrame_common()) return;

	if (Completed())
		if (!ai()->GetStrategy()->Detected<ZerglingRush>() ||
			(him().AllUnits(Zerg_Zergling).size() >= 10) && (Blocking::Instances().size() <= 3))
			if (Completed())
				if (GetStronghold())
				{
					const int distToCC = roundedDist(Pos(), GetStronghold()->HasBase()->BWEMPart()->Center());

					if (ai()->Frame() - m_lastCallForRepairer > 250 ||
						Life() < MaxLife()) m_nextWatchingRadius = max(10*32, distToCC + 3*32);

				///	bw->drawCircleMap(Pos(), 40*32, Colors::White);
				///	bw->drawCircleMap(Pos(), m_nextWatchingRadius, Colors::Green);

					if (RepairersCount() < 2)
						if (any_of(him().Units(Zerg_Zergling).begin(), him().Units(Zerg_Zergling).end(), [this](const HisUnit * u)
								{ return dist(Pos(), u->Pos()) < m_nextWatchingRadius; }) ||
							any_of(him().Units(Zerg_Hydralisk).begin(), him().Units(Zerg_Hydralisk).end(), [this](const HisUnit * u)
								{ return dist(Pos(), u->Pos()) < m_nextWatchingRadius; }))
							if (Repairing::GetRepairer(this, 40*32))
							{
							//	ai()->SetDelay(1000);
								m_nextWatchingRadius -= 1*32;	// decrease watching radius each time (avoid exloit)

								m_nextWatchingRadius = max(m_nextWatchingRadius, 6*32);

								m_lastCallForRepairer = ai()->Frame();
								return;
							}
				}

}


void My<Terran_Bunker>::Load(BWAPIUnit * u, check_t checkMode)
{CI(this);
	assert_throw(!u->Loaded());

///	bw << NameWithId() << " load! " << u->NameWithId() << endl;
	bool result = Unit()->load(u->Unit());
	OnCommandSent(checkMode, result, NameWithId() + " load " + u->NameWithId());
}


void My<Terran_Bunker>::Unload(BWAPIUnit * u, check_t checkMode)
{CI(this);
	assert_throw(u->Loaded());

///	bw << NameWithId() << " unload! " << u->NameWithId() << endl;
	bool result = Unit()->unload(u->Unit());
	OnCommandSent(checkMode, result, NameWithId() + " unload " + u->NameWithId());
}


int My<Terran_Bunker>::LoadedUnits() const
{
	return Unit()->getLoadedUnits().size();
}


int My<Terran_Bunker>::Snipers() const
{
	return count_if(Sniping::Instances().begin(), Sniping::Instances().end(), [this](const Sniping * pSniper)
								{ return pSniper->Where() == this; });
}


int My<Terran_Bunker>::MaxRepairers() const
{
	int n =	(Life()*4 > MaxLife()*3) ? 1 :
			(Life()*4 > MaxLife()*2) ? 2 : 3;

	if (him().ZerglingPressure() ||
		him().HydraPressure())
		n += 1;

	if (ai()->GetStrategy()->Detected<ZerglingRush>() && (him().AllUnits(Zerg_Zergling).size() > 8))
		n += 3;

	return n;
}

	
} // namespace iron



