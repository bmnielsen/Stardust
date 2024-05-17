//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "retraiting.h"
#include "kiting.h"
#include "walking.h"
#include "laying.h"
#include "defaultBehavior.h"
#include "../strategy/strategy.h"
#include "../strategy/wraithRush.h"
#include "../strategy/terranFastExpand.h"
#include "../territory/vgridMap.h"
#include "../units/factory.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{

bool Retraiting::m_allowed = true;

bool Retraiting::Allowed()
{
//	return false;

	if (me().SupplyUsed() > 80) return m_allowed = false;

	if (him().IsZerg()) return m_allowed = false;

	if (me().CompletedUnits(Terran_Siege_Tank_Tank_Mode) || me().CompletedUnits(Terran_Goliath))
		return m_allowed = false;

	if (ai()->GetStrategy()->Detected<WraithRush>())
		return m_allowed = false;

	if (ai()->GetStrategy()->Detected<TerranFastExpand>())
		return m_allowed = false;

	if (him().Buildings(Terran_Factory).size() == 1)
		if (him().Buildings(Terran_Command_Center).size() >= 2)
			if (him().AllUnits(Terran_Vulture).size() == 0)
				if (me().Units(Terran_Vulture).size() >= 3 *
					(him().AllUnits(Terran_Siege_Tank_Tank_Mode).size() + him().AllUnits(Terran_Siege_Tank_Siege_Mode).size())
					)
					return m_allowed = false;

	m_allowed = false;
	return m_allowed;
}


int Retraiting::Condition(const MyUnit * u, const vector<const FaceOff *> & Threats)
{
	assert_throw(u->Is(Terran_Vulture));

	if (!Allowed()) return 0;

	if (contains(me().GetVArea()->EnlargedArea(), u->GetArea())) return 0;

	int dangerousUnits = 0;
	int hisVultureScore = 0;

	for (const auto * pFaceOff : Threats)
		if (HisUnit * pHisUnit = pFaceOff->His()->IsHisUnit())
			if (!pHisUnit->Type().isWorker())
				switch (pHisUnit->Type())
				{
				case Terran_Vulture:				hisVultureScore += 3;	++dangerousUnits;	break;
				case Terran_Marine:					hisVultureScore += 1;						break;
				case Terran_Siege_Tank_Tank_Mode:	hisVultureScore += 6;	++dangerousUnits;	break;
				case Terran_Goliath:				hisVultureScore += 4;						break;
				case Protoss_Dragoon:				hisVultureScore += 6;	++dangerousUnits;	break;
				case Protoss_Zealot:				hisVultureScore += 1;						break;
				}
	hisVultureScore /= 2;

	if (dangerousUnits == 0) return 0;

	int myVultureScore = 0;
	for (const auto & v : me().Units(Terran_Vulture))
		if (v->Completed() && !v->Loaded())
			if ((v->Life()*3 > v->MaxLife()) || !v->WorthBeingRepaired())
				if (!v->GetBehavior()->IsRetraiting())
				if (!v->GetBehavior()->IsLayingBack())
					if (dist(u->Pos(), v->Pos()) < 12*32)
						++myVultureScore;

	if (myVultureScore <= hisVultureScore) return hisVultureScore + 1;

	return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Retraiting
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Retraiting *> Retraiting::m_Instances;

Retraiting::Retraiting(MyUnit * pAgent, Position target, int numberNeeded)
	: Behavior(pAgent, behavior_t::Retraiting), m_target(target),
		m_numberNeeded(numberNeeded),
		m_pGroup(make_shared<vector<Retraiting *>>())
{
	assert_throw(pAgent->Is(Terran_Vulture));
	assert_throw(!Agent()->GetStronghold());
	assert_throw(pathExists(pAgent->Pos(), target));

	m_pTargetArea = ai()->GetMap().GetNearestArea(WalkPosition(Target()));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
	m_pGroup->push_back(this);

///	bw << pAgent->NameWithId() << " retraiting, need " << numberNeeded << endl;
///	ai()->SetDelay(50);
}


Retraiting::Retraiting(MyUnit * pAgent, Retraiting * pExistingGroup)
	: Retraiting(pAgent, pExistingGroup->Target(), pExistingGroup->m_numberNeeded)
{
	assert_throw(!contains(*pExistingGroup->m_pGroup, this));

	m_pTargetArea = pExistingGroup->m_pTargetArea;

	m_pGroup = pExistingGroup->m_pGroup;
	m_pGroup->push_back(this);
}


Retraiting::~Retraiting()
{
#if !DEV
	try //3
#endif
	{
		really_remove(*m_pGroup, this);

		if (m_releaseGroupWhenLeaving)
			while (!m_pGroup->empty())
			{
				Retraiting * r = m_pGroup->back();
				r->Agent()->ChangeBehavior<DefaultBehavior>(r->Agent());
			}

		assert_throw(contains(m_Instances, this));
		really_remove(m_Instances, this);
	}
#if !DEV
	catch(...){} //3
#endif
}


void Retraiting::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


string Retraiting::StateName() const
{CI(this);
	switch(State())
	{
	case retraiting:
	{
		string s = "retraiting";
		s += " (need " + to_string(m_numberNeeded);
		s += ", time = " + to_string(ai()->Frame() - InStateSince()) + ")";
		return s;
	}
	default:				return "?";
	}
}


void Retraiting::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), m_target, GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
//		if (Agent()->Flying())
//			return Agent()->Move(Target());
//		else
			return SetSubBehavior<Walking>(Agent(), Target(), __FILE__ + to_string(__LINE__));
	}
	
	if (ai()->Frame() - m_inStateSince > 60)
		if (Agent()->Life() < Agent()->PrevLife(10))
			return Agent()->ChangeBehavior<Kiting>(Agent());

	if ((dist(Agent()->Pos(), Target()) < 32*5) && (Agent()->GetArea(check_t::no_check) == TargetArea()) ||
		(ai()->Frame() - m_inStateSince > 400) ||
		(ai()->Frame() - m_inStateSince > 200) && m_growed ||
		contains(me().GetVArea()->EnlargedArea(), Agent()->GetArea()))
	{
		ResetSubBehavior();
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}

	{
		const int tileRadius = 10;
		vector<MyUnit *> MyUnitsAround = ai()->GetGridMap().GetMyUnits(ai()->GetMap().Crop(TilePosition(Agent()->Pos())-tileRadius), ai()->GetMap().Crop(TilePosition(Agent()->Pos())+tileRadius));

		for (MyUnit * u : MyUnitsAround)
			if (u->Is(Terran_Vulture))
				if (!u->GetStronghold())
					if (!contains(me().GetVArea()->EnlargedArea(), u->GetArea()))
						if ((u->Life()*2 > u->MaxLife()) || !u->WorthBeingRepaired())
							if (groundDist(u->Pos(), Agent()->Pos()) < tileRadius*32)
								if (u->GetBehavior()->IsRaiding() ||
									u->GetBehavior()->IsKiting() ||
									u->GetBehavior()->IsRepairing() ||
									u->GetBehavior()->IsRetraiting() && (m_pGroup.get() != u->GetBehavior()->IsRetraiting()->m_pGroup.get()))
								{
									assert_throw(u != Agent());
									m_growed = true;
								///	bw << "merge !!!!!!!!!!!!!!!" << endl;
									if (u->GetBehavior()->IsRetraiting())
										assert_throw(!contains(*u->GetBehavior()->IsRetraiting()->m_pGroup, this));
									u->ChangeBehavior<Retraiting>(u, this);
								}
	}

	{
		if ((int)m_pGroup->size() >= m_numberNeeded)
		{
		///	bw << "release Retraiting group" << endl;
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
		}
	}
/*
	{
		int distToTarget = groundDist(Agent()->Pos(), Target());
		int maxDistToTargetInGroup = numeric_limits<int>::min();
		for (const Retraiting * r : *m_pGroup)
			maxDistToTargetInGroup = max(maxDistToTargetInGroup, groundDist(r->Agent()->Pos(), Target()));
	
		if (distToTarget + 7*32 < maxDistToTargetInGroup)
		{
#if DEV
			for (const Retraiting * r : *m_pGroup)
				drawLineMap(Agent()->Pos(), r->Agent()->Pos(), GetColor());
#endif
		///	bw << Agent()->NameWithId() << " wait" << endl;
			return ResetSubBehavior();
		}
	}
*/

	if (ai()->Frame() - m_inStateSince > 15)
	{
		if (!GetSubBehavior())
			if (ai()->Frame() - Agent()->LastFrameMoving() > 10)
				return SetSubBehavior<Walking>(Agent(), Target(), __FILE__ + to_string(__LINE__));

		if (ai()->Frame() - Agent()->LastFrameMoving() > 50)
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
	}
}



} // namespace iron



