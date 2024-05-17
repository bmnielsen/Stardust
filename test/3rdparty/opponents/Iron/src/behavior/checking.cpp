//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "checking.h"
#include "walking.h"
#include "fleeing.h"
#include "defaultBehavior.h"
#include "../units/my.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


MyUnit * Checking::FindCandidate(HisBWAPIUnit * target)
{
	for (UnitType type : {Terran_SCV, Terran_Marine, Terran_Vulture})
		for (MyUnit * u : me().Units())
			if (u->Completed() && !u->Loaded())
				if (u->Type() == type)
					if (!u->GetStronghold())
						if (u->Life() > u->MaxLife()*9/10)
						{
							if (u->Is(Terran_SCV))
							{
								if (!(u->GetBehavior()->IsHarassing() || u->GetBehavior()->IsFleeing())) continue;
							}
							else
							{
								if (!(u->GetBehavior()->IsRaiding() || u->GetBehavior()->IsKiting())) continue;
							}

							bool targetInFaceOffs = false;
							const FaceOff * pClosestFaceOff = nullptr;
							int minDistToHisFire = numeric_limits<int>::max();
							for (const auto & faceOff : u->FaceOffs())
								if (faceOff.HisAttack())
								{
									if (faceOff.His() == target) targetInFaceOffs = true;

									if (faceOff.DistanceToHisRange() < minDistToHisFire)
									{
										minDistToHisFire = faceOff.DistanceToHisRange();
										pClosestFaceOff = &faceOff;
									}
								}

							if (targetInFaceOffs && (pClosestFaceOff->His() == target))
								return u;
						}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class Checking
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<Checking *> Checking::m_Instances;

Checking::Checking(MyUnit * pAgent, HisBWAPIUnit * target)
	: Behavior(pAgent, behavior_t::Checking), m_target(target->Unit()), m_where(target->Pos())
{
	assert_throw(target->InFog());
	assert_throw(!Agent()->GetStronghold());
	assert_throw(Agent()->Flying() || pathExists(Agent()->Pos(), m_where));

	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);

///	ai()->SetDelay(100);
}


Checking::~Checking()
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


void Checking::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


void Checking::OnFrame_v()
{CI(this);
#if DEV
	drawLineMap(Agent()->Pos(), m_where, GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
		return SetSubBehavior<Walking>(Agent(), m_where, __FILE__ + to_string(__LINE__));
	}

	if (Agent()->Life() < Agent()->PrevLife(10))
		return Agent()->ChangeBehavior<Fleeing>(Agent());

//	if (roundedDist(Agent()->Pos(), m_where) < Agent()->Sight() - 32)
//		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());

	const HisUnit * pHisUnit = him().FindUnit(Target());
	const HisBuilding * pHisBuilding = him().FindBuilding(Target());
	if (!((pHisUnit && pHisUnit->InFog()) || (pHisBuilding && pHisBuilding->InFog())))
		return Agent()->ChangeBehavior<Fleeing>(Agent());


	if (ai()->Frame() - m_inStateSince > 5 + ai()->Latency())
		if (ai()->Frame() - Agent()->LastFrameMoving() > 50)
			return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}



} // namespace iron



