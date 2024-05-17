//////////////////////////////////////////////////////////////////////////
//
// This file is part of Iron's source files.
// Iron is free software, licensed under the MIT/X11 License. 
// A copy of the license is provided with the library in the LICENSE file.
// Copyright (c) 2016, 2017, Igor Dimitrijevic
//
//////////////////////////////////////////////////////////////////////////


#include "killingMine.h"
#include "defaultBehavior.h"
#include "../Iron.h"

namespace { auto & bw = Broodwar; }


namespace iron
{


//////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                          //
//                                  class KillingMine
//                                                                                          //
//////////////////////////////////////////////////////////////////////////////////////////////

vector<KillingMine *> KillingMine::m_Instances;

KillingMine::KillingMine(MyUnit * pAgent, HisBWAPIUnit * pTarget)
	: Behavior(pAgent, behavior_t::KillingMine), m_pTarget(pTarget), m_FaceOff(pAgent, CI(pTarget)->IsHisBWAPIUnit())
{
	PUSH_BACK_UNCONTAINED_ELEMENT(m_Instances, this);
	assert_throw(pTarget);

///	ai()->SetDelay(100);
///	bw << Agent()->NameWithId() << " attacks " << pTarget->NameWithId() << endl;
}


KillingMine::~KillingMine()
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


void KillingMine::ChangeState(state_t st)
{CI(this);
	assert_throw(m_state != st);
	
	m_state = st; OnStateChanged();
}


void KillingMine::OnOtherBWAPIUnitDestroyed_v(BWAPIUnit * other)
{CI(this);
	if (Target() == other)
		return Agent()->ChangeBehavior<DefaultBehavior>(Agent());
}


HisBWAPIUnit * KillingMine::NearestTarget() const
{
	int minDist = numeric_limits<int>::max();
	HisBWAPIUnit * pNearestTarget = nullptr;

	for (const auto & fo : Agent()->IsMyUnit()->FaceOffs())
		if (fo.MyAttack())
			if (fo.AirDistanceToHitHim() < minDist)
			{
				pNearestTarget = fo.His();
				minDist = fo.AirDistanceToHitHim();
			}

	return pNearestTarget;
}


void KillingMine::OnFrame_v()
{CI(this);

#if DEV
	assert_throw(ai()->GetMap().Valid(Agent()->Pos()));
	assert_throw(ai()->GetMap().Valid(Target()->Pos()));
	drawLineMap(Agent()->Pos(), Target()->Pos(), GetColor());//1
	bw->drawBoxMap(Target()->Pos() - 25, Target()->Pos() + 25, GetColor());//1
#endif

	if (!Agent()->CanAcceptCommand()) return;

	m_FaceOff = FaceOff(Agent()->IsMyUnit(), Target()->IsHisBWAPIUnit());


	if (JustArrivedInState())
	{
		m_inStateSince = ai()->Frame();
	}

	if (!Agent()->CoolDown())
	{
		if (!Agent()->Is(Terran_SCV))
			if (roundedDist(Agent()->Pos(), Target()->Pos()) < Agent()->GroundRange())
				if (NearestTarget() == Target())
					return Agent()->HoldPosition();

		if (ai()->Frame() - m_lastFrameAttack > 10)
		{
			m_lastFrameAttack = ai()->Frame();
			Agent()->Attack(Target());
		}
	}
	else if (!Agent()->Flying())
		if (m_FaceOff.HisRange() < m_FaceOff.MyRange())
			if (m_FaceOff.DistanceToMyRange() < 0)
			{
				Vect d = toVect(Agent()->Pos() - Target()->Pos());
				d.Normalize();
				Position dest = Agent()->Pos() + toPosition(d*3*32);
				dest = ai()->GetMap().Crop(dest);
#if DEV
				drawLineMap(Agent()->Pos(), dest, GetColor());
				bw->drawCircleMap(dest, 5, GetColor(), bool("isSolid"));
#endif
				return Agent()->Move(dest);
			}

}



} // namespace iron



