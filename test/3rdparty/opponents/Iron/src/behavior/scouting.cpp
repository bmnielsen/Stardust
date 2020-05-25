//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "scouting.h"
#include "walking.h"
#include "harassing.h"
#include "repairing.h"
#include "exploring.h"
#include "fleeing.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/walling.h"
#include "../units/cc.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Scouting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Scouting *> Scouting::m_Instances;
int Scouting::m_instancesCreated = 0;

Scouting::Scouting(MyUnit * pAgent)
	: Behavior(pAgent, behavior_t::Scouting)
{
	const auto & HisPossibleLocations = ai()->GetStrategy()->HisPossibleLocations();
	assert_throw(!HisPossibleLocations.empty());

	int index = min(2, (int)Instances().size() + (pAgent->GetLastBehaviorType() == behavior_t::Scouting ? 1 : 0));
	if (index < (int)HisPossibleLocations.size())
	{
	///	Log << Agent()->NameWithId() << ": unassigned target = " << HisPossibleLocations[index] << endl;
	}
	else
	{
		index %= HisPossibleLocations.size();	// TODO find best target
	///	Log << Agent()->NameWithId() << ": assigned target = " << HisPossibleLocations[index] << endl;
	}
	assert_throw(index >= 0);
	assert_throw(index < (int)HisPossibleLocations.size());
	m_Target = HisPossibleLocations[index];
	assert_throw_plus(ai()->GetMap().Valid(m_Target), my_to_string(m_Target));
	SetSubBehavior<Walking>(pAgent, Position(Target()), __FILE__ + to_string(__LINE__));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
	++m_instancesCreated;

	if (Agent()->GetStronghold()) Agent()->LeaveStronghold();
}


Scouting::~Scouting()
{
#if !DEV
	try //3
#endif
	{
		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);
	}
#if !DEV
	catch(...){} //3
#endif
}


bool Scouting::CanRepair(const MyBWAPIUnit * , int) const
{
	if (ai()->GetStrategy()->Active<Walling>())
		return false;

	return true;
}

void Scouting::OnFrame_v()
{CI(this);
	if (!Agent()->CanAcceptCommand()) return;
	/*
	{
		if (auto s = ai()->GetStrategy()->Active<Walling>())
			if (me().Units(Terran_Vulture).empty())
				if (!him().AllUnits(Zerg_Zergling).empty())
					if (!none_of(me().Buildings().begin(), me().Buildings().end(),
						[](const MyBuilding * b){ return b->Completed() && b->Life() < b->MaxLife(); }))
						return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}*/

	{	// --> Fleeing
		if (Agent()->Life() < Agent()->PrevLife(10))
			return Agent()->ChangeBehavior<Fleeing>(Agent());

		auto Threats3 = findThreats(Agent(), 3*32);
		if (!Threats3.empty())
		{
			for (auto * faceOff : Threats3)
				if (!faceOff->His()->Type().isWorker())
					return Agent()->ChangeBehavior<Fleeing>(Agent());
		}
	}

	{	// --> Repairing
		if (Agent()->Life() < Agent()->MaxLife())
			if (Agent()->Type().isMechanical())
				if (Agent()->RepairersCount() < Agent()->MaxRepairers())
					if (Repairing * pRepairer = Repairing::GetRepairer(Agent(), (Agent()->Life() > Agent()->MaxLife() - 10) ? 10*32 : (Agent()->Life() > Agent()->MaxLife() - 20) ? 20*32 : 1000000))
						return Agent()->ChangeBehavior<Repairing>(Agent(), pRepairer);
	}

	if (him().StartingBase())
		if (auto pSCV = Agent()->IsMy<Terran_SCV>())
		{
			if (Harassing::Instances().size() == 0)
				if (pSCV->GetArea() == him().GetArea())
					return pSCV->ChangeBehavior<Exploring>(pSCV, pSCV->GetArea());

			return pSCV->ChangeBehavior<Harassing>(pSCV);
		}
		else
		{
			assert_throw(!Agent()->Is(Terran_Siege_Tank_Siege_Mode));
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}

	const bool targetAlreadyReached = !contains(ai()->GetStrategy()->HisPossibleLocations(), Target());
	if (targetAlreadyReached ||
		Agent()->Pos().getApproxDistance(Position(Target()) + Position(UnitType(Terran_Command_Center).tileSize())/2) < 32*6)
	{
	///	if (targetAlreadyReached) Log << Agent()->NameWithId() << ": target already reached" << endl;
		if (!targetAlreadyReached)
			ai()->GetStrategy()->RemovePossibleLocation(Target());

		return Agent()->ChangeBehavior<Scouting>(Agent());
	}
}



} // namespace iron



